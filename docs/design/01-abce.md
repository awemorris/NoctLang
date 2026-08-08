# 01 — Array Boundary Check Elimination (ABCE) for Packed

Status: **implemented** (2026-08-09; suites: tests/run-abce.sh,
run-scoping.sh, run-class.sh, run-typing.sh — all green).

**v2 addendum (overnight run, 2026-08-09):**
- **Width family**: `OP_PLOAD{8S,16U,16S,32,64}` / `OP_PSTORE{16,32,64}`
  added (element-indexed; semantics mirror `rt_get/set_packed_elem`,
  incl. uint32→int32 wrap and int64/uint64→long). `PSTORE8/16/32`
  assume an int-tagged source — guaranteed by rejecting long constants
  in the safe-expression rules plus the one-packed-per-loop rule;
  `PSTORE64` dispatches on int/long and stays a helper call.
- **Element-type constant propagation** (speculation seeding): a
  flow-insensitive scan collects `p = Packed.uint16(...)` creation
  facts with one-level copy propagation; the loop bets on the fact
  (float facts leave the loop unversioned), else defaults to uint8.
  Soundness rests entirely on the runtime `PCHECK` guard, not the
  analysis. Verified by tests/abce/width*.noct incl. a
  creation-invisible case that must take the slow path.
- **Guard is evaluated exactly once** into `$abceN_g`: the fast body
  may change the runtime type of a TYPEIS-guarded local (int
  accumulator turning long via 64-bit loads), so re-evaluating the
  guard for the else-branch would run BOTH versions (caught by
  tests/abce/width64.noct as a doubled sum).
- **Inline machine code on all 10 JIT backends** (x86_64, x86, arm64,
  arm32, riscv64, riscv32, mips32, mips64, ppc32, ppc64el), validated
  per-arch under qemu-user with run-abce + the 3-pattern run-syntax.
  32-bit backends keep PLOAD64/PSTORE64 as helper calls. Big-endian
  targets (mips32/ppc32) read the base long's low word at +12 and the
  packed pointer member at +8; BE-64 (mips64) stores int results as
  32-bit words at +8.
- **Soundness audit fixes (post-implementation review)**: (1) the
  bounds guard now checks BOTH endpoints against BOTH bounds — with
  int32 wrapping, checking only `f(lo) >= 0 && f(hi-1) < len` is
  unsound (a large invariant addend wraps `f(hi-1)` negative, passing
  `< len` while the loop reads out of bounds; caught by
  tests/abce/wrap_delta.noct).  With four checks a passing guard
  provably keeps every wrapped iteration index in `[0, len)`.
  (2) float/double constants are now rejected by the safe-expression
  rules alongside long constants: a float flowing into `PSTORE8/16/32`
  would write raw bits where the slow path converts
  (tests/abce/float_store.noct).  `ABCE_MAX_GUARDS` raised 4 → 8.
- **remacs is now "aware"**: `bufCharToByte` and `lineEndPosition`
  (editor/buffer.noct) were rewritten from data-dependent while-loops
  into byte-wise ranged-for scans — both version at level 2 (sites=2,
  bet=uint8).  Editor-level benchmark (multibyte random `gotoChar` +
  line movement, dev launcher with `REMACS_OPT_LEVEL=2`): **~1.33-1.37x**
  end-to-end, oracle-identical results.  Synthetic benches
  (tests/bench/run-bench.sh): interpreter 1.1-1.3x, JIT 1.2-1.6x.
- **Pre-existing arch bugs found & fixed in passing**: `env->line`
  written at offset 8 instead of 4 by the x86/arm32/riscv32/mips32/
  ppc32 JIT LINEINFO emitters (error line numbers were lost on all
  32-bit JITs). Pre-existing and NOT fixed (chipped): mips32/mips64/
  ppc32 JIT long-comparison bug (tests/syntax/48-numeric-cond).
Decisions herein are backed by the Decision Log in [00-overview.md](00-overview.md) (D1–D7).

## 1. Goal

Make ranged-for loops over `Packed` buffers run without per-access bounds
checks, by versioning the loop: a guard before the loop proves all
accesses in range, then a fast copy of the loop uses raw base-relative
loads/stores. The concrete beneficiary is the remacs gap buffer
(`apps/remacs/editor/buffer.noct`), whose hot loops look like:

```
var data = Buf.data;            /* Packed.uint8 — already hoisted to a local */
var delta = Buf.gapEnd - Buf.gapStart;
for (b in start..stop) {
    lead = data[b];             /* or data[b + delta] */
    ...
}
```

All real index expressions in buffer.noct are affine with coefficient 1:
`b`, `b + delta`, `b - v`. That is the entire pattern language for v1.

## 2. Current state of the code (verified facts)

Read this section carefully; the design leans on every one of these.

* **Ranged-for survives into HIR structurally.** `struct hir_block`
  (src/core/hir.h) has a FOR variant `val.for_` with `bool is_ranged`,
  `char *counter_symbol`, `struct hir_expr *start`, `*stop`, and
  `struct hir_block *inner` (first body block). Ranged-for iterates the
  half-open interval `[start, stop)` (docs/syntax.md: "from the start
  to one less than the end value").
* **LIR lowering** (`lir_visit_for_range_block`, src/core/lir.c):
  start and stop are each evaluated **once** into fresh tmpvars before
  the loop header; the counter is a named local
  (`lir_get_local_index`); the exit test is `OP_EQI cmp, counter,
  stop` (3-operand — a fresh cmp tmpvar) + `OP_JMPIFEQ cmp, exit`
  (exit when equal); increment is `OP_INC` (in-place, single
  operand); the back edge is a plain `OP_JMP`.
  **No `OP_SAFEPOINT` is emitted on the natural back edge.**
  `OP_SAFEPOINT` is emitted in exactly one place in the whole
  compiler: `lir_visit_basic_block` (src/core/lir.c:385–396), on an
  explicit `continue` edge to a loop head.
* **Subscript access:** `p[e]` is `HIR_EXPR_SUBSCR`, stored as a binary
  expr (`val.binary.expr[0]` = container, `expr[1]` = index).  It
  lowers to `OP_LOADARRAY dst,arr,subscr` / `OP_STOREARRAY
  arr,subscr,val`, which dispatch at runtime in
  `noct_ex_loadarray_helper` / `noct_ex_storearray_helper`
  (src/core/execution.c) to `rt_get_packed_elem` /
  `rt_set_packed_elem` (src/core/runtime.c) — these do the bounds check
  (`index >= packed->elem_size` → error `"Array index %ld is
  out-of-range."`) plus a per-access type switch.
* **Packed object** (`struct rt_packed`, src/core/runtime.h:143–160):
  payload pointer `void *packed_buffer`, element count `size_t
  elem_size`, element type `int type` (values like `NOCT_PACKED_UINT8`,
  `NOCT_PACKED_INT8`, … `NOCT_PACKED_ANY` accepted by intrinsics).
  remacs buffers are created with `Packed.uint8(n)` →
  `NOCT_PACKED_UINT8`. **No Packed resize exists** (no intrinsic, no
  `rt_resize_packed`) — a packed's payload never moves except when the
  GC moves the object.
* **GC can move a packed** (young copy `rt_gc_copy_packed_to_graduate`,
  promotion `rt_gc_promote_packed`, tenure compaction memmove +
  `packed_buffer` fixup in src/core/gc.c). GC runs from allocation
  sites (`rt_gc_alloc_*`); in MT builds, from safepoint polls
  (`om_safepoint` via `OP_SAFEPOINT`, and `rt_call` polls at function
  calls); plus the explicit `GC.youngGC`/`oldGC`/`compactGC`
  intrinsics and the host `noct_*_gc` API — the latter two are
  function calls and therefore already excluded from the fast body by
  E5. In ST builds `om_safepoint` is a no-op.
* **No HIR optimization pass exists yet**; HIR is built once
  (`hir_build`) and consumed by `lir_build`. HIR nodes are
  arena-allocated (`hir_malloc`, `hir_strdup`; individual free is a
  no-op). **There is no HIR clone helper** — versioning must add one.
* **Pipeline** (`rt_register_source`, src/core/runtime.c:340–403):
  `ast_build` → `hir_build` → per function `lir_build` →
  `rt_register_lir`. The C backend (`src/backend/cback.c`,
  `noct_cback_translate`) runs its own `ast_build`→`hir_build`→
  `lir_build` and decodes the resulting bytecode. The Emacs Lisp and
  Scheme backends (`elback.c`, `scmback.c`) consume **HIR directly**
  and never build LIR.
* **optimize level plumbing is currently missing.** `NoctConfig`
  has `int optimize_level` (include/noct/noct.h:122), the CLI parses
  `--optimize-level=N` (src/cli/cli-run.c:177), but nothing ever
  propagates it to the compiler; `int lir_optimize_level`
  (src/core/lir.c:89) is file-local, always 0, and only gates
  `OP_LINEINFO` emission (level 0 emits line info; ≥1 omits).
* **tmpvar budget:** `TMPVAR_MAX` = 128 per function (lir.c), matching
  `RT_TMPVAR_MAX` (runtime.h). Locals occupy the low slots (indices
  assigned in `hir_add_local`), scratch tmpvars live above them, LIFO.
* Backends: 10 JIT arch files in src/core (`jit-x86.c, jit-x86_64.c,
  jit-arm32.c, jit-arm64.c, jit-mips32.c, jit-mips64.c, jit-ppc32.c,
  jit-ppc64.c, jit-riscv32.c, jit-riscv64.c`), selected at compile
  time by `NOCT_ARCH_*` in jit.c. The msdos (OpenWatcom, DOS4G) preset
  builds with **JIT ON** using `jit-x86.c` (32-bit). A JIT switch that
  hits an unknown opcode does `assert(JIT_OP_NOT_IMPLEMENTED)` and in
  NDEBUG silently misdecodes — so every new opcode **must** be
  implemented in **all 10** JIT backends (a helper-call implementation
  is sufficient; see §7).

## 3. Specification

### 3.1 Eligibility (all must hold)

A ranged-for loop L in function F is eligible iff:

* **E1** `is_ranged` is true. (Step is always +1, interval `[lo, hi)`.)
* **E2** The loop body contains **only basic blocks and IF blocks** —
  no nested FOR/WHILE (their back edges are out of scope for v1), and
  **no `continue`** (a continue edge emits `OP_SAFEPOINT`).  `break`
  and `return` are allowed (they only leave the loop).
* **E3** Every subscript on a Packed inside the body has the shape
  `p[f(i)]` where:
  - `p` is a **local variable** of F (not a dot/global expression) and
    is never assigned inside the body;
  - `f(i)` is one of: `i`, `i + v`, `v + i`, `i - v`, `i + c`,
    `i - c`, `c + i`, where `i` is the loop counter, `c` an integer
    constant, and `v` a local never assigned inside the body.
  Subscripts on things that are *not* provably these shapes make the
  loop ineligible. (We cannot tell statically whether `x[e]` is a
  packed, an array, or a dict — the guard settles that at runtime, but
  only for the shapes above.)
* **E4** The loop counter `i` is never assigned inside the body.
* **E5** The body contains **no function calls** (`HIR_EXPR_CALL`,
  `HIR_EXPR_THISCALL`, `HIR_EXPR_NEW`), **no allocating literals**
  (`HIR_EXPR_ARRAY`, `HIR_EXPR_DICT`), and **no string constants**
  (`HIR_TERM_STRING`). String constants are NOT interned: `OP_SCONST`
  allocates a fresh GC string on every execution
  (`rt_visit_sconst_op` → `rt_make_string_with_hash` →
  `rt_gc_alloc_string`), which could trigger GC inside the fast body
  and move the packed. `HIR_EXPR_PLUS` and every other arithmetic
  node is constrained by E6 instead. (int/long/float/double constants
  are unboxed — fine.)
* **E6 (int-purity)** Every leaf operand of every arithmetic /
  comparison / bitwise expression in the body must be one of:
  - the loop counter `i`,
  - an integer constant (`HIR_TERM_INT`),
  - the result of a Packed load `p[f(i)]` matching E3 (uint8 loads
    produce ints),
  - a local `v` that is **never assigned in the body** — each distinct
    such `v` gets an `is-int` test added to the guard (§3.2), capped
    below,
  - a local that **is** assigned in the body, provided every
    assignment to it inside the body has an RHS that is itself
    int-pure under these rules (accumulators like
    `lead = data[b];` qualify).
  Rationale: `OP_ADD` on two ints never allocates, but on strings it
  allocates (→ GC → the cached base pointer dangles). Int-purity plus
  the runtime `is-int` guards makes allocation in the fast body
  impossible without needing any static type information.
* **E7 (caps)** At most **4** distinct guarded subscript sites and at
  most **4** distinct `is-int`-guarded locals. Over the cap →
  ineligible (guard cost would eat the win).

Everything ineligible simply keeps today's code. There is no partial
optimization.

### 3.2 The guard

For each guarded packed `p` with access set `{f_k}` over `[lo, hi)`,
with each `f_k` monotone increasing or decreasing in `i` (coefficient
±1 — note `i - v` still has coefficient +1; there is no `-i` form in
v1, so all forms are increasing):

```
guard :=  TYPEIS(lo, int) && TYPEIS(hi, int)   /* FIRST: TYPEIS never errors */
       && lo < hi                              /* non-empty, sane   */
       && PCHECK(p, NOCT_PACKED_UINT8)         /* packed + elem type */
       && TYPEIS(v_j, int)   for each guarded invariant local v_j
       && f_k(lo)     >= 0         for each access site k
       && f_k(hi - 1) <  PLEN(p)   for each access site k
```

**Ordering matters:** the int-ness of `lo`/`hi` must be established
before `lo < hi`, because `OP_LT` raises "Value is not a number." on
non-numeric operands while today's slow lowering (`OP_EQI`) does not —
evaluating `lo < hi` first would create a level-2-only error on
degenerate inputs, breaking level-identical behavior. `TYPEIS`,
`PCHECK`, and `PLEN` never raise. `PLEN` is a new opcode: `OP_LEN`
exists but has **no HIR expression kind** (it is emitted only inside
for-each lowering), so the guard needs its own guard-safe length op.

Notes:

* The interval is half-open, so the largest index is `f_k(hi-1)`.
  The upper check is **strict less-than** (`0 <= idx && idx < len`).
* With coefficient +1, `f_k` is increasing, so checking the two
  endpoints bounds every iteration. Evaluate the endpoint expressions
  with `OP_ADD`/`OP_SUB` on the already-type-guarded ints; int
  arithmetic here cannot allocate.
* `lo`/`hi` are the same tmpvars the slow loop would use; evaluate
  start/stop **once**, before the guard, and share them with whichever
  loop version runs (do not re-evaluate user expressions twice —
  they may have side effects; see §3.4).
* Guard failure falls through to the original, checked loop. There is
  no deopt, no OSR, no mid-loop transfer: the version choice is made
  entirely before loop entry. This is the main simplicity win.

### 3.3 The fast loop

```
base = PBASE(p)          /* 64-bit payload address, as a long value */
for (i in lo..hi) {      /* same lowering as today: EQI/JMPIFEQ/INC */
    ... p[f(i)]      -->  PLOAD8U(base, f(i))
    ... p[f(i)] = x  -->  PSTORE8(base, f(i), x)
}
```

* `PBASE` materializes `packed->val.packed->packed_buffer` as an
  int64 in a tmpvar (`NOCT_VALUE_LONG`). GC never scans int/long
  slots, so holding a raw address in a value slot is safe.
* **Invariant (hard):** no safepoint source may exist between the
  `PBASE` and the last use of `base`. By construction (E2/E5/E6) the
  fast body has no calls, no allocation, no `OP_SAFEPOINT`; the back
  edge has none today. Add a debug-build check in the ABCE pass that
  asserts the fast subtree contains no CALL/THISCALL/NEW/ARRAY/DICT
  nodes after construction.
* Bounded STW delay (MT builds): the fast loop cannot poll, but it is
  finite (`hi - lo` iterations of straight-line code), so the worst
  case STW latency added is the loop's own runtime (ms-scale for
  full-buffer scans). Accepted per D3. Strip-mining is the future
  escape hatch; do not build it now.
* On 32-bit targets (x86 JIT for msdos), the 64-bit base value is
  truncated to 32 bits when loaded into registers; real pointers fit.

### 3.4 Transform shape (HIR level)

The pass rewrites an eligible FOR block `L` in place into:

```
t_lo = <start expr>          /* hoisted: evaluated exactly once  */
t_hi = <stop expr>
IF (guard(t_lo, t_hi, ...)) {
    base = PBASE(p)
    FOR (i in t_lo .. t_hi) { body' }     /* body with PREF ops   */
} ELSE {
    FOR (i in t_lo .. t_hi) { body  }     /* verbatim clone       */
}
```

Blocks link via `succ`, `inner`, **and if-chain edges**
(`val.if_.chain_next`/`chain_prev` — lowering traverses `chain_next`
as an owned successor, lir.c:523–524); only the loop-body-tail `succ`
(back edge) and exit `succ` are non-owning. Implement:

* `hir_clone_expr`, `hir_clone_stmt_list` — straightforward recursive
  arena copies (`hir_malloc` / `hir_strdup`).
* `hir_clone_body(for_block)` — clones the inner block chain,
  **including, for IF blocks: `val.if_.cond`, `val.if_.inner`, and
  the whole `chain_next` list (fixing `chain_prev` back-pointers)**.
  **Wiring rules (critical):** inner blocks chain via `succ` with the
  tail marked `stop = true`; a loop-body tail's `succ` is a
  **non-owning back edge**. Mirror exactly what `hir_visit_for_stmt` +
  `hir_visit_stmt_list` produce (src/core/hir.c). The practical recipe:
  after building the two new FOR blocks and the IF/ELSE skeleton,
  re-establish `parent`, `succ`, `stop` the same way those builders
  do, with the IF chain's exit `succ` pointing at the original loop's
  `succ`. Verify by writing the equivalent source by hand
  (`if (1) { for ... } else { for ... }`), dumping both HIR-lowered
  bytecodes (`lir_dump`), and comparing shapes.
* **Placement note:** `hir_malloc`, `hir_strdup`, `hir_add_local`, and
  the arena are all `static` inside hir.c — a separate hiropt.c
  translation unit cannot reach them. Therefore the pass lives
  **inside src/core/hir.c** (a clearly-delimited `/* ABCE */` section
  of static functions) with one exported entry point declared in
  hir.h: `bool hir_optimize_func(struct hir_block *func_block, int
  level);`. (This supersedes §3.8's separate-file instruction.)
* The hoisted `t_lo`/`t_hi`/`base` become fresh locals via
  `hir_add_local`, named **`$abce<N>_lo` / `$abce<N>_hi` /
  `$abce<N>_base`** with `N` a per-function counter (collision-proof:
  `$` is unlexable; unique per versioned loop so `hir_add_local`'s
  dedup-by-name cannot alias two loops). They consume 3 of the 128
  tmpvar slots per optimized loop; guard scratch uses the normal LIFO
  tmpvars in lir.
* The new statements for the guard are ordinary `hir_stmt`s using
  existing expr kinds plus the new ones below; the IF/ELSE uses
  ordinary `HIR_BLOCK_IF` machinery.

### 3.5 New HIR expression kinds

Append to `enum hir_expr_type` (src/core/hir.h):

```c
HIR_EXPR_PBASE,     /* unary:  packed local  -> payload address (long)  */
HIR_EXPR_PLEN,      /* unary:  packed local  -> element count (int)     */
HIR_EXPR_PCHECK,    /* binary: value, elemtype-int-const -> 0/1         */
HIR_EXPR_TYPEIS,    /* binary: value, type-int-const     -> 0/1         */
HIR_EXPR_PLOAD8U,   /* binary: base(long), offset(int)   -> int (0..255)*/
HIR_EXPR_PSTORE8,   /* as LHS only: base(long), offset(int); RHS = value*/
```

* Use the same storage as the existing unary ops (`HIR_EXPR_NEG`) for
  `PBASE`, and `val.binary.expr[0..1]` for the rest, mirroring how
  `HIR_EXPR_SUBSCR` doubles as load (RHS) and store target (LHS).
* Only the ABCE pass creates these nodes; the parser never does.
  elback/scmback therefore never see them (they run `hir_build`
  without the pass — see §6).
* The width/sign family is deliberately wider in the opcode namespace
  (see below) than what v1 emits: v1 emits **only** `PLOAD8U`/`PSTORE8`
  guarded by `NOCT_PACKED_UINT8`. Other widths are future work driven
  by typed annotations; reserve the enum space, do not implement.

### 3.6 New LIR opcodes

**Append at the end** of `enum bytecode` (src/core/bytecode.h) — never
insert in the middle (comments carry fixed hex numbers; inserting
renumbers everything):

```c
OP_PBASE,    /* 0x2e: dst:u16 = payload_addr(src:u16)                  */
OP_PCHECK,   /* 0x2f: dst:u16 = (src:u16 is packed && elemtype==imm:u8)*/
OP_TYPEIS,   /* 0x30: dst:u16 = (typeof(src:u16) == imm:u8)            */
OP_PLOAD8U,  /* 0x31: dst:u16 = *(uint8*)(base:u16 + ofs:u16)          */
OP_PSTORE8,  /* 0x32: *(uint8*)(base:u16 + ofs:u16) = src:u16          */
OP_PLEN,     /* (after OP_CHECKTYPE): dst:u16 = elem_size(src:u16), 0 if not packed */
```

(Six ABCE opcodes total; the enum order interleaves OP_CHECKTYPE from
02-typing — see bytecode.h for the authoritative numbering.)

Operand encoding follows existing conventions (u8 opcode, u16 BE
tmpvars, u8 immediate for the type constants — use `lir_put_imm8`).
`OP_TYPEIS`'s immediate is a `NOCT_VALUE_*` constant; `OP_PCHECK`'s is
a `NOCT_PACKED_*` constant.

Semantics (implemented as `noct_ex_*_helper` in src/core/execution.c,
shared by interpreter and JIT):

* `OP_PBASE`: src must be packed (the guard guarantees it, but the
  helper still checks and errors if not — belt and braces, and it
  keeps the opcode safe if some future pass misuses it). dst :=
  LONG((int64)(intptr_t)packed_buffer).
* `OP_PCHECK`: dst := INT(src.type == NOCT_VALUE_PACKED &&
  src.val.packed->type == imm). Never errors.
* `OP_TYPEIS`: dst := INT(src.type == imm). Never errors.
* `OP_PLOAD8U`: dst := INT(*(uint8_t *)((intptr_t)base.val.l +
  (intptr_t)ofs_int)). **No checks** — the guard proved the range.
  ofs is read as int (guarded).
* `OP_PSTORE8`: `*(uint8_t*)(base + ofs) = (uint8_t)src_int`. No
  checks. (Store of an int; the guard's int-purity covers the source.)

### 3.7 Optimization level plumbing (Task 0 — prerequisite)

* Add `void lir_set_optimize_level(int level);` to lir.h/lir.c
  (writes the existing `lir_optimize_level` — a non-static global that
  no header declares, so nothing outside lir.c touches it today).
* In `rt_register_source` (src/core/runtime.c), before the per-function
  loop: `lir_set_optimize_level(env->vm->config.optimize_level);`
  and run the ABCE pass (below) when `optimize_level >= 2`.
* Same plumbing in `rt_register_bytecode` is NOT needed (bytecode is
  already compiled).
* C backend: `noct_cback_translate(fname, data)` takes no config and
  no VM exists on that path. Add
  `void noct_cback_set_optimize_level(int level);` to
  include/noct/backend.h + cback.c (stores a static level);
  `src/cli/cli-ctrans.c` parses `--optimize-level=N` and calls it
  before translating; `noct_cback_translate` then calls
  `lir_set_optimize_level(level)` and runs `hir_optimize_func` in its
  own per-function loop when level ≥ 2. This is the "emit
  bounds-check-free C" payoff. Update the CLI usage text
  (src/cli/cli-main.c:128) from `(0/1)` to `(0/1/2)`.
* remacs dev launcher: apps/remacs/src/main.c currently calls
  `noct_create_vm(&vm, &env, NULL)` (NULL config). For the level-2
  acceptance run, build a `NoctConfig` via `noct_set_default_config`
  and set `optimize_level` from a `REMACS_OPT_LEVEL` environment
  variable (default 0 = today's behavior).
* Level semantics: 0 = debug (line info, no opts); 1 = no line info;
  2 = level 1 + ABCE (+ typed entry checks, see 02-typing.md).

### 3.8 The pass itself

* Lives inside `src/core/hir.c` (see §3.4 placement note — the arena
  and local machinery are static there). No CMake change needed.
  C89 discipline as everywhere in core.
* Entry point: `bool hir_optimize_func(struct hir_block *func_block,
  int level);` (declared in hir.h) called from `rt_register_source`
  for each `hir_get_function(i)` before `lir_build`, and from
  `noct_cback_translate`'s own function loop. Returns false only on
  internal error (arena OOM).
* Structure:
  1. Walk blocks recursively; find FOR blocks with `is_ranged`.
  2. `abce_check_eligibility()` — E1..E7. Implement the def-set walk
     as a recursive scan of the body subtree collecting
     "locals assigned" (LHS symbol terms of statements, counter of
     nested loops) and rejecting on forbidden node kinds.
  3. `abce_version_loop()` — build the guard, clone, rewrite
     subscripts in the fast clone (`HIR_EXPR_SUBSCR` on `p` with shape
     f(i) → `HIR_EXPR_PLOAD8U`/`PSTORE8` against the `base` local).
  4. Debug verification: assert the fast subtree has no
     CALL/THISCALL/NEW/ARRAY/DICT, **no HIR_TERM_STRING**, and no
     FOR/WHILE.
* Do not transform nested candidates inside an already-versioned loop
  (E2 excludes nesting anyway). Process siblings independently.
* Keep the pass allocation in the HIR arena (`hir_malloc`) — the arena
  is alive until `hir_cleanup()` after all functions are lowered.

## 4. What NOT to do (explicitly out of scope)

* No SSA (D2). No IV discovery beyond the ranged-for structure.
* No coefficient ≠ 1 affine forms, no `hi`-exclusive vs inclusive
  variants, no negative-step loops (the language has none).
* No strip-mining / safepoint re-insertion.
* No Array support, no dict, no strings.
* No widths other than 8-bit unsigned in the *pass* (opcodes for other
  widths are reserved but unimplemented).
* No OSR/deopt machinery of any kind.

## 5. Backend work

For each of the 5 new opcodes:

| Component | What to do |
|---|---|
| `src/core/interpreter.c` | `case OP_*:` in `rt_visit_op` + a `rt_visit_*_op` inline handler calling the `ex_*_helper` alias. Follow `OP_ADD`'s pattern. |
| `src/core/execution.c` | The 6 helpers (long names `noct_ex_*_helper`). Declare them in `include/noct/aot.h`, **and add matching `#define ex_X_helper noct_ex_X_helper` lines to BOTH `src/core/interpreter.h` (17–55) and `src/core/jit.h` (23–55)** — those duplicated alias lists are what the interpreter and all 10 JIT files actually reference; aot.h alone leaves them undeclared. |
| `src/core/lir.c` | Emission from the new HIR exprs in `lir_visit_expr` / store path in `lir_visit_stmt` (mirror the SUBSCR handling), **and** `lir_dump` disassembler cases (unknown opcodes assert there). |
| `jit-x86_64.c` | `OP_PLOAD8U`/`OP_PSTORE8`/`OP_PBASE` inline: `movzx reg8` / `mov [base+ofs], reg8` style with the tmpvar frame base in `r15` (see `jit_visit_assign_op` for the frame layout idiom). `OP_PCHECK`/`OP_TYPEIS` may call helpers. |
| `jit-x86.c` (msdos!) | Same, 32-bit registers; base long value truncates. Inline if straightforward, otherwise helper-call. |
| other 8 JIT files | **Helper-call implementations only** — copy the `OP_ADD` → `ASM_BINARY_OP(ex_add_helper)` pattern each backend already has. ~15 lines per backend per opcode group. Correctness first; inlining is future work. |
| `src/backend/cback.c` | `case OP_*:` emitting direct C: `uint8_t *` arithmetic for PLOAD/PSTORE (this is the "ABCE'd C output" payoff), helper calls for the checks — **emit the long `noct_ex_*` names** (aot.h declares only those; existing short-name emission relies on C89 implicit declarations, a pre-existing wart — do not imitate it for new ops). |
| `elback.c` / `scmback.c` | **No change.** They consume HIR and never run the pass. Add a defensive `default:`-style fatal for the new HIR expr kinds if trivial, but they are unreachable. |
| `bcback.c` | No change — it compiles via its own `hir_build`/`lir_build` **without** the pass, so `.nb` output stays unoptimized (intentional for v1). |

Known pre-existing gaps you will trip over (fix them as part of this
work, they are one-liners): `lir_dump` is missing `case OP_SHL` /
`OP_SHR`; `cback.c` is missing `OP_LICONST`, `OP_LFCONST`,
`OP_SAFEPOINT`.

## 6. Interaction with other backends and features

* elback/scmback run `ast_build`+`hir_build` themselves and never call
  `hiropt_run_func` → they never see ABCE output. Do not add the pass
  there; the Emacs oracle must keep seeing plain HIR.
* The typing feature (02-typing.md) reuses `OP_TYPEIS`'s helper family
  and the same level-≥2 gate. Land ABCE first (D-order in overview).
* MT builds: nothing new to do — the fast body cannot reach
  `om_safepoint`, allocation, or `rt_call`. The ST/MT object model
  split does not touch these opcodes.

## 7. Testing plan

Create `tests/abce/` following the `tests/syntax/` golden-diff pattern
(`run-abce.sh`, cases as `NAME.noct` + `NAME.noct.out`), and run each
case at `--optimize-level=0` AND `--optimize-level=2`, plus
`--disable-jit` / `--force-jit` variants — **four runs per case, all
outputs must be identical to the golden file**:

1. `sum.noct` — sum of `p[i]` over the whole buffer; prints the sum.
2. `offset.noct` — `p[i + delta]`, `p[i - delta]`, `p[delta + i]`.
3. `store.noct` — fill/copy loop writing `p[i] = ...`, then verify by
   reading back and printing.
4. `oob.noct` — loop whose range exceeds `elem_size` (guard fails →
   slow path → the exact existing error message
   `Array index %ld is out-of-range.` must appear, same as level 0).
   Note: error-message LINE NUMBERS are debug info — emitted at level
   0, omitted at level ≥ 1 (`OP_LINEINFO`), so level-2 errors read
   `:0:`. The runner supports a `NAME.noct.out2` golden for exactly
   this delta (it is a property of level ≥ 1, not of ABCE).
5. `notpacked.noct` — same loop shape over an Array (guard PCHECK
   fails → slow path; results identical).
6. `strlocal.noct` — body reads an invariant local that holds a string
   (TYPEIS guard fails → slow path; identical output).
7. `empty.noct` — `for (i in 5..5)` (guard `lo < hi` fails; slow path
   runs zero iterations). **Do NOT add a reversed-range case**: with
   today's `OP_EQI` exit test, `for (i in 5..3)` is a de-facto
   infinite loop — it cannot be captured in a golden file. (The guard
   correctly routes it to the slow path, preserving that behavior.)
8. `sideeffect.noct` — start/stop expressions with side effects
   (e.g. calling a function that prints) — must print exactly once at
   both levels (catches double-evaluation bugs in hoisting).
9. `mutbase.noct` — `p` reassigned inside the body → must remain
   unoptimized (behavioral identity is the observable).
10. `nested.noct`, `continue.noct` — ineligible shapes; identity.
11. `int8.noct` — a `Packed.int8` buffer: PCHECK(uint8) fails → slow
    path; identical results.

Then:

* **remacs gate:** full `apps/remacs/tests` suite green at default
  level, and again with the remacs launcher forced to
  `--optimize-level=2` (edit `apps/remacs/src/main.c`'s config or use
  env-var plumbing if present — verify how config is set there first).
* **Cross-target builds:** Linux, mingw, msdos all compile (msdos
  exercises the x86 JIT path and C89 discipline).
* **Perf smoke:** a crude before/after timing script (10 MB
  `Packed.uint8`, sum loop, `time` at level 0 vs 2, interpreter and
  JIT). Not a pass/fail gate; record numbers in the PR description.
* **MT build:** `cmake --preset mt-debug`, run `tests/run-all.sh`
  (core suites) to confirm nothing regressed under MULTITHREAD.

## 8. Acceptance criteria

* All new tests pass; all pre-existing suites pass unchanged.
* Level 0/1 output is byte-identical to pre-change builds (the pass
  must be a no-op below level 2).
* `lir_dump` can disassemble every opcode the compiler can emit
  (including the 5 new ones and the SHL/SHR fix).
* All 10 JIT backends compile; x86_64 and x86 execute the abce test
  suite correctly with `--force-jit`.
* `--ansic` output of `sum.noct` at level 2 contains a raw pointer
  access in the loop and no `ex_loadarray_helper` call for it.
* No commits — leave everything uncommitted for owner review.
