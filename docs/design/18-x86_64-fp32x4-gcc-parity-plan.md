# 18 — x86_64 FP32x4 vector-loop GCC code-generation parity

Status: **implemented; awaiting review** (2026-08-11).

The recurrent `blend2` loop now contains 39 instructions (202 bytes), equal
to the GCC 14.2 native x4 reference count and within the 42-instruction
acceptance ceiling.  The implementation keeps the loop at XMM/128-bit x4,
selects all four planned common-value caches, emits three FMA3 operations at
O3, and retains the two-instruction `add`/`jne` latch.  Exact evidence and
the completed verification matrix are recorded in
[`../bench-blend2-1000.md`](../bench-blend2-1000.md).

This document is the implementation handoff for reducing the recurrent
x86_64 machine-code loop of `tests/simd/blend2.noct` from the current 70
instructions to approximately the same instruction count as GCC for the same
FP32x4 work.

The implementation must preserve the existing fixed-width 128-bit, four-lane
vector contract.  AVX2 FP32x8, AVX-512 FP32x16, loop widening, and native-width
vector semantics are future work and are explicitly outside this plan.

Read these documents before changing code:

- [05-cse.md](05-cse.md), especially the vector-loop exclusion;
- [06-simd.md](06-simd.md), especially D-SIMD13 and vector LIR lowering;
- [08-jit-simd-portability.md](08-jit-simd-portability.md);
- [16-arm64-vector-loop-codegen-plan.md](16-arm64-vector-loop-codegen-plan.md);
- [17-x86_64-vector-loop-fma-o3-plan.md](17-x86_64-vector-loop-fma-o3-plan.md);
  and
- [../bench-blend2-1000.md](../bench-blend2-1000.md).

The repository may contain the still-uncommitted implementation of Design 17.
Treat those changes as the baseline for this work.  Do not overwrite, revert,
or reimplement them.  Resolve symbols by name rather than relying on line
numbers in this plan.

---

## 1. Required outcome

On a SysV x86_64 machine with usable AVX, SSE4.1, and FMA3, Noct O3 must emit
an FP32x4 recurrent blend loop that:

1. processes exactly four pixels per iteration;
2. loads the packed source vector once;
3. loads the packed destination vector once;
4. computes `src_a`, `pix_a`, and `pix_inv_a` once per four-pixel iteration;
5. keeps those common values live across the red, green, and blue channel
   calculations;
6. emits three native FMA3 channel operations at O3;
7. uses VEX three-operand forms for eligible ordinary vector operations;
8. emits no copy solely to adapt a three-address LIR operation to legacy
   SSE's destructive two-address form;
9. retains the existing two-instruction `add/jne` negative-index latch; and
10. produces the same result as the interpreter and portable helper tiers.

The primary code-size target is the recurrent loop only, including its latch
and excluding preheader, guards, scalar remainder, and post-loop extraction:

| Generator | Width | Pixels/trip | Baseline recurrent instructions |
|---|---:|---:|---:|
| GCC 14.2 `-O3 -march=native -mprefer-vector-width=128` | 128 | 4 | 39 |
| GCC 14.2 `-O3 -march=haswell -mprefer-vector-width=128` | 128 | 4 | 40 |
| current Noct JIT `-O3` | 128 | 4 | 70 |
| target Noct JIT `-O3` | 128 | 4 | 39–42 |

The acceptance ceiling is 42 recurrent instructions and a gap of no more than
three instructions from the freshly regenerated same-width GCC reference.
The intended result is 39 or 40.  A result above 42 is not complete merely
because it is faster than the baseline.

Instruction count is a code-generation gate, not a performance claim.  The
implementation must also report runtime measurements, but timing variance
must not be used to waive structural failures.

---

## 2. Non-goals and prohibited scope expansion

The implementing agent must not do any of the following as part of this plan:

- do not emit YMM or ZMM arithmetic for the vectorized loop;
- do not reinterpret any `*X4` opcode as a native-width operation;
- do not add FP32x8, FP32x16, integer x8, or integer x16 opcodes;
- do not add an AVX2 or AVX-512 loop-width selector;
- do not add general loop unrolling;
- do not add a general-purpose register allocator;
- do not move the ordinary HIR CSE pass before SIMD;
- do not allow ordinary HIR CSE to insert `HIR_EXPR_CAPTURE` inside a
  vector-marked loop;
- do not add a new packed-load opcode merely to cache `dst_pix`;
- do not change O0, O1, or O2 fused-rounding semantics;
- do not emit FMA below O3;
- do not require AVX for correctness; SSE2 and portable fallbacks remain
  required;
- do not enable native vector-register lowering on Win64 in this change;
- do not use xmm6..xmm15 on Win64 without preserving its nonvolatile ABI;
- do not change arm64's current x4 NEON/FMLA lowering;
- do not add `vpternlogd` as a prerequisite for meeting the primary target;
- do not mix unrelated parser, module, app, accelerator, or parallelization
  changes into the patch; and
- do not commit or push unless the user gives a new explicit instruction.

AVX-512VL `vpternlogd` in a 128-bit XMM form may be evaluated only after the
39–42 instruction target is met.  It is an optional follow-up because it does
not widen the loop, but it is not part of the required implementation.

---

## 3. Audited baseline facts

### 3.1 The ordinary HIR CSE pass is not the missing feature

The pass order is ABCE -> SIMD -> CSE.  `hir_opt_cse.c` deliberately skips the
body of a vector-marked `HIR_BLOCK_FOR`.  This is required because a CSE
`HIR_EXPR_CAPTURE` node is outside the vector-lowerable HIR grammar.

Do not remove that exclusion.

### 3.2 A vector-local structural cache already exists

`src/core/lir.c` already contains a target-aware vector expression planner:

- `lir_vfor_cache_collect()` finds repeated eligible expressions;
- `lir_vfor_expr_equal()` provides structural equality;
- `lir_vfor_cache_eligible()` accepts repeated packed loads and selected
  floating-point `MUL`/`MINUS` expressions;
- `lir_vfor_plan_fits()` proves the home/cache/scratch register budget;
- `lir_vfor_cached_reg()` reuses a materialized expression; and
- `lir_visit_vfor_block()` emits selected cache entries before the recurrent
  statement body.

For a one-store loop, cache selection intentionally considers repeated loads
first, then repeated floating blend expressions.  This is already the correct
model for blend2.  Do not replace it with a second CSE implementation.

### 3.3 The current architecture budget explains the arm64/x86_64 difference

The current budget is:

~~~c
#if defined(NOCT_ARCH_ARM64)
#define VFOR_VREG_MAX 16
#else
#define VFOR_VREG_MAX 8
#endif
~~~

arm64 can keep the repeated source load, repeated destination load, `pix_a`,
and `pix_inv_a` resident.  x86_64 cannot fit all four caches plus constants,
invariants, destinations, and expression scratch in eight logical registers,
so `lir_vfor_plan_fits()` removes cache entries until the plan fits.

The observed x86_64 machine code consequently keeps `src_pix`, but reloads
`dst_pix` for each channel and reconstructs alpha expressions per channel.

### 3.4 The common vector LIR is already three-address

Existing operations already carry separate destination and source operands:

~~~text
VMULF32X4 vd, va, vb
VSUBF32X4 vd, va, vb
VAND128   vd, va, vb
VSHRI32X4 vd, va, immediate
VFMAF32X4 vd, va, vb, vc
~~~

The current x86_64 emitter inserts `movdqa va,vd` before destructive legacy
SSE operations.  A new arithmetic LIR opcode is not needed for AVX
three-operand emission.

### 3.5 The portable register file already has sixteen entries

`struct rt_env` contains `vreg[16][16]`.  arm64 already uses logical vreg
indices above seven.  The comment in `bytecode.h` that says all vector indices
are 0..7 is stale and must be corrected to describe the portable 0..15
storage limit and target-specific native budgets.

Do not enlarge `env->vreg` in this change.

### 3.6 Current SysV x86_64 register ownership

In SysV x86_64 all XMM registers are caller-saved.  The accepted vector strip
region is call-free.  The current emitter nevertheless maps only logical
vregs 0..7 and uses:

- xmm8 and xmm9 as fixed scratch for the SSE2 `pmulld` emulation; and
- xmm15 as the preheader-materialized opaque-alpha vector.

Any extension of logical vregs must first reserve non-conflicting scratch.

### 3.7 Win64 remains memory-canonical

The x86_64 JIT currently disables inline vector-register lowering when
`IS_MSABI` is true.  It uses `env->vreg` and portable scalar/vector code
instead.  Preserve this policy.  The enlarged logical vreg indices are safe
in that tier because `env->vreg` contains sixteen entries.

### 3.8 Measured instruction composition

The current recurrent x86_64 O3 loop is:

| Category | GCC native x4 | Noct current x4 |
|---|---:|---:|
| packed loads/stores | 3 | 5 |
| register copies | 0 | 12 |
| channel shifts/masks | 11 | 16 |
| int/float conversions | 10 | 15 |
| FP arithmetic | 8 | 15 |
| packing | 4–5 | 5 |
| loop control | 3 | 2 |
| total | 39–40 | 70 |

The intended Noct target without AVX-512 ternary logic is:

| Category | Target |
|---|---:|
| packed loads/stores | 3 |
| register copies caused by two-address lowering | 0 |
| channel shifts/masks | 11 |
| int/float conversions | 10 |
| FP arithmetic | 8 |
| packing | 5 |
| loop control | 2 |
| total | 39 |

This table is a review oracle.  If a final count differs, classify every
extra instruction rather than merely reporting the total.

---

## 4. Authoritative design decisions

### D-X4-1: vector width remains exactly 128 bits

Every existing x4 operation retains its current semantics:

- four 32-bit lanes;
- sixteen-byte loads and stores;
- loop index increment of four elements; and
- scalar remainder computed relative to four lanes.

AVX is used only as an instruction encoding and three-address register model.
The VEX L bit must be zero for every operation in this work.

Reject any patch that emits YMM/ZMM instructions, changes the VINDEX lane
operand from four, or changes SUBJNZ's step from four.

### D-X4-2: reuse the existing vector-local cache

Do not change the ordinary HIR CSE pass.  Do not introduce a new opcode for
cached loads.  Make the existing vector-local cache fit by increasing only
the x86_64 logical-register budget and teaching its emitter to map the added
registers.

For blend2, the selected cache set must contain structurally equivalent
expressions for:

1. the source packed load;
2. the destination packed load;
3. `Float.from(src_a) * alpha_float`; and
4. `1.0 - pix_a`.

Selection remains guarded by the current one-store rule.  Do not relax alias
or memory-clobber assumptions merely to increase cache hits.

### D-X4-3: x86_64 gets thirteen logical native vregs

For `NOCT_ARCH_X86_64`, define the LIR budget as 13, meaning logical indices
0..12 map directly to xmm0..xmm12 on native SysV x86_64.

Reserve physical registers as follows:

| Physical register | Ownership inside an accepted native vector region |
|---|---|
| xmm0..xmm12 | logical vreg 0..12 |
| xmm13,xmm14 | SSE2 integer-multiply emulation scratch only |
| xmm15 | opaque-alpha immediate invariant only |

Keep arm64 at 16.  Keep x86 32-bit and other existing targets at their current
budget.  Do not globally change every target to 13 or 16.

Thirteen is intentionally less than the architectural sixteen.  It gives
blend2 enough cache and scratch capacity while retaining explicit scratch
ownership and avoiding a general allocator.

The implementing agent must confirm using planner debug output that blend2's
four caches fit within 0..12.  If they do not, fix the scratch-size model or
cache schedule; do not consume xmm13..xmm15 as ad-hoc logical registers.

### D-X4-4: high-register support is complete, not opcode-specific

Every inline x86_64 vector opcode that can receive a logical vreg must either:

1. encode xmm0..xmm12 correctly; or
2. route the whole vector function to the memory-canonical tier before native
   emission starts.

It is not acceptable for blend2-only opcodes to support high registers while
another existing opcode silently truncates a ModRM field.

Audit at least these families:

- VLOAD/VSTORE;
- VSPLAT and VGETLANE;
- VMOV128;
- integer ADD/SUB/MUL/AND/OR/XOR;
- immediate shifts;
- FP ADD/SUB/MUL/DIV;
- int/float conversions;
- VORI32X4I; and
- VFMAF32X4.

Use small encoding helpers for REX, ModRM, and VEX fields.  Do not continue
open-coding expressions such as `(dst << 3) | src` where high register bits
can be lost.

### D-X4-5: AVX is a capability independent of FMA

Add `JIT_SIMD_CAP_AVX`.  On x86/x86_64 it means all of the following were
proved:

- CPUID.1:ECX.AVX is set;
- CPUID.1:ECX.OSXSAVE is set;
- XGETBV is executed only after OSXSAVE is known; and
- XCR0 has both XMM state bit 1 and YMM state bit 2 enabled.

FMA capability additionally requires CPUID FMA.  Therefore:

~~~text
FMAF32X4 capability => AVX capability
AVX capability does not imply FMAF32X4 capability
~~~

Do not infer usable AVX from the compiler used to build Noct.  Do not execute
XGETBV before checking OSXSAVE.

Extend `NOCT_JIT_SIMD_MAX` with an `avx` ceiling that retains SSE2/SSE3/SSE4.1
and AVX but removes FMA.  Existing `sse41` must remove AVX and FMA.  Existing
`fma` continues to retain detected capabilities and must never add a missing
feature.

### D-X4-6: AVX selects VEX.128 three-address lowering

When `JIT_SIMD_CAP_AVX` is present, use VEX.128 encodings for eligible
ordinary vector operations.  Preserve the LIR operand order exactly.

At minimum cover:

- `VADDF32X4`, `VSUBF32X4`, `VMULF32X4`, `VDIVF32X4`;
- `VADDI32X4`, `VSUBI32X4`, `VMULI32X4` when the ISA supports the selected
  native form;
- `VAND128`, `VOR128`, `VXOR128`;
- `VSHLI32X4`, `VSHRI32X4`;
- `VCVTI32F32X4`, `VCVTF32I32X4`; and
- register moves, loads, stores, splats, and extracts where using a VEX form
  simplifies complete high-register encoding.

The ordinary binary form must implement `vd = va op vb` without first copying
`va` to `vd`.  Immediate shifts must implement `vd = va shift imm` without a
pre-copy.

Use L=0.  Do not emit 256-bit instructions.  Do not add `vzeroupper` merely
because VEX.128 is used; VEX.128 writes clear the upper portion of the
destination and this plan never emits a legacy instruction that consumes a
dirty YMM upper half.  If implementation evidence contradicts this statement,
document the exact transition hazard before adding code.

### D-X4-7: SSE lowering remains a complete fallback

Without AVX, retain legacy SSE2/SSE4.1 two-address lowering and its required
copies.  The enlarged logical map still uses xmm0..xmm12; high registers are
available in x86-64 legacy SSE through REX prefixes and do not require AVX.

Move the current SSE2 `pmulld` emulation scratch from xmm8/xmm9 to the reserved
xmm13/xmm14 pair.  It must preserve all logical operands and xmm15.  Add REX
bits to those instructions and verify every lane against the portable helper.

Do not call a helper in the middle of a native-register vector region.

### D-X4-8: opaque alpha keeps xmm15 ownership

`OP_VORI32X4I` may continue to use a preheader-materialized xmm15 invariant in
an accepted vector loop.  No logical vreg may map to xmm15.  The SSE2 integer
multiply fallback must not clobber xmm15.

On an AVX-capable path, lower the final operation without a destructive
source copy when possible, for example as a three-operand `vpor` using xmm15.
Keep the immediate construction outside the recurrent loop.

Do not add `vpternlogd` in the required phase.

### D-X4-9: FMA3 accepts high logical registers

Extend the existing `VFMAF32X4` FMA3 encoder from logical 0..7 to 0..12.
Correctly encode inverted VEX R/B/vvvv bits.  Preserve the common fused
semantics and existing whole-function portable fallback when FMA is not
usable.

The normal O3 vector schedule uses `vd == addend`.  That form should emit one
FMA instruction and no preparatory copy.  Other legal alias forms must remain
correct, even if one requires a move.

### D-X4-10: planner observability is developer-only

Add developer observability sufficient to prove the LIR decision, either by
extending an existing debug-only path or adding `NOCT_LIR_VFOR_DEBUG=1`.
Recommended output fields are:

~~~text
noct-lir-vfor: func=blend target=x86_64 max=13 homes=N candidates=N caches=4 peak=N
~~~

Tests may match stable field names and numeric values, but `--simd-info`
output must not change.  Do not print planner diagnostics during normal use.

---

## 5. Detailed implementation work

### 5.1 Preserve and regenerate the baseline

Before editing:

1. build the existing debug or mt-debug configuration;
2. run `tests/run-simd.sh` and `tests/run-fma-helper.sh`;
3. capture the current O3 blend JIT binary after branch patching;
4. disassemble the recurrent loop and confirm 70 instructions;
5. save the mnemonic histogram; and
6. regenerate both GCC references from the same signed-conversion C kernel.

Use these GCC reference classes:

~~~sh
gcc -O3 -march=native -mprefer-vector-width=128 \
    -fno-unroll-loops -ffp-contract=fast

gcc -O3 -march=haswell -mprefer-vector-width=128 \
    -fno-unroll-loops -ffp-contract=fast
~~~

Do not use `-ffast-math`.  Channel results in C must use signed `int32_t`
conversion to match `Int.from`; using unsigned conversion causes GCC to emit
unrelated range-correction code.

### 5.2 Refactor x86_64 register encoders first

Before changing the LIR budget, introduce reviewed helpers in
`src/core/jit-x86_64.c` for:

- validating a logical/physical XMM index;
- emitting a legacy mandatory prefix plus optional REX.R/REX.B;
- emitting ModRM register-register operands with high register bits;
- emitting ModRM/SIB memory operands with a high XMM reg field;
- emitting a three-byte VEX prefix with explicit map, pp, W, L, R, X, B,
  and vvvv fields; and
- emitting the small set of VEX.128 register forms used by vector LIR.

Do not build one overly generic assembler.  Helpers should be small enough to
audit against Intel/AMD encoding tables and assembler-produced oracle bytes.

For each encoding family, assemble representative instructions using both
low and high registers, for example xmm1/xmm2/xmm3 and xmm10/xmm11/xmm12.
Compare exact bytes with GNU `as`/`objdump`.  Required oracle coverage:

- VEX FP binary operation;
- VEX integer logical operation;
- VEX immediate shift, whose operand fields differ from ordinary NDS forms;
- VEX conversion;
- VEX FMA3;
- legacy SSE high-register binary operation;
- high-register unaligned load and store; and
- high-register splat/extract.

Do not guess VEX inversion or immediate-shift field placement from another
opcode.

### 5.3 Add AVX capability detection

In `jit_detect_simd_caps()`:

1. read CPUID leaf 1 once;
2. set SSE capabilities as today;
3. if AVX and OSXSAVE are present, read XCR0;
4. set AVX only if XMM and YMM OS state are enabled; and
5. set FMA only if AVX was set and the FMA CPUID bit is present.

Apply the same logical test to GCC/Clang inline assembly and MSVC intrinsic
branches.  Keep per-build/per-VM behavior; do not create an unsynchronized
process-global cache.

Update `jit_apply_simd_max()` and debug output so tests can distinguish
SSE4.1, AVX without FMA, and FMA.

### 5.4 Implement VEX.128 lowering while the vreg budget is still eight

First switch eligible operations to VEX based on the AVX capability without
changing `VFOR_VREG_MAX`.  This isolates encoder errors from register-budget
errors.

Acceptance at this checkpoint:

- all existing SIMD numerical tests pass;
- `NOCT_JIT_SIMD_MAX=sse41` emits legacy SSE and remains correct;
- `NOCT_JIT_SIMD_MAX=avx` at O2 emits VEX ordinary operations but no FMA;
- `NOCT_JIT_SIMD_MAX=fma` at O3 emits three FMA3 operations;
- the recurrent blend loop loses the copies caused solely by two-address
  lowering; and
- no YMM/ZMM register appears in disassembly.

Expected intermediate blend count is approximately 58.  Do not make this
number a hard correctness gate, because actual OP_VMOV semantics may leave a
small number of valid copies.  Classify every remaining move.

### 5.5 Expand the x86_64 logical budget to thirteen

Only after the encoder tests pass:

1. define target-specific `VFOR_VREG_MAX` values in `lir.c`:
   arm64=16, x86_64=13, existing others unchanged;
2. update `lir_vfor_physical_reg()` assumptions if necessary;
3. preserve `lir_vfor_plan_fits()` as the single sizing authority;
4. do not bypass its scratch-need calculation;
5. add debug output for selected caches and peak register use; and
6. verify blend2 selects all four required cache entries.

The cache ordering must remain source-order/dependency-order safe.  Do not
sort expressions by textual name or machine opcode.  Do not cache a load
across an intervening possible memory clobber.

Expected recurrent structural changes:

- destination loads: 3 -> 1;
- source-alpha shifts: 6 -> 1;
- source-alpha int-to-float conversions: 6 -> 1;
- alpha-scale multiplications: 6 -> 1; and
- inverse-alpha subtractions: 3 -> 1.

### 5.6 Complete high-register legacy and FMA lowering

Run the entire vector opcode matrix with plans that deliberately allocate
logical registers 8..12.  Do not rely on blend2 alone because it may not place
every opcode in a high destination/source position.

Add a test Noct source, or a focused LIR/helper harness, that covers:

- high destination only;
- high first source only;
- high second source only;
- high shift source/destination;
- high load destination and store source;
- high FMA destination, both factors, and addend; and
- SSE2 integer multiply while other high logical registers are live.

Every case must compare with interpreter output.  An encoding that happens to
work when all high bits are zero is not adequate.

### 5.7 Final blend scheduling audit

After both AVX and the larger cache are active, inspect the recurrent loop.
It must have the following semantic shape:

~~~text
src = load src[x..x+3]
dst = load dst[x..x+3]
src_a = cvt(src >> 24)
pix_a = src_a * alpha_float
pix_inv_a = 1.0 - pix_a

red   = fma(cvt(src_r), pix_a, cvt(dst_r) * pix_inv_a)
green = fma(cvt(src_g), pix_a, cvt(dst_g) * pix_inv_a)
blue  = fma(cvt(src_b), pix_a, cvt(dst_b) * pix_inv_a)

packed = opaque_alpha | (red << 16) | (green << 8) | blue
store packed
add index, 4
jne loop
~~~

The precise instruction schedule may differ from GCC.  The structural facts
above and the category budget in Section 3.8 are mandatory.

---

## 6. Testing and verification matrix

### 6.1 Numerical tiers

Run blend2 and the complete SIMD suite under:

| Ceiling | Expected x86_64 behavior |
|---|---|
| `scalar` | memory-canonical portable vector operations |
| `sse2` | native legacy SSE2 where supported |
| `sse3` | legacy SSE; no VEX |
| `sse41` | legacy SSE4.1; no VEX/FMA |
| `avx` | VEX.128 ordinary operations; FMA removed |
| `fma` | VEX.128 plus native FMA3 when detected |

O3 bytecode containing the strict FMA opcode must continue to select the
existing whole-function portable vector tier when the ceiling removes FMA.
Use O2 or a non-FMA vector expression to test native AVX-without-FMA lowering.

### 6.2 Optimization levels

- O0: no vectorization regression;
- O1: existing behavior unchanged;
- O2: vectorized, no `VFMAF32X4`, VEX allowed when AVX is usable;
- O3: exactly three `VFMAF32X4` operations for blend2 and three native FMA3
  instructions on a capable SysV host.

Do not change O2 goldens to fused O3 results.

### 6.3 ABI/build matrix

Required builds and tests:

- native SysV x86_64 debug/mt-debug;
- native SysV x86_64 release used for measurement;
- MinGW x86_64 compile at minimum, verifying MSABI memory-canonical code
  accepts logical vregs 0..12;
- x86 32-bit build, proving its logical budget remains eight;
- arm64 build/test or QEMU-user smoke test, proving its x4/16-register path is
  unchanged; and
- existing C backend/interpreter tests with high logical vregs.

Do not require M5 or POWER hardware performance measurements as a correctness
gate for this x86_64-only change.

### 6.4 Encoder oracle tests

For every new encoder helper, keep the assembler/objdump oracle bytes in test
comments or a small focused test.  Include at least one register >=8 in every
field that can carry a high bit.

Malformed or out-of-range vector indices must produce `BROKEN_BYTECODE` or
select a proven portable fallback; they must never wrap into a low XMM
register.

### 6.5 Instruction-count gate

Capture native code only after branch patching.  Identify loop boundaries by
the recurrent direct packed load and the backward `jne`, not by hard-coded
addresses from an older build.

Report:

- full mnemonic dump;
- start/end native addresses;
- byte size;
- instruction count including latch;
- mnemonic histogram;
- Section 3.8 category counts; and
- explicit confirmation that no YMM/ZMM register appears.

The final x86_64 O3 blend loop must satisfy all of:

- 39–42 recurrent instructions;
- one source packed load;
- one destination packed load;
- one destination packed store;
- one source-alpha extraction;
- one `pix_a` computation;
- one `pix_inv_a` computation;
- three FMA3 instructions;
- zero two-address adaptation copies on the AVX path;
- one add and one backward conditional branch in the latch; and
- exactly four pixels processed per trip.

### 6.6 Performance report

After structural gates pass, repeat the existing blend benchmark with JIT
compilation excluded.  Record compiler revision, CPU, governor/load caveat,
sample count, best, median, and output checksum.  Compare C and Noct using the
same pixel count and call count.

Do not report an instruction-count ratio as a speedup ratio.

### 6.7 Regression suites

At minimum run:

~~~text
tests/run-simd.sh
tests/run-fma-helper.sh
tests/run-cli-options.sh
tests/run-cse.sh
tests/run-abce.sh
tests/run-typing.sh
tests/run-syntax.sh
tests/run-all.sh
~~~

Use the repository's configured build/test commands rather than inventing a
second build system.  If a suite is unavailable on one cross target, document
the exact compile-only or QEMU substitute.

---

## 7. File-by-file implementation checklist

### `src/core/lir.c`

- add the x86_64-specific logical budget of 13;
- preserve arm64=16 and other targets' current values;
- keep existing vector cache equality/eligibility rules;
- keep `lir_vfor_plan_fits()` authoritative;
- add developer-only plan/cache observability;
- verify cache materialization order; and
- do not add new vector arithmetic/load opcodes.

### `src/core/bytecode.h`

- correct the stale 0..7 comment to describe portable 0..15 storage and
  target native budgets;
- keep every x4 opcode's fixed 128-bit semantics; and
- do not renumber or add width variants.

### `src/core/jit.h`

- add `JIT_SIMD_CAP_AVX` without changing existing bit meanings;
- add the `avx` test ceiling;
- ensure `sse41` removes AVX/FMA;
- preserve the rule that ceilings remove but never add features; and
- make debug output distinguish AVX and FMA.

### `src/core/jit-x86_64.c`

- factor safe CPUID/XGETBV capability detection;
- add complete high-XMM legacy encoding helpers;
- add audited VEX.128 helpers;
- emit VEX ordinary vector operations when AVX is usable;
- retain legacy SSE lowering without AVX;
- map logical 0..12 to xmm0..xmm12;
- move SSE2 integer-multiply scratch to xmm13/xmm14;
- retain xmm15 opaque-alpha ownership;
- extend FMA3 to high registers;
- validate every vector operand before encoding;
- preserve the accepted VINDEX/PBASE/direct-addressing path;
- preserve the negative-index two-instruction latch; and
- preserve MSABI memory-canonical lowering.

### `src/core/jit-x86.c`

- do not increase its eight-register native budget;
- update shared capability/ceiling handling only as needed;
- ensure the new AVX bit cannot accidentally select unimplemented x86
  32-bit VEX lowering; and
- keep existing scalar/native correctness.

### Other JIT backends

- no native-code changes should be necessary;
- verify the shared AVX bit is ignored;
- ensure shared ceilings do not remove NEON/AltiVec unexpectedly; and
- do not introduce x8 operations.

### Portable interpreter/helpers/C backend

- no semantic opcode changes are expected;
- verify logical indices through 12 remain within `env->vreg[16]`;
- add a focused high-vreg test if current tests never exercise them; and
- do not widen helper storage or lane count.

### Tests

- add AVX-without-FMA ceiling coverage;
- add high-XMM operand-position coverage;
- assert four blend cache entries through developer debug output;
- preserve O2/O3 fused distinction;
- preserve forced scalar/SSE tiers; and
- add or document a reproducible post-patch native dump procedure.

### Documentation/reports after implementation

- update Design 17's status only if its own acceptance gates are still true;
- append before/after x86_64 dumps and counts to the blend benchmark report;
- record the GCC comparison commands and signed conversion requirement; and
- leave AVX2 x8 explicitly listed as future work.

---

## 8. Implementation phases and stop conditions

### Phase 0 — baseline

Capture tests, dumps, compiler versions, and current counts without editing
code.

Stop if the baseline is no longer 70 instructions: first explain and classify
the changed source state, then update this plan's measured table if warranted.

### Phase 1 — encoding infrastructure

Implement and oracle-test high-register legacy and VEX helpers.  Do not change
the LIR register budget yet.

Stop if high-bit encoding cannot be verified byte-for-byte.

### Phase 2 — AVX capability and VEX.128 ordinary operations

Add the independent capability, ceiling, and three-address lowering with the
existing eight-vreg plan.

Exit only when SSE and AVX ceiling tests both pass and disassembly contains no
YMM/ZMM operation.

### Phase 3 — x86_64 thirteen-vreg plan

Increase the target-specific budget, reserve xmm13..xmm15 as specified, and
prove high-register correctness.

Exit only when blend selects all four required cache entries and every high
operand-position test matches the interpreter.

### Phase 4 — recurrent-loop parity

Capture the final loop, categorize every instruction, and compare with freshly
generated GCC x4 loops.

If the count is above 42, do not proceed to optional optimizations.  Identify
whether the excess is a load/CSE failure, a remaining adaptation copy, an
encoder limitation, or packing overhead.

### Phase 5 — full regression and performance evidence

Run the complete matrix and update reports.  Performance work begins only
after correctness and code-shape gates pass.

### Optional Phase 6 — XMM `vpternlogd`

This phase requires separate approval or an explicit decision during review.
It must remain XMM/128-bit and may not introduce a wider loop.  Measure whether
one packing instruction is saved and whether the added capability complexity
is justified.

---

## 9. Review hazards and rejection checklist

Reject or revise an implementation that exhibits any of these:

- a YMM or ZMM instruction in the recurrent loop;
- L=1 in a VEX prefix;
- changing a four-lane loop step to eight or sixteen;
- enabling ordinary HIR CSE inside the vector-marked loop;
- adding a duplicate CSE pass instead of using `lir_vfor_cache_*`;
- adding a cached-load opcode;
- mapping a logical vreg onto xmm13, xmm14, or xmm15;
- retaining fixed xmm8/xmm9 scratch after logical vregs can own those
  registers;
- clobbering xmm15 between preheader materialization and opaque-alpha use;
- emitting VEX based only on CPUID AVX without OSXSAVE/XCR0 proof;
- treating FMA as synonymous with AVX;
- running XGETBV before checking OSXSAVE;
- losing a high register bit in REX, ModRM, or VEX;
- testing only low registers;
- calling a portable helper from the middle of a native-register loop;
- enabling native Win64 vector registers without ABI save/restore;
- changing O2 to fused arithmetic;
- using `-ffast-math` for the GCC reference;
- using unsigned float-to-int channel conversion in the GCC reference;
- counting preheader instructions for one compiler but not the other;
- comparing GCC's eight-pixel YMM loop directly with Noct's four-pixel XMM
  loop without normalization;
- claiming completion with more than 42 recurrent instructions without a
  reviewed blocker;
- accepting numerical output without inspecting native disassembly; or
- updating benchmark numbers without recording revisions and checksums.

---

## 10. Definition of done

The implementation described by this plan is complete only when every item
below is true:

- existing x4 opcodes still mean exactly four 32-bit lanes;
- Noct emits no x8/x16 vector loop in this work;
- AVX capability is detected independently and safely;
- the `avx` ceiling tests VEX without FMA;
- SysV x86_64 maps logical vregs 0..12 correctly;
- xmm13/xmm14/xmm15 ownership matches this plan;
- every vector opcode safely handles a high logical operand or chooses a
  whole-function portable fallback;
- AVX ordinary operations use three-address VEX.128 lowering;
- SSE-only machines retain correct legacy lowering;
- blend2 caches source load, destination load, `pix_a`, and `pix_inv_a`;
- blend2 performs one source load, one destination load, and one store per
  recurrent trip;
- O3 blend2 emits three native FMA3 operations on a capable SysV host;
- O2 emits no fused operation;
- the recurrent O3 loop contains 39–42 instructions and is within three of
  the freshly generated same-width GCC reference;
- the recurrent loop contains no YMM/ZMM register;
- interpreter, portable, SSE, AVX, FMA, MSABI build, x86-32 build, and arm64
  regression gates pass;
- final reports contain exact dumps, category counts, test commands,
  revisions, checksums, and timing methodology; and
- the patch contains no AVX2-width or unrelated changes.

Do not mark the work complete merely because all tests pass or because the
benchmark becomes faster.  The fixed-width code-shape and complete
high-register correctness gates are part of the required result.
