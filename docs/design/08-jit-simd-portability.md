# 08 — Portable SIMD JIT Tiers and Long Branches

Status: **implemented and functionally validated** (2026-08-10).
Implementation proceeded in dependency order: long branches, portable IR,
capability plumbing, x86, ARM, PowerPC, then the lower x86 SIMD tier.  This
document extends
[06-simd.md](06-simd.md); its bytecode semantics and 8-vreg budget remain
normative.

The Global Invariants in [00-overview.md](00-overview.md) apply: C89 core,
all opcodes on all backends, portable bytecode, explicit runtime feature
checks, no SIGILL probes, and byte-identical results at every optimization
level.  Local commits are allowed for this work; **do not push**.

## 1. Decisions

- D-JSP1. A vector opcode always has the scalar semantics defined in design
  06.  A JIT chooses an implementation tier at runtime; bytecode never
  encodes a host ISA requirement.
- D-JSP2. If a required host feature is absent, JIT compilation emits
  ordinary scalar host instructions over `env->vreg`; it does not emit an
  unsupported SIMD instruction.  The portable C helpers remain the
  interpreter/reference implementation.  This keeps portable bytecode
  executable on every backend without turning each vector opcode into a C
  call from generated code.
- D-JSP3. Feature detection is non-trapping.  x86 uses CPUID; Linux ARM and
  PowerPC use `AT_HWCAP`; an ABI-mandatory feature may be reported as such.
  An unknown OS/feature combination selects scalar.  Deliberately executing
  an instruction and catching SIGILL is forbidden (unsafe in MT builds).
- D-JSP4. Every SIMD-capable backend has a test-only maximum-tier override.
  It may lower the detected tier but never raise it.  This makes scalar and
  old-CPU paths testable on new machines.
- D-JSP5. x86 tiers are `SCALAR`, `SSE2`, and `SSE41`.  The current fast path
  uses SSE4.1, not SSE4.2.  SSE3 adds no instruction needed by the i32x4
  opcode set; an SSE3-only machine therefore uses the SSE2 tier.
- D-JSP6. Win64 uses the direct scalar tier and touches only volatile GPRs and
  xmm0, so it does not hold vector state in nonvolatile xmm6/xmm7 and needs no
  XMM prologue save.  SysV x86_64 and all x86-32 ABIs supported here treat
  xmm0..7 as volatile.
- D-JSP7. ARMv7 maps the eight program vregs to q8..q15 (d16..d31), avoiding
  the AAPCS callee-saved d8..d15 range.  ARM64 keeps v0..v7.  PowerPC maps
  program vregs to volatile v0..v7 and reserves volatile v8+ as scratch.
- D-JSP8. AltiVec loads/stores must accept the bytecode contract's 4-byte
  aligned but not necessarily 16-byte aligned addresses.  The implemented
  tier therefore synchronizes v0..v7 with the aligned `env->vreg` file and
  emits direct scalar loads/stores for external memory.  Operations absent
  from baseline AltiVec use the same direct scalar synchronization path.  It
  never silently strengthens the language-level alignment requirement.
- D-JSP9. A backend must not abandon JIT compilation merely because a branch
  target exceeds its short conditional encoding when the target is inside
  the configured JIT region.  Emission reserves the maximum expansion size;
  patching chooses short or long form and fills unused slots with NOPs.
- D-JSP10. `PC_ENTRY_MAX`, `BRANCH_PATCH_MAX`, and exhaustion of the configured
  code region are separate hard resource limits.  They remain explicit,
  diagnosed failures; this work does not disguise them as branch failures.
- D-JSP11. Packed parameter annotations remain single-symbol type names.  The
  integer names are `packedint8`, `packeduint8`, `packedint16`,
  `packeduint16`, `packedint32`, `packeduint32`, `packedint64`, and
  `packeduint64`.  Prefixing any name with `r` declares the restricted form.
  Floating-point names are `packedfloat` and `packeddouble`; the same rule
  also defines `rpackedfloat` and `rpackeddouble`.
- D-JSP12. A restricted packed parameter is a checked optimization contract,
  not unchecked C undefined behaviour.  Function entry validates element
  kind and pairwise non-overlap for restricted parameters used by a vector
  strip.  A failed optimization guard executes the scalar implementation;
  it does not make an otherwise valid Noct call invalid.
- D-JSP13. The 128-bit vector shapes are i8x16, i16x8, i32x4, i64x2, and
  f32x4.  Packed double receives complete type-annotation and guard support,
  while f64x2 arithmetic is outside the present SIMD requirement unless it
  is subsequently requested.  FP32x4 arithmetic is in scope on x86,
  ARM/NEON, and PowerPC/AltiVec.
- D-JSP14. Signedness is carried by load/store and comparison semantics, not
  by the physical 128-bit vector register.  Arithmetic opcodes state their
  element width explicitly, so bytecode remains portable across host ISAs.

## 2. Runtime capability model

Each architecture-specific JIT computes a capability value into its
`jit_context` before emitting a function.  Do not use an unsynchronised
process-global `static int` cache: concurrent first-time JIT builds would be
a C data race.  Per-build detection is cheap and deterministic.

| Backend | Capability tiers | Detection |
|---|---|---|
| x86 / x86_64 | scalar, SSE2, SSE4.1 | CPUID leaf 1: EDX.26, ECX.19 |
| ARMv7 | scalar, NEON | Linux `HWCAP_NEON`; conservative elsewhere |
| ARM64 | scalar, ASIMD | ABI baseline; Linux may confirm `HWCAP_ASIMD` |
| PPC32 / PPC64 | scalar, AltiVec | Linux `PPC_FEATURE_HAS_ALTIVEC` |
| MIPS / RISC-V | direct scalar | no SIMD ISA in this phase |

The override is read for each JIT build from `NOCT_JIT_SIMD_MAX`.  Accepted
backend-relevant values are `scalar`, `sse2`,
`sse41`, `neon`, and `altivec`.  A value cannot enable a feature missing from
hardware.  `tests/run-simd.sh` launches separate processes for each tier.

## 3. Vector-function metadata

Add `has_vector_ops` to `lir_func` and `rt_func`.

- `lir_put_opcode()` sets it for `OP_VLOADI32X4..OP_VSHRI32X4`.
- `rt_register_lir()` copies it.
- Bytecode files carry an optional `Vector Ops` line (`0` or `1`) before
  `Temporary Size`; absent means false for old files.  New writers emit it
  only when true.

This metadata records whether the function contains vector bytecode; it is
not an optimization hint.  A future Win64 register-mapped tier must not use
nonvolatile XMM registers unless it also adds symmetric save/restore to every
normal and exceptional exit.

### Packed parameter metadata

Parser and AST keep the source spelling, but HIR resolves it into three
orthogonal fields: runtime value kind (`packed`), element kind, and restricted
flag.  LIR and runtime function metadata preserve those fields.  The existing
plain `packed` annotation remains accepted as an element-unspecified,
unrestricted compatibility spelling and cannot by itself justify typed
vectorization.

The optimizer may use `rpacked...` facts only after it has proved the accessed
ranges for the vector strip.  Entry guards compare the backing storage ranges,
not merely object identity, so slices or future views cannot evade the alias
check.  Tail elements and a failed guard use the ordinary scalar loop.

## 4. Direct scalar tier

The scalar tier keeps canonical vector state in `env->vreg[vd]` and emits
ordinary scalar host instructions for four lanes.  Integer arithmetic uses
GPRs; FP32 arithmetic uses the architecture's scalar FP facility.  MIPS and
RISC-V always use this tier.  The capability override forces it on
SIMD-capable hosts for testing.  The portable C helpers remain the
interpreter/reference semantics and are not the generated-code fallback.

Register tiers normally keep vregs in hardware across a strip region.  PPC is
the exception: operations absent from baseline AltiVec synchronize v0..v7 to
the aligned runtime register file, perform the operation with direct scalar
instructions, and reload v0..v7.  Thus a mixed native/scalar region has one
coherent state at every boundary.

## 5. x86 tiering

SSE4.1 retains the `pmulld`/`pextrd` sequence.  SSE2 reuses every other
operation.  `VGETLANE*` joins two `pextrw` results.  `VMULI32X4` uses the
`pmuludq` even/odd-lane construction.  x86_64 uses volatile xmm8/xmm9 as
scratch; x86-32 temporarily preserves two non-operand xmm0..7 registers in a
32-byte unaligned stack area.

No SSE3-specific sequence exists because SSE3 does not improve these two
operations.  If SSE2 is absent, use the fully scalar tier.

Win64 deliberately stays on the direct scalar tier.  It uses only volatile
GPRs and xmm0 and therefore requires no XMM prologue/epilogue change.  A
future register-mapped Win64 tier using xmm6..xmm15 must add symmetric
save/restore first.

## 6. ARM and PowerPC

ARMv7 NEON uses q8..q15.  VLD1/VST1 handle the required unaligned accesses;
integer arithmetic maps directly to NEON.  FP32 regions use direct scalar VFP
because baseline ARMv7 NEON has no vector FP divide.  The complete direct
scalar tier is selected without `HWCAP_NEON`.

ARM64 ASIMD remains the normal tier.  The new capability/override path exists
so the scalar generator is tested and so nonstandard ports fail safely.

PowerPC AltiVec uses volatile v0..v7 plus v8 as scratch.  Move, i32 add/sub,
bitwise operations, and FP32 add/sub/multiply are native.  External
load/store, splat/getlane, i32 multiply/shifts, and FP divide synchronize the
register file and use direct scalar integer/FP instructions.  This works on
baseline AltiVec, avoids Power8-only instructions, and is endian-neutral.

## 7. Long-branch lowering

The configured region is at most 16 MiB (1 MiB on DOS/PC-98).

| Backend | Existing reach | Long form |
|---|---:|---|
| x86 / x86_64 | rel32 ±2 GiB | always sufficient; validate before cast |
| ARM32 | B/cond ±32 MiB | always sufficient; correct signed range test |
| ARM64 | cond ±1 MiB | invert condition + B (±128 MiB); plain B for JMP |
| PPC32/64 | cond ±32 KiB | invert condition + B (±32 MiB) |
| RISC-V32/64 | JAL ±1 MiB | invert condition + AUIPC/JALR |
| MIPS32 | branch ±128 KiB | invert condition + absolute `lui/ori/jr` |
| MIPS64 | branch ±128 KiB | invert condition + 64-bit address materialise + `jr` |

MIPS delay slots are part of the reserved maximum form.  Long-form patching
must not overwrite the following bytecode instruction.  Every backend gets a
test that creates a branch beyond its short range and asserts that JIT code
actually ran, rather than accepting the runtime's silent interpreter fallback.

## 8. Implementation and commit order

1. Design + long-branch stress harness.
2. Long branches, grouped by ISA family.
3. Packed annotation metadata and checked restricted-range guards.
4. Width-explicit integer vector IR plus FP32x4 IR and reference semantics.
5. Capability and vector-function metadata.
6. x86_64 Win64 preservation/scalar tier; x86-32 SSE4.1 port.
7. ARMv7 NEON and ARM64 tier unification.
8. PPC32/64 AltiVec.
9. x86/x86_64 SSE2 compatibility tier.
10. Direct scalar machine-code lowering on every JIT backend.
11. Cross matrix and status update; optional M5 Mac and POWER8 benchmark.

Steps 1–11 are implemented.  QEMU-user validation covers all ten JIT targets;
forced scalar and lower feature tiers are tested separately.  The selected
QEMU CPU models advertise and execute ARM NEON/ASIMD and PowerPC AltiVec, so
this is sufficient for functional validation.  The project owner's M5 Mac
and POWER8 machines remain useful only for meaningful performance numbers.

Each gate is committed locally only after `git diff --check`, relevant native
suites, all available cross builds, and forced lower-tier output comparison.
