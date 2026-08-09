# Design 05: HIR Optimizer Split + Common Subexpression Elimination (CSE)

Status: **implemented** (2026-08-10; suites: tests/run-cse.sh 15 cases
x {level 0,2} x {interpreter,JIT}, run-abce.sh, run-syntax.sh,
run-class.sh, run-scoping.sh, run-typing.sh, run-ctrans.sh,
run-elisp.sh, run-thread.sh incl. a manual level-2 pass — all green.
tests/bench/b7_dict_cse.noct: interp 1.40s -> 0.94s, JIT 0.45s ->
0.22s at level 2).

**Implementation addenda (deviations from the plan below, reviewed
rationale):**

- **Join/exit alias invalidation (soundness fix).**  Section 3.4's
  original claim "kills from any arm persist, which is exactly the
  required join invalidation" was insufficient: an assignment inside
  an arm not only *kills* the local's old VN, it *aliases* the local
  to a value computed inside the arm (the copy-VN rule in 3.4 step 2),
  and that alias is a positive fact that must not survive the join —
  on the untaken path the local holds a different value.  The
  implementation therefore re-keys (fresh VN) every local assigned in
  any arm at the if-join, and every local assigned in a loop body at
  the loop exit (in addition to the loop-head kill summary).  Both
  reuse the same def-scan (`cse_defs_chain`).  Re-keying is monotone,
  so this is a pure strengthening; the avail-flag scoping was already
  correct.
- **No dump changes**: hir.c's debug dump prints blocks only (no
  expression printer exists), so Part 2.2's dump case does not apply.
- **run-cse.sh is standalone**, mirroring run-abce.sh; neither suite
  is wired into tests/run-all.sh (run-all.sh covers
  syntax/thread/httpserver/webapp only — and its webapp step
  references a nonexistent ../examples/webapp path, a pre-existing
  breakage unrelated to this work).
- CMakePresets.json: `NOCT_ENABLE_OPTIMIZER: ON` was added to the
  `static`, `debug`, `mt-debug`, and `mt-tsan` presets (the ones test
  scripts build against); all cross/target presets keep the default
  OFF.

**Adversarial-review round (2026-08-10, post-implementation).**  A
25-agent multi-lens adversarial review of the implementation found and
confirmed six defects, all fixed the same day:

- R1 (critical, miscompile): the copy-VN alias installed by an
  assignment inside if-arm i leaked into the walks of arm j>i and of
  later elif conditions — regions that execute exactly when arm i did
  not — so mutually-exclusive arms could share value numbers through a
  local (e.g. `if (c) { x = d.k; } else { print(x + 1); }` printed
  d.k+1 instead of x+1 at level 2).  Fix: cse_walk_if re-keys each
  arm's defs immediately after that arm's walk (which also covers the
  join), not once after the whole chain.
- R2 (major, latent miscompile): closing an untracked scope (nesting
  > CSE_MAX_SCOPES) popped the undo log to 0, stranding still-open
  outer scopes' marks above undo_top so facts created afterwards could
  escape their region.  Fix: untracked closes revert to the deepest
  tracked mark (marks are monotone in nesting order, so this never
  under-reverts).
- R3 (major, D-CSE12 violation): $cseN homes could push a function
  past LIR's frame limit, making a program compile at level 0 but fail
  at level 2.  Fix: LIR_TMPVAR_MAX moved to lir.h and the pass budgets
  homes against a conservative per-statement peak-temp bound
  (expression node count + loop-held temps per nesting level); too
  tight ⇒ pass skipped.
- R4 (major, silent optimization loss): sym_vn used 0 as its
  "unassigned" sentinel, but 0 is a valid VN (the first interned
  entry), so the flagship shape `func f(d) { return d.k + d.k; }` got
  zero CSE whenever the base was the first-touched local (e.g. a
  parameter).  Fix: sentinel is now -1.
- R5 (major, crash path): pre-existing hir.c bug, newly reachable from
  cse_assign_homes — hir_add_local's hir_strdup-failure branch was
  missing `return false` and linked a symbol==NULL node into the local
  list (strcmp(NULL) later).  Fixed.
- R6 (minor): cse_run returned success without checking ctx->oom when
  cse_assign_homes registered zero homes; check reordered.

One reported issue was analyzed and deliberately NOT changed: the
persisted is_candidate upgrade can make REWRITE's avail bookkeeping
differ from ANALYZE's (I6 is stricter than necessary).  This is
harmless by construction — REWRITE's soundness rests on its own
avail/scope/kill discipline, ANALYZE only selects which VNs get homes;
a divergence can misplace captures or waste a slot, never miscompile.
A comment above cse_visit_value records the argument.

**Instructions for the test-writing model**: add golden cases for R1
(all three shapes below MUST be included verbatim — they miscompiled
before the fix), plus coverage for R2 (a function with >32 nested ifs
or a >64-deep `&&` chain computing a heap read twice inside the
deepest tracked region and using it after the joins) and R3 (a
function with many locals + 32 profitable candidates near the frame
limit; it must compile and run identically at both levels, possibly
with CSE silently skipped).  R1 shapes:

```
/* R1a */ func main() { var d = { k: 41 }; var x = 0; var a = d.k + 1;
           if (d.k == 0) { x = d.k; } else { print(x + 1); } print(a); }
/* R1b */ func f(c, d) { var x = 100; if (c == 1) { x = d.k; }
           else { var a = (x + 1) * 2; var b = (d.k + 1) * 2;
                  print(a); print(b); } return 0; }
           func main() { f(0, { k: 5 }); }
/* R1c */ func cond() { return 0; }
           func main() { var d = 10; var x = 7; var a = (5 + d) * 2;
           if (cond() == 1) { x = 5; }
           else { var y = (x + d) * 2; print(y); } print(a); }
```

Also verify NOCT_CSE_DEBUG reports ≥1 capture for
`func f(d) { return d.k + d.k; }` (R4 regression: parameters are the
first-touched locals).

This document was designed in discussion with the project owner; every
open question has been resolved.  Treat it as the single source of
truth and do NOT re-litigate decisions recorded in the Decision Log.
Read docs/design/01-abce.md first for the existing optimizer's context
and for the "Global invariants" in docs/design/00-overview.md (C89
rules etc.) — they all apply here.

The work has three parts, to be done in this order:

- Part 1: Build-system change and mechanical split of ABCE out of
  `src/core/hir.c` into `src/core/hir_opt_abce.c` (no behavior change).
- Part 2: A new optimizer-only HIR node `HIR_EXPR_CAPTURE` and its LIR
  lowering (dead code until Part 3).
- Part 3: The CSE pass in `src/core/hir_opt_cse.c`, plus tests.

Each part ends with a verification gate.  Do not start the next part
until the gate passes.  Do NOT commit anything; leave all changes
uncommitted for the owner's review.

---

## Decision Log (authoritative)

- D-CSE1. **Algorithm**: forward symbolic-execution-style value
  numbering over the structured CFG.  No SSA, no DU/UD chains, no
  GEN/KILL bit vectors, no dataflow fixpoint.  Kills are implemented by
  *re-keying* (fresh VN on assignment; memory epoch in heap-fact keys),
  not by scanning tables.
- D-CSE2. **Loop heads use the kill-summary approximation**, not
  fixpoint iteration.  Rationale: without phi-VNs the exact fixpoint
  adds nothing except heap-fact regeneration cases, and those are
  excluded anyway by D-CSE6.
- D-CSE3. **Join handling is the "economy" variant**: facts created
  inside a conditionally-executed region are discarded at its exit
  (undo-log revert).  No cross-arm intersection, no phi-slot trick.
  Diamond redundancy is PRE territory and explicitly out of scope.
- D-CSE4. **Order preservation via `HIR_EXPR_CAPTURE`**: the first
  occurrence of a redundant expression is wrapped in place
  (`capture($cseN, E)`), never hoisted before the statement.  Error
  ordering and effect ordering are therefore preserved exactly; there
  is no relaxed-semantics mode.
- D-CSE5. **Purity tiers**: P (arith/compare/bit/NEG/NOT/TYPEIS/PCHECK/
  PBASE/PLEN — killed only by operand re-keying), M (DOT/SUBSCR/PLOAD*
  and *global variable reads* — additionally keyed on the memory
  epoch), E (CALL/THISCALL/NEW/ARRAY/DICT — never a CSE value, and
  each occurrence bumps the memory epoch).
- D-CSE6. **MT soundness rule**: the memory epoch is bumped at every
  loop-body entry, unconditionally (even if the body has no store or
  call).  Rationale: a shared-dict read cached across a back edge can
  turn a cross-thread spin-wait (`while (!g.flag) {}` style) into a
  livelock; the MT runtime makes racy reads legal, so eventual
  visibility must be preserved.  Do not weaken this for ST builds in
  this round.
- D-CSE7. **No commutative canonicalization** (`a+b` vs `b+a` stay
  distinct VNs).  `+` doubles as string concatenation, where operand
  order matters; blanket canonicalization is unsound.  Future work.
- D-CSE8. Strings are immutable (rt_string caches its hash; no
  in-place mutation API exists), so `+` on strings is CSE-able like
  any P-tier op even though it allocates: identity is unobservable.
  Verification step: grep the runtime for writes into
  `rt_string.data` after construction; if any exist, this decision is
  void — report to the owner instead of proceeding.
- D-CSE9. **Budget**: at most 32 capture locals per function
  (`$cse0`..`$cse31`), first-come.  Beyond the budget, stop creating
  new candidates (existing ones keep substituting).
- D-CSE10. **Pass order**: ABCE first, then CSE, both only at
  optimize level >= 2, both only when `NOCT_USE_OPTIMIZER` is
  defined.  CSE therefore also cleans up ABCE fast-body PLOAD/PBASE
  redundancy.
- D-CSE11. **Build**: CMake option `NOCT_ENABLE_OPTIMIZER`, default
  OFF.  When ON it compiles `hir_opt_abce.c` + `hir_opt_cse.c` and
  defines `NOCT_USE_OPTIMIZER`.  When OFF the optimizer sources are
  not compiled at all and `hir_optimize_func()` compiles to a no-op
  returning true.
- D-CSE12. All observable behavior must be bit-identical with the
  optimizer on and off, at every optimize level, interpreter and JIT.
  The test harness enforces this (same golden-output scheme as ABCE).

---

## Part 1: Split ABCE into hir_opt_abce.c + CMake option

### 1.1 New private header `src/core/hir_opt.h`

Shared between `hir.c`, `hir_opt_abce.c`, `hir_opt_cse.c`.  Contents:

```c
/* Pass entry points (defined in hir_opt_abce.c / hir_opt_cse.c). */
bool hir_opt_abce_func(struct hir_block *func_block);
bool hir_opt_cse_func(struct hir_block *func_block);

/* hir.c internals shared with the optimizer files. */
void *hir_malloc(size_t size);
char *hir_strdup(const char *s);
void hir_out_of_memory(void);
bool hir_add_local(struct hir_block *cur_block, const char *symbol);
int hir_next_block_id(void);
extern char *hir_file_name;
```

Notes:
- `hir_malloc`/`hir_strdup`/`hir_out_of_memory`/`hir_add_local` are
  currently `static` in hir.c — remove `static` from both the
  definitions and the forward declarations (forward decls are near
  lines 119–121 and 435; delete those lines and rely on hir_opt.h).
- `hir_file_name` already has external linkage (hir.c line ~56).
- `block_id_top` stays static in hir.c; add a tiny accessor
  `int hir_next_block_id(void) { return block_id_top++; }`.  The moved
  ABCE code has exactly one use (in `abce_mk_block`); rewrite it to
  call the accessor.
- Header guard `NOCT_HIR_OPT_H`; include `"hir.h"` and
  `<noct/c89compat.h>` as needed.  C89 rules apply.

### 1.2 Move the ABCE section

Move hir.c from the banner comment `/* ABCE: Array Boundary Check
Elimination for Packed. ... */` (line ~3158) through the end of
`hir_optimize_func` (end of file region, line ~4768) into
`src/core/hir_opt_abce.c`, unchanged except:

- Rename `hir_optimize_func` → `hir_opt_abce_func(struct hir_block
  *func_block)`.  Drop the `level` parameter and its `if (level < 2)`
  early-return (level gating moves to the driver, below).  Keep
  everything else, including `abce_loop_seq = 0;`, the
  `static struct abce_facts facts;` block, `NOCT_ABCE_DEBUG` output,
  and the `#ifndef NDEBUG` verifier.
- Add the file header comment in house style and includes:
  `"hir.h"`, `"hir_opt.h"`, plus whatever the moved code needs
  (`<assert.h>`, `<string.h>`, `<stdio.h>`, `<stdlib.h>` for getenv).
- The moved code's calls to `hir_malloc`/`hir_strdup`/
  `hir_out_of_memory`/`hir_add_local`/`hir_file_name` now resolve via
  hir_opt.h.  `N_TR` — check where it comes from (i18n header) and
  include the same header hir.c uses.

### 1.3 Driver stays in hir.c

Replace the moved `hir_optimize_func` with a thin driver at the same
place in hir.c (public API in hir.h is unchanged; runtime.c is not
touched):

```c
bool
hir_optimize_func(
	struct hir_block *func_block,
	int level)
{
#if defined(NOCT_USE_OPTIMIZER)
	assert(func_block != NULL);
	assert(func_block->type == HIR_BLOCK_FUNC);

	if (level < 2)
		return true;
	if (!hir_opt_abce_func(func_block))
		return false;
	if (!hir_opt_cse_func(func_block))
		return false;
	return true;
#else
	(void)func_block;
	(void)level;
	return true;
#endif
}
```

Match the codebase's unused-parameter convention (grep for
`UNUSED_PARAMETER` in src/core; if it exists, use it instead of
`(void)` casts).  Until Part 3 exists, have `hir_opt_cse_func` be a
stub in hir_opt_cse.c that just returns true — create the file in
Part 1 so the build wiring is complete from the start.

### 1.4 CMakeLists.txt

- Add to the option block (line ~9-30), matching alignment style:
  `option(NOCT_ENABLE_OPTIMIZER "Enable the HIR optimizer" OFF)`
- After `NOCT_BASE_SOURCE` is set (line ~170) add:

```cmake
# Determine the HIR optimizer files.
if(NOCT_ENABLE_OPTIMIZER)
  list(APPEND NOCT_BASE_SOURCE
    src/core/hir_opt_abce.c
    src/core/hir_opt_cse.c
  )
  list(APPEND NOCT_PRIVATE_CPPFLAGS NOCT_USE_OPTIMIZER)
endif()
```

  The `NOCT_PRIVATE_CPPFLAGS` append must occur before the
  `target_compile_definitions(... PRIVATE ${NOCT_PRIVATE_CPPFLAGS})`
  calls (line ~414-418).  Check where the existing code first touches
  `NOCT_PRIVATE_CPPFLAGS` (line ~342 area) and follow the same
  pattern.
- Audit ALL other places that compile src/core sources: grep
  CMakeLists.txt for `NOCT_BASE_SOURCE` and for `src/core/` (there is
  a ctrans/test target region around line ~590).  Every target that
  compiles hir.c must see the same two additions (sources + define)
  under the same condition.  `noctcli` only needs changes if it
  compiles core sources directly (it likely just links libnoct —
  verify).
- Other build systems (OpenWatcom/msdos makefiles etc.): grep the repo
  for files listing `hir.c` as a build input
  (`grep -rl "hir\.c" --include="Makefile*" --include="*.mk" .` plus
  any `*.wpj/*.tgt`).  Since the option defaults to OFF, those builds
  need no optimizer support; just confirm they still build (they must
  not define NOCT_USE_OPTIMIZER).

### 1.5 Test-harness wiring

`tests/run-abce.sh` requires a binary with the optimizer built in.
Find how the test binary (`../build-static/noct` by default) is
produced: check `tests/run-all.sh`, any build scripts, and CI
workflows (`.github/workflows/`).  Everywhere the ABCE suite runs, the
cmake invocation must gain `-DNOCT_ENABLE_OPTIMIZER=ON`.  Also make
sure at least one CI configuration still builds with the option OFF
(the default) so the no-op path stays compiling.

### 1.6 Part 1 gate

- Build with `-DNOCT_ENABLE_OPTIMIZER=OFF` (default) and `=ON`; both
  clean, no new warnings.
- With ON: `tests/run-abce.sh` fully green, `tests/run-all.sh` green.
- With OFF: full suite green (ABCE cases still pass because level-2
  output equals level-0 output by design).
- `git diff` over hir.c shows only: removed ABCE section, driver
  replacement, de-static'd helpers, accessor addition.  No logic
  edits inside moved code (verify by diffing the moved region against
  the new file, modulo the renames listed above).

---

## Part 2: HIR_EXPR_CAPTURE

Semantics: `capture($sym, E)` evaluates E, stores the result into the
local variable `$sym`, and yields that value.  It is created only by
the CSE pass; the parser never produces it, and the elback/scmback
source backends never see it (they do not call hir_optimize_func).
cback consumes LIR bytecode, so it is unaffected.  No new LIR opcode,
no interpreter or JIT work: the lowering reuses the existing MOVE.

### 2.1 hir.h

- Append `HIR_EXPR_CAPTURE` to `enum hir_expr_type` after
  `HIR_EXPR_PSTORE64`, with a comment in the style of the ABCE block:
  optimizer-only, created by hir_opt_cse.c, see this document.
- Add to the `hir_expr` union, after `new_`:

```c
		/* Capture Expression (optimizer-only; CSE) */
		struct {
			/* Captured expression. */
			struct hir_expr *expr;

			/* Home local variable symbol. */
			char *symbol;
		} capture;
```

### 2.2 hir.c

- `hir_free_expr`: add a `case HIR_EXPR_CAPTURE:` mirroring the DOT
  case (free the child expr and the symbol string the same way DOT
  frees `dot.obj` and `dot.symbol`).
- `hir_dump_expr` (or the equivalent dump helper used by
  `hir_dump_block`): add a case printing
  `capture(SYMBOL, ...)` in the local dump style.

### 2.3 lir.c

In `lir_visit_expr`'s dispatch, add:

```c
case HIR_EXPR_CAPTURE:
	if (!lir_visit_expr(dst_tmpvar, expr->val.capture.expr, block))
		return false;
	/* MOVE the value into the home local's slot. */
	local = lir_get_local_index(block, expr->val.capture.symbol);
	... emit MOVE local <- dst_tmpvar ...
	break;
```

Find how an existing plain `x = expr` assignment to a local emits its
MOVE (grep `OP_MOVE` in lir.c) and emit exactly that opcode/operand
sequence.  `lir_get_local_index` exists (line ~113).  If the symbol is
somehow not a local, that is a pass bug: `assert` it (>= 0).

### 2.4 hir_opt_abce.c (defensive)

`abce_verify_fast_expr`'s `default:` arm recurses as if binary; a
CAPTURE node reaching it would walk the wrong union member.  CSE runs
after ABCE so this cannot happen today, but add an explicit
`case HIR_EXPR_CAPTURE:` that recurses into `val.capture.expr`, so a
future pass-reordering cannot corrupt the verifier.

### 2.5 Part 2 gate

Same as Part 1 gate (the node is dead code so nothing changes), plus:
both configs compile with no switch-coverage warnings.

---

## Part 3: The CSE pass (hir_opt_cse.c)

### 3.1 Shape of the pass

Two passes over the function with an IDENTICAL traversal, sharing one
walker parameterized by mode:

- `CSE_PASS_ANALYZE`: build the VN universe, count how many times each
  VN would be substituted-from-available ("hits"), assign home slots
  to profitable VNs within the budget.
- `CSE_PASS_REWRITE`: re-run the same traversal with the same VN
  numbering (reset all volatile state; keep the entry table and hit
  counts); at a candidate's first available-miss wrap it in CAPTURE,
  at hits replace the node with a symbol term of the home local.

Determinism requirement: the two passes MUST visit nodes in the same
order and make the same VN insertions.  Achieve this by resetting
`sym_vn`, `mem_epoch`, all `avail` flags, and the undo log between
passes, while keeping the VN entry array and `hits` intact.  VN ids
are indices into the entry array, assigned in insertion order.

### 3.2 Data structures (all fixed-size, C89)

```c
#define CSE_MAX_VALUES   256   /* VN entries per function        */
#define CSE_MAX_CAPTURES 32    /* $cseN home locals per function */
#define CSE_MAX_UNDO     512   /* scoped avail insertions        */

struct cse_value {
	int op;          /* HIR_EXPR_* or CSE_VN_SYM/CSE_VN_CONST */
	int vn0, vn1;    /* child VNs, -1 if unused               */
	int epoch;       /* memory epoch for M-tier, -1 for P     */
	const char *aux; /* field name / symbol name, or NULL     */
	/* const payload for CSE_VN_CONST: */
	int const_type;         /* HIR_TERM_*                     */
	unsigned char bits[8];  /* value bits, memcmp-compared    */
	/* pass state: */
	int hits;        /* ANALYZE: would-substitute count       */
	int home;        /* capture slot no, -1 = not profitable  */
	int avail;       /* value currently available in home?    */
};
```

- Lookup is a linear scan or a small open-addressed index over the
  entry array — with a 256 cap, linear scan keyed on a precomputed
  hash field is acceptable; keep it simple.
- On table overflow (>= CSE_MAX_VALUES): stop creating entries; nodes
  that fail to get a VN return the sentinel `CSE_NOVN` (-1), which
  poisons parents (see 3.4).  This is sound (facts are only lost).
- Symbol VNs: an array `sym_vn[]` indexed by the local's index
  (`hir_local.index`; size it generously, e.g. 128, and bail out of
  the whole pass gracefully if a function has more locals).  Fresh
  VNs for symbols are entries with `op = CSE_VN_SYM` and a serial in
  `vn0` (a plain counter) so each assignment produces a distinct key.
- Global reads (symbol not found in the func's local list — reuse the
  lookup idiom of `lir_get_local_index`, or walk
  `func->val.func.local`): VN key is `(CSE_VN_SYM, name, mem_epoch)`.
  This makes every call/store invalidate global-read facts for free.
- `mem_epoch`: a plain int, bumped by kills (3.5).
- Undo log: array of entry indices whose `avail` went 0→1, plus a
  scope-mark stack (`int undo_mark[CSE_MAX_SCOPES]`).  On scope exit,
  pop to the mark, clearing `avail`.  On undo-log overflow: treat like
  table overflow — stop setting new avail flags until the scope
  unwinds (sound: facts lost, never wrongly kept).

### 3.3 The two core soundness mechanisms (do not "optimize" these)

1. **Kills are monotone and never reverted.**  Symbol re-keying
   (`sym_vn[i] = fresh`) and `mem_epoch++` survive scope exits.  A
   stale fact keyed on an old VN/epoch is simply unreachable — no
   table scan needed.  This is why break/continue/return need NO
   special handling: any path that jumps ahead has, if anything,
   *more* kills applied than the fall-through analysis assumed, and
   over-killing is always sound for a must-analysis.
2. **Avail flags are scoped.**  A fact created inside a
   conditionally- or repeatedly-executed region must not survive that
   region (the zero-iteration / untaken-branch problem: the home slot
   was never written).  The undo log implements this.

### 3.4 Traversal rules (must mirror LIR evaluation order)

Return value of the expression walker: the node's VN, or CSE_NOVN.
Post-order: children first, left to right, then the node.

- `TERM`/symbol, local: VN = `sym_vn[index]` (allocate on first
  read).  Never a candidate.
- `TERM`/symbol, global: VN = `(CSE_VN_SYM, name, mem_epoch)`.
  Never a candidate itself (bare read), but participates in parents.
- `TERM`/const (int/long/float/double/string): VN keyed on exact
  value bits (memcpy into `bits`, memcmp to compare — this
  intentionally distinguishes -0.0/0.0 and NaN payloads: VN equality
  means syntactic-value identity, which is what we need).  Strings:
  key on the string content pointer's text (strcmp via `aux`).
  EMPTY_ARRAY/EMPTY_DICT terms: Tier E — return CSE_NOVN and bump
  the epoch? No: they allocate but read nothing; they poison
  (CSE_NOVN) and do NOT bump the epoch (they cannot mutate existing
  objects).
- `PAR`: transparent; return the child's VN; never a candidate.
- Unary P (`NEG`, `NOT`, `PBASE`, `PLEN`): `(op, vn_child)`.
- Binary P (LT/LTE/GT/GTE/EQ/NEQ/PLUS/MINUS/MUL/DIV/MOD/AND/OR/XOR/
  SHL/SHR/TYPEIS/PCHECK): `(op, vn0, vn1)`.  No commutative
  canonicalization (D-CSE7).
- `LAND`/`LOR`: walk lhs normally; then `scope_open()`, walk rhs,
  `scope_close()` (rhs is conditionally evaluated; facts created in
  it must not escape, but epoch bumps from calls inside it must —
  which the monotone-kill rule gives automatically).  Node VN =
  `(op, vn0, vn1)`; the node itself IS a legal candidate (capturing
  the whole short-circuit expression preserves its internal
  conditional evaluation).
- `DOT`: `(DOT, vn_obj, field_name, mem_epoch)`.  Tier M.
- `SUBSCR`: `(SUBSCR, vn_obj, vn_index, mem_epoch)`.  Tier M.
- `PLOAD8U/8S/16U/16S/32/64`: `(op, vn_base, vn_offset, mem_epoch)`.
  Tier M.
- `CALL`/`THISCALL`: walk func/obj and args left-to-right, then
  `mem_epoch++`, return CSE_NOVN.
- `NEW`: walk the init expr, then `mem_epoch++` (the initializer may
  reference and the constructor semantics may touch the heap; be
  conservative), return CSE_NOVN.
- `ARRAY`/`DICT` literals: walk elements, return CSE_NOVN, no epoch
  bump (pure construction).
- Any node with a CSE_NOVN child: return CSE_NOVN (poison
  propagates).  Still walk all children (they may contain candidates
  internally).

Statements (`hir_stmt`), mirroring `lir_visit_stmt`:

1. Walk `rhs` (full tree).
2. If `lhs` is a bare local symbol term: `sym_vn[local] = fresh`;
   then, as a free improvement, if the rhs VN is not CSE_NOVN, set
   `sym_vn[local]` to the rhs VN instead (the local now aliases that
   value; identical in both passes, so determinism holds).
3. If `lhs` is a bare global symbol term: `mem_epoch++`.
4. If `lhs` is DOT/SUBSCR/PSTORE*: walk the lhs subexpressions in
   LIR's order (DOT: obj; SUBSCR: obj then index; PSTORE*: base then
   offset) — these are themselves reads and CSE candidates — then
   `mem_epoch++` (the store kill comes after the address
   computation).

Blocks (structured CFG walk; model on `abce_collect_loops`'s
recursion over succ/stop/inner/chain):

- `HIR_BLOCK_BASIC`: statements in order.
- `HIR_BLOCK_IF` chain: walk the first cond in the current scope
  (every path through the chain evaluates it).  Then `scope_open()`;
  for each chain element: `scope_open()`, walk its inner blocks,
  `scope_close()`; then walk the next element's cond in the chain
  scope (it is evaluated on strictly fewer paths than cond1, so its
  facts may serve later arms but must die at the join).  Finally
  `scope_close()` at the chain's end.  Kills from any arm persist
  (monotone), which is exactly the required join invalidation.
- `HIR_BLOCK_WHILE`: `scope_open()`; apply the loop-head kill
  summary: (a) `mem_epoch++` unconditionally (D-CSE6), (b) fresh VN
  for every local assigned anywhere in the body — collect with a
  small def-scan walk over the body subtree (statement lhs bare
  locals, plus for-counter/key/value symbols of nested loops; model
  on ABCE's def walk `abce_add_assigned`/`abce_scan_chain`).  Then
  walk the cond, then the body, in this scope.  `scope_close()`.
- `HIR_BLOCK_FOR` (ranged): walk `start` and `stop` exprs in the
  parent scope (evaluated once, before the loop).  Then
  `scope_open()`, head kill summary as above plus fresh VN for the
  counter symbol, walk body, `scope_close()`.
- `HIR_BLOCK_FOR` (for-each): walk `collection` in the parent scope;
  `scope_open()`, head kill summary plus fresh VNs for key/value
  symbols, walk body, `scope_close()`.
- `HIR_BLOCK_FUNC`/`HIR_BLOCK_END`: enter inner / stop.
- Follow `succ` until `stop` is set or the region's end is reached,
  exactly like the ABCE collector; do not follow the non-local succ
  edges of break/continue/return blocks (their targets are reached
  by the structural walk; soundness is covered by 3.3(1)).

### 3.5 Candidates, profitability, and rewriting

A node is a **candidate** iff its VN != CSE_NOVN and:

- it is DOT, SUBSCR, or PLOAD* (single heap read already beats a
  slot MOVE), or
- it is a P-tier operator node whose subtree contains at least two
  operator/M-tier nodes (a single `a+b` over terms is not worth a
  capture; `(a+b)*c` or `a.b+1` is).

ANALYZE: at a candidate whose VN is `avail` → `hits++`.  At a
candidate whose VN is not avail → mark avail (undo-logged), and if the
entry has no home yet and the budget allows, tentatively reserve one
(assign `home` in reservation order; entries that end the pass with
`hits == 0` release their reservation — simplest: after ANALYZE, walk
entries, clear `home` where `hits == 0`, then re-number the surviving
homes in order so slot names are dense).

REWRITE: only entries with `home >= 0` participate.  At such a
candidate: if not avail → wrap the node in CAPTURE (allocate the
wrapper with `hir_malloc`; `symbol = hir_strdup("$cse<home>")`;
child = the original node), set avail;  if avail → replace the node
with a TERM/symbol expr of the home name.  Replaced subtrees need not
be freed individually (HIR uses an arena, freed wholesale in
hir_cleanup) — just overwrite the parent's child pointer.
Register each used home once per function with
`hir_add_local(func_block, "$cse<N>")` (LIR sizes the frame from the
local list automatically).

Implementation note: to rewrite a node you need the parent's pointer
to it; have the expression walker take `struct hir_expr **slot` (the
address of the parent's child pointer) instead of a bare node
pointer.  Statement rhs/lhs, cond fields, for-start/stop/collection
are the roots (`&stmt->rhs` etc.).  Never rewrite a bare-local-lhs
term, PAR-transparent positions rewrite the inner slot.

CAPTURE nodes and their home symbol terms must NOT be re-walked as
candidates when encountered (they are results of this pass; the
walker will see them only if the pass is ever re-run on optimized
HIR — make the walker treat HIR_EXPR_CAPTURE like PAR: transparent,
return child VN, never a candidate).

### 3.6 Debug aid

`NOCT_CSE_DEBUG` environment variable, in the mold of
NOCT_ABCE_DEBUG: per function print to stderr
`[cse] FILE:FUNC: N captures, M substitutions` (and nothing when the
pass does nothing).

### 3.7 Part 3 gate

All of 1.6 plus `tests/run-cse.sh` (below) green in all four
configurations, and `tests/thread/run-thread.sh` (MT build) green.

---

## Tests: tests/cse/

Clone the tests/run-abce.sh harness as tests/run-cse.sh (same 4-way
matrix: level 0/2 x interpreter/JIT, byte-identical golden output;
same `.out2` convention for level-dependent error line numbers).  Wire
it into tests/run-all.sh next to the ABCE suite.

Design principle: every case must be constructed so that an UNSOUND
CSE changes stdout (never rely on dump output).  Required cases:

1. `basic` — `v = d.k + d.k;` and cross-statement reuse; verifies the
   plumbing and that values are correct.
2. `dot_chain` — repeated `a.b.c` chains, reuse of the inner and the
   full chain.
3. `subscr` — repeated `arr[i]`, and `arr[i]` vs `arr[j]` staying
   distinct.
4. `call_kill` — read `g.x`, call a function that mutates `g.x`, read
   again; the second read must see the new value.
5. `store_kill` — same with a direct `g.x = ...;` store, and with an
   aliasing store through a second reference to the same dict.
6. `alias_dot_subscr` — `d.f` read, then `d["f"] = ...;` store, then
   `d.f` read again (DOT/SUBSCR aliasing is covered by the blanket
   epoch, but pin it with a test).
7. `if_untaken` — compute `d.k` only inside an `if (false)`-style arm,
   use `d.k` after the join; wrong scope handling reads an unwritten
   slot and changes output.
8. `loop_zero` — `while` whose body never runs computes `d.k`; use
   `d.k` after; catches missing scope revert at loops.
9. `loop_varying` — `i + k` inside a loop that increments `i`;
   catches missing re-keying at the head kill summary.
10. `loop_mutation` — body mutates a dict then re-reads it same
    iteration and next iteration.
11. `shortcircuit` — `c && (d.k == 1)` where c is false, then use
    `d.k`; RHS facts must not escape.
12. `global_call` — global-variable read cached across a call that
    reassigns the global.
13. `string_concat` — repeated `s + t`; verifies D-CSE8 end-to-end.
14. `abce_mix` — a packed ranged-for eligible for ABCE with redundant
    subexpressions in the body; both passes active (level 2).
15. `budget` — a function with > 32 distinct redundant expressions;
    output stays correct with the budget cap engaged.

Also add one microbenchmark under tests/bench (repeated dict-field
chains in a hot loop) so the win is measurable; follow the existing
bench file conventions there.

---

## Documentation follow-ups

- Add a row for this document to the table in
  docs/design/00-overview.md (already done when this file landed —
  verify, and fix the "four coupled features" wording if the owner
  asks; do not rewrite that document otherwise).
- In docs/design/01-abce.md, add a one-line note that the ABCE
  implementation now lives in src/core/hir_opt_abce.c (do not touch
  anything else in that document).

## Hard rules (repeat of house rules — violating these is a failed task)

- C89 only in src/core: declarations at block top, `/* */` comments,
  no C99 loops, tabs/indent style of the surrounding code.
- Do NOT commit or push; leave the working tree for owner review.
- Do NOT modify interpreter/JIT opcodes, the object model, or the GC.
- Do NOT re-litigate Decision Log items (here or in 00-overview.md).
- Every part's gate must pass before moving on; if a gate cannot be
  made to pass, stop and report instead of weakening a test.
