# WS-SIMD: Separate SIMD auto-vectorization

Status: Planned

## Goal

Separate automatic SIMD vectorization from the basic optimizer so that
`NOCT_ENABLE_OPTIMIZER` can provide typed operations, CSE, ABCE, and scalar
loop unrolling without enabling the SIMD transformation pass.

## Intended configuration

- Add `NOCT_ENABLE_SIMD`, defaulting to `OFF`.
- Require `NOCT_ENABLE_OPTIMIZER` when `NOCT_ENABLE_SIMD` is enabled.
- Keep ABCE and scalar unrolling in the basic optimizer.
- Compile `hir_opt_simd.c` and call `hir_opt_simd_func()` only when SIMD
  auto-vectorization is enabled.
- Keep existing SIMD bytecode and JIT execution support available. The option
  controls automatic vectorization, not execution of existing vector code.
- Do not make accelerator support depend on SIMD.

The optimization order at level 2 remains:

```text
ABCE -> optional SIMD vectorization -> scalar unrolling
```

## Milestones

1. Add the CMake option, dependency check, and private compile definition.
2. Split the optimizer source list and guard the SIMD pass invocation.
3. Decide how `--simd-info` reports a build without auto-vectorization.
4. Update presets so basic-optimizer and SIMD-enabled builds are both covered.
5. Add a CI matrix for optimizer/SIMD configuration boundaries.

## Acceptance criteria

- Optimizer ON and SIMD OFF passes typed-op, CSE, ABCE, and scalar-unroll tests.
- Optimizer ON and SIMD ON passes the complete SIMD suite.
- SIMD ON and optimizer OFF fails during CMake configuration with a clear
  diagnostic.
- Accelerator builds remain valid with SIMD disabled.
- No existing SIMD bytecode becomes unreadable or unexecutable solely because
  auto-vectorization was disabled at build time.

## Non-goals

- Redesigning the vector bytecode or native SIMD lowering.
- Changing the optimization-level policy for ABCE or scalar unrolling.
- Implementing polyhedral vectorization.
