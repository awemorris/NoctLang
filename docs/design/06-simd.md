# 06 — 128-bit SIMD Auto-Vectorization of ABCE Fast Loops

Status: **Phase A and FP32x4 Phase B implemented and functionally validated**
(2026-08-10).  Runtime capability checks, native JIT tiers, and portable
scalar fallbacks are in place.  QEMU-user executes the ARM NEON/ASIMD and
PowerPC AltiVec paths; M5 Mac and POWER8 runs are optional performance work.

Implementation notes vs. this plan (reviewed deviations):
- Operand encoding: vreg indices / lanes / shift counts are **imm8**
  operands, not u16 (u16 operands go through the tmpvar frame-size
  validators — the design-07 lesson).  Helpers keep the uniform
  (env, int, int, int) signature.
- The LIR/HIR shared slot-need formula uses destination-reuse
  (Sethi-Ullman-style: non-term left operands and commutative lone
  right operands build in the destination), or the flagship blend
  does not fit the 8-vreg budget (measured: bases=2 consts=1 temps=4
  depth=3 = 8 exactly).
- Win64 (IS_MSABI) uses the direct scalar path instead of register-mapped
  SSE, touching only volatile xmm0.  It therefore does not need to save
  callee-saved xmm6/xmm7.  SysV x86_64 and x86-32 use CPUID-gated
  SSE2/SSE4.1 tiers.
- Packed parameters can carry element and alias facts through the
  `packedint*`/`packeduint*`/`packedfloat`/`packeddouble` annotations.
  Prefix `r` supplies the checked restrict contract used by vectorization.
- Multi-packed loops reject 64-bit bets (cross-width store hazard);
  single-packed 64-bit keeps scalar behavior.

Benchmarks (x86_64, tests/bench/b12_vblend.noct: 16 frames x 1M
RGBA32 pixels, per-pixel alpha, best of 3):
  interpreter: L0 18.33s / L2-noSIMD 14.55s / L2+SIMD 3.23s
               (x5.7 vs L0; the C lane emulation alone wins x4.5)
  JIT:         L0 2.750s / L2-noSIMD 0.766s / L2+SIMD 0.093s
               (x29.6 vs L0; SIMD alone x8.2 over ABCE+typed-ops)
arm64 under qemu-user runs byte-identical (timings there are not
hardware-representative).

This document was designed in discussion with the project owner; every
open question has been resolved.  Treat it as the single source of
truth and do NOT re-litigate decisions recorded in the Decision Log.
Read [01-abce.md](01-abce.md) first (this design is an extension of the
ABCE fast loop), then [05-cse.md](05-cse.md) (pass ordering, budget
discipline, golden-test culture).  The Global Invariants in
[00-overview.md](00-overview.md) — C89 core, GC discipline, all three
targets build, every backend handles every opcode, remacs acceptance
gate, DO NOT COMMIT — all apply here without exception.

---

## 0. One-paragraph summary

At optimize level >= 2, a ranged-for loop that ABCE has already
versioned (guarded fast body with raw `PLOAD32`/`PSTORE32` accesses on
int32/uint32 Packed buffers) is additionally split into a **4-lane
strip loop plus a scalar remainder loop**.  The strip loop's body is
the same HIR expression tree lowered by a parallel LIR visitor into new
128-bit vector opcodes over a small **vector register file** (16 x 16
bytes in `struct rt_env`).  The x86_64 JIT emits SSE2/SSE4.1
(runtime-gated by CPUID), the arm64 JIT emits NEON (always available on
AArch64).  SIMD-capable ARMv7 and PowerPC targets use NEON/AltiVec when
detected.  Every JIT has a direct scalar machine-code tier over
`env->vreg`; the interpreter and cback use the portable C89 reference
implementation.  All observable behavior is bit-identical at every level,
on every backend (the ABCE/CSE golden-test bar, D-CSE12, applies).

The concrete beneficiary is RGBA32 image processing: pixels stored one
per element in a `Packed.uint32`, channels extracted with shifts and
masks.  An alpha-blend loop over two such buffers is the flagship test
and benchmark.

```
/* Flagship shape (tests/simd/blend.noct).  Every operation in this
   body is on the Phase A vector op list. */
func blend(src, dst, n, a) {
    var na = 255 - a;
    for (i in 0..n) {
        var s = src[i];
        var d = dst[i];
        var sr = (s >> 16) & 255;   /* logical shift: see 6.1 */
        var dr = (d >> 16) & 255;
        var r  = ((sr * a + dr * na) >> 8) & 255;
        /* ... same for g, b ... */
        dst[i] = (r << 16) | (g << 8) | b;
    }
}
```

## 1. Decision Log (authoritative)

- D-SIMD1. **Vector width is fixed at 128 bits, VF = 4 lanes of
  int32.**  No 256-bit, no AVX, no scalable vectors.  The portable
  128-bit subset is a proven design point (WebAssembly SIMD128 maps
  1:1 onto both SSE and NEON); we take the same subset philosophy.
- D-SIMD2. **Phase A is i32x4 only.**  Element type bet must be
  `NOCT_PACKED_INT32` or `NOCT_PACKED_UINT32` for every packed in the
  loop.  i8x16/i16x8 are rejected permanently for auto-vectorization
  (exact scalar semantics widen every u8 to int32; proving 8/16-bit
  lane sufficiency needs relational range analysis we will not build).
  f32x4 is Phase B.  RGBA pixels travel as one uint32 element each;
  channel math is shifts and masks, which vectorize as-is.
- D-SIMD3. **Only ABCE fast bodies are vectorized.**  The SIMD pass
  runs immediately after ABCE inside `hir_optimize_func()` and only
  looks at loops ABCE marked as versioned-fast.  It never creates its
  own guards for bounds/types — it reuses ABCE's proof and adds only
  the strip-entry condition (5.4) and the alias condition (5.5).
- D-SIMD4. **Semantics are bit-identical, no exceptions.**  A
  vectorized program must produce byte-identical output to level 0 on
  every backend.  Anything that cannot be made bit-identical is
  rejected from the eligible-op list (int division, comparisons,
  float in Phase A).  The test harness enforces this the same way
  run-abce.sh / run-cse.sh do.
- D-SIMD5. **Vector state lives in a per-env scratch register file**
  (`env->vreg[16][16]`, raw bytes): never in `rt_value` slots, never
  scanned by the GC, never live across a safepoint, helper call, or
  the strip loop's exit (except via the explicit lane-3 extraction,
  5.7).  Register-mapping JITs (x86_64, arm64) may keep vregs entirely
  in hardware registers because the strip region is guaranteed
  helper-call-free and safepoint-free by construction (5.6).
- D-SIMD6. **Program-visible vregs are capped at 8 (indices 0..7).**
  x86_64 maps vreg k -> xmm k, arm64 maps vreg k -> v k.  Rationale:
  xmm0..7 avoid REX-encoded register numbers; v0..v7 are caller-saved
  under AAPCS64 so the arm64 prologue needs no change.  On Win64
  (mingw preset) xmm6..15 are callee-saved, so the current JIT keeps
  vector opcodes on the direct scalar path and needs no XMM save/restore.
  If a loop needs more than 8 vregs
  it is silently not vectorized.
- D-SIMD7. **All vector opcodes take exactly three imm8 operands**
  (unused trailing operands are encoded as 0), and every C helper has
  the signature `bool helper(NoctEnv *env, int a, int b, int c)`.
  This lets the interpreter reuse `BINARY_OP(...)` verbatim.  JIT backends
  decode the same uniform operands and emit either a capability-gated native
  SIMD sequence or direct scalar host instructions over `env->vreg`.
- D-SIMD8. **Multi-packed ABCE (up to 4 packeds per loop) is a
  prerequisite** and is Part A of this work.  The alpha-blend loop
  reads `src` and writes `dst`; the current one-packed-per-loop rule
  makes it ineligible.  The element-type bet becomes per-packed; a
  loop is SIMD-eligible only if every bet is int32/uint32 (mixed-width
  loops stay scalar-versioned).
- D-SIMD9. **Alias discipline** (5.5): same packed local on both a
  store site and any other site => all its sites must have
  syntactically identical offsets, else no vectorization.  Two
  different packed locals with a store on either side => a runtime
  disjointness check is added to the strip-entry condition (payload
  ranges disjoint, OR bases equal when static offsets match).  This is
  required because Packed payloads can partially overlap through the
  preallocated-buffer C API; object distinctness alone is not enough.
- D-SIMD10. **The strip loop iterates a prefix of the scalar
  iteration sequence** — `lo, lo+4, ..., mid` with
  `mid = hi - ((hi - lo) & 3)` — and is entered only when
  `0 <= lo && lo < mid` (all int arithmetic).  The `0 <= lo` conjunct
  is a soundness requirement, not an optimization: with a negative or
  wrapping counter range, lanes `i+1..i+3` of a vector access are not
  address-consecutive with scalar iterations (sign-extension of the
  32-bit counter breaks 64-bit address contiguity at the 2^31
  boundary).  See 5.4 for the full argument.
- D-SIMD11. **No new scalar-side semantics.**  Vector op semantics
  are defined by lane-wise application of the existing scalar helper
  semantics: ADD/SUB/MUL wrap modulo 2^32; AND/OR/XOR are bitwise;
  SHL/SHR are **logical** shifts on the uint32 bit pattern (this is
  what `noct_ex_shl_helper` / `noct_ex_shr_helper` do for int
  operands — verify by reading them before implementing; do NOT use
  arithmetic-shift vector instructions).  Shift counts must be int
  constants 0..31 or the loop is not vectorized.
- D-SIMD12. **MT model**: vector loads/stores are not element-atomic
  and may tear across the 16-byte group differently than scalar
  accesses.  The MT runtime already declares racy Packed access
  unordered; this is accepted.  The strip loop, like the ABCE fast
  loop, has no safepoint (D3's bounded-STW argument covers it: the
  strip loop is strictly shorter than the scalar fast loop it
  replaces).
- D-SIMD13. **Pass order: ABCE -> SIMD -> CSE.**  CSE must skip the
  subtree of a vector-marked FOR block (its body must stay within the
  vector-eligible expression grammar; a `HIR_EXPR_CAPTURE` node would
  break the vector LIR visitor).  The two scalar clones (remainder and
  else-branch full loop) remain fair game for CSE.
- D-SIMD14. **Kill switch and observability**: `NOCT_SIMD_DISABLE=1`
  in the environment skips the pass entirely;
  the public `--simd-info` CLI option reports only successful
  transformations as `SIMD: <file>:<line>: vectorized (<kind>x4)` to
  stderr;
  `NOCT_SIMD_DEBUG=1` prints one line per vectorized loop to stderr
  (`SIMD: <func>: vectorized (packeds=%d sites=%d inv=%d temps=%d consts=%d depth=%d)`).
  Mirrors the NOCT_CSE_DEBUG precedent.  The debug line is load-bearing
  for tests: golden output tests pass trivially when the pass silently
  stops firing, so run-simd.sh asserts the debug line for the cases
  that must vectorize.

## 2. Verified facts about the current code

Every one of these was read from the source; the design leans on all
of them.  Re-verify with grep before implementing — if any fact no
longer holds, STOP and report instead of adapting silently.

* **ABCE loop versioning** (`abce_version_loop`,
  src/core/hir_opt_abce.c): converts the original FOR into a hoist
  BASIC block (`$lo = start; $hi = stop; $g = <guard>`), then
  `G1: if ($g) { B1: $base = PBASE(p); FAST-for }`,
  `G2: if (!$g) { SLOW-for }`.  FAST's body is a **clone**
  (`abce_collect_blocks` / `abce_clone_blocks` / `abce_map_block`,
  bounded by `ABCE_MAX_BLOCKS`); packed subscripts in it are rewritten
  to `PLOAD*/PSTORE*($base, idx)` by `abce_rewrite_block`.  The guard
  (`abce_mk_guard`) is `TYPEIS($lo,int) && TYPEIS($hi,int) &&
  TYPEIS(v,int)... && $lo < $hi && PCHECK(p,bet) && <4 endpoint bounds
  checks per site>`, evaluated exactly once into `$abceN_g`.
* **Eligibility** (`abce_check_expr` / `abce_check_stmt` /
  `abce_scan_chain`): body blocks are BASIC or IF only; no nested
  loops; no `continue`; no calls/allocs/DOT; long/float/double/string
  constants rejected; every local read adds a TYPEIS-int guard
  (`ABCE_MAX_GUARDS` cap); counter assignment rejected; global
  reads/writes rejected.  Index shapes: `i`, `i+u`, `u+i`, `i-u` with
  `u` an int constant or invariant local (`abce_check_site`).
  **`ctx->packed` is a single local** — the one-packed-per-loop rule
  lives in `abce_check_site` ("All sites must target one packed
  local").  Part A relaxes exactly this.
* **`struct rt_value` is 16 bytes**: `type` at offset 0, 8-byte value
  union at offset 8 (include/noct/noct.h).  Zero-cleared = int 0.
* **x86_64 JIT conventions** (src/core/jit-x86_64.c): r13 =
  exception_handler, r14 = env, r15 = `&env->frame->tmpvar[0]`;
  rax/rcx/rdx/rbx are per-op scratch.  `jit_visit_pload32_op` emits:
  `movq base+8(%r15),%rax; movslq ofs+8(%r15),%rcx;
  movl (%rax,%rcx,4),%edx; movl $NOCT_VALUE_INT,dst(%r15);
  movl %edx,dst+8(%r15)`.  Bytes are emitted through `IB()`/`ID()`
  inside `ASM { }` blocks; r15-relative accesses carry a REX 0x41/0x49
  prefix.
* **arm64 JIT conventions** (src/core/jit-arm64.c): x0 = env, x1 =
  `&env->frame->tmpvar[0]`; x2..x4 scratch; emission through macros
  (`LDR_IMM`, `STR_IMM`, `MOVZ`, `LSL_IMM`, `ADD`, ...) that assemble
  32-bit words.  `jit_visit_pload32_op` computes the address as
  `x2 = [x1+base+8]; x3 = sext32([x1+ofs+8]) << 2; x2 += x3` then
  loads and writes tag+value.
* **`OP_INC` is emitted inline** on both x86_64 (`incq 8(%rax)`) and
  arm64 (LDR/ADD_IMM/STR) — no helper call.  `OP_EQI`/`OP_JMPIFEQ`
  are likewise inline (the backends fuse the pair).  This matters for
  invariant 5.6 (no helper calls inside the strip region on
  register-mapping backends).
* **Scalar int shifts are LOGICAL with a range check**:
  `noct_ex_shr_helper` int/int does
  `dst = (int32_t)((uint32_t)src1 >> n)` after erroring unless
  `0 <= n < 32`; `noct_ex_shl_helper` is symmetric.  Scalar int
  mul/add/sub wrap (C int arithmetic on `.val.i`).
* **Interpreter plumbing**: `BINARY_OP(helper)` in
  src/core/interpreter.c consumes three imm8 operands and calls
  `helper(env, dst, src1, src2)`.  `rt_visit_pload32_op` is exactly
  `BINARY_OP(ex_pload32_helper)`.
* **Helper-call fallback precedent**: jit-x86.c (and the other 32-bit
  backends) implement `OP_PLOAD64` as `ASM_BINARY_OP(ex_pload64_helper)`
  — three int operands, env first.  This is the template for vector
  ops on all non-SIMD backends.
* **`lir_visit_for_range_block`** (src/core/lir.c): evaluates start
  and stop once into fresh tmpvars, emits an **empty-range pre-guard**
  (`OP_GTE guard,start,stop; OP_JMPIFTRUE guard,succ`), assigns the
  counter local, then loops with `OP_EQI cmp,counter,stop;
  OP_JMPIFEQ cmp,succ; <body>; OP_INC counter; OP_JMP head`.
  `OP_LINEINFO` is emitted **only at optimize level 0**.  Branch
  addresses are u32s patched via `patch_block_address()` /
  `lir_put_branch_addr()`.
* **Tmpvars are a LIFO stack** (`lir_increment_tmpvar` /
  `lir_decrement_tmpvar` assert LIFO order), capped by
  `LIR_TMPVAR_MAX` (128) shared with named locals and CSE homes.
* **cback** (src/backend/cback.c) translates LIR bytecode to C and
  already open-codes `OP_PLOAD32` etc. by writing
  `env->frame->tmpvar[...]` accesses into the generated source, which
  compiles against the runtime headers.
* **No CPUID/feature-detection machinery exists anywhere** in
  src/core/.  Part E adds the first one.
* Bytecode opcode space: last assigned is `OP_PSTORE64` = 0x3c
  (src/core/bytecode.h).  Vector opcodes start at 0x3d.  If another
  in-flight change claims 0x3d first, renumber — opcode VALUES in this
  document are illustrative; opcode NAMES and operand layouts are
  normative.
* `NOCT_ENABLE_OPTIMIZER` (CMake, default OFF) compiles
  hir_opt_abce.c/hir_opt_cse.c and defines `NOCT_USE_OPTIMIZER`
  (CMakeLists.txt).  hir_opt_simd.c joins that block.  The
  interpreter/execution.c vector helpers are **always** compiled
  (bytecode containing vector ops can be loaded from a precompiled
  file on targets that never run the optimizer, e.g. msdos — same
  reason the PLOAD helpers are unconditional).

## 3. Work plan and gates

Six parts, in order.  Each part ends with a verification gate; do not
start the next part until the gate passes.  Do NOT commit anything.

| Part | Contents | Gate |
|------|----------|------|
| A | Multi-packed ABCE | 4.4 |
| B | Vector opcodes, env vreg file, C helpers, interpreter, cback | 5.9 gate 1 |
| C | hir_opt_simd.c pass + LIR vector lowering (interpreter-only correctness) | 5.9 gate 2 |
| D | x86_64 SSE backend + CPUID gate | 7.6 |
| E | arm64 NEON backend | 7.6 |
| F | Helper-call cases in the 8 remaining JIT backends; full multiarch sweep; bench; docs | 8/9 |

## 4. Part A: multi-packed ABCE

Goal: `for (i in 0..n) { dst[i] = f(src[i], dst[i]); }` becomes
ABCE-eligible with both `src` and `dst` versioned in one loop.  This
part is useful on its own (remacs copy loops) and must land and pass
its gate before any SIMD code is written.

### 4.1 Data structure changes (hir_opt_abce.c only)

```c
#define ABCE_MAX_PACKED  4

struct abce_ctx {
    ...
    const char *packed[ABCE_MAX_PACKED];   /* was: const char *packed */
    int packed_bet[ABCE_MAX_PACKED];       /* per-packed element bet  */
    int packed_count;
    char base_name[ABCE_MAX_PACKED][32];   /* $abceN_base0 .. base3   */
    ...
};

struct abce_site {
    ...
    int packed_index;                      /* NEW: owner packed       */
};
```

### 4.2 Behavior changes

1. `abce_check_site`: instead of enforcing a single packed local, look
   the base symbol up in `ctx->packed[]`; append if absent (fail
   eligibility when `packed_count == ABCE_MAX_PACKED`).  Record
   `site.packed_index`.  Site dedup (structural equality) must now
   also compare `packed_index`.
2. `abce_check_eligibility`: the invariance check
   (`abce_is_assigned`) runs for **every** `ctx->packed[k]`.  The
   element-bet lookup runs per packed: each packed gets its own
   `packed_bet[k]` from the facts scan, defaulting to
   `NOCT_PACKED_UINT8`; **any** float fact still rejects the whole
   loop.  (Different int bets in one loop are allowed — e.g. a u8
   source and a u32 LUT — each site is rewritten with its owner's
   width ops.)
3. `abce_mk_guard`: emit `PCHECK(p_k, bet_k)` for every packed, and
   the four endpoint bounds checks per site against **the owner
   packed's** `PLEN(p_k)`.
4. `abce_version_loop`: allocate `$abceN_base0..` per packed
   (`hir_add_local` each), and B1 gains one `$baseK = PBASE(p_k)`
   statement per packed.  `abce_rewrite_expr`/`abce_rewrite_block`
   rewrite each site with its owner's base symbol and its owner's
   width-specific load/store kind (`abce_load_kind(bet_k)`).
5. `abce_expr_is_packed_subscr` must match any of the registered
   packeds (currently it matches the single `ctx->packed`).

### 4.3 What does NOT change

The guard structure, the `$g` evaluate-once rule, the safe-expression
rules (still int-only), block cloning, the LIR/opcode layer, all
backends.  This is a pass-internal generalization.

### 4.4 Gate A

* New tests `tests/abce/multi1.noct` (src->dst u8 copy loop, both
  packeds), `multi2.noct` (u8 source indexed into a u32 LUT — mixed
  bets), `multi3.noct` (5 packeds -> not versioned, output identical),
  `multi4.noct` (one of two packeds assigned in the body -> not
  versioned).  Golden scheme identical to run-abce.sh; extend
  run-abce.sh.
* Entire existing suites green: run-abce, run-cse, run-syntax,
  run-typing, run-scoping, run-class (level 0 and 2, interpreter and
  JIT).
* All three targets build (linux gcc, windows-mingw-x86_64, msdos).
* remacs acceptance gate green.

## 5. The SIMD transform (Parts B and C)

### 5.1 New opcodes (bytecode.h)

All operands are u16.  `vd/va/vb` are vreg indices 0..7; `t*` are
tmpvar indices; `imm` is a small immediate carried in a u16 operand.
Unused operands are written as 0 and must be consumed by every
decoder.  Append after `OP_PSTORE64`:

```
OP_VLOADI32X4,   /* 0x3d: vreg[a] = 16 bytes at (char*)tmp[b].val.l + sext(tmp[c].val.i)*4 */
OP_VSTOREI32X4,  /* 0x3e: 16 bytes at (char*)tmp[a].val.l + sext(tmp[b].val.i)*4 = vreg[c] */
OP_VSPLATI32,    /* 0x3f: vreg[a].i[0..3] = tmp[b].val.i          (c unused) */
OP_VGETLANEI32,  /* 0x40: tmp[a] = int32 vreg[b].i[c]; tag=INT    (c = lane 0..3) */
OP_VMOV128,      /* 0x41: vreg[a] = vreg[b]                       (c unused) */
OP_VADDI32X4,    /* 0x42: vreg[a].i[k] = vreg[b].i[k] + vreg[c].i[k]  (wrap) */
OP_VSUBI32X4,    /* 0x43: lane-wise subtract (wrap) */
OP_VMULI32X4,    /* 0x44: lane-wise multiply, low 32 bits (wrap) */
OP_VAND128,      /* 0x45: bitwise and (width-agnostic) */
OP_VOR128,       /* 0x46: bitwise or  */
OP_VXOR128,      /* 0x47: bitwise xor */
OP_VSHLI32X4,    /* 0x48: vreg[a].u[k] = vreg[b].u[k] << c        (c = const 1..31) */
OP_VSHRI32X4,    /* 0x49: vreg[a].u[k] = vreg[b].u[k] >> c        (c = const 1..31; LOGICAL) */
```

Notes:
* Loads/stores are **unaligned by contract** (payloads move under GC
  and offsets are arbitrary).  Never emit an alignment-checking
  instruction form.
* Shift-by-0 is **never emitted**: the LIR generator lowers a shift
  whose constant is 0 to `OP_VMOV128` (NEON's USHR cannot encode
  shift 0; keeping the rule arch-independent keeps backends
  identical).  Decoders may `assert(c >= 1 && c <= 31)`.
* There is deliberately no vector compare, select, div, mod, neg,
  not, or lane shuffle in Phase A.  Do not add "while you're in
  there" opcodes.

### 5.2 The vreg file (runtime.h)

Add to `struct rt_env` (after the existing members; do NOT reorder
existing members — the JITs address `env` fields by offset):

```c
/* SIMD scratch register file (design 06).  Raw lane bytes; never
 * holds references; never scanned by the GC; content is dead outside
 * a single vectorized strip region.  Byte order within a vreg is
 * memory order (lane k of an i32x4 is bytes 4k..4k+3). */
uint8_t vreg[16][16];
```

Only indices 0..7 are program-visible (D-SIMD6); 8..15 are reserved.
The register file is 16-byte aligned for PPC register synchronization; all C
access still goes through memcpy and makes no alignment assumption.

### 5.3 C helper semantics (execution.c — ALWAYS compiled)

One helper per opcode, `noct_ex_v*_helper(NoctEnv *env, int a, int b,
int c)`, wired exactly like the `noct_ex_pload32_helper` family
(declaration site, any `ex_*` alias macro, interpreter `BINARY_OP`
use — clone the PLOAD32 plumbing end-to-end and rename).  Reference
implementation pattern (C89):

```c
union vlanes { uint8_t b[16]; int32_t i[4]; uint32_t u[4]; float f[4]; };

bool noct_ex_vaddi32x4_helper(NoctEnv *env, int a, int b, int c)
{
    union vlanes x, y;
    int k;
    memcpy(&x, env->vreg[b], 16);
    memcpy(&y, env->vreg[c], 16);
    for (k = 0; k < 4; k++)
        x.u[k] = x.u[k] + y.u[k];      /* unsigned: wrap without UB */
    memcpy(env->vreg[a], &x, 16);
    return true;
}
```

Hard rules:
* **All int lane arithmetic is done in `uint32_t`** (add, sub, mul,
  shifts) and stored back — this wraps portably, has no UB, and
  matches both the scalar helpers' observed behavior and the vector
  instructions.  Do not write `int32_t * int32_t`.
* Loads: `memcpy(&x, (char *)(intptr_t)tmp[b].val.l + (int64_t)tmp[c].val.i * 4, 16)`
  — mirror the pointer/index arithmetic of `noct_ex_pload32_helper`
  exactly (sign-extended int index, scale 4).  No bounds or type
  checks: the ABCE guard already proved them, same trust model as
  scalar PLOAD.
* `VGETLANEI32` writes `tmp[a].type = NOCT_VALUE_INT` and the int
  lane value — a full, valid rt_value (GC invariant).
* Helpers never fail; always `return true`.
* Lane order = element memory order, so a big-endian port that uses
  these helpers is automatically self-consistent (lane k is element
  base+ofs+k on every byte order).  Do not "fix" endianness here.

Interpreter: add the 13 opcode cases to `rt_visit_op`'s dispatch,
each a `BINARY_OP(ex_v..._helper)` one-liner (three imm8 operands are
consumed uniformly, including the unused ones — this is why D-SIMD7
fixed the operand count).

cback: add the vector cases, open-coding the same lane loops into the
generated C (it can access `env->vreg` just as it accesses
`env->frame->tmpvar`).  elback/scmback translate pre-optimizer HIR
and never see any of this (existing invariant — verify, don't assume:
grep elback.c for PLOAD to confirm it has no ABCE cases either).

### 5.4 HIR shape of the transform (hir_opt_simd.c)

Input: the ABCE output region `G1: if ($g) { B1 -> FAST -> FEXIT }`.
ABCE marks FAST for us — add to `struct hir_block.val.for_` (hir.h):

```c
/* Optimizer-only (ABCE/SIMD); parser leaves these zero. */
bool abce_fast;      /* set by abce_version_loop on FAST   */
bool is_vector;      /* set by hir_opt_simd on the strip loop */
```

(`abce_version_loop` sets `FAST->val.for_.abce_fast = true`.  The
bet(s) and site info are re-derived by scanning the body — see 5.5 —
so no more fields are needed.)

The SIMD pass rewrites the region under G1 (B1's successor chain)
into:

```
B1:  $base0 = PBASE(p0); ... ;                    /* existing stmts   */
     $vN_mid  = $hi - (($hi - $lo) & 3);          /* NEW, appended    */
     $vN_sb0  = <adjusted base, per site: 5.6>; ...
     $vN_vg   = (0 <= $lo) && ($lo < $vN_mid)
                && <disjointness terms: 5.5>;     /* NEW              */
GV:  if ($vN_vg)  { VFOR (i in $lo .. $vN_mid) { <original FAST body> }   /* is_vector */
                    RFOR (i in $vN_mid .. $hi) { <clone 1> } }
GS:  if (!$vN_vg) { SFOR (i in $lo .. $hi)     { <clone 2> } }
```

* All the glue is ordinary scalar HIR (MINUS/AND/LTE/LT/LAND/INT
  terms) — **no new HIR expression kinds and no LIR-level synthetic
  branches are needed for the guards**.  Build expressions with the
  same `abce_mk_*` constructor style (export the tiny constructors
  through hir_opt.h or duplicate them; exporting is preferred, as
  Part C also needs the block-clone machinery — rename the ABCE clone
  trio to `hir_opt_clone_*` in hir_opt.h as a mechanical refactor,
  gate: run-abce.sh unchanged).
* The `$vN_*` locals are registered with `hir_add_local` on the
  function block.  Before creating them, count: if locals would
  exceed the LIR frame budget (mirror CSE's discipline against
  `LIR_TMPVAR_MAX`), skip vectorization for this loop.
* `is_vector` FOR keeps `is_ranged = true`, the same counter symbol,
  start `$lo`, stop `$vN_mid`.
* GV/GS mirror the G1/G2 pattern including the evaluate-once guard
  local `$vN_vg` (same rationale: the strip body may change temp
  locals' values; the guard must not be re-evaluated).
* If design 07 (typed ops) is in the tree, copy
  `typed_int_region` from the FAST loop to the VFOR, RFOR, and SFOR
  clones — all three run under G1 where `$g` held.  Never set it on
  anything under G2.  (07-typed-ops.md 3.2 is the authority.)
* Clone 1 and clone 2 are made with the shared clone machinery;
  parent/succ fix-ups follow `abce_version_loop`'s pattern exactly
  (reparent direct children, retarget break edges — but note 5.5
  E2 rejects bodies with control flow, so in practice the clones are
  single BASIC blocks and the fix-ups are trivial; keep the general
  code anyway for safety, it is already written).

**Why `0 <= $lo` (D-SIMD10).**  The counter is a 32-bit int.  Scalar
iterations index elements at `sext64(i)`; consecutive iterations wrap
`i` through INT_MAX -> INT_MIN, where `sext64` jumps by -2^32 and the
accessed addresses are NOT consecutive.  A 16-byte vector load at
lane base `i` touches `sext64(i)+0..3`, which matches the four scalar
iterations **only if** `i .. i+3` does not cross 2^31.  Requiring
`0 <= lo` and `lo < mid <= hi <= INT_MAX` confines the whole strip
range to `[0, 2^31)` where no wrap can occur.  (`mid` is computed in
wrapping int arithmetic; in any case where wrapping made `mid`
nonsensical, either `0 <= lo` or `lo < mid` fails and the loop takes
the untouched scalar path — which is always semantically correct.)
`(mid - lo)` is divisible by 4 by construction (two's-complement
identity, holds even under wrap), so the strip loop's `OP_EQI` exit
test hits `mid` exactly.

### 5.5 Eligibility (checked by hir_opt_simd on each abce_fast FOR)

Reject silently (leave the loop scalar-versioned) unless ALL hold:

* **E1 — bets.**  Re-derive each packed's element width from the body:
  every load/store site must be `HIR_EXPR_PLOAD32` / `HIR_EXPR_PSTORE32`
  (i.e. every packed in the loop had an int32/uint32 bet).  Any
  PLOAD8U/16/64 etc. in the body => reject.  (Identify sites
  syntactically: binary nodes of those types whose expr[0] is a
  SYMBOL term — that symbol names the owner base local `$abceN_baseK`,
  which identifies the packed.)
* **E2 — shape.**  The FAST body is exactly one `HIR_BLOCK_BASIC`
  block (its `stop` set or `succ` returning to the loop).  Any IF
  chain, any second block => reject.  (ABCE allows IFs; vector v1
  does not — no if-conversion.)
* **E3 — statements.**  Each stmt is `local = expr` or
  `PSTORE32(base, idx) = expr`.  Reject bare-expression stmts and any
  `$return` LHS.
* **E4 — expression grammar.**  Allowed nodes: `PAR`; `TERM` of an
  int constant, a local symbol, or the counter (counter ONLY in the
  index position of a site — E5); `PLUS`, `MINUS`, `MUL`, `AND`,
  `OR`, `XOR`; `SHL`/`SHR` whose right operand is an int constant in
  [0,31]; `PLOAD32` sites.  EVERYTHING else rejects — explicitly
  including DIV, MOD, NEG, NOT, LT/LTE/GT/GTE/EQ/NEQ, LAND, LOR,
  long/float/double/string terms, DOT, SUBSCR, CALL, CAPTURE,
  PBASE/PLEN/PCHECK/TYPEIS.
* **E5 — counter uses.**  The counter symbol appears only as (part
  of) the index operand `expr[1]` of a PLOAD32/PSTORE32 site, in the
  ABCE-normalized affine shapes (`i`, `i+u`, `u+i`, `i-u`).  A
  counter read anywhere else (e.g. `dst[i] = i`) => reject (would
  need an IOTA op; explicitly out of scope).
* **E6 — locals discipline.**  Classify each local mentioned in the
  body (excluding the counter and base symbols):
  - **INV**: read, never assigned in the body.  (These are ABCE
    TYPEIS-int guarded already.)  -> splat once in the preheader.
  - **TEMP**: assigned in the body.  Every TEMP must be assigned
    **before** any read, at statement granularity, in body order (a
    stmt's RHS reads happen before its LHS write; a TEMP read in
    stmt k requires an assignment in some stmt j < k).  A TEMP read
    before its first assignment is loop-carried => reject.
* **E7 — alias discipline** (D-SIMD9).  For every unordered pair of
  sites (A, B) on packeds (identified by base symbol) where at least
  one is a store:
  - same base symbol: their offset shapes must be syntactically
    identical (same shape enum, same `u` constant or same `u`
    symbol).  Different offsets on the same packed => reject.
    (Rationale: e.g. a store to `p[i]` with a load of `p[i-1]` is a
    loop-carried dependence; and two stores at distance < 4 change
    the last-writer under 4-wide grouping.)
  - different base symbols: record the pair for a runtime
    disjointness term (below).  If the pair's offsets are
    syntactically identical, the equal-bases escape is permitted.
  Disjointness term per recorded pair (P, Q), appended to `$vN_vg`
  with LAND, built from existing HIR kinds only (bases are the long
  `$baseK` locals; lengths via `PLEN` on the packed locals; the
  multiply uses a **LONG constant 4** so byte lengths cannot wrap in
  int arithmetic):
  ```
  ($baseP + 4L * PLEN(pP) <= $baseQ) || ($baseQ + 4L * PLEN(pQ) <= $baseP)
  [ || ($baseP == $baseQ)   -- emitted only when offsets matched statically ]
  ```
* **E8 — budget.**  Compute the vreg plan (5.6) and reject if
  `|INV| + |TEMP| + |CONST| + maxdepth > 8`, where maxdepth is the
  maximum vector-expression stack depth over all statements (for a
  binary node: `max(depth(l), depth(r)+1, 2)`; leaves: 1 — the
  standard LIFO-evaluation depth).  Also reject if new locals would
  break the frame budget (5.4) or the clone would exceed
  `ABCE_MAX_BLOCKS`.
* **E9 — shift constants.**  Every SHL/SHR right operand is an int
  constant in [0,31].  A constant outside that range means the scalar
  loop always errors at runtime; leave it scalar so it errors
  identically.

If NOCT_SIMD_DEBUG is set, print the D-SIMD14 line on success, and
(optionally, same flag) `SIMD: <func>: rejected (<E-rule>)` on the
first failing rule — invaluable for test triage; keep the strings
stable.

### 5.6 Vector LIR lowering (lir.c)

`lir_visit_block`'s FOR case dispatches first on
`block->val.for_.is_vector` to a new `lir_visit_vfor_block`.

**Site base pre-adjustment (done by hir_opt_simd, described here for
context).**  Vector loads take (base_tmpvar, index_tmpvar) and the
strip body must not compute `i+u` scalarly per iteration (a generic
`OP_ADD` is a helper call and would violate 5.6-NHC below).  So for
each distinct site offset the HIR pass precomputes an **adjusted
base** in B1: `$vN_sbS = $baseK + 4L * u` (long arithmetic; LONG
constant 4; for shape `i-u`, minus), and rewrites the site inside the
VFOR body only to `PLOAD32(SYMBOL($vN_sbS), SYMBOL(counter))` /
`PSTORE32` likewise.  After this rewrite every site's index operand
is the bare counter.  (Clones 1 and 2 keep the original
`$baseK`-relative form — they lower through the existing scalar
path.)

**The no-helper-call invariant (NHC).**  Between the first vector
opcode of the strip region (the first preheader splat) and the last
(the last lane extraction), the emitted bytecode contains no opcode
that any register-mapping backend implements as a call: allowed are
exactly `OP_ICONST`, `OP_ASSIGN`, `OP_EQI`, `OP_JMPIFEQ`, `OP_JMP`,
`OP_INC`, `OP_GTE`+`OP_JMPIFTRUE` (empty-range pre-guard), the
13 vector ops — and, once design 07 lands, the `OP_I*` typed int ops
(inline GPR-only sequences on both register-mapping backends; the
`OP_F*` float ops cannot appear here, the Phase A grammar is
int-only).  The generator must be structured so this holds by
construction; assert it in a debug build by scanning the emitted
range.  (This is what lets x86_64/arm64 keep vregs in hardware
registers with zero spills, and what makes the strip region
GC-atomic: no safepoint, no allocation, no error exit.)
`OP_GTE` — CHECK its JIT emitters: if `OP_GTE` is a helper call on
x86_64 or arm64, emit the empty-range pre-guard BEFORE the first
splat (reorder: pre-guard, then preheader splats, then loop) so the
NHC region starts after it.  This ordering is the safe default —
specify it as the required layout:

```
  <start/stop eval>                      ; scalar, helpers allowed
  GTE/JMPIFTRUE empty-range skip -> succ ; helpers allowed here
  ICONST+VSPLATI32 per CONST             ; NHC region begins
  VSPLATI32 per INV local
  ASSIGN counter, start
head:
  EQI cmp, counter, stop
  JMPIFEQ cmp -> exit_label              ; LOCAL forward label!
  <vector body stmts>
  INC counter  (x4)
  JMP head
exit_label:
  VGETLANEI32 local, vreg, 3   per TEMP  ; NHC region ends
  ; falls through to block->succ as usual
```

* **Local label mechanics**: `exit_label` is inside this emission, so
  `lir_put_branch_addr` (block-based) cannot express it.  Add a tiny
  private helper pair: remember `bytecode_top` position of the
  JMPIFEQ's u32 address operand, emit `0xffffffff`, and after
  emitting the back-jump, patch the real address using the same byte
  layout `patch_block_address()` uses (copy its write code verbatim).
* **The empty-range skip jumps to `block->succ`** (like the scalar
  for-range) — NOT to exit_label: if the strip loop never runs, the
  TEMP vregs are undefined and must not be extracted.  This cannot
  happen when `$vN_vg` held (`lo < mid` implies non-empty), so the
  pre-guard is dead code in practice; keeping it mirrors the scalar
  lowering and costs two instructions once.
* **Increment**: emit `OP_INC` four times (verified inline on both
  register-mapping backends).  Do not add a new scalar opcode for +4.
* **Vreg allocation plan** (must match E8's count exactly; implement
  once as a small pure function and call it from both hir_opt_simd.c
  and lir.c, declared in hir_opt.h — if that layering is awkward
  because lir.c must build without NOCT_USE_OPTIMIZER, duplicate the
  ~60 lines and keep both copies textually adjacent to this spec):
  - vregs `[0 .. nconst)`: one per distinct int constant used as a
    vector operand (dedup by value; shift counts and site offsets are
    NOT vector constants), in order of first appearance.
  - vregs `[nconst .. nconst+ninv)`: one per INV local, in order of
    first appearance.
  - vregs `[.. +ntemp)`: one per TEMP local, in order of first
    assignment.
  - vregs above that: LIFO expression stack.
* **Preheader**: for each CONST vreg: `OP_ICONST scratch_tmpvar` then
  `OP_VSPLATI32 vreg, scratch_tmpvar, 0` (one scratch tmpvar,
  LIFO-released after).  For each INV local: `OP_VSPLATI32 vreg,
  local_tmpvar, 0`.
* **Statement lowering** inside the body:
  - `local(TEMP) = expr` -> `lir_visit_vexpr(temp_vreg, expr)`.
  - `PSTORE32(sb, i) = expr` -> `lir_visit_vexpr(stack_vreg, expr)`
    then `OP_VSTOREI32X4 sb_tmpvar, counter_tmpvar, stack_vreg`.
* **`lir_visit_vexpr(dst_vreg, expr)`** mirrors `lir_visit_expr`'s
  destination-directed style:
  - `PAR` -> recurse.
  - `TERM` int constant / INV local / TEMP local -> `OP_VMOV128
    dst, mapped_vreg, 0` (or, as a peephole, let binary ops read
    mapped vregs directly and only VMOV at the top level when the
    whole RHS is a bare term).
  - `PLOAD32(sb, i)` -> `OP_VLOADI32X4 dst, sb_tmpvar,
    counter_tmpvar`.
  - Binary `PLUS/MINUS/MUL/AND/OR/XOR` -> evaluate lhs into a pushed
    stack vreg s1, rhs into pushed s2, emit `OP_V* dst, s1, s2`, pop
    both.  (With this discipline `dst != s2` always holds, which the
    x86 two-address lowering relies on — assert it.)
  - `SHL/SHR` -> evaluate lhs into s1; constant c: c == 0 ->
    `OP_VMOV128 dst, s1, 0`; else `OP_VSHLI32X4/OP_VSHRI32X4 dst,
    s1, c`.
* Stack depth is bounded by the E8 plan; `lir_fatal` on overflow
  (must be unreachable — the HIR pass rejected over-budget loops).
* LINEINFO: none of this emits OP_LINEINFO (we are at level >= 2 by
  construction; the scalar for-range already gates LINEINFO to
  level 0 — keep the vector path free of it unconditionally).  No
  OP_SAFEPOINT anywhere in the region (matches the ABCE fast loop).

### 5.7 TEMP extraction (why lane 3, why it is always right)

A TEMP local is observable after the loop (e.g. read after the blend
loop).  The scalar semantics: after the loop, the local holds the
value from the last executed iteration.
* If the remainder loop (RFOR) runs >= 1 iteration, it recomputes all
  TEMPs scalarly — whatever the strip left there is overwritten.
* If the remainder is empty (`(hi-lo) % 4 == 0`), the last executed
  iteration is `mid-1` = lane 3 of the final strip group — exactly
  what `OP_VGETLANEI32 local, vreg, 3` extracts at the strip exit.
* The strip runs >= 1 group whenever it is entered (`lo < mid`), so
  the extracted vregs are always defined.
* If the strip is not entered at all (`$vN_vg` false), the untouched
  scalar loop (SFOR) runs and the extraction code is never reached.

Extraction is emitted unconditionally at exit_label for every TEMP —
no liveness analysis.  A dead extraction costs a handful of cycles
once per loop execution and keeps the rule analysis-free.

### 5.8 CSE interaction (hir_opt_cse.c)

In the CSE block walker's `HIR_BLOCK_FOR` descent, when
`b->val.for_.is_vector` is set: do NOT walk the body (no analysis, no
rewrites inside), but still apply the standard conservative loop
treatment on the outside (memory-epoch bump, kill/re-key of locals
assigned inside — the def-scan may walk the body read-only).  Clones
1 and 2 are ordinary loops; CSE handles them normally.  Add a comment
at the skip citing this document.

### 5.9 Gates for Parts B and C

**Gate B** (opcodes + helpers are dead code): all three targets
build; the full existing suite (run-abce, run-cse, run-syntax,
run-typing, run-scoping, run-class, run-thread) is green at levels 0
and 2, interpreter and JIT; msdos build compiles the new execution.c
helpers (C89: declarations at block top, `/* */` comments, no
compound literals).

**Gate C** (the transform, interpreter-only): build with
NOCT_ENABLE_OPTIMIZER=ON and **JIT disabled**; the new
tests/run-simd.sh suite (section 8) passes with golden outputs
identical across level 0/2; NOCT_SIMD_DEBUG confirms the must-vectorize
cases fired; run-abce/run-cse remain green (the transform must not
perturb ABCE-only or CSE-only behavior); remacs gate green.

## 6. Semantics equivalence table (normative)

| Op | Lanes | Scalar definition it must match | x86 (SSE) | arm64 (NEON) |
|----|-------|--------------------------------|-----------|--------------|
| VADDI32X4 | i32x4 | int + int, wrap | `paddd` | `add Vd.4s` |
| VSUBI32X4 | i32x4 | int - int, wrap | `psubd` | `sub Vd.4s` |
| VMULI32X4 | i32x4 | int * int, low 32, wrap | `pmulld` (SSE4.1) | `mul Vd.4s` |
| VAND128/VOR128/VXOR128 | 128b | int & \| ^ | `pand/por/pxor` | `and/orr/eor Vd.16b` |
| VSHLI32X4 | i32x4 | `(uint32)x << c`, c in 1..31 | `pslld imm` | `shl Vd.4s, #c` |
| VSHRI32X4 | i32x4 | `(uint32)x >> c`, c in 1..31 — **LOGICAL** | `psrld imm` (NOT psrad) | `ushr Vd.4s, #c` (NOT sshr) |
| VLOADI32X4 | 16B | 4 x PLOAD32 of consecutive elements | `movdqu` | `ldr q` |
| VSTOREI32X4 | 16B | 4 x PSTORE32 | `movdqu` store form | `str q` |
| VSPLATI32 | i32x4 | broadcast `.val.i` | `movd` + `pshufd 0` | `dup Vd.4s, Wn` |
| VGETLANEI32 | 1 lane | int-tagged rt_value | `pextrd` (SSE4.1) | `umov Wd, Vn.s[k]` |
| VMOV128 | 128B | copy | `movdqa` | `mov` (orr Vd,Vn,Vn) |

### 6.1 The two classic traps (both verified against this codebase)

1. **Shifts are logical.**  The scalar int helpers shift the
   **unsigned** bit pattern (`noct_ex_shr_helper`:
   `(int32_t)((uint32_t)v >> n)`).  `(pix >> 24)` on an
   0xFF-alpha pixel yields 255, not -1.  Emitting `psrad`/`sshr`
   would be a silent miscompile that most test images won't catch —
   the test suite includes a case with bit 31 set specifically for
   this (8.x).
2. **Narrowing/saturation does not exist here.**  Phase A has no
   narrow ops at all, so the packuswb-vs-xtn saturation mismatch
   cannot arise.  Do not introduce narrow ops in a refactor.

## 7. JIT backends (Parts D, E, F)

### 7.1 General structure (all 10 backends)

Each backend handles the complete `OP_V*` range.  The portable C helpers are
the interpreter/reference semantics, while generated code selects one of two
forms:

1. a runtime-capability-gated native SIMD sequence; or
2. direct scalar host instructions over the canonical `env->vreg` file.

x86/x86_64 have scalar, SSE2, and SSE4.1 tiers.  ARMv7 has scalar and NEON;
ARM64 has scalar and ASIMD.  PPC32/64 have scalar and AltiVec.  MIPS32/64 and
RISC-V32/64 currently use direct scalar lowering only.  No opcode may be left
unhandled on any backend (Global invariant 6).  See
[08-jit-simd-portability.md](08-jit-simd-portability.md) for the authoritative
capability, ABI, and fallback design.

### 7.2 x86_64 (Part D)

The implemented x86 capability probe checks CPUID availability before
executing CPUID, validates the maximum leaf, then reads leaf 1 EDX.26 (SSE2)
and ECX.19 (SSE4.1).  Capabilities are stored per JIT build, avoiding an
unsynchronised process-global cache.  Unknown compilers/OS combinations
conservatively select scalar.  `NOCT_JIT_SIMD_MAX` can lower, but never raise,
the detected tier.

SSE4.1 uses `pmulld` and `pextrd`.  SSE2 supplies equivalent multiply and
lane-extract sequences, so SSE3-only CPUs use the SSE2 tier.  CPUs without
SSE2 use direct scalar generated code.

Vreg mapping: vreg k -> xmm k, k in 0..7.  Scratch GPRs: rax
(pointer), rcx (index), rdx (lane value), exactly like the scalar
PLOAD32 emitter.  Two-address lowering rule for `OP_V* vd, va, vb`
ALU ops: if `vd != va`, emit `movdqa xmm_vd, xmm_va` first, then
`op xmm_vd, xmm_vb`.  (`vd == vb` with `vd != va` cannot occur —
guaranteed by the LIFO stack discipline in 5.6; add an assert in the
emitter.)

Reference encodings (operands in Intel order, ModRM `/r` between two
xmm registers 0..7 is `0xC0 | (dst << 3) | src`; memory operand
`(rax,rcx,4)` is ModRM `0x04` + SIB `0x88`; r15-relative disp32 is
ModRM `0x87` + disp32 with a REX 0x41 prefix, exactly as in the
existing emitters):

```
movdqu xmm, m128     F3 0F 6F /r          movdqu m128, xmm   F3 0F 7F /r
movdqa xmm, xmm      66 0F 6F /r
paddd                66 0F FE /r          psubd              66 0F FA /r
pmulld (SSE4.1)      66 0F 38 40 /r
pand                 66 0F DB /r          por  66 0F EB /r   pxor  66 0F EF /r
pslld xmm, imm8      66 0F 72 /6 ib   (ModRM F0|xmm)
psrld xmm, imm8      66 0F 72 /2 ib   (ModRM D0|xmm)
movd xmm, m32        66 41 0F 6E 87 <disp32>      (from tmpvar slot)
pshufd xmm,xmm,0     66 0F 70 /r 00
pextrd m32, xmm, ib  66 41 0F 3A 16 87 <disp32> ib  (to tmpvar slot)
```

Emission sketches (mirror the scalar PLOAD32 style; `base`/`ofs`/`dst`
are tmpvar byte offsets, `vd` etc. vreg numbers):

```
VLOADI32X4 vd, base, ofs:
    movq  base+8(%r15), %rax      49 8B 87 <disp32>
    movslq ofs+8(%r15), %rcx      49 63 8F <disp32>
    movdqu (%rax,%rcx,4), %xmm_vd F3 0F 6F ModRM(04|vd<<3) SIB(88)
VSTOREI32X4 base, ofs, vs:  same address setup, then
    movdqu %xmm_vs, (%rax,%rcx,4) F3 0F 7F ModRM(04|vs<<3) SIB(88)
VSPLATI32 vd, src:
    movd  src+8(%r15), %xmm_vd    66 41 0F 6E ModRM(87|vd<<3)... <disp32>
    pshufd $0, %xmm_vd, %xmm_vd   66 0F 70 C0|vd<<3|vd 00
VGETLANEI32 dst, vs, lane:
    pextrd $lane, %xmm_vs, dst+8(%r15)   66 41 0F 3A 16 87... <disp32> lane
    movl  $NOCT_VALUE_INT, dst(%r15)     41 C7 87 <disp32> <imm32>
```

(The exact ModRM/disp byte layout MUST be cross-checked: after
implementing each op, run the disassembly smoke test in 7.5 before
moving to the next op.  Do not implement all 13 and debug later.)

Win64 note (D-SIMD6): the implemented Win64 backend selects the direct scalar
tier and uses only volatile xmm0, so generated code never owns xmm6/xmm7 and
the prologue needs no vector save area.  SysV maps program vregs to volatile
xmm0..7.  If Win64 gains a register-mapped tier later, it must save/restore
xmm6/xmm7 on both normal and exception exits before that tier can be enabled.

### 7.3 arm64 (Part E)

Vreg mapping: vreg k -> v k, k in 0..7 (caller-saved under AAPCS64;
no prologue change).  Scratch GPRs x2/x3 as in the scalar emitters.
ASIMD is an AArch64 ABI baseline.  The shared capability/override path still
allows the direct scalar tier to be forced for validation.

Add emit macros in the style of the existing ones.  Mnemonics and
semantics (encodings must be derived/verified with the 7.5 harness,
not trusted from memory — this document deliberately gives fields,
not opcode words, for arm64):

```
LDR  Qd, [X2]                ; 16B load,  Q-form LDR (unsigned imm 0)
STR  Qs, [X2]
ADD  Vd.4S, Vn.4S, Vm.4S     ; SUB, MUL same shape
AND  Vd.16B, Vn.16B, Vm.16B  ; ORR, EOR
SHL  Vd.4S, Vn.4S, #c        ; c in 1..31
USHR Vd.4S, Vn.4S, #c        ; c in 1..31 (encoding cannot express 0
                             ;  — the LIR layer guarantees c >= 1)
DUP  Vd.4S, Wn               ; splat from GPR
UMOV Wd, Vn.S[lane]          ; lane extract to GPR
ORR  Vd.16B, Vn.16B, Vn.16B  ; register MOV
```

Address setup for LDR/STR Q reuses the existing scalar pattern
verbatim: `LDR_IMM(x2, x1, base+8); LDR_W_IMM(x3, x1, ofs+8);
LSL_IMM(x3, x3, 2); ADD(x2, x2, x3)` then the Q-form access at
`[x2]`.  DUP's source Wn comes from `LDR_W_IMM(x3, x1, src+8)`.
UMOV's result goes through the existing tag+value store pattern
(MOVZ tag; STR both), writing a full valid rt_value.

### 7.4 What must NOT be emitted

* No `psrad`/`sshr` (6.1).
* No aligned load forms (`movdqa` with a memory operand, `ldr` with
  alignment-checking addressing).  `movdqa` is register-register
  only.
* No VEX/AVX encodings — mixing VEX and legacy SSE has penalty and
  complexity; everything stays legacy-SSE.  No FPCR/MXCSR writes.
* Nothing inside the strip region may call a helper on these two
  backends (NHC, 5.6) — if an op seems to need one, the design is
  being violated; stop and re-read 5.6.

### 7.5 Encoding verification harness (mandatory during D and E)

For each newly implemented op, before proceeding to the next:
1. Write the intended instruction(s) in a `.s` file and assemble with
   the system/cross assembler; `objdump -d` it.
2. Add a temporary debug hook (or reuse an existing JIT dump flag if
   one exists — grep for how the JIT buffer can be dumped) to hexdump
   the emitted bytes for a one-op test script; diff against step 1.
3. Run that op's golden test (8) under the target (qemu-user for
   arm64: same flow run-abce.sh's multiarch validation used).
Delete the temporary hook before finishing; the golden suite is the
permanent guard.

### 7.6 Gates D and E

Gate D: run-simd.sh fully green on x86_64 with JIT at levels 0/2,
plus interpreter; run-abce/run-cse/run-syntax green with JIT;
NOCT_SIMD_DEBUG-asserted vectorization; the SSE4.1=absent path
exercised once by forcing `simd_sse41 = 0` under a temporary env
override (add `NOCT_SIMD_NOSSE41=1` honored only in this check — keep
it, it costs three lines and gives the fallback path permanent
coverage).  Gate E: the same suite under qemu-user aarch64 (and on
real arm64 hardware if available).  Both: all three targets still
build; remacs gate green.

## 8. Tests (tests/simd/, run-simd.sh)

Mirror run-abce.sh exactly: each case runs at optimize level 0 and 2,
interpreter and JIT, and all four outputs must be byte-identical.
Cases marked (V) must additionally produce the NOCT_SIMD_DEBUG
"vectorized" line at level 2; cases marked (R) must NOT.

1. (V) `blend.noct` — the flagship RGBA32 alpha blend (section 0
   shape), n=1000+3 pixels, prints a checksum and 8 specific pixels
   including ones with alpha 0, 255, and bit-31-set colors (0xFFxxxxxx
   — catches an arithmetic-shift miscompile, 6.1).
2. (V) `remainder.noct` — one loop body run for every
   n in {0,1,2,3,4,5,7,8,9,16,17}; prints all outputs.  Covers
   strip-skipped (n<4), remainder 0..3, and the n=0 empty loop.
3. (V) `tempafter.noct` — a TEMP local read after the loop, for n%4
   == 0 (lane-3 extraction path), n%4 != 0 (remainder overwrite
   path), n < 4 (strip skipped), n == 0 (loop never runs; the local
   keeps its pre-loop value).
4. (V) `inplace.noct` — `p[i] = f(p[i])` (same offsets, single
   packed): must vectorize; result equals scalar.
5. (R) `overlap_reject.noct` — `p[i]` load with `p[i+1]` store (same
   packed, different offsets): must NOT vectorize; output identical.
6. `overlap_dynamic.noct` — blend called twice: once with distinct
   src/dst (guard passes), once with src == dst aliased through two
   locals with different offsets (disjointness guard fails at
   runtime, scalar path runs).  Outputs identical across levels.
7. (R) `neg_lo.noct` — counter range starting below 0 with a
   positive site offset keeping accesses in bounds: ABCE versions it,
   SIMD's `0 <= lo` strip guard routes it scalar at runtime.  Output
   identical.  (This is a runtime-guard case, not an eligibility
   reject — it still prints the "vectorized" line; assert instead
   that its output matches.)
8. (R) `u8_reject.noct` — same loop over Packed.uint8: not
   vectorized (bet not int32).
9. (R) `counter_value.noct` — `dst[i] = i` (E5 reject).
10. (R) `carried.noct` — `s = s + p[i]` reduction (E6 reject:
    read-before-write TEMP).
11. (V) `shift_edges.noct` — shifts by 0 (VMOV lowering), 1, 8, 24,
    31; constants dedup (two uses of 255 must share one vreg — assert
    via the debug line's consts count).
12. (R) `if_reject.noct` — an IF inside the body (E2).
13. (V) `mixed_bases.noct` — three packeds: two loaded, one stored,
    distinct offsets, exercising per-pair disjointness terms.
14. `budget.noct` — a body needing > 8 vregs: must compile and run
    identically (silently scalar), plus a function near the 128
    tmpvar frame limit containing a vectorizable loop: must compile
    at both levels (vectorization skipped if the $vN_* locals don't
    fit — either outcome is legal, outputs must match).
15. Level-2 pass over the full existing tests/syntax suite (the
    run-cse.sh precedent): no output may change.

Benchmark: `tests/bench/b8_blend.noct` (alpha blend, ~2M pixels,
timed loop) following the existing bench harness.  Record
interpreter/JIT, level 0/2 numbers in this document's status header
when Part F lands, like 05-cse.md does.

## 9. Part F closeout checklist

* Complete vector-op dispatch and direct scalar fallback added to every JIT
  backend; multiarch qemu sweep of run-simd.sh plus forced lower tiers and
  long-branch tests on all 10 arches (the 01-abce.md validation flow).
* cback: run-ctrans.sh extended with blend.noct (compile at level 2,
  run the generated C, diff).
* windows-mingw and msdos builds.
* remacs full acceptance gate.
* Update docs/design/00-overview.md's table (a row for 06) and add a
  one-paragraph note to docs/syntax.md ONLY if the owner asks —
  language surface is unchanged, so default is no doc change.
* Leave everything uncommitted.

## 10. Phase B implementation (f32x4)

The portable reference path and auto-vectorizer are implemented.  Every JIT
can execute the new vector bytecode through direct scalar generated code, so
unsupported hosts retain correct generated-machine-code execution.  Native
x86, NEON, and AltiVec lowering is layered on this reference path and
capability model.

* **Scalar prerequisite**: ABCE support for `NOCT_PACKED_FLOAT32`:
  new scalar ops `OP_PLOADF32`/`OP_PSTOREF32` (float-tagged rt_value;
  interpreter + all 10 JITs + cback), float bet acceptance, guard uses
  `TYPEIS(v, float)` for float locals, and the safe-expression rules
  split into an all-int or all-float discipline per loop (mixed
  int/float bodies stay unversioned; revisit later).
* **Vector ops**: `OP_VLOADF32X4/VSTOREF32X4`,
  `OP_VADDF32X4/VSUBF32X4/VMULF32X4/VDIVF32X4`
  (`addps/subps/mulps/divps` — all SSE2, no new CPUID bit;
  `fadd/fsub/fmul/fdiv Vd.4s`), `OP_VSPLATF32`, `OP_VGETLANEF32`.
  Loads/stores have distinct portable opcodes even though their 16-byte
  memory representation is identical to i32x4.  `VDIVF32X4` **requires
  07-typed-ops Part 0** (float
  division by zero -> IEEE inf/NaN, owner decision D-TOP12): under
  the old error semantics a zero in any lane would have made vector
  division diverge from the scalar loop (error vs. value), killing
  the op entirely.  With Part 0 shipped, lane-wise `divps`/`fdiv`
  matches the scalar helper bit-exactly, zeros included.
* **Bit-exactness argument**: scalar float arithmetic is C `float`
  ops (verified: `noct_ex_add_helper` float/float case) = IEEE-754
  binary32; SSE and NEON vector single-precision ops are IEEE-exact
  including division; default MXCSR/FPCR (no FTZ/DAZ changes —
  7.4's "no control-register writes" rule guarantees this) make
  lane k bit-identical to the scalar op, denormals included.
  Float compares/min/max/int<->float conversions are excluded until
  specified (conversion must use the truncating forms
  `cvttps2dq`/`fcvtzs` if ever added).
* **Register-clash note**: design 07's scalar float typed ops use
  xmm0/xmm1 (s0/s1) as scratch, which are vregs 0/1 under this
  design's mapping.  Phase A cannot collide (int-only strip
  grammar), but Phase B must either exclude typed float ops from
  strip regions (NHC list) or shift the vreg mapping — decide when
  Phase B is designed.
* Eligibility: E4 grammar becomes PLUS/MINUS/MUL/DIV on float lanes
  (note DIV joins in Phase B: IEEE division vectorizes exactly,
  unlike int division); shifts/bitwise reject in float loops; float
  constants allowed as splats (HIR_TERM_FLOAT — note this requires
  relaxing the ABCE float-constant rejection inside float-mode loops
  only).
* Regression gate: `tests/simd/f32.noct` must vectorize and must agree at
  optimization levels 0/2 under the interpreter and forced JIT.  The same
  case is translated and compiled by `tests/run-ctrans.sh`; all ten JIT
  target binaries are exercised with QEMU/native execution.

## 11. Failure-mode appendix (read this when something is weird)

* **Output differs only at level 2 + JIT on x86_64/arm64** -> an
  encoding bug; bisect with NOCT_SIMD_DISABLE=1, then per-op via the
  7.5 harness.  Check shift family first (6.1).
* **Output differs at level 2 everywhere (incl. interpreter)** -> the
  transform itself; check E6/E7 classification against the failing
  body, then the strip-guard arithmetic (5.4) with the actual lo/hi.
* **Crash inside a strip loop under GC stress** -> an NHC violation
  (a helper/safepoint sneaked into the region and moved the packed);
  scan the emitted range (5.6 debug assert).
* **Wrong TEMP value after the loop, only when n%4==0** -> lane
  extraction (5.7): wrong lane index or extraction emitted before the
  exit label.
* **Vectorization silently stopped firing** -> run-simd.sh's debug
  assertions catch this; check pass ordering in `hir_optimize_func`
  and the abce_fast flag.
* **Win64-only corruption after enabling a future native tier** -> xmm6/7
  clobber: native lowering was enabled without the required ABI save/restore.
