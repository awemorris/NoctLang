# Draw-image SIMD admission implementation plan

Status: implemented and regression-tested on 2026-08-12.

Implementation note: compare/select and the Arm64 masked store have native
lowerings. Strict FP induction and checked gather initially select the
memory-canonical direct-scalar vector tier for the whole region on x86_64 and
Arm64. This is intentional: it establishes admission, exact semantics, and
portable bytecode without mixing native-register and memory-resident vector
state. Target-native induction/gather contraction remains a performance-only
follow-up and is not required for semantic correctness.

The implemented regression set includes every `blend-*.noct` probe plus
checked-gather valid/negative/equal-length cases, a masked-store false-lane
canary, and positive/zero/negative strict-FP induction with vector-only and
vector-plus-remainder live-out checks.

Target repository: `/home/awe/noct-simd`

Baseline source: `/home/awe/drawimage.h`, SHA-256
`75dbc7ed68a92309116b8ea4335bd3ab9cbbd2586f7425fd4791bbbf1df43ee3`
(inspected 2026-08-12).

Related probes:
`tests/testcases/simd/drawimage/blend-*.noct`.

## 1. Objective

Make the current draw-image essence loops pass the existing HIR SIMD
optimization safely.  The first objective is admission and semantic
correctness, not matching GCC/Clang instruction counts.

The milestones must be implemented in order.  Each milestone must leave the
tree buildable and the full SIMD suite passing before the next milestone is
started.

Expected admission order:

1. `blend-dim.noct`, `blend-cross.noct`
2. `blend-add.noct`, `blend-sub.noct`
3. `blend-glyph.noct`, `blend-melt.noct`
4. `blend-rule.noct`
5. `blend-3d-alpha.noct`, `blend-3d-cross.noct`

`blend-copy.noct` is already vectorized and is a permanent regression test.

## 2. Non-goals for this work

Do not implement any of the following merely to reduce instruction count:

- AVX2 x8 or any vector width other than the current 128-bit x4 width.
- AVX2 native gather, AVX-512 masked stores, SVE, or SVE2.
- `pminud`, `pmaxsd`, NEON `umin`/`smax` canonicalization.
- `pblendvb` selection or other target-specific select contraction.
- A general-purpose whole-function if-conversion pass.
- Fast-math reassociation at `-O2`.
- Rewriting a strict floating recurrence as `start + i * step`.
- Matching GCC/Clang loop instruction counts.
- Performance tuning before the loop is admitted and produces correct output.

The first native lowering may use several SSE2/NEON instructions, scalar lane
loads, or scalar lane stores.  Shorter lowering is separate future work.

## 3. Baseline and measured simulation

Current admission results at both `-O2` and `-O3`:

| Probe | Current result | First missing facility |
|---|---|---|
| copy | vectorized x4 | none |
| dim | E8 vreg budget | synchronized logical-vreg planning |
| cross | E8 vreg budget | synchronized logical-vreg planning |
| add | E2 body shape | predication |
| sub | E2 body shape | predication |
| glyph | E2 body shape | predication |
| melt | E2 body shape | predication |
| rule | E2 body shape | predicated store |
| 3d-alpha | no SIMD candidate | FP induction and gather |
| 3d-cross | no SIMD candidate | indirect gather |

The E8 cases were simulated without changing source code.  The HIR rejection
was skipped in GDB for that process only, and the unchanged LIR/JIT pipeline
was allowed to continue.

### DIM

- HIR estimate: 3 constants + 1 invariant + depth 5 = 9, over the fixed limit 8.
- Existing x86_64 LIR plan: 12 logical vregs.
- Existing x86_64 JIT completed and produced the same output as `-O0`:
  `-6852792 -16777216`.

### CROSS

- HIR estimate: 3 constants + 1 invariant + depth 6 = 10, over the fixed limit 8.
- Existing x86_64 LIR plan: 13 logical vregs.
- Existing x86_64 JIT completed and produced the same output as `-O0`:
  `-9863592 -16777216`.

Therefore DIM and CROSS do not require new arithmetic opcodes.  They are
stopped by the HIR-side fixed budget before the more capable LIR planner runs.

## 4. Semantic invariants

Every milestone must preserve these contracts.

1. `-O2` is strict.  Do not reassociate FP operations and do not introduce
   fused arithmetic beyond the already defined `-O3` FMA behavior.
2. SIMD bytecode remains executable when the runtime CPU has no matching SIMD.
   The interpreter/direct-scalar vector tier is part of the bytecode contract.
3. A compiled `.nb`/`.nap` must not depend on the optimizer being linked into
   the loading runtime.
4. Existing opcode values must not be renumbered.  New opcodes are appended
   after `OP_VFMAF32X4`.
5. All new bytecode operands are bounds-checked and validated exactly like the
   existing SIMD operands.  An imm8 vreg is valid only in 0..15.
6. Old bytecode, whose `OP_VINDEX_HINT` id operand is zero, remains valid.
7. Scalar remainder and scalar fallback loops retain source semantics.
8. If-conversion accepts only pure expressions.  Calls, I/O, global mutation,
   nested loops, `return`, and arbitrary stores reject the candidate.
9. Float comparisons use ordered Noct semantics: a comparison involving NaN
   is false unless the source comparison itself specifies otherwise.
10. Packed out-of-range access still reports a runtime error.  New gather
    lowering must never perform an unchecked invalid native load.
11. No source-level exception exists.  The checked-gather design below assumes
    `rt_error()` terminates the current execution path; it reports the first
    invalid lane in lane order.
12. `restrict` is an alias promise, not an out-of-bounds promise and not an
    arbitrary-index validity promise.

## 5. Current architecture problem to fix first

The logical SIMD register limits currently disagree:

- `hir_opt_simd.c`: `SIMD_VREG_MAX == 8`
- `lir.c`, x86_64: `VFOR_VREG_MAX == 13`
- `lir.c`, Arm64: `VFOR_VREG_MAX == 16`
- `lir.c`, other targets: usually 8
- bytecode/runtime storage: vreg indices 0..15

The comment in `hir_opt_simd.c` says its calculation mirrors LIR, but it does
not mirror the current cache-aware plan or architecture limits.  Do not fix
this by changing only `SIMD_VREG_MAX`.

The required end state is:

- HIR and LIR plan against 16 *logical* vregs.
- Each JIT backend declares its native physical mapping limit.
- A vector region that exceeds the native limit is sent wholly to the existing
  direct-scalar vector lowering.  Native and memory-canonical vector state must
  never be mixed inside one region.
- x86_64 remains native through logical vreg 12 (13 registers total).
- Arm64 remains native through logical vreg 15 (16 registers total).
- i386 and smaller mappings fall back when a region needs more than 8.

This is necessary for portable optimized bytecode.  Architecture-dependent
LIR rejection would make the same source produce incompatible vector bytecode.

## 6. Milestone 1: shared logical-vreg planning

### 6.1 Extract one planner

Move the cache collection, expression equality/size calculation, scratch-need
calculation, home allocation, and fit calculation out of their duplicated HIR
estimate/LIR implementation into a shared internal planner.

Recommended new internal files:

- `src/core/simd_plan.h`
- `src/core/simd_plan.c`

The planner is not public API.  It operates on a normalized VFOR HIR basic
body.  A suitable result structure contains at least:

```c
struct simd_plan {
    int const_count;
    int inv_count;
    int temp_count;
    int cache_count;
    int stack_base;
    int required_vregs;       /* 1..16 */
    bool is_float;
    /* existing const/invariant/temp/cache mappings needed by lir.c */
};
```

Required API behavior:

```c
bool simd_plan_build(struct hir_block *vfor_body,
                     struct hir_block *function,
                     int logical_limit,
                     struct simd_plan *out,
                     const char **reject_reason);
```

The exact signature may follow local conventions, but both HIR admission and
LIR emission must call the same implementation.  Do not maintain a third
approximate formula.

Set the logical limit to 16.  A need of 17 or more remains an E8 rejection.
HIR should report the exact planner result in `NOCT_SIMD_DEBUG`:

```text
logical-vregs=12 homes=4 caches=4 stack=8
```

The current `NOCT_LIR_VFOR_DEBUG` output may remain, but its count must agree.

Update build manifests which enumerate core sources.  Search all CMake and
Makefile source lists instead of assuming only the top-level CMake file.

### 6.2 Record the required count in VINDEX_HINT

`OP_VINDEX_HINT` currently encodes:

```text
index(u16), stop(u16), remaining(u16), id(imm8), lanes(imm8), flags(imm8)
```

The `id` operand is currently emitted as zero and ignored by every consumer.
Rename its documented meaning to `required_vregs` without changing bytecode
length or opcode value.

- New bytecode emits 1..16.
- Zero means legacy bytecode; treat it as requiring at most 8.
- `lir_dump` prints `vregs:N`, not `id:N`.
- Interpreter and C backend consume and otherwise ignore the value.

The hint must appear before the first VSPLAT or any other vector opcode in the
region.  Reorder the VFOR preheader so that counter/remaining setup and the
hint precede vector constant/invariant initialization.

### 6.3 JIT region-tier selection

Add a context field such as `vector_force_scalar`.  At `OP_VINDEX_HINT`:

```text
required = encoded_required != 0 ? encoded_required : 8
vector_force_scalar = required > backend_native_vreg_limit
```

Every SIMD dispatch path, including special handlers for `VORI32X4I` and FMA,
must use direct-scalar lowering when this flag is true.  Do not check only the
ordinary contiguous opcode table.

`vector_force_scalar` may remain set until the next `OP_VINDEX_HINT` or
function end.  Non-vector opcodes ignore it.  This deliberately covers the
post-latch `VGETLANE` writeback operations.

`vector_hint_active` (the native index/base register optimization) must be
false when `vector_force_scalar` is true, so that counter and remaining
tmpvars continue to be updated in their bytecode-visible slots.

Native limits for this milestone:

| Backend | Native logical vregs |
|---|---:|
| x86_64 SysV | 13 |
| Arm64 | 16 |
| i386 SSE | 8 |
| Arm32 NEON | keep current proven limit |
| PPC32/PPC64 Altivec | keep current proven limit |
| MIPS/RISC-V/no SIMD | 0, direct scalar |
| Win64 x86_64 current direct-scalar tier | 0 until its save-area work exists |

Do not silently mask high vreg indices in ModRM/register fields.  A native
handler seeing an out-of-tier vreg is a bytecode/JIT bug, not a reason to wrap
the index.

### 6.4 Milestone 1 acceptance

- `blend-copy`, `blend-dim`, and `blend-cross` report vectorized at `-O2`.
- DIM reports 12 logical vregs on the current x86_64 plan.
- CROSS reports 13 logical vregs on the current x86_64 plan.
- Their `-O0/-O2`, `-j0/-j` output is byte-for-byte identical.
- `NOCT_JIT_SIMD_MAX=scalar` produces identical output.
- An x86_64-compiled bytecode file containing these loops runs under i386
  QEMU without native-register aliasing or broken bytecode.
- All existing MUST_VECTORIZE/MUST_NOT tests retain their status, except that
  a dedicated old `budget` rejection case must continue to require more than
  16 if it is meant to remain a rejection test.

No new arithmetic opcode is part of Milestone 1.

## 7. Milestone 2: HIR predication and compare/select opcodes

This milestone targets ADD, SUB, GLYPH, and MELT.  Do not special-case their
function names or source line numbers.

### 7.1 Internal HIR SELECT

Add an optimizer-only ternary expression:

```text
HIR_EXPR_SELECT(cond, true_expr, false_expr)
```

Add a ternary union member to `struct hir_expr`; do not try to fit SELECT into
the existing binary union and do not encode it as arithmetic.

Update every generic HIR walker which can encounter an optimizer-created
expression:

- allocation/clone/free paths in `src/core/hir.c`
- HIR debug dumping
- liveness walkers in `hir_opt_simd.c`
- inline cloning/substitution in `hir_opt_inline.c` where applicable
- typed-expression handling in `hir_opt_typed.c`
- CSE hash/equality/read/kill walkers in `hir_opt_cse.c` if SELECT can reach it
- LIR scalar visitor defensively, even though accepted SELECT should normally
  be confined to a vector body

The selected values must have the same proven scalar type.  The condition is a
scalar comparison in HIR and becomes a lane mask only during VFOR lowering.

### 7.2 Transactional loop-body normalization

Implement SIMD-local if-conversion in `hir_opt_simd.c`.  It runs only on an
ABCE fast loop candidate and only for the following structured pattern:

```text
basic statements
IF (pure comparison) {
    one or more assignments to locals
}
continuation
```

An optional `else` may be accepted only when both arms assign the same local
set with pure expressions.  Reject else-if, nested IF, loop, call, return, and
arbitrary stores in this milestone.

Normalize:

```text
x = old
if (cond) { x = replacement }
```

to:

```text
x = SELECT(cond, replacement, old)
```

For two-armed assignment:

```text
x = SELECT(cond, true_value, false_value)
```

The normalization is transactional:

1. Preserve the original fast-loop body CFG.
2. Build a detached normalized basic body.
3. Run SIMD grammar checks and the shared planner on that body.
4. On rejection/OOM, restore the original body exactly.
5. On acceptance, use the normalized body only for the VFOR.

Do not flatten the source scalar remainder merely for convenience.  Generalize
the existing `simd_vectorize()` body cloning so RFOR and SFOR receive deep
clones of the original structured body.  VFOR receives the normalized basic
body.  Parent/succ/stop links must satisfy the existing HIR loop conventions.

This separation is also required by the RULE masked-store milestone.

### 7.3 New comparison/select bytecodes

Append these opcodes after all current opcodes:

```text
OP_VCMPI32X4   vd(imm8), va(imm8), vb(imm8), pred(imm8)
OP_VCMPF32X4   vd(imm8), va(imm8), vb(imm8), pred(imm8)
OP_VSELECT128  vd(imm8), vm(imm8), vt(imm8), vf(imm8)
```

Define a shared predicate enum in `bytecode.h`:

```text
VCMP_EQ, VCMP_NE, VCMP_LT, VCMP_LE, VCMP_GT, VCMP_GE
```

Semantics:

- Compare results contain `0xffffffff` for true and `0x00000000` for false in
  every 32-bit lane.
- I32 relations are signed Noct int32 relations.  The current draw-image
  clamps compare values already proven nonnegative where required.
- F32 relations are ordered and follow the source HIR relation exactly.
- SELECT copies bits and is type-neutral:
  `vd = (vm & vt) | (~vm & vf)` lane-wise.
- SELECT does not expose mask values to source-visible locals.

Do not add VMIN/VMAX yet.  ADD/SUB clamps intentionally lower through compare
and select first.

### 7.4 LIR planner/lowering changes

The shared planner and LIR visitor must understand:

- comparison expression equality and size
- comparison operand types
- SELECT's three value dependencies
- mask-result scratch lifetime
- destination aliasing rules

SELECT may require a mask and one simultaneously live unselected operand.
Calculate this through the shared scratch/liveness model; do not add a guessed
constant to the old `max_depth` formula.

The following source forms should normalize without a source-specific rule:

- ADD: `r > 255 ? 255 : r`
- SUB: `r < 0 ? 0 : r`
- GLYPH: conditional `out_a`
- MELT: sequential clamp to 0.0 and 1.0

MELT must remain compare/select at `-O2` so NaN behavior is unchanged.

### 7.5 Runtime/interpreter/C backend

Add typed helper declarations to `include/noct/aot.h`, implementations to
`src/core/execution.c`, decoders to `src/core/interpreter.c`, and C translator
emission to `src/backend/cback.c`.

The helper ABI has three integer operands after `env`.  Pack the final two
imm8 operands into one int where necessary, following the existing FMA helper
pattern.  Validate/unpack as unsigned bytes.

Do not extend the contiguous `OP_VLOADI32X4..OP_VSHRI32X4` helper table by
inserting opcodes into it.  The new opcodes are appended and receive explicit
dispatch handlers, like VORI/FMA.

### 7.6 Native JIT lowerings

x86_64 first functional lowering:

- I32 EQ/GT from `pcmpeqd`/`pcmpgtd`; derive LT/LE/GE/NE by operand reversal or
  mask inversion.
- F32 comparisons from `cmpps` with ordered predicates.
- SELECT from `pand`, `pandn`, and `por` using the already reserved scratch
  policy.  Preserve all logical operands unless `vd` aliases one by contract.
- AVX may use existing three-operand encoders, but AVX contraction is not a
  milestone requirement.

Arm64 first functional lowering:

- I32 compare from `cmeq`, `cmgt`, `cmge` and reversed operands.
- F32 compare from `fcmeq`, `fcmgt`, `fcmge` and reversed operands.
- SELECT from `bsl`, `bit`, or `bif`, with alias behavior tested explicitly.

All other JIT files must decode the opcodes and use their direct-scalar helper
tier until a native implementation is deliberately added.  Never return
`JIT_OP_NOT_IMPLEMENTED` for optimized portable bytecode.

Remember to update bytecode-length scanners in x86_64/Arm64 vector-hint code
and any other scanner with a switch over vector opcode sizes.

### 7.7 Milestone 2 acceptance

- ADD, SUB, GLYPH, and MELT report vectorized at `-O2`.
- `-O0/-O2`, interpreter/JIT outputs match.
- Test alpha/threshold boundary values, including 0, 1, 254, 255, negative
  threshold, and threshold above 255 where the language permits it.
- Add a focused F32 compare/select test containing `+0.0`, `-0.0`, infinity,
  and NaN bit patterns.  O2 output must match scalar execution.
- Force scalar/SSE2/SSE4.1/AVX tiers on x86_64.
- Run Arm64 native and scalar tiers under QEMU; M5 measurement is optional and
  not a blocker.

## 8. Milestone 3: RULE predicated store

Do not implement RULE as unconditional load-old-value + SELECT + store unless
the language memory model explicitly declares same-value writeback
unobservable.  The conservative implementation uses a real masked store.

### 8.1 HIR representation

Add an optimizer-only masked packed store form.  A practical representation is
an LHS node carrying base, element offset, and mask, with the statement RHS as
the stored value:

```text
HIR_EXPR_PMASKSTORE32(base, ofs, mask) = value
```

It is accepted only in the normalized VFOR body.  RFOR and SFOR use the deep
cloned original branchy CFG from Milestone 2.

### 8.2 Bytecode

Append:

```text
OP_VMASKSTOREI32X4
    base(u16), ofs(u16), vs(imm8), vm(imm8)
```

Only lanes with all-one mask values are written.  False lanes perform no
memory write.  ABCE/version guards already prove the contiguous four-lane
destination range.

Initial native lowering may extract/test each mask lane and issue up to four
scalar stores.  It is acceptable for this to be slower than scalar RULE; the
admission/cost model can later decide whether to enable it by default.

Do not introduce an unconditional vector store, and do not require AVX-512.

### 8.3 Acceptance

- RULE reports vectorized in a forced-admission developer test.
- True/false/alternating/all-true/all-false masks match scalar output.
- A guard-page or canary test proves false lanes are not written.
- Alias/disjointness guards remain active.
- A later cost-model switch may keep RULE scalar by default, but that policy is
  separate from proving the transformation and opcode correct.

## 9. Milestone 4: strict FP induction and checked gather

This milestone is deliberately last.  It affects memory safety and strict FP
semantics and must not be mixed into the predication patches.

### 9.1 Recognize strict scalar induction

Recognize only the canonical form:

```text
state initialized before loop
...
state = state + invariant_step   /* tail update */
```

Conditions:

- state and step are proven F32
- exactly one tail update on every loop iteration
- no conditional update
- no other assignment to state in the body
- step is loop-invariant
- no call/I/O between state use and update
- live-out state is allowed only because the vector opcode performs exact
  writeback

Do not rewrite it to `start + i * step` at O2.

Append this opcode:

```text
OP_VINDUCTF32X4 vd(imm8), state(u16), step(u16)
```

Exact semantics, with a binary32 rounding/store after every addition:

```text
x0 = state
x1 = round_f32(x0 + step)
x2 = round_f32(x1 + step)
x3 = round_f32(x2 + step)
state = round_f32(x3 + step)
vd = {x0, x1, x2, x3}
```

This preserves scalar recurrence order.  At O3, a later and separately tested
optimization may choose a faster reassociated ramp.

The scalar remainder starts from the state written back by the final vector
group, so it naturally continues the exact recurrence.

### 9.2 Recognize indirect packed loads

Extend ABCE/SIMD site classification with a gather site:

```text
src[index_stream[i]]
src[Int.from(ty) * width + Int.from(tx)]
```

The outer source must be a typed packed int32/uint32 parameter or local for the
first implementation.  The index expression must vectorize entirely to I32x4.
Stores through gathered indices are out of scope.

ABCE still emits/uses type guards, `PBASE`, and `PLEN`.  Unlike a contiguous
PLOAD, gather does not claim that all indices are in range.

Represent a gather explicitly in the normalized vector HIR so that the normal
contiguous `simd_rewrite_expr()` does not replace its index by the bare loop
counter.

### 9.3 Checked gather bytecode

Append:

```text
OP_VGATHERI32X4_CHECKED
    vd(imm8), base(u16), plen(u16), vi(imm8)
```

Semantics:

1. Read indices in lane order 0..3.
2. Reject a negative signed index.
3. Reject an index greater than or equal to `plen`.
4. On success, load four uint32/int32 bit patterns into `vd`.
5. On failure, report the same packed out-of-range error as scalar indexing
   for the first invalid lane and return failure from interpreter/JIT.

Because Noct has no recoverable exception, no optimized store after the failed
gather executes.  If the embedding API later promises observation of partial
writes after `rt_error`, this contract must be revisited before release.

Do not emit raw `VLOADI32X4` for arbitrary indices.

### 9.4 First native implementation

x86_64:

- Extract or spill four indices.
- Perform signed-negative and length checks before each native load.
- Load scalar uint32 values.
- Assemble with `pinsrd` when available; use SSE2 `movd` and unpack operations
  otherwise.
- Do not require AVX2 gather.

Arm64:

- Extract indices with `umov`/`smov` as appropriate.
- Perform lane bounds checks.
- Load into vector lanes with lane `ld1.s`/`ins`.

Other backends use the checked runtime helper.  A helper call on the success
path may be used only in the direct-scalar vector tier.  Native mapped vector
regions must inline the checks/loads or preserve all live mapped registers.

### 9.5 Planner/liveness requirement

Coordinate/index temporaries must die before the blend arithmetic begins.
Do not allocate one permanent home for every source local in 3D-ALPHA.

Required scheduling regions are conceptually:

1. build tx/ty lanes
2. convert and build index lanes
3. checked gather pixels
4. release coordinate/index vregs
5. run the existing alpha-blend vector DAG
6. store output

The shared planner must reuse dead logical vregs between these regions.  If the
plan still requires more than 16, reject E8 with the exact peak live set; do
not silently spill until a deliberate vector-spill design exists.

### 9.6 Milestone 4 acceptance

- 3D-ALPHA and 3D-CROSS report vectorized at `-O2` for their essence probes.
- O0/O2 outputs match for positive, zero, and negative coordinate increments.
- FP induction tests compare lane-by-lane bit patterns, not decimal text only.
- A live-out tx/ty test proves state writeback across vector and remainder
  iterations.
- Gather tests cover first/middle/last valid indices and negative/equal-length
  invalid indices.
- Error tests verify the first invalid lane is reported.
- Interpreter, forced-scalar JIT, x86_64 native JIT, and Arm64 native JIT agree.
- 3D-CROSS tests both index streams independently.

## 10. File-by-file implementation checklist

The implementing agent must search again at implementation time; this list is
the minimum known surface, not permission to ignore new matches.

### HIR and optimizer

- `src/core/hir.h`
  - ternary SELECT representation
  - masked-store/gather/induction internal nodes as selected above
  - optional per-VFOR required-vreg metadata
- `src/core/hir.c`
  - clone/free/dump/walk support
  - optimizer pass ordering remains typed -> ABCE -> SIMD -> CSE -> typed
- `src/core/hir_opt_simd.c`
  - transactional normalization
  - original CFG preservation/deep clone
  - shared planner call
  - gather/induction recognition and vector rewrite
- `src/core/hir_opt_abce.c`
  - gather-site type/base/length guards without falsely claiming contiguous
    index bounds
- `src/core/hir_opt_inline.c`, `hir_opt_typed.c`, `hir_opt_cse.c`
  - update exhaustive expression walkers where optimizer-created nodes can
    reach them
- `src/core/hir_opt.h`
  - declarations only if new internal entry points are added
- `src/core/simd_plan.[ch]`
  - single cache/scratch/liveness planner

Do not move the normal CSE pass before SIMD as a shortcut.  Current CSE creates
CAPTURE nodes and does not solve control-flow/gather admission by itself.

### LIR/bytecode/runtime

- `src/core/bytecode.h`
  - append opcodes and predicate enum
  - document exact operand shapes and semantics
- `src/core/lir.c`, `src/core/lir.h`
  - use shared plan
  - emit required-vreg hint before all vector initialization
  - lower SELECT/masked store/induction/gather
  - update bytecode dump and opcode metadata scans
- `src/core/runtime.h`, `src/core/execution.c`
  - helper declarations/implementations
- `include/noct/aot.h`
  - exported helper declarations needed by generated C
- `src/core/interpreter.c`
  - validated decode and semantic fallback
- `src/backend/cback.c`
  - bytecode decode and helper calls for ANSI C output

### JIT backends

- `src/core/jit.h`
  - shared context fields/constants only when genuinely common
- `src/core/jit-x86_64.c`
- `src/core/jit-arm64.c`
  - first native comparison/select/induction/gather implementations
  - vector-hint scanner sizes
- `src/core/jit-x86.c`
- `src/core/jit-arm32.c`
- `src/core/jit-ppc32.c`, `src/core/jit-ppc64.c`
- `src/core/jit-mips32.c`, `src/core/jit-mips64.c`
- `src/core/jit-riscv32.c`, `src/core/jit-riscv64.c`
  - decode every new opcode
  - use semantic direct-scalar fallback until native support is implemented

For every new opcode, use `rg OP_<NAME>` to verify coverage comparable to
`OP_VFMAF32X4` and `OP_VORI32X4I`.  Missing a bytecode-length scanner is as
serious as missing a dispatch switch.

## 11. Test integration

### 11.1 Golden outputs

Create `.out` files beside every draw-image probe from an `-O0 -j0` run.  Add
the nested drawimage cases explicitly to `tests/testcases/run-simd.sh`; its
current `simd/*.noct` glob does not include `simd/drawimage/*.noct`.

For each probe run all four combinations:

```text
-O0 -j0
-O0 -j
-O2 -j0
-O2 -j
```

O3 is additional coverage, not a substitute for O2 semantic testing.

### 11.2 Admission assertions

Maintain milestone-specific lists instead of marking all cases vectorized in
one patch.  A case enters MUST_VECTORIZE only in the milestone that implements
its missing facility.

After the final milestone:

```text
copy dim cross add sub glyph melt rule 3d-alpha 3d-cross
```

must have an explicit admission assertion, subject only to a documented RULE
cost-policy mode.

### 11.3 Bytecode portability

Add a round-trip test:

1. Optimizer-enabled x86_64 Noct compiles DIM/CROSS to bytecode.
2. Optimizer is not needed by the loader.
3. Run the bytecode with `NOCT_JIT_SIMD_MAX=scalar`.
4. Run the same bytecode under i386, Arm32, Arm64, PPC32, and PPC64 QEMU where
   builders are available.
5. Compare to the same golden output.

Extend `tests/testcases/run-simd-qemu.sh` to accept a selected probe or add a
new small wrapper; do not duplicate architecture tables in multiple scripts.

### 11.4 Commands

Host gate after every milestone:

```sh
cd /home/awe/noct-simd
./build.sh build static
./build.sh test simd
./build.sh test abce
./build.sh test cse
./build.sh test typedop
```

Focused diagnostics:

```sh
NOCT_SIMD_DEBUG=1 NOCT_LIR_VFOR_DEBUG=1 \
  build-static/noct -j -O2 \
  tests/testcases/simd/drawimage/blend-dim.noct

build-static/noct --simd-info -j0 -O2 \
  tests/testcases/simd/drawimage/blend-add.noct
```

Run `git diff --check` before review.

## 12. Failure handling and diagnostics

Add precise rejection breadcrumbs; do not collapse new failures into E2/E8.
Suggested developer-only reasons:

```text
E10 if-conversion impure arm
E10 if-conversion unsupported CFG
E11 select type mismatch
E12 masked store unsupported site
E13 induction noncanonical
E13 induction conditional update
E14 gather index type
E14 gather source type
E15 logical vreg peak N > 16
```

`--simd-info` remains success-only.  Rejection details remain under
`NOCT_SIMD_DEBUG`.

OOM and malformed-bytecode failures must propagate normally.  Do not turn an
internal LIR/JIT planning failure into silent scalar source recompilation; an
optimizer-free bytecode loader cannot perform that recovery.

## 13. Review checkpoints

Request review at these boundaries:

1. Shared planner and portable logical-vreg contract; DIM/CROSS only.
2. HIR SELECT and scalar-CFG preservation before adding new bytecodes.
3. Compare/select semantic helpers and x86_64/Arm64 native lowering.
4. RULE masked-store memory semantics.
5. Strict induction semantics.
6. Checked-gather error semantics and bounds checks.

Do not combine all milestones into one large unreviewable patch.

For the current `/home/awe/noct-simd` task, do not commit and do not push.
Leave changes staged or unstaged as requested by the reviewer at implementation
time.

## 14. Completion definition

This plan is complete only when:

- every draw-image probe has a golden semantic test;
- every intended loop passes SIMD admission at `-O2`;
- `-O0` and `-O2` agree across interpreter, forced-scalar JIT, x86_64 native,
  and Arm64 native;
- optimized bytecode remains executable on smaller/no-SIMD backends through
  the semantic fallback tier;
- invalid gathers cannot issue invalid native memory reads;
- strict FP induction is bitwise consistent with scalar recurrence;
- no existing SIMD, ABCE, CSE, typed-op, bytecode round-trip, or QEMU test
  regresses;
- instruction-count tuning remains clearly separated as follow-up work.
