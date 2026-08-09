# 07 — Typed Arithmetic Opcodes (int + float, inline-emitted)

Status: **implemented, Parts 0-3** (2026-08-10; rev 2 same day:
float division by zero becomes IEEE inf/NaN by owner decision —
Part 0 below — which unrestricts OP_FDIV; rev 3: OP_EQI reuse
withdrawn and shift counts moved to imm8, see D-TOP2).

Verification: tests/run-typedop.sh (11 cases x levels 0/2 x
interpreter/JIT, golden byte-identity + positive emission
assertions), plus run-abce/run-cse/run-syntax (incl. a full
interpreter level-2 sweep)/run-typing/run-scoping/run-class/
run-ctrans/run-thread — green on x86_64 and on arm64 under
qemu-user (inline backends), and on arm32/riscv64/ppc64/mips32/
mips64/ppc32 under qemu-user (helper-call backends; the latter three
modulo the documented pre-existing 48-numeric-cond long-comparison
JIT bug).  i386 passes except fdiv_ieee under JIT (pre-existing
double-constant emitter bug, reproduced at the pre-change baseline)
and riscv32 crashes on large JIT functions (pre-existing, ditto) —
both flagged as separate tasks.  remacs acceptance gate green
(run-all.sh's two pty failures, case_gud and case_skk_okuri_real,
reproduce identically at the pre-change baseline in this
environment; run-lisp.sh fully green).  mingw/msdos builds not run
here (no cross toolchains installed).

Benchmarks (x86_64, tests/bench, level 0 -> 2):
b8_typedop_int: JIT 0.183s -> 0.085s (x2.15), interp x1.05;
b9_typedop_float: JIT 0.162s -> 0.097s (x1.67), interp x1.38.

This document supersedes and expands **02-typing.md §6** ("v2"): the
op list grows (bitwise and constant-count shifts for int; a float
family), division/modulo get a literal-divisor-only rule instead of
being unconditional, and the rollout is staged so the first stage
needs **zero type inference**.  §6's acceptance bar carries over
verbatim: behavioral identity between levels is enforced by running
the whole tests/syntax suite at level 2.

Read first: [02-typing.md](02-typing.md) (annotation semantics,
CHECKTYPE), [01-abce.md](01-abce.md) (guarded regions, Stage A's
foundation), [06-simd.md](06-simd.md) (NHC invariant; this design
feeds its allowed-op list).  The Global Invariants in
[00-overview.md](00-overview.md) apply: C89 core, all three targets,
every backend handles every opcode, remacs gate, DO NOT COMMIT.

Relationship to 06-simd: independent — 07 can land before or after
06.  If 06 is already in the tree, hir_opt_simd's clones must copy
the `typed_int_region` flag (3.2 below); 06-simd.md carries the
matching note.

---

## 0. One-paragraph summary

Today every `+ - * / % & | ^ << >> < <= > >= == !=` compiles to a
generic helper call that type-dispatches at runtime; the only
int-assuming inline ops are `OP_INC` and `OP_EQI`.  This design adds
**typed opcodes** that assume operand tags and read the value union
directly — `OP_IADD` reads `.val.i`, `OP_FADD` reads `.val.f` — so
the x86_64/arm64 template JITs can emit a load/op/store instruction
sequence instead of a call.  Emission is sound only under proof of
the operand types; proofs come from three sources, staged: (A) ABCE
guard regions, where `TYPEIS(v,int)` was already evaluated at runtime
— zero inference; (B) a function-wide {INT, FLOAT, UNKNOWN} lattice
over locals, seeded by literals and CHECKTYPE-backed parameter
annotations; both consumed by `lir_visit_binary_expr` at optimize
level >= 2.  Behavior is bit-identical at every level on every
backend; anything that cannot be made bit-identical (error paths,
type promotion) falls back to the generic helper at that site.

## 1. Decision Log (authoritative)

- D-TOP1. **Typed ops are undefined on wrong-typed operands** (they
  trust tags like `OP_PLOAD32` trusts the ABCE guard).  They may
  therefore only ever be emitted under one of the two proof regimes
  in §3.  There is no "checked typed op" variant.
- D-TOP2. **Int family** (tag `NOCT_VALUE_INT`, i.e. int32):
  `IADD ISUB IMUL` (wrap mod 2^32), `IAND IOR IXOR`,
  `ISHL ISHR` (LOGICAL, count = compile-time constant 0..31 carried
  as an **imm8** third operand — a u16-tmpvar-shaped count would be
  rejected by the operand validators' frame-size check),
  `ILT ILTE IGT IGTE` (result int 0/1),
  `IDIV IMOD` (**only when the divisor is an int literal other than
  0 and -1** — see D-TOP5).  `==`/`!=`, unary neg/not: deferred
  (v2), the generic path remains.  (Implementation finding, rev 3:
  the original plan reused `OP_EQI` for proven int `==`, but OP_EQI
  is a **fused-only** op on the JIT backends — it sets host flags
  for the following OP_JMPIFEQ and never writes dst — so emitting it
  standalone in an expression context would read garbage.  Never
  reuse it outside the loop-exit pattern.)
- D-TOP3. **Float family** (tag `NOCT_VALUE_FLOAT`, binary32):
  `FADD FSUB FMUL FDIV` (IEEE, no environment changes; FDIV is
  unrestricted — Part 0 removes the float division-by-zero error, so
  FDIV has no error path and no divisor condition),
  `FLT FLTE FGT FGTE` (result int 0/1, C comparison semantics: any
  comparison with NaN is false).  Doubles (`NOCT_VALUE_DOUBLE`) are
  a symmetric v2 (`DADD`...) — not in this round.  Float `==`/`!=`:
  deferred.
- D-TOP4. **Long is out of scope** in this round (`LADD`... = v2).
  The lattice must therefore treat any long-producing construct
  (long literals, `PLOAD64`, int+long promotion) as UNKNOWN, and
  Stage A must exclude regions containing 64-bit loads (3.2).
- D-TOP5. **No inline error paths.**  After Part 0, the only typed-op
  candidates with an error path are INTEGER division and modulo
  (runtime "Division by zero.", plus the INT_MIN/-1 hardware trap).
  Rather than build per-backend inline-check-and-call-out machinery,
  typed IDIV/IMOD are emitted only when the divisor is an int literal
  other than 0 and -1 — statically error-free, straight-line.  This
  covers the real uses (`x/255`).  The same reasoning gives shifts
  their constant-count rule (variable counts have a range-error
  path).  Float division needs none of this: Part 0 makes it total
  (IEEE inf/NaN), so FDIV is unrestricted.
- D-TOP12. **Owner decision (2026-08-10): float and double division
  by zero stop being runtime errors and yield IEEE special values**
  (`x/0.0` = +/-inf for x != 0, `0.0/0.0` = NaN) — a breaking
  language-semantics change, accepted without a compat flag (the D19
  precedent).  Rationale: hardware FP division does not trap under
  the default (masked) FP environment on every supported target;
  removing the software check both speeds the scalar path and makes
  FDIV / future vector division (06-simd Phase B VDIVF32X4)
  well-defined.  On any hypothetical port whose FP environment traps
  by default, port-init must mask FP exceptions — the runtime never
  unmasks them (this is the same "no MXCSR/FPCR writes" rule as
  06-simd 7.4, now load-bearing for scalar semantics too).
  INTEGER division by zero remains a runtime error, unchanged.
- D-TOP6. **Emission sites**: `lir_visit_binary_expr` (and only it),
  at `lir_optimize_level >= 2`, when `lir_expr_proven_type()` (§4)
  proves BOTH operands INT (resp. FLOAT) and the per-op extra
  conditions hold.  Mixed int/float — even though the helper would
  promote — always falls back to the generic helper in this round.
- D-TOP7. **Backend split, PLOAD64 precedent**: inline machine code
  on x86_64 and arm64 only.  The other 8 JIT backends implement
  every typed op as a call to its dispatch-free typed helper
  (`ASM_BINARY_OP(ex_iadd_helper)` style) — still a win (no type
  switch), trivially correct, upgradeable later.  Interpreter uses
  `BINARY_OP(...)` with the typed helpers; cback open-codes C.
  Every backend handles every opcode (Global invariant 6).
- D-TOP8. **All typed opcodes take exactly three u16 operands**
  (`dst, src1, src2`; for ISHL/ISHR the third is the immediate count,
  the same convention as 06-simd's `OP_VSHLI32X4`) and every typed
  helper is `bool h(NoctEnv *env, int a, int b, int c)` — the same
  uniform ABI as design 06 (D-SIMD7), for the same reason: every
  backend reuses its existing operand-decode and call templates.
- D-TOP9. **Stage order**: Part 1 opcodes+helpers+all backends;
  Part 2 = Stage A (ABCE-region emission, zero inference); Part 3 =
  Stage B (function-wide lattice).  Each part has a gate; each part
  is independently shippable and useful.
- D-TOP10. **Proof-carrying fields initialize to "unknown"
  explicitly.**  `NOCT_VALUE_INT == 0`, so a zero-initialized
  "proven type" field would silently mean "proven int" — the
  catastrophic default.  Every new proof field uses -1 = unknown and
  is set explicitly at every allocation site; this is called out per
  field below and MUST be re-checked in review.
- D-TOP11. Kill switch `NOCT_TYPED_DISABLE=1` (skip all typed
  emission); `NOCT_TYPED_DEBUG=1` prints per function:
  `TYPED: <func>: emitted=<n> (int=<n> float=<n>) sites_generic=<n>`.
  run-typedop.sh asserts the count is nonzero for must-fire cases
  (same rationale as D-SIMD14: golden tests cannot catch a silently
  dead optimization).

## 2. Verified facts (re-verify by grep before implementing)

* Generic helpers type-dispatch on both operands and, for int/int:
  ADD/SUB/MUL do C `int` arithmetic on `.val.i` (wraps in practice);
  AND/OR/XOR are C bitwise; **SHL/SHR are LOGICAL on `(uint32_t)`**
  with a runtime error unless `0 <= count < 32`; LT family yields
  int 0/1 via C comparisons; `INT_MIN / -1` is not guarded by the
  div helper (it would trap in the helper too; typed ops must simply
  never be emitted for it, D-TOP5).
* `noct_ex_div_helper` TODAY checks `== 0` on the divisor in **every**
  numeric case — including all float/double-performed divisions —
  and raises "Division by zero.".  Part 0 deletes the check from the
  float/double-performed cases only (those whose result tag is FLOAT
  or DOUBLE); the int/int, int/long, long/int, long/long cases keep
  it.  `noct_ex_mod_helper` has **no float/double arithmetic cases**
  (float `%` falls to the type-error default) — modulo is untouched
  by Part 0.
* cback lowers `OP_DIV` as a call to `noct_ex_div_helper` (verified
  `cback_visit_div_op`), and the interpreter and every JIT reach
  division through the same helper — so Part 0 is a
  **single-file change** in execution.c.  elback/scmback translate
  pre-optimizer source and inherit the host runtime's division;
  their behavior is out of scope (note it in their headers if they
  document semantics).
* float/float ADD is C `float +` (binary32, execution.c
  `noct_ex_add_helper` FLOAT/FLOAT case); float/float LT is C `<`
  yielding int 0/1.  Result tags: arithmetic on float/float yields
  FLOAT; comparisons always yield INT.
* `OP_INC` and `OP_EQI` are the only existing type-assuming ops;
  both are emitted inline by x86_64 and arm64 (and EQI fuses with
  `OP_JMPIFEQ`).  `OP_CHECKTYPE` exists and is emitted for every
  annotated parameter **unconditionally at level >= 2** in
  `lir_build` (verified in lir.c) — this is what makes annotation-
  seeded proofs sound with no extra work.
* `struct hir_local` is `{symbol, index, next}` — no type field.
  Parameter annotations live in `hir_block.val.func.param_type[]`
  (NOCT_VALUE_* tag or -1); **local annotations are not stored in
  HIR** — the lattice must not (and cannot) use them.
* Locals and tmpvars are zero-cleared = int 0; a local read before
  any dynamic assignment yields int 0 (this seeds the lattice, 4.2).
* `lir_visit_capture_expr` (CSE) evaluates the inner expr into dst
  then `OP_ASSIGN`s it to the home local — so a CAPTURE both yields
  its inner type and assigns its home local (both matter in §4).
* The ABCE guard `$g` evaluates `TYPEIS(v, int)` for every local
  read in the fast body, `$lo`, `$hi` included, ONCE before G1; the
  SLOW loop under G2 runs precisely when `$g` is false — types there
  are NOT proven.  Fast-body loads are `PLOAD8U/8S/16U/16S/32`
  (int-yielding) or `PLOAD64` (long-yielding — the poison case,
  D-TOP4); stores don't change local tags.
* x86_64 JIT: r14 = env, r15 = tmpvar base, rax/rcx/rdx/rbx scratch;
  Win64 note: xmm0..5 are caller-saved on BOTH ABIs, so float typed
  ops may use xmm0/xmm1 freely with no prologue changes (unlike
  design 06's vregs).  arm64: x0 env, x1 tmpvar base, x2..x4 GPR
  scratch; s0/s1 (low halves of v0/v1, caller-saved) for float.
* Opcode space: 0x3d.. may be claimed by design 06 first.  Opcode
  NAMES and operand layouts here are normative; VALUES are assigned
  at implementation time after whatever is already in bytecode.h.

## 3. Proof regimes

### 3.1 Overview

`lir_visit_binary_expr` asks `lir_expr_proven_type(expr, block)` for
each operand.  That function returns INT, FLOAT, or UNKNOWN using
purely local, already-computed facts (no analysis at lir time):

1. int literal -> INT; float literal -> FLOAT; long/double/string/
   array/dict literals -> UNKNOWN.
2. `PAR` -> type of inner; `CAPTURE` -> type of inner.
3. Binary arith node (`PLUS MINUS MUL DIV MOD AND OR XOR SHL SHR`):
   INT if both children INT (for DIV/MOD: regardless of the literal
   rule — the RESULT is still int when it doesn't error); FLOAT if
   both children FLOAT and the node is `PLUS MINUS MUL DIV`;
   else UNKNOWN.  Comparison nodes and EQ/NEQ: INT if both children
   are proven (any proven combination — comparisons always yield
   0/1) else UNKNOWN.  LAND/LOR: UNKNOWN in this round (they yield
   one of the operand values; refine in v2).
4. `PLOAD8U/8S/16U/16S/32` -> INT.  `PLOAD64` -> UNKNOWN (long).
   `PLEN` -> INT.  `PCHECK`/`TYPEIS` -> INT.  `PBASE` -> UNKNOWN
   (long).
5. Symbol term -> the symbol's proof:
   a. parameter k with `param_type[k]` == NOCT_VALUE_INT (or a
      sized-int alias already normalized to INT by the parser) ->
      INT; == NOCT_VALUE_FLOAT -> FLOAT.  Sound because CHECKTYPE
      already runs at level >= 2 (§2).  Other tags -> UNKNOWN.
   b. local with Stage B proof (`hir_local.proven_type`, 4.2) ->
      that.
   c. ANY local or parameter, when the innermost enclosing FOR block
      (walk `block->parent`) has `typed_int_region` set -> INT
      (Stage A, 3.2).  This clause may upgrade an UNKNOWN from a/b.
6. Everything else (DOT, SUBSCR, CALL, THISCALL, ARRAY, DICT, NEW,
   NEG, NOT, string ops) -> UNKNOWN.

Rule 5c is Stage A; rule 5b is Stage B; rules 1-4 and 5a are free
and land with Part 1... but note they alone almost never fire (two
literals fold nowhere yet — there is no constant folding), so Part 1
without Part 2 is dead code by design, exactly like 06-simd Part B.

### 3.2 Stage A: ABCE guard regions (zero inference)

`abce_version_loop` sets a new optimizer-only flag on the FAST loop:

```c
/* hir.h, val.for_, next to abce_fast/is_vector (design 06): */
bool typed_int_region;   /* every local/param read in this subtree
                            is guard-proven int AND stays int */
```

Condition for setting it (checked on the eligible body ONCE, before
cloning): the body contains **no 64-bit load site** (`PLOAD64` after
rewrite; equivalently, no site whose owner packed's bet is
INT64/UINT64).  Everything else is already guaranteed by the ABCE
eligibility rules: every local read is TYPEIS-int guarded, constants
are int-only (long/float/double rejected), loads of width <= 32
yield int, and int-closed arithmetic re-establishes int at every
assignment — so the region invariant holds inductively over the
body's own stores.  With a 64-bit load, an accumulator can turn long
at runtime (the width64.noct doubled-sum precedent) and the flag must
stay false.

Set the flag on FAST only.  The SLOW loop must NEVER get it (it runs
when `$g` is false).  If design 06 is present, hir_opt_simd copies
the flag from FAST to its VFOR, RFOR (remainder), and SFOR
(full-scalar) clones — all three live under G1 where `$g` held.
(06-simd.md 5.4 carries the mirror note.)

Effect: every arithmetic node inside a flagged region lowers to a
typed op.  In a blend-like remainder loop that is every `+ - * & |
>> <<` — the scalar tail and every non-vectorized ABCE loop lose
their per-op helper calls.

`memset` zeroing of hir_block leaves the flag false — the safe
default for a bool flag (contrast D-TOP10, which is about tag-typed
fields).

### 3.3 Stage B: function-wide lattice (Part 3)

New pass `hir_opt_typed.c`, entry `bool hir_opt_typed_func(struct
hir_block *func_block)`, called from `hir_optimize_func` **after**
CSE (order: abce -> simd -> cse -> typed), level >= 2 only,
NOCT_USE_OPTIMIZER only.

Add to `struct hir_local` (hir.h):

```c
int proven_type;   /* -1 unknown; else NOCT_VALUE_INT or
                      NOCT_VALUE_FLOAT.  MUST be initialized to -1
                      at the single allocation site (hir_add_local);
                      0 means "int" — see design 07 D-TOP10. */
```

(`hir_add_local` in hir.c is the single constructor for hir_local
nodes — including CSE homes and the $abce/$v locals; initialize
there.  Grep for any other `struct hir_local` allocation and treat a
hit as a design violation to report, not adapt to.)

Algorithm (a 2-level lattice per local: INT / FLOAT / UNKNOWN;
"meet" of equal = itself, of different = UNKNOWN):

1. Initialize every local to **INT** — this models the zero-init
   (`type = 0` = int 0) that a read-before-first-assignment
   observes.  Parameters are NOT in the local list; their proof is
   annotation-based only (3.1-5a) and they take no part here.
2. Collect assignment edges by walking the whole function body once:
   - `stmt->lhs` is a local symbol: edge (local, rhs-expr).
   - ranged FOR: pseudo-edge (counter, start-expr) — `OP_INC` does
     not change the tag, so the counter's runtime tag is start's tag
     for the whole loop.
   - for-each (kv / v): edges (key, UNKNOWN), (value, UNKNOWN).
   - every `CAPTURE` node anywhere in any expression: edge (home
     local, inner-expr).
   - `$return`: ignored.
3. Iterate to fixpoint (the lattice has height 2; two or three
   passes settle it): for each edge, evaluate the rhs type with the
   §3.1 rules — using current local proofs, rule 5c included where
   applicable — and meet it into the local.  UNKNOWN rhs pins the
   local to UNKNOWN.
4. Write results into `proven_type`.

Notes:
* Callees cannot write to a caller's locals (functions are
  top-level; no closures) — assignments enumerated above are
  exhaustive.  Globals are irrelevant (proofs are per-local).
* Annotated LOCALS (e.g. `var x: float = ...`) contribute nothing:
  HIR does not retain local annotations (§2), and per 02-typing an
  annotation alone is a hint, not a checked fact.  Only the
  assignment closure proves locals.  (If float locals fail to prove
  because of the int-0 phantom init — e.g. assigned only inside a
  branch — that is the conservative intended outcome for v1.)
* The pass mutates nothing; it only fills proof fields.  It cannot
  fail except on OOM (follow the CSE pass's oom convention).

## 4. Opcodes and semantics (normative)

Operands: `dst, src1, src2`, u16 each.  All write a full rt_value
(tag + value).  "u32 arithmetic" = compute in `uint32_t`, store as
int32 — defined wrap, matches both the shipped helper behavior and
the inline hardware ops.

| Op | dst tag | Definition |
|----|---------|------------|
| OP_IADD | INT | `u32(a) + u32(b)` |
| OP_ISUB | INT | `u32(a) - u32(b)` |
| OP_IMUL | INT | `u32(a) * u32(b)` (low 32) |
| OP_IDIV | INT | `int32 a / int32 b` — emitted only for literal b, b != 0, b != -1 |
| OP_IMOD | INT | `int32 a % int32 b` — same literal rule |
| OP_IAND/IOR/IXOR | INT | bitwise |
| OP_ISHL | INT | `u32(a) << c`, src2 = immediate c in 0..31 |
| OP_ISHR | INT | `u32(a) >> c` — **LOGICAL**, immediate c in 0..31 |
| OP_ILT/ILTE/IGT/IGTE | INT | int32 compare, 0/1 |
| OP_FADD/FSUB/FMUL | FLOAT | C float arithmetic (binary32) |
| OP_FDIV | FLOAT | C float division, IEEE-total after Part 0 (`x/0.0` = +/-inf, `0.0/0.0` = NaN); no divisor condition |
| OP_FLT/FLTE/FGT/FGTE | INT | C float compare, 0/1; **NaN => 0** |

Emission preconditions recap (all at level >= 2, all in
`lir_visit_binary_expr`):
* both operands proven INT -> int op; both proven FLOAT and the op
  is + - * / or a comparison -> float op; anything else -> generic.
* `==` with both proven INT -> existing `OP_EQI`.  (`!=`, float
  `==`: generic.)
* SHL/SHR additionally require the rhs to be an int literal 0..31
  (out-of-range literal: generic helper, which errors at runtime,
  preserving behavior).  Emit the count as the third operand
  directly; do NOT materialize it with OP_ICONST.
* IDIV/IMOD literal rules per the table (FDIV needs none).  The
  literal divisor IS still materialized through the normal operand
  evaluation (it sits in a tmpvar; src2 names that tmpvar as usual)
  — only the emission decision looks at the literal.  [Rationale:
  keeps operand shape uniform; the win is skipping dispatch+call,
  not one ICONST.]
* MOD on proven floats: generic (C `%` doesn't apply to floats; the
  helper uses fmod semantics? — do not guess: float `%` stays
  generic in this round regardless of what the helper does).

## 5. Implementation parts

### Part 0: float division becomes IEEE-total (semantics change, D-TOP12)

Do this FIRST; it is independent of every other part and changes
observable behavior, so it must be isolated in its own verification
step.

1. In `noct_ex_div_helper` (src/core/execution.c), delete the
   `== 0` divisor check (the `rt_error(env, N_TR("Division by
   zero.")); return false;` arm) from every case whose division is
   performed in float or double — i.e. every case that sets
   `dst_val->type` to `NOCT_VALUE_FLOAT` or `NOCT_VALUE_DOUBLE`.
   The four integer-performed cases (int/int, int/long, long/int,
   long/long) KEEP the check.  No other function changes:
   `noct_ex_mod_helper` has no float arithmetic cases (§2), and
   every backend (interpreter, all 10 JITs, cback) reaches division
   through this one helper.
2. Do NOT add any FP-environment manipulation.  All three targets
   run with FP exceptions masked by default; the "no MXCSR/FPCR
   writes" rule (06-simd 7.4) is now load-bearing for scalar
   semantics.  If a future port traps on FP division by zero, the
   fix belongs in that port's init (mask the exceptions), never in
   the helper.
3. Grep the whole tests/ tree for golden outputs that contain
   "Division by zero." produced by a FLOAT or DOUBLE division and
   update them to the new value-producing behavior.  Integer
   division tests must stay byte-identical.  (If any i18n message
   catalog in locale/ becomes unreferenced by this deletion it stays
   in place — the integer cases still use it.)
4. New test `tests/typedop/fdiv_ieee.noct` (runs under run-typedop.sh
   later, but write and run it now standalone): pins
   `1.0/0.0` -> +inf, `-1.0/0.0` -> -inf, `0.0/0.0` -> NaN, plus a
   double-typed division by zero, plus `int / 0.0` (float-performed:
   inf, not an error) and `0 / 0` (int: still the error, message
   unchanged).  **Print classifications, not raw values**: printf
   renderings of inf/NaN differ across libcs (glibc "inf" vs legacy
   Windows "1.#INF" vs OpenWatcom's own) and would make golden
   outputs platform-dependent.  Classify in-script — NaN via
   `x != x`, infinities via comparison against a huge finite
   threshold and sign — and print stable strings ("nan", "+inf",
   "-inf").

**Gate 0**: the new test green at levels 0/2 x interpreter/JIT on
x86_64; full existing suite green (with any float-div goldens
consciously updated and listed for the owner's review); all three
targets build; remacs gate green.

### Part 1: opcodes, helpers, interpreter, cback, 8 helper-call JITs

1. bytecode.h: append the 21 opcodes (12 int + 8 float + spare NONE —
   exact count: IADD ISUB IMUL IDIV IMOD IAND IOR IXOR ISHL ISHR
   ILT ILTE IGT IGTE = 14; FADD FSUB FMUL FDIV FLT FLTE FGT FGTE
   = 8; total 22).
2. execution.c: 22 dispatch-free helpers, `noct_ex_iadd_helper` etc.,
   `(env, a, b, c)`, ALWAYS compiled (precompiled-bytecode targets
   need them — msdos/OpenWatcom C89 rules apply).  They trust tags
   (D-TOP1) and never fail... except IDIV/IMOD MUST keep a
   defensive divisor check mirroring the generic helper's error
   (`b == 0 || (a == INT_MIN && b == -1)` -> route to the generic
   helper's error path): the emission rule makes it unreachable from
   our compiler, but bytecode files are an external input and a
   division trap takes down the process — same defensiveness the
   doc requires nowhere else precisely because no other typed op can
   trap.
3. interpreter.c: 22 `BINARY_OP(ex_..._helper)` cases.
4. cback.c: 22 cases, open-coded C (`tmpvar[d].type = 0;`... use the
   named NOCT_VALUE_* constants, u32 arithmetic for the int family).
5. The 8 non-inline JIT backends: 22 helper-call cases each, copying
   that backend's OP_PLOAD64 case verbatim (operand decode + 
   ASM_BINARY_OP-equivalent).
6. lir.c: lir_dump entries; no emission yet (dead code, like 06
   Part B).

**Gate 1**: all three targets build; entire existing suite green at
levels 0/2 x interpreter/JIT (opcodes are unreachable; this gate
catches build/dispatch-table breakage only).

### Part 2: Stage A emission + x86_64/arm64 inline backends

1. hir.h flag + `abce_version_loop` sets it (3.2); 06-clone
   propagation if applicable.
2. `lir_expr_proven_type()` in lir.c (rules 3.1 minus 5b) +
   emission in `lir_visit_binary_expr` + NOCT_TYPED_DISABLE/DEBUG.
3. x86_64 inline emitters.  Int pattern (mirror the scalar op
   style — tag write included, offsets are tmpvar*16):
   ```
   movl src1+8(%r15), %eax        41 8B 87 <disp32>
   addl src2+8(%r15), %eax        41 03 87 <disp32>   ; sub 2B, imul 0F AF, and 23, or 0B, xor 33
   movl $NOCT_VALUE_INT, dst(%r15)
   movl %eax, dst+8(%r15)
   ```
   Shifts: `shll/shrl $imm, %eax` (C1 /4, C1 /5 — LOGICAL shr).
   IDIV/IMOD: `movl src1+8,%eax; cltd; idivl src2+8(%r15)` — result
   eax (div) / edx (mod).  Comparisons:
   `movl src1+8,%eax; cmpl src2+8(%r15),%eax; setl %al; movzbl
   %al,%eax` then tag+store (setle/setg/setge per op).
   Float: `movss src1+8(%r15),%xmm0; addss src2+8(%r15),%xmm0`
   (F3 0F 10 / F3 0F 58; sub 5C, mul 59, div 5E), tag FLOAT, movss
   out (F3 0F 11).  Float compares — **the NaN trap, get this
   exactly right**: `a < b` must be computed as "b > a":
   ```
   movss  b+8(%r15), %xmm0
   ucomiss a+8(%r15), %xmm0     ; flags = (b ? a), unordered sets CF=ZF=PF=1
   seta   %al                   ; b > a, and 0 on NaN  => a < b  ✓
   ```
   FLT: load b, ucomiss a, seta.  FLTE: load b, ucomiss a, setae.
   FGT: load a, ucomiss b, seta.  FGTE: load a, ucomiss b, setae.
   Never use setl/setb after ucomiss (unordered would read as
   "less").  xmm0/xmm1 only (caller-saved on SysV AND Win64; no
   prologue work, no interaction with design 06's vreg budget —
   float typed ops cannot appear inside a Phase-A strip region,
   whose grammar is int-only).
4. arm64 inline emitters.  Int: `LDR w2,[x1,#src1+8]; LDR w3,[...];
   ADD/SUB/MUL/AND/ORR/EOR w2,w2,w3; LSL/LSR w2,w2,#imm;
   SDIV w2,w2,w3` (+ `MSUB` for IMOD: `w4 = w2 - (w2/w3)*w3`);
   compares `CMP w2,w3; CSET w2, LT/LE/GT/GE`; then the standard
   tag+value store pair.  Float: `LDR s0,[x1,#a+8]; LDR s1,[...];
   FADD/FSUB/FMUL/FDIV s0,s0,s1; STR`.  Float compares — **the
   AArch64 condition-code trap**: after `FCMP s_a, s_b` the
   unordered result sets C and V, so the SIGNED-INT conditions are
   wrong for `<`/`<=`.  Use exactly:
   ```
   FLT : FCMP s_a, s_b ; CSET w2, MI    (N set: less; NaN -> 0)
   FLTE: FCMP s_a, s_b ; CSET w2, LS    (C clear or Z: le; NaN -> 0)
   FGT : FCMP s_a, s_b ; CSET w2, GT    (NaN -> 0)
   FGTE: FCMP s_a, s_b ; CSET w2, GE    (NaN -> 0)
   ```
   (MI/LS for the less-side, GT/GE for the greater-side — do not
   "simplify" to LT/LE.)  Verify every encoding with the 06-simd
   §7.5 assemble-and-diff harness — that procedure is mandatory
   here too, one op at a time.
5. Add the typed ops to 06-simd's NHC allowed list if 06 is present
   (they are inline on both register-mapping backends; int ops touch
   only GPRs).

**Gate 2**: new suite tests/run-typedop.sh (§7) green on x86_64
(interp + JIT) and arm64 under qemu (interp + JIT), levels 0/2
byte-identical; NOCT_TYPED_DEBUG assertions fire; run-abce, run-cse,
run-simd (if present), run-syntax, run-typing all green; the whole
tests/syntax suite passes at level 2 (the 02-typing §6 bar); all
three targets build; remacs gate.

### Part 3: Stage B lattice

1. `hir_local.proven_type` (+ the -1 init in hir_add_local —
   D-TOP10 review item), hir_opt_typed.c pass, wiring in
   `hir_optimize_func` after CSE.
2. `lir_expr_proven_type` rule 5b activation.
3. CMakeLists: add hir_opt_typed.c to the NOCT_ENABLE_OPTIMIZER
   source list.

**Gate 3**: Gate 2's matrix, plus the Stage-B-specific cases in §7;
multiarch sweep (all 10 arches, qemu) of run-typedop.sh +
run-syntax; bench numbers recorded in this header (b9, §8).

## 6. Traps appendix (the ways this goes wrong)

* **Zero-default = proven-int** (D-TOP10): `proven_type` and any
  future proof field must init to -1 explicitly.  Grep-audit at
  review time.
* **SHR emitted arithmetic**: the scalar semantics is logical
  (verified `noct_ex_shr_helper`).  `sarl`/`ASR` anywhere = silent
  miscompile on bit-31 values; the §7 tests include `(x >> 1)` on a
  negative int.
* **Float compare on NaN**: seta/setae-after-swap on x86, MI/LS/GT/GE
  on arm64.  §7 includes NaN operands — after Part 0 a NaN is
  producible in-language as `0.0 / 0.0` (and inf as `1.0 / 0.0`), so
  the compare tests need no intrinsic support.
* **Printing inf/NaN is not portable**: never put a raw
  printf-rendered inf/NaN into a golden output (libc spellings
  differ).  Classify in-script (`x != x`, sign + huge-threshold
  compare) and print stable strings.  Part 0's test shows the
  pattern; every later float test follows it.
* **SLOW-loop flag leak**: `typed_int_region` on any block reachable
  when `$g` is false = unsound.  The flag is set in exactly one
  place (FAST) and copied to exactly three (06's clones); anything
  else is a bug.
* **PLOAD64 contamination**: a 64-bit load in an ABCE body kills the
  region flag (long accumulator).  width64.noct exists and must stay
  green; add a typed-region variant asserting no typed emission
  there (debug counter == 0).
* **Counter tag subtlety**: OP_INC preserves the start value's tag;
  the lattice's pseudo-edge (counter := start) is what keeps a
  long-started loop counter out of INT proofs.  Do not special-case
  counters to INT.
* **IDIV INT_MIN/-1**: the emission rule forbids literal -1; the
  helper keeps the defensive check for foreign bytecode.  Both
  matter; neither substitutes for the other.
* **`==` reuse**: do NOT emit OP_EQI from expression context — on
  the JIT backends it is fused-only (sets flags for the following
  OP_JMPIFEQ, never writes dst).  Typed `==` stays deferred.
* **Shift-count encoding**: the count is an imm8, not a u16 tmpvar
  slot — rt_get_tmpvar/jit_get_opr_tmpvar validate every u16 operand
  against tmpvar_size, so a count >= the frame size reads as "Broken
  bytecode" (found the hard way: counts 0..3 pass, 8+ fail).

## 7. Tests (tests/typedop/, run-typedop.sh)

Golden scheme as run-abce.sh (level 0/2 x interp/JIT, byte-identical
outputs; (T) = must emit typed ops per NOCT_TYPED_DEBUG, (G) = must
not).

1. (T) `arith.noct` — annotated `func f(a: int, b: int)` exercising
   every int op incl. wrap cases (`2147483647 + 1`, `-2147483648 - 1`,
   large IMUL), `x / 255`, `x % 7`, shifts 0/1/24/31 on values with
   bit 31 set, all four comparisons + `==`, printed.
2. (T) `farith.noct` — `func f(x: float, y: float)`: + - * and `/`
   with variable divisors incl. zero (inf/NaN classified per the §6
   printing rule), comparisons incl. equal values, NaN operands
   (`0.0/0.0`) on all four comparison ops (must all print 0), and
   denormal-ish smalls; finite results printed with enough digits to
   pin bits (reuse whatever float printing the suite already
   trusts).
3. (T) `abce_region.noct` — an ABCE-versioned u8 loop whose body has
   `* + >> &` on guarded locals: Stage A emission inside the fast
   body (assert emitted>0 at level 2), identical outputs.
4. (G) `abce_w64.noct` — width64-style body (u64 load into
   accumulator): zero typed emission (region flag suppressed).
5. (G) `mixed.noct` — int+float mixed expressions: all generic;
   outputs identical (promotion semantics preserved).
6. (G) `div_edge.noct` — integer `x / 0` (runtime error text
   identical at both levels — Part 0 must not have touched it),
   integer `x / -1` with x = -2147483648 — CAUTION: if the scalar
   path traps the process on INT_MIN/-1, this case documents (and
   pins) whatever the current behavior is; probe the current
   behavior FIRST and write the golden from level 0.  Variable int
   divisor `x / y`: generic.  (Float division-by-zero coverage lives
   in fdiv_ieee.noct and farith.noct — it is typed AND total, so it
   belongs in the (T) set, not here.)
7. (T/G) `lattice.noct` (Part 3): float local proven through a chain
   of assignments (fires); a local assigned float only in one branch
   (int-0 phantom -> generic); counter of `for (i in 0..n)` used in
   arithmetic (INT via pseudo-edge; fires); a CAPTURE-home local
   (CSE on) in arithmetic.
8. (G) `checktype_off.noct` — annotated function violating its
   annotation, called at level 0: no error (annotations inert below
   2), and at level 2: the existing CHECKTYPE error, unchanged text.
9. `shift_var.noct` — variable shift count in range and a constant
   33 shift: both generic; the 33 case errors identically at both
   levels.
10. Full tests/syntax at level 2 (scripted like run-cse.sh's pass).

## 8. Bench

`tests/bench/b9_typedop.noct`: (a) an integer kernel — checksum/LCG
loop of `* + ^ >>` on locals, annotated params; (b) a float kernel —
dot-product-style loop on float locals (Stage B proofs).  Record
interp/JIT x level 0/2 on x86_64 and arm64 in this doc's Status
line when Part 3 lands.  Expected shape: JIT gains dominate (call
elimination), interpreter gains modest (dispatch-free helpers).

## 9. Out of scope (recorded so nobody "helpfully" adds them)

Long family (LADD...), double family (DADD...), unary INEG/FNEG/INOT,
INEQ/FEQ/FNEQ, mixed int/float typed promotion, inline INTEGER
div/mod with variable divisors (needs per-backend error-exit
plumbing; float division is already unrestricted via Part 0), typed
LAND/LOR refinement, constant folding (separate future design — it
would make rules 1-4 fire on literal pairs), and any use of these ops
by the SIMD strip loop beyond the NHC-allowed-list update (the strip
body is vector ops by construction).
