# 17 — x86_64 vector-loop parity, O3 FP32x4 FMA, and -O aliases

Status: **implementation plan; not yet implemented** (2026-08-11).

This document is the implementation handoff for three related changes:

1. make the recent vector-loop opcodes useful in the x86_64 JIT instead of
   merely decoding them;
2. add an architecture-neutral FP32x4 fused multiply-add bytecode operation,
   selected at optimize level 3 or higher and lowered natively on arm64 and
   x86_64; and
3. add the command-line aliases -O0, -O1, -O2, and -O3.

Read these documents before changing code:

- [00-overview.md](00-overview.md), especially the C89/OpenWatcom invariants
- [06-simd.md](06-simd.md)
- [08-jit-simd-portability.md](08-jit-simd-portability.md)
- [16-arm64-vector-loop-codegen-plan.md](16-arm64-vector-loop-codegen-plan.md)

The audited baseline is branch main at revision cc6eda2.  At that revision the
working tree was clean.  Do not assume line numbers in this plan are stable;
use the function and symbol names given below.

The work must remain reviewable.  Do not combine unrelated cleanup, a general
register allocator, scalar FMA, FP64 FMA, AVX2, or 256-bit vectors with this
change.

---

## 1. Required outcome

After implementation:

- existing optimize levels 0, 1, and 2 keep their current behavior;
- optimize level 3 and every future level greater than 3 may contract eligible
  vector FP32 multiply-plus-add expressions to one common bytecode opcode;
- OP_VFMAF32X4 has strict fused semantics: one IEEE binary32 rounding per lane;
- arm64 lowers that opcode to FMLA;
- x86_64 lowers it to 128-bit FMA3 only when CPUID and OS vector-state checks
  prove it is safe;
- a machine without native FMA still executes O3 bytecode correctly using
  fmaf() in the portable tier;
- x86_64 uses OP_VINDEX_HINT and OP_SUBJNZ to eliminate recurrent tmpvar
  index/base loads in an accepted vector strip loop;
- OP_VORI32X4I no longer constructs its constant on the stack on every x86_64
  vector iteration when the vector hint is accepted;
- -O0 through -O3 work in run, bytecode compile/app, and ANSI C translation
  modes; and
- tests distinguish O2's uncontracted result from O3's fused result.

For tests/simd/blend2.noct, the arm64 recurrent loop target is:

| Mode | Recurrent instructions | FP arithmetic |
|---|---:|---|
| O2 | 42 | 7 FMUL + 1 FSUB + 3 FADD |
| O3 | 39 | 4 FMUL + 1 FSUB + 3 FMLA |

The three saved instructions are the three channel expressions.  Each changes
from two multiplies plus one add to one multiply plus one fused multiply-add.

Performance is evidence, not the correctness gate.  Correct fused results,
portable fallback, and complete opcode decoding are mandatory.

---

## 2. Baseline facts that must not be rediscovered or accidentally undone

### 2.1 Optimization levels

The current HIR optimizer runs its optimization pipeline only at level 2 or
higher.  LIR emits line information only at level 0.  Consequently:

- O0: no HIR optimization, with line information;
- O1: no current HIR optimization, without O0 line information;
- O2: all current typing, ABCE, SIMD, CSE, and typed-op work; and
- O3 currently behaves exactly like O2.

The O3 work in this plan adds FP contraction after vectorization.  It does not
move or disable any O2 pass.

### 2.2 Existing vector-loop opcodes

src/core/bytecode.h already ends with:

~~~text
OP_VINDEX_HINT
OP_SUBJNZ
OP_VORI32X4I
~~~

x86_64 currently consumes all three opcodes, but:

- OP_VINDEX_HINT is a no-op;
- OP_SUBJNZ decrements the remaining tmpvar in memory and emits a patched JNE;
- OP_VORI32X4I constructs a 16-byte stack constant inside the recurrent loop.

arm64 already accepts the hint, keeps two packed cursors and the remaining
count in registers, suppresses the semantic OP_INC, and emits a two-word
SUBS/B.NE latch.

### 2.3 SIMD register state and helper calls

On SysV x86_64, logical vector registers 0..7 live directly in xmm0..xmm7
inside a native vector strip loop.  env->vreg is not kept synchronized.
arm64 has the same property for its native vector registers.

Therefore an unsupported OP_VFMAF32X4 must not call only the FMA helper while
the surrounding operations remain native.  The helper would read stale
env->vreg values and a call could clobber loop-live vector registers.

The required fallback is function-wide:

1. mark an LIR/runtime function that contains OP_VFMAF32X4;
2. before JIT emission, determine whether the backend has native support;
3. if it does not, disable native vector lowering for that entire function;
4. execute every vector opcode through the existing memory-canonical portable
   tier; and
5. execute OP_VFMAF32X4 through the fmaf() helper.

Do not replace a fused operation with VMULF32X4 followed by VADDF32X4.  That
would change the opcode's specified result.

### 2.4 ABI constraints

- SysV x86_64 owns xmm0..xmm7 for the existing plan and may use caller-saved
  xmm8..xmm15 as audited scratch/invariants inside a call-free accepted loop.
- Win64 currently uses the memory-canonical scalar/vector tier because
  xmm6/xmm7 are nonvolatile.  Preserve that policy.
- arm64 logical vector registers 8..15 map to v16..v23, avoiding callee-saved
  v8..v15.
- The accepted vector-loop region must remain helper-call free.

---

## 3. Authoritative design decisions

### D-FMA1: one common, strict fused opcode

Append this opcode after OP_VORI32X4I:

~~~text
OP_VFMAF32X4 vd:u8, va:u8, vb:u8, vc:u8
~~~

For every lane k:

~~~text
vd.f[k] = fmaf(va.f[k], vb.f[k], vc.f[k])
~~~

The bytecode is five bytes including the opcode.  It is architecture neutral.
It does not expose the destructive operand order of FMA3 or FMLA.

Do not insert it into the existing contiguous
OP_VLOADI32X4..OP_VCVTF32I32X4 helper-table range.  Appending it avoids
renumbering existing opcodes and avoids changing that table's three-operand
function type.

### D-FMA2: the destination aliases the addend in optimized LIR

The general bytecode permits all four register operands.  The vector LIR
scheduler should normally emit:

~~~text
OP_VFMAF32X4 dst, mul_lhs, mul_rhs, dst
~~~

This lets arm64 emit one FMLA and x86_64 emit one VFMADD231PS without a move.
Backends must still implement vd != vc correctly by copying vc to vd first.

The scheduler must not overwrite a multiplication operand while preparing the
addend.  If va or vb conflicts with dst and the values are not structurally
the same value, copy that operand to scratch or decline contraction for that
expression.

### D-FMA3: O2 remains strict and uncontracted

Only lir_optimize_level >= 3 may select OP_VFMAF32X4.

O2 must continue to emit the current VMULF32X4/VADDF32X4 sequence.  Do not use
the host compiler's FP contraction setting as the policy switch.  The bytecode
must explicitly record whether contraction was selected.

O3 is allowed to differ from O2 for rounding, signed zero, NaN payload
propagation, and floating-point exception behavior.  This numerical difference
is intentional and must be documented and tested.

### D-FMA4: first implementation has a deliberately small grammar

Contract only FP32x4 HIR_EXPR_PLUS patterns:

~~~text
(a * b) + c
c + (a * b)
(a * b) + (c * d)
~~~

Parentheses may be stripped and operands may already be resident cached values.
For the two-product form, emit one ordinary multiply and one FMA.

Do not initially contract:

- subtraction or negated multiplication;
- scalar float expressions;
- double/FP64 expressions;
- integer expressions;
- division;
- more general reassociation such as (a + b) * c;
- a multiply and add separated by assignments; or
- expressions whose register-preservation proof fails.

FMSUB/FNMSUB opcodes can be a later extension.  Do not encode them by changing
the meaning of OP_VFMAF32X4.

### D-FMA5: capability means usable native implementation

Add a target-neutral capability:

~~~text
JIT_SIMD_CAP_FMAF32X4
~~~

This bit means the current backend can safely lower OP_VFMAF32X4 natively.  It
is not merely a raw CPU feature bit.

- arm64 sets NEON | FMAF32X4 because AArch64 Advanced SIMD includes the needed
  FP32 vector FMLA operation;
- x86_64 sets FMAF32X4 only after the complete FMA3/AVX/OSXSAVE/XGETBV test;
- other backends leave it clear until they gain a reviewed native lowering.

This definition gives the common JIT setup enough information to choose the
function-wide portable fallback.

### D-FMA6: no 256-bit AVX in this change

x86_64 emits only VEX.128 FMA3.  Do not widen the loop to YMM registers.
VEX.128 writes clear the upper part of their destination, and this change does
not create dirty upper YMM state with 256-bit operations.  No vzeroupper is
required for code emitted by this phase.  Re-audit this decision before adding
any 256-bit operation.

### D-X64VL1: x86_64 hint acceptance is optional and transactional

OP_VINDEX_HINT remains a hint.  A rejected hint changes only performance.
Fallback bytecode semantics must remain executable.

The x86_64 pre-scan must reject a region unless it proves:

- SysV ABI native SSE2 lowering is active;
- the hint has VINDEX_CURSOR_ONLY;
- all vector loads/stores use the declared index tmpvar as a bare element
  index;
- at most two packed bases are used;
- the only index update is OP_INC index_tmp, lanes;
- the latch is OP_SUBJNZ remaining_tmp, lanes, body_lpc;
- the branch target is exactly the first body opcode;
- the region contains only the audited vector opcode whitelist;
- there is no helper call, return, break, or nested accepted hint; and
- any OP_VFMAF32X4 has native FMAF32X4 capability.

If any check fails, emit the current memory-based implementation.

### D-X64VL2: use a negative remaining index to retain a two-instruction latch

Two moving buffer cursors would require address increments in addition to the
latch.  Instead use three GPRs in the accepted SysV vector region:

| Register | Meaning |
|---|---|
| rbx | base 0 adjusted to vector_stop |
| rsi | base 1 adjusted to vector_stop |
| rdi | signed index = current_index - vector_stop |

The existing prologue already saves rbx, rsi, and rdi and moves the environment
to r14.  Re-audit every prologue, exception epilogue, and ordinary epilogue
before relying on this table.

At hint entry:

~~~text
rbx = base0 + vector_stop * 4
rsi = base1 + vector_stop * 4
rdi = start - vector_stop       # negative remaining element count
~~~

Every memory operation addresses:

~~~text
[adjusted_base + rdi * 4]
~~~

The recurrent latch is:

~~~asm
addq $4, %rdi
jne  vector_body
~~~

This has the same two-instruction control count as arm64.  It walks forward:
the first address is base + start*4 and the last is base +
(vector_stop-4)*4.

On fallthrough, outside the recurrent range:

- write stop_tmp to index_tmp when VINDEX_WRITEBACK_STOP is set;
- write zero to remaining_tmp, or prove and document that it is dead;
- clear the active hint state.

Use 64-bit sign extension for the negative index.  Loading a positive count
into edi and then using rdi without sign extension is incorrect because a
32-bit write zero-extends.

### D-X64VL3: do not keep PBASE registers live across arbitrary bytecode

Continue consuming the PBASE base_id operand.  Do not load rbx/rsi permanently
at OP_PBASE: arbitrary operations between PBASE and the loop may use or clobber
those registers, and current base-id assignment is intentionally small.

The accepted OP_VINDEX_HINT pre-scan discovers the actual base tmpvars used by
the loop and loads their pointer payloads into rbx/rsi at the loop boundary.
Thus PBASE hints remain portable and the hot loop still benefits.

### D-X64VL4: materialize one immediate OR invariant outside the loop

When an accepted x86_64 vector region contains one distinct OP_VORI32X4I
constant, reserve caller-saved xmm15 for that replicated constant.

At the hint/preheader, emit once:

~~~text
mov imm32, eax
movd eax, xmm15
pshufd $0, xmm15, xmm15
~~~

Inside the loop, OP_VORI32X4I becomes an optional register copy followed by:

~~~asm
por %xmm15, %xmmDst
~~~

The existing SSE2 i32 multiply lowering uses xmm8/xmm9, so do not reuse those
as the invariant.  If the region needs multiple distinct immediate constants,
or the hint is rejected, retain the current stack-constant fallback.

### D-CLI1: the canonical long option remains unchanged

The existing option is --optimize-level=N, not --optimizer-level=N.  Do not add
the misspelled long form.

Add exact aliases:

~~~text
-O0  == --optimize-level=0
-O1  == --optimize-level=1
-O2  == --optimize-level=2
-O3  == --optimize-level=3
~~~

The long form accepts a non-negative decimal int.  Values greater than 3
currently receive O3 behavior because all FMA policy checks use level >= 3.
This preserves the requested "3 or higher" rule and leaves room for future
levels.

Only the four requested short aliases are valid in this phase.  Reject -O,
-O4, suffixes such as -O2foo, negative long values, nondecimal input, and
overflow.  Multiple valid options are processed left-to-right; the last one
wins.

---

## 4. CLI implementation

### 4.1 Add one shared parser

Do not duplicate another atoi() parser in three files.  Add a helper declared
in src/cli/cli-main.h and implemented in src/cli/cli-main.c, for example:

~~~c
enum cli_opt_level_result {
	CLI_OPT_LEVEL_NOT_MATCHED,
	CLI_OPT_LEVEL_VALID,
	CLI_OPT_LEVEL_INVALID
};

enum cli_opt_level_result
parse_optimize_level_option(const char *arg, int *level);
~~~

Required behavior:

- exact -O0, -O1, -O2, -O3 return VALID;
- a --optimize-level= prefix with no digits returns INVALID;
- parse the long value with strtol(), reset and check errno, require complete
  consumption, require 0 <= value <= INT_MAX;
- a different option returns NOT_MATCHED; and
- never silently turn malformed text into zero.

Use the helper in:

- src/cli/cli-run.c;
- src/cli/cli-compile.c; and
- src/cli/cli-ctrans.c.

In compile mode it must work with --compile and --compile --app in the same
option position where the long form currently works.  In C translation mode,
the user-visible command is currently --ansic; preserve its current argument
ordering.

Do not expand Elisp/Scheme backends in this change because they do not
currently expose the long optimize option.

### 4.2 Help and translations

Update src/cli/cli-main.c usage to show:

~~~text
-O0..-O3, --optimize-level=N ... optimize level (0/1/2/3+)
~~~

Search src/i18n/translation.c for stale optimize-level text.  Update every
language entry consistently.  Do not change unrelated translations.

### 4.3 CLI tests

Add a focused script, preferably tests/run-cli-options.sh, and include it in
tests/run-all.sh.

It must cover:

- run mode with every short alias;
- equivalence of -O2 and --optimize-level=2;
- O3 and a long level greater than 3 taking the FMA-enabled policy;
- --compile with -O2;
- --compile --app with -O3;
- --ansic with -O3;
- last-option-wins ordering; and
- failure for -O, -O4, -O2foo, --optimize-level=,
  --optimize-level=-1, --optimize-level=x, and decimal overflow.

Check both nonzero exit status and a stable diagnostic for invalid options.

---

## 5. Bytecode, metadata, and portable semantics

### 5.1 Add the opcode without changing the contiguous vector table

In src/core/bytecode.h append:

~~~c
OP_VFMAF32X4,	/* vd = fmaf(va, vb, vc), lane-wise; imm8 x4 */
~~~

Update the bytecode comment that still claims all vector registers are 0..7:
portable storage has 16 slots, while each backend validates the logical range
it supports.

Every bytecode-size scanner must know that OP_VFMAF32X4 occupies five bytes.
In particular update the arm64 vector-base scanner and the new x86_64 scanner.

### 5.2 Track functions that require fused native support

Add a bool named has_fma_ops (or requires_fma; choose one spelling and use it
everywhere) to:

- struct lir_func in src/core/lir.h;
- struct rt_func in src/core/runtime.h; and
- any temporary LIR/runtime initialization structure that copies these fields.

In lir_put_opcode(), set both has_vector_ops and has_fma_ops when the new
opcode is emitted.  Reset the LIR builder's static flag at the start of every
function.

Propagate the flag in src/core/runtime.c when registering freshly generated
LIR.

For persisted .nb/.nap files, add an optional metadata section after
Vector Ops:

~~~text
FMA Ops
1
~~~

Update:

- src/backend/bcback.c to write it only when true; and
- src/core/runtime.c to parse it while leaving old bytecode default false.

The bytecode format is internal and trusted, but old bytecode without the
optional section must continue to load.

### 5.3 Portable helper ABI

The existing helper type accepts env plus three int arguments.  Preserve that
ABI by packing vb and vc for the portable helper:

~~~c
bool noct_ex_vfmaf32x4_helper(
	NoctEnv *env,
	int vd,
	int va,
	int packed_vb_vc);
~~~

Use:

~~~text
packed_vb_vc = (vb << 8) | vc
vb = (packed_vb_vc >> 8) & 0xff
vc = packed_vb_vc & 0xff
~~~

The bytecode itself still carries four independent imm8 operands.  Packing is
only a helper-call ABI detail.

Implement the helper in src/core/execution.c:

1. call a reviewed noct_fmaf32() wrapper described below;
2. copy va, vb, and vc lanes to local unions before writing vd, so every alias
   combination is correct;
3. calculate each lane with strict fused semantics, not a*b+c;
4. copy the result to env->vreg[vd]; and
5. validate or consistently mask register indices in the same manner as the
   existing vector helpers.

Add declarations/macros in:

- include/noct/aot.h;
- src/core/interpreter.h; and
- src/core/jit.h.

The core is C89 and must still build with OpenWatcom 1.9.  The audited
OpenWatcom headers do not declare fmaf().  Therefore provide two
implementations behind one internal noct_fmaf32() interface:

1. when configure-time or compiler/platform checks prove a correctly linked
   system fmaf() exists, call it; and
2. otherwise use a C89-compatible software binary32 fused multiply-add.

The software implementation must be correctly rounded, including cancellation,
subnormals, overflow, signed zero, infinities, and NaNs.  A plain
(float)((double)a * (double)b + (double)c) is not acceptable: rare double
rounding cases differ from fmaf().  Prefer adapting a small, proven
permissively licensed implementation and retain its attribution/license.
If implementing from first principles, decode IEEE binary32 sign/exponent/
significand fields, form the exact 48-bit product, align the addend with
guard/round/sticky information, add/subtract by sign, normalize, and perform
round-to-nearest-even.  The algorithm must use only C89 syntax and integer
types available to OpenWatcom; do not assume __int128.

Keep this wrapper internal unless another feature needs it.  It may live in
execution.c or a small src/core/fp32.c/fp32.h pair.  If a new source file is
added, update every build-system source list and dependency path, including
CMake and non-CMake targets.

UNIX CLI/tests already link libm, but verify static-library consumers and the C
backend test link lines.  Windows CRT builds must exercise the selected system
or software path.  The MS-DOS/OpenWatcom build must select the software path.
Do not enable compiler-wide fast-math.

### 5.4 Interpreter and C backend

Because the opcode is outside the contiguous vector helper table:

- add a dedicated interpreter visitor that consumes four imm8 operands,
  packs vb/vc, and calls the helper;
- add a dedicated OP_VFMAF32X4 switch case in rt_visit_bytecode();
- add a dedicated C backend visitor in src/backend/cback.c; and
- emit a call to noct_ex_vfmaf32x4_helper() with the packed final argument.

Do not extend rt_typed_helper_t to five C arguments; that would touch every
existing vector table entry unnecessarily.

### 5.5 Every JIT must decode the opcode before LIR can emit it

Add a dedicated visitor and main-switch case to all architecture files:

- src/core/jit-x86.c;
- src/core/jit-x86_64.c;
- src/core/jit-arm32.c;
- src/core/jit-arm64.c;
- src/core/jit-mips32.c and jit-mips64.c;
- src/core/jit-ppc32.c and jit-ppc64.c; and
- src/core/jit-riscv32.c and jit-riscv64.c.

For backends without native FMAF32X4, the function-wide capability policy must
select their memory-canonical vector tier before any vector opcode is emitted.
Their OP_VFMAF32X4 visitor then calls the portable helper with packed vb/vc.

An unknown/default opcode assertion on any configured target is a release
blocker.

### 5.6 Common JIT fallback policy

In jit_configure_simd(), after applying NOCT_JIT_SIMD_MAX:

~~~text
if func.has_fma_ops and FMAF32X4 capability is absent:
    force the function's vector implementation to memory-canonical mode
~~~

This may be represented by a separate vector_scalar_only field or by clearing
the vector capability bits.  A separate field gives clearer debug output;
clearing caps requires fewer backend changes.  Choose one representation, but
the observable rule is mandatory.

Extend NOCT_JIT_SIMD_DEBUG output so a test can distinguish:

- native-fma;
- no-fma/function-wide-portable; and
- explicit scalar ceiling.

Update jit_apply_simd_max():

- scalar removes every vector capability;
- sse2/sse3/sse41 do not retain FMAF32X4;
- neon retains FMAF32X4 only if the backend detected both;
- altivec does not gain FMA implicitly; and
- add a test ceiling named fma that leaves detected prerequisite SIMD bits and
  FMAF32X4 available.  A ceiling may remove a detected feature, never add it.

---

## 6. O3 contraction in vector LIR

### 6.1 Placement

Implement the first contraction in src/core/lir.c, next to
lir_vfor_expr() and lir_vfor_scratch_need().  A new general HIR expression
kind is not required for this phase.

The reason for LIR placement is deliberate:

- vectorization has already proved an FP32x4 strip loop;
- the vector planner knows resident constants, invariant locals, temp homes,
  cached values, and target vreg budget; and
- the common bytecode opcode is the portable representation.

Do not teach ordinary scalar expression lowering to emit this vector opcode.

### 6.2 Use one matcher shared by sizing and emission

Add a small description structure, for example:

~~~c
struct vfor_fma_match {
	struct hir_expr *first_product;
	struct hir_expr *fused_lhs;
	struct hir_expr *fused_rhs;
	struct hir_expr *addend;
	bool two_products;
};
~~~

Add one pure matcher that:

1. strips HIR_EXPR_PAR;
2. requires lir_optimize_level >= 3;
3. requires HIR_EXPR_PLUS with proven float result;
4. finds exactly one or two HIR_EXPR_MUL children; and
5. returns false for every pattern outside D-FMA4.

Both scratch sizing and emission must call the same matcher.  Do not implement
slightly different pattern recognition twice.

### 6.3 Materialize operands without clobbering the addend

Add a helper conceptually equivalent to:

~~~text
materialize_operand(expr, first_free_slot) -> {reg, next_free_slot}
~~~

If the expression is already a term/cache value, return its resident register
without consuming a slot.  Otherwise evaluate it into first_free_slot using
the existing recursive emitter and advance by:

~~~text
1 + lir_vfor_scratch_need(expr)
~~~

The conservative algorithm is:

For one product plus an addend:

1. evaluate the addend into dst;
2. materialize multiplication lhs after sp while preserving dst;
3. materialize multiplication rhs after the lhs's occupied span;
4. ensure neither materialization overwrote dst;
5. emit OP_VFMAF32X4 dst, lhs_reg, rhs_reg, dst.

For two products:

1. preserve source order: evaluate the left product into dst with ordinary
   VMULF32X4;
2. materialize both factors of the right product while preserving dst;
3. do not swap the two products merely to reduce register pressure;
4. emit OP_VFMAF32X4 dst, lhs_reg, rhs_reg, dst.

If this source-order-preserving schedule exceeds VFOR_VREG_MAX, decline
contraction for that expression and use existing lowering.  Do not reject
vectorization solely because FMA contraction does not fit.  The eligible
vector grammar is side-effect free, but preserving source order avoids adding
an unnecessary NaN/FP-exception ordering change beyond the explicitly allowed
fused rounding.

### 6.4 Scratch-need formula must mirror emission

The scratch calculation is part of correctness, not only performance.

For each nonresident materialized operand, reserve its destination slot plus
its recursive scratch need.  While the second multiplication operand is being
built, the first operand and the addend remain live.  Include both in the peak.

Update lir_vfor_plan_fits() through lir_vfor_scratch_need(); do not add a
separate unchecked assumption in the emitter.  Add assertions in debug builds
for:

- every selected register is within the target VFOR_VREG_MAX;
- dst is still the addend at emission;
- multiplication source values are live; and
- a failed contraction leaves no partially emitted bytecode.

If transactional emission is difficult, complete all matching and sizing
before writing any bytes.

### 6.5 Interaction with cache/CSE

Use lir_vfor_value_vreg() before materializing an operand.  This preserves the
current vector-local cache and lets blend reuse pix_a and pix_inv_a.

Do not alter structural equality to consider fused and unfused arithmetic
equal.  A fused expression has different FP semantics and must not reuse an O2
unfused cached result.

No stable --simd-info text change is required.  Developer-only
NOCT_SIMD_DEBUG output may add a contracted_fma count for automated tests.

### 6.6 LIR dump

Add a dedicated lir_dump() case:

~~~text
VFMAF32X4(vd:N, va:N, vb:N, vc:N)
~~~

Keep the new opcode outside the vec_name array indexed by the old contiguous
range.

---

## 7. arm64 native lowering

### 7.1 Decode and register validation

Add a dedicated jit_visit_vfmaf32x4_op() or extend the vector visitor with a
four-operand path.  A dedicated visitor is less likely to break the existing
three-operand decode.

Consume vd, va, vb, vc as imm8 and map all four through jit_arm64_vreg().
Reject broken bytecode if any logical register cannot be mapped.

Add OP_VFMAF32X4 size 5 to jit_arm64_scan_vector_bases().  It is an allowed,
call-free body opcode only when FMAF32X4 capability is active.

### 7.2 Instruction selection

If vd != vc, first copy vc to vd using the existing 128-bit VMOV/ORR form.
Then emit:

~~~asm
fmla vD.4s, vA.4s, vB.4s
~~~

The expected base encoding for the vector form is 0x4e20cc00 with vm in bits
20..16, vn in bits 9..5, and vd in bits 4..0.  Do not trust this sentence
alone: assemble representative instructions with Apple Clang or GNU
assembler, disassemble them, and add a byte-for-byte encoder test.

When the LIR invariant vd == vc holds, exactly one word must be emitted.

### 7.3 arm64 acceptance

On the M5 machine:

1. build current source natively;
2. run the full SIMD suite at O0, O2, and O3;
3. capture the branch-patched blend2 loop;
4. verify O2 remains 42 recurrent instructions;
5. verify O3 is 39 recurrent instructions;
6. verify exactly three FMLA instructions in the O3 recurrent body;
7. verify the O3 output against portable fmaf semantics; and
8. rerun the 50-sample/100-call benchmark excluding JIT time.

Do not compare O3's repeated-blend hash to the O2/-ffp-contract=off hash.
Compare it to a C reference that explicitly calls fmaf() or to contracting
Clang with the operation grouping verified in assembly.

---

## 8. x86_64 FMA3 lowering

### 8.1 Safe feature detection

Extend jit_detect_simd_caps() in src/core/jit-x86_64.c.

Native 128-bit FMA3 is usable only if all of these are true:

- CPUID leaf 1 ECX bit 12: FMA;
- CPUID leaf 1 ECX bit 28: AVX;
- CPUID leaf 1 ECX bit 27: OSXSAVE;
- optionally check CPUID leaf 1 ECX bit 26: XSAVE for defensive clarity; and
- XGETBV(0) has both XCR0 bit 1 (XMM state) and bit 2 (YMM state) set.

Never execute XGETBV unless OSXSAVE is set.

For GCC/Clang, use a reviewed inline-assembly helper for xgetbv with EAX/EDX
outputs.  For MSVC use _xgetbv(0) from intrin.h.  Preserve the existing
SSE2/SSE3/SSE4.1 detection.

Set JIT_SIMD_CAP_FMAF32X4 only after all checks succeed.

Although the operation is 128-bit, FMA3 uses VEX encoding and therefore
requires AVX OS state support.

### 8.2 Native SysV register lowering

For vd != vc, copy xmmVc to xmmVd.  Then encode:

~~~asm
vfmadd231ps xmmVd, xmmVa, xmmVb
~~~

with semantics:

~~~text
xmmVd = xmmVa * xmmVb + old(xmmVd)
~~~

For logical vector registers 0..7, the expected VEX encoding is:

~~~text
C4
E2
((~va & 0x0f) << 3) | 0x01
B8
0xC0 | (vd << 3) | vb
~~~

This is VEX.NDS.128.66.0F38.W0 opcode B8 /r.  Verify the byte sequence with
objdump/llvm-objdump for several nonzero register combinations.  Add a test
that catches swapped va/vb/addend fields with noncommutative test data at the
whole expression level; multiplication itself is commutative, but the old
destination/addend position is not.

Do not emit FMA3 when only the raw CPUID FMA bit is present.

### 8.3 Win64 memory-canonical lowering

Win64 keeps existing vector values in env->vreg.  When native FMA is available,
it may still lower this one operation with volatile registers:

1. load vc lanes into xmm0;
2. load va lanes into xmm1;
3. use vb as a memory operand or load it into another volatile XMM register;
4. execute VFMADD231PS;
5. store xmm0 to vd.

Use only Win64 volatile xmm0..xmm5.  If this path is not implemented in the
first patch, the complete function must use the portable tier; do not claim
x86_64 Windows native FMA support until the ABI-safe path exists.

### 8.4 No-FMA behavior

With NOCT_JIT_SIMD_MAX=sse41 on an FMA-containing function:

- no VEX FMA instruction may be emitted;
- the function-wide vector tier must be memory canonical;
- OP_VFMAF32X4 must call the fmaf() helper; and
- output must match the native-FMA execution bit-for-bit for finite lanes.

This test is mandatory even on an FMA-capable development CPU.

---

## 9. x86_64 vector-loop hint implementation

This phase handles the user's request for the recent loop opcodes independently
of FMA.  Land and test it separately if practical.

### 9.1 Add x86_64 hint state

Reuse the target-neutral fields already present in struct jit_context and add
x86_64-specific state only if required:

- accepted/rejected flag;
- index, stop, and remaining tmpvars;
- lanes and flags;
- up to two base tmpvars;
- last memory-use LPC per base;
- body LPC and latch LPC;
- optional VORI immediate and invariant-valid flag.

Reset every field at function start and when the accepted latch falls through.
One malformed/rejected region must not leak state into a later loop.

### 9.2 Pre-scan with an explicit opcode-size decoder

Implement an x86_64 scanner analogous to jit_arm64_scan_vector_bases(), but
make its validation match D-X64VL1 rather than copying only the current arm64
minimum.

Recognize exact sizes for:

- vector load/store/splat/getlane/move;
- all three-operand vector ALU/conversion operations;
- OP_VORI32X4I;
- OP_VFMAF32X4;
- OP_INC;
- OP_SUBJNZ.

Reject unknown opcodes and out-of-range operand reads before accessing bytes.
The bytecode is trusted at the file boundary, but the JIT must not read beyond
func->bytecode_size.

Record the actual base tmpvar operands from vector loads/stores.  Do not infer
them only from PBASE base_id.

### 9.3 Preheader setup

For an accepted, nonzero vector trip:

1. load stop_tmp as a signed 64-bit element index;
2. load each raw packed base pointer from its PBASE-derived tmpvar;
3. form adjusted_base = raw_base + stop*4 in rbx/rsi;
4. load remaining_tmp with sign extension into rdi;
5. negate rdi so it equals start-stop; and
6. materialize the optional OP_VORI32X4I invariant into xmm15.

All of these instructions are outside the recurrent loop address.

### 9.4 Direct memory operations

For an accepted base:

~~~asm
movdqu (%rbx,%rdi,4), %xmmN
movdqu (%rsi,%rdi,4), %xmmN
movdqu %xmmN, (%rbx,%rdi,4)
~~~

Choose rbx/rsi according to the scanner's base table.  Do not reload the base
or semantic index from r15 in the recurrent loop.

Unaccepted bases and unaccepted hints retain the current tmpvar-based address
sequence.

### 9.5 Suppress only the matching semantic increment

When the hint is active, suppress native emission for exactly:

~~~text
OP_INC index_tmp, lanes
~~~

Any other OP_INC must be emitted normally.  The bytecode remains necessary for
interpreter and fallback JIT semantics.

### 9.6 Native hinted latch

For the matching OP_SUBJNZ:

~~~asm
addq $lanes, %rdi
jne body
~~~

The add sets flags.  For a known backward target:

- use rel8 JNE if it fits and long-branch forcing is disabled;
- otherwise use rel32 JNE;
- validate the rel32 range even though normal JIT region limits should fit.

Extend the existing forced-long-branch test to exercise the hinted SUBJNZ path.

After the branch fallthrough, perform tmpvar writebacks outside the loop and
clear hint state.

### 9.7 x86_64 acceptance checks

For blend2 O2 and O3 on SysV x86_64, recurrent disassembly must show:

- one destination vector load;
- one source vector load;
- one destination vector store;
- no base or index load from r15;
- no address-generation instruction other than SIB addressing;
- one addq $4 to the negative index and one conditional back edge;
- no recurrent stack allocation for OP_VORI32X4I;
- O2 has no FMA instruction;
- O3 has three VFMADD instructions when FMA is usable; and
- an sse41 ceiling executes the portable fused fallback correctly.

Do not set a total x86_64 instruction-count gate until the first audited dump
is added to the benchmark report.  x86 instruction selection differs from
arm64 for shifts, conversions, and immediate vector constants.

---

## 10. Numerical and opcode tests

### 10.1 Direct helper test

Add a C-level SIMD helper test that writes all input lanes, calls
noct_ex_vfmaf32x4_helper(), and compares output bits with direct fmaf().

Include:

- ordinary positive and negative values;
- zero and signed zero;
- subnormal input/output;
- infinity;
- NaN, checking classification where payload is not portable;
- complete alias cases vd==va, vd==vb, vd==vc, and all equal; and
- a value that distinguishes fused from separate rounding.

Use this exact distinguishing construction in C:

~~~c
a = 0x1.000002p0f;   /* 1 + 2^-23 */
b = 0x1.000002p0f;
c = -0x1.000004p0f;  /* -(1 + 2^-22) */
~~~

The exact product contains a 2^-46 residue.  fmaf(a,b,c) retains it, while a
separately rounded volatile product followed by addition becomes zero.

Compile this test without fast-math.  Use a volatile temporary for the
unfused reference so the test compiler cannot contract it.

Run the same vector corpus directly against the software noct_fmaf32 path,
even on hosts that have system fmaf().  On a host with trusted system fmaf(),
add a deterministic randomized comparison over normal, subnormal, zero,
infinity, and NaN bit patterns.  Compare result bits for non-NaNs and compare
classification/sign rules for NaNs according to the chosen documented policy.

### 10.2 O2 versus O3 emission test

Add a small vectorizable Noct program containing both:

~~~text
a*b + c
a*b + c*d
~~~

At O2:

- it vectorizes;
- no OP_VFMAF32X4 is reported in developer LIR diagnostics; and
- its distinguishing result matches separate multiply/add.

At O3 and long level 4:

- it vectorizes;
- the developer diagnostic reports the expected FMA count; and
- its distinguishing result matches fmaf().

Do not require O3 to have the same golden output as O2.  Extend
tests/run-simd.sh to select an .out3 golden where present, analogous to its
current .out2 support.

### 10.3 Runtime matrix

Run O3 cases through:

- interpreter;
- forced JIT native SIMD;
- NOCT_JIT_SIMD_MAX=scalar;
- x86_64 sse2, sse3, and sse41 ceilings;
- x86_64 fma ceiling;
- bytecode compile/load round trip;
- ANSI C backend compile/run; and
- qemu-user targets covered by tests/run-simd-qemu.sh.

Native arm64 M5 and native x86_64 are required for instruction selection.
qemu-user functional results are sufficient for other architectures.

### 10.4 Capability tests

On x86_64:

- debug output must report FMA only when CPUID plus XGETBV allow it;
- sse41 ceiling must strip FMA;
- scalar ceiling must strip all native vector code;
- no test may execute XGETBV after a mocked/forced OSXSAVE failure; and
- native and portable FMA results must match.

Where practical, isolate the CPUID/XGETBV decision into a pure helper taking
feature words and xcr0 so unit tests can cover impossible/partial combinations
without changing host CPU state.

### 10.5 Regression matrix

Before declaring completion run:

- tests/run-syntax.sh;
- tests/run-typing.sh;
- tests/run-abce.sh;
- tests/run-cse.sh;
- tests/run-typedop.sh;
- tests/run-simd.sh;
- tests/run-ctrans.sh;
- tests/run-app.sh;
- tests/run-cli-options.sh;
- tests/run-jit-long-branch.sh; and
- tests/run-all.sh when the configured build supports it.

Also build every architecture-forced JIT translation unit so a missing opcode
visitor cannot hide behind the host preprocessor.

Build at least the repository's Linux GCC, MinGW x86_64, and
MS-DOS/OpenWatcom configurations.  The OpenWatcom build is specifically a gate
for the C89 software FMA path.

---

## 11. File-by-file implementation checklist

The implementing model must search for every existing OP_VORI32X4I switch case
and use it as a checklist for the new standalone opcode.  At minimum inspect:

| File | Required work |
|---|---|
| src/cli/cli-main.[ch] | shared strict parser, help |
| src/cli/cli-run.c | short/long option use |
| src/cli/cli-compile.c | compile/app option use |
| src/cli/cli-ctrans.c | ANSI C option use |
| src/i18n/translation.c | help translations |
| src/core/bytecode.h | append opcode/capability comments |
| src/core/lir.[ch] | FMA metadata, matching, sizing, emission, dump |
| src/core/execution.c | strict fmaf helper |
| src/core/fp32.[ch] (optional) | C89 system/software fused wrapper |
| CMake and legacy build inputs | add fp32 source and fmaf capability check if needed |
| include/noct/aot.h | helper declaration |
| src/core/interpreter.[ch] | standalone decode/helper mapping |
| src/core/runtime.[ch] | FMA metadata propagation/loading |
| src/backend/bcback.c | FMA metadata serialization |
| src/backend/cback.c | standalone opcode emission |
| src/core/jit.h | capability, helper alias, fallback policy/state |
| src/core/jit-arm64.c | FMLA and scanner size |
| src/core/jit-x86_64.c | FMA detection/encoding and hint optimization |
| every other jit-*.c | decode and portable fallback |
| tests/run-simd.sh | O3 golden/tier matrix |
| tests/run-simd-qemu.sh | O3 portable coverage |
| tests/run-cli-options.sh | aliases and validation |
| tests/run-all.sh | new CLI test hook |
| docs/design/06-simd.md | O2/O3 FP policy |
| docs/design/08-jit-simd-portability.md | FMA capability/fallback |
| docs/design/16-arm64-vector-loop-codegen-plan.md | successor note |
| docs benchmark report | new dumps and measurements |

After edits, run a final repository search for:

~~~text
OP_VORI32X4I
OP_VCVTF32I32X4
has_vector_ops
--optimize-level
JIT_SIMD_CAP_
~~~

Every table, scanner, switch, serializer, and help string adjacent to those
symbols must be consciously accepted or updated.

---

## 12. Implementation phases and stop conditions

### Phase 0 — preserve a baseline

1. Record git status and revision.
2. Build the current tree.
3. Run current native x86_64 SIMD/CLI tests.
4. Retain the known arm64 O2 42-instruction dump and benchmark data.

Stop if baseline tests already fail for reasons related to these files.  Do
not hide a pre-existing failure by changing goldens.

### Phase 1 — CLI aliases and validation

Implement Section 4 only.

Exit criteria:

- all three command paths use one parser;
- aliases work;
- malformed values fail;
- long level >=3 is accepted; and
- no optimization behavior changes yet.

### Phase 2 — common opcode and portable execution

Add opcode, metadata, helper, interpreter, C backend, and all JIT fallback
decoders.  Do not emit the opcode from LIR yet.

Exit criteria:

- hand-constructed/helper tests prove strict fused semantics;
- old bytecode loads;
- new bytecode metadata round-trips;
- every target builds; and
- no existing program emits the new opcode.

### Phase 3 — x86_64 current-op parity

Implement accepted VINDEX/SUBJNZ/addressing and preheader VORI invariant without
FMA contraction.

Exit criteria:

- O2 output is unchanged;
- accepted blend loop has direct SIB memory operands and a two-instruction
  latch;
- rejected hints execute existing fallback; and
- forced long branches work.

### Phase 4 — O3 LIR selection

Implement the shared FMA matcher, scratch sizing, and transactional emission.

Exit criteria:

- O2 emits zero FMA opcodes;
- O3 emits expected FMA opcodes;
- a contraction that exceeds the register budget falls back to unfused vector
  lowering rather than failing compilation; and
- interpreter/C backend/portable JIT results match fmaf().

### Phase 5 — arm64 native FMLA

Implement Section 7 and validate the M5 dump.

Exit criteria:

- O2 remains 42 recurrent instructions;
- O3 reaches 39;
- three native FMLA instructions are present; and
- numerical tests pass in native and scalar ceilings.

### Phase 6 — x86_64 native FMA3

Implement complete capability detection, SysV FMA3, and the reviewed Win64
choice.

Exit criteria:

- native FMA is never emitted without the complete feature/OS proof;
- native and portable results match;
- sse41 ceiling forces the whole-function portable vector tier;
- O3 blend contains three native VFMADD instructions on capable SysV hosts;
  and
- O2 contains none.

### Phase 7 — full regression and reports

Run Section 10, capture both architecture dumps, and update design/benchmark
documents.

Exit criteria:

- no unknown opcode path remains;
- every required test passes;
- the exact source revisions and compiler versions are recorded;
- performance numbers clearly separate O2 strict and O3 fused modes; and
- the working tree contains only intentional review changes.

---

## 13. Review hazards

Reject or revise an implementation exhibiting any of these:

- using a*b+c in the portable helper instead of fmaf();
- assuming C99 fmaf() exists on OpenWatcom, or using a double expression as a
  supposedly exact software substitute;
- emitting FMA at O2;
- deciding whether to emit the common opcode from the build host's CPUID;
- calling only the FMA helper from inside a native-register vector loop;
- executing XGETBV without first checking OSXSAVE;
- treating CPUID FMA alone as usable FMA3;
- placing the four-operand opcode in a three-argument contiguous helper table;
- changing vector scratch sizing without mirroring the emitter;
- partially emitting an FMA schedule and then falling back;
- keeping x86 PBASE pointers live across arbitrary non-vector bytecode;
- using a zero-extended negative x86 index;
- using Win64 nonvolatile xmm6/xmm7 without save/restore;
- silently accepting malformed optimize options via atoi();
- changing O2 numerical goldens to O3 results;
- forgetting bytecode metadata serialization/loading;
- forgetting an architecture JIT switch or byte-size scanner; or
- reporting an instruction-count improvement without branch-patched native
  disassembly and output verification.

---

## 14. Definition of done

This plan is complete only when all statements below are true:

- -O0, -O1, -O2, and -O3 are documented, parsed consistently, and tested;
- --optimize-level=N is strictly parsed and level >=3 enables contraction;
- OP_VFMAF32X4 is common, persisted, dumped, and decoded everywhere;
- the portable result uses fmaf() and survives every alias combination;
- unsupported JITs choose function-wide memory-canonical fallback;
- arm64 emits three FMLA operations for blend2 O3 and retains the O2 shape;
- capable x86_64 emits FMA3 only after CPUID/XGETBV approval;
- x86_64 accepted loops use direct register-indexed memory operands and a
  two-instruction latch;
- O2 and O3 numerical behavior is intentionally distinguished by tests;
- native, forced-scalar, interpreter, bytecode, and C backend tests pass; and
- updated reports contain the x86_64 and M5 instruction dumps and measurements.

Do not mark the implementation complete merely because blend2 becomes faster.
Portable correctness and complete cross-backend decoding are the release
criteria.
