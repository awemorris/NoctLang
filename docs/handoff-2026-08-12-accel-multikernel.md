# GPU-only multi-kernel `__accel func` handoff

Date: 2026-08-12  
Branch: `accel`  
Implementation range: `db4e3a4` through `8c40643` (all checkpoint messages
are exactly `WIP`)

## Outcome

Managed `__accel func` is now a synchronous GPU-only program rather than a CPU
function with an optional accelerator path.  Supported top-level loops are
analyzed before optional optimization, classified as DOALL or DOSUM, and
lowered to ordered internal `__gpu func` descriptors.  Host input is uploaded at
the program boundary, local Packed intermediates stay in device buffers across
steps, and host output is downloaded after the last step.  A missing/disabled
backend or any rejected program is an error; there is no CPU replay.

The executed backend is headless EGL/OpenGL.  The native gate passed on Mesa
Intel Iris Xe (Alder Lake GT2), OpenGL 4.6 / EGL 1.5.  Vulkan execution was not
tested or enabled by this work.  D3D12 is out of scope on Linux.

No SIMD implementation or ONNX converter/generated-model source was changed.

## Implemented compiler architecture

- `hir_parallel.h`, `hir_loop_analyze.c`, `hir_doall.c`, and `hir_dosum.c` are
  base compiler modules, not optional optimizer modules.  They provide stable
  reason codes, logical memory catalogs, affine accesses, scalar dataflow,
  dependence results, and canonical additive-reduction recognition.
- HIR locals retain source declaration kind, exact scalar/packed type,
  storage class, source line, initializer, and parameter identity.  Synthetic
  optimizer locals keep explicit unknown sentinels.
- Analysis is mandatory for `__accel func` at `-O0` through `-O3` and when
  `NOCT_ENABLE_OPTIMIZER=OFF`.  SIMD still uses its existing implementation.
- `accel_program` is a versioned backend-neutral descriptor containing owned
  expressions, buffers, internal kernels, bindings, and ordered steps.
- Local `Packed.int32/uint32/float32(length)` declarations become unique
  call-local device buffers.  Read-before-definition and unsupported length or
  dispatch expressions reject compilation.
- `gpu_ir.c` and `gpu_glsl.c` form the shared validation/emission facade used
  by raw source kernels and compiler-generated managed kernels.
- LIR deliberately omits an executable body for `NOCT_FUNC_ACCEL`.  Ordinary
  calls and non-GPU transpilers reject it.
- Accelerator program descriptors, internal kernels, expressions, buffers,
  bindings, and steps have checked textual bytecode serialization and survive
  source, `.nb`, and `.nap` paths.

## Currently accepted managed subset

- zero-based unit-step top-level ranged loops with an integer constant or
  scalar-parameter exclusive upper bound;
- multiple DOALL loops in source order;
- multiple typed additive DOSUM steps (`int32`, `uint32`, or `float32`)
  before, between, or after DOALL loops, each with distinct result and scratch
  buffers;
- canonical `sum = sum + value`, commuted `sum = value + sum`, and `sum +=
  value`, with exact typed zero identity and one update on every path;
- structured supported DOALL branches and proven independent affine accesses;
- local typed 32-bit Packed buffers whose length is a parameter or constant;
- DOSUM publication through the immediately following `_out[0] = sum` or
  `_ptr[0] = sum` store;
- downstream DOALL access to an `_ptr[0]` result without host synchronization.

The representative GPU-resident pipelines are covered by
`tests/accel/doall-dosum-doall.noct` and `tests/accel/multi-dosum.noct`.  The
first keeps a local uint32 buffer and one reduction result on the GPU.  The
second executes DOALL, DOSUM, DOALL, DOSUM, DOALL in source order with two
local buffers, two persistent results, and no intermediate host copy.  Its
checked endpoints are both `131`.

## Deliberately rejected or deferred

- direct later use of the scalar accumulator symbol without first publishing
  it to `_ptr[0]`;
- non-additive or multiple-accumulator reductions, scans, atomics, nested
  source loops, non-zero starts, strides, and general calls;
- GPU-produced dispatch/allocation sizes or host control flow;
- int8/int16/int64/float64 managed arithmetic;
- asynchronous `Accel.call`, overlap, dependency scheduling, multiple queues,
  CPU parallelization, or CPU/GPU simultaneous work;
- Vulkan execution or any D3D12/HLSL path.

Unknown or unsupported cases must remain deterministic compile errors.  Do not
restore the former serial GPU fallback or a hidden CPU fallback.

## Runtime behavior

The OpenGL executor evaluates checked descriptor expressions, allocates
call-local SSBOs, binds existing `_ptr` resources, uploads/downloads only at the
managed boundary, and dispatches steps in order with storage barriers.  DOSUM
uses a 64-lane map reduction and repeated ping-pong folds through two scratch
buffers; zero-trip reductions write typed zero.  All call-local GL objects are
released on success and failure.

`Accel.call` preflights scalar/buffer types, element kinds, restricted aliasing,
ranges, and the selected backend before submission.  `_in`/`_out` require host
Packed storage and `_ptr` requires a matching `Accel.*` resource.

## Validation completed

The following passed from `/home/awe/noct-gpu`:

```sh
cd tests
EGL_PLATFORM=surfaceless \
NOCT=../build-stage-b-opengl/noct sh run-accel-opengl.sh

NOCT=../build-stage-b-static-full/noct sh run-accel.sh
NOCT=../build-stage-b-noopt/noct sh run-accel-analysis.sh

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
NOCT=../build-stage-c-asan/noct sh run-accel-analysis.sh

EGL_PLATFORM=surfaceless \
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
NOCT=../build-stage-b-opengl-asan/noct sh run-accel-opengl.sh

NOCT=/home/awe/noct-gpu/build-stage-b-static-full/noct \
sh tests/test.sh simd
```

The OpenGL runner passed source and serialized/JIT cases including multiple
DOALL, local buffers, DOSUM, terminal DOALL-to-DOSUM,
DOALL-to-DOSUM-to-DOALL, and interleaved multiple DOSUM.  The target-neutral
analysis runner passed with the optimizer disabled and under ASan/UBSan.  The
complete hardware OpenGL suite also passed with leak detection and
undefined-behavior checks enabled.  The full SIMD suite passed unchanged.
Feature-off MinGW x86-64 and OpenWatcom 1.9 DOS4GW builds completed; the latter
was configured without the unrelated model-weight feature.

## Maintenance boundaries

- Keep the analysis modules in `NOCT_BASE_SOURCE`; moving them behind
  `NOCT_ENABLE_OPTIMIZER` breaks GPU-only `-O0` execution.
- Do not modify `hir_opt_simd.c` as part of managed GPU follow-up.  A future
  SIMD migration to the common analysis is a separate project.
- Do not serialize HIR/AST pointers, GL handles, driver binaries, or absolute
  paths.  Validate all counts, enums, indexes, ownership, and references on
  load.
- Keep feature-off builds free of OpenGL/Vulkan headers and unresolved symbols.
- Preserve raw `__gpu func`, resource/copy/event APIs, and stable `ACCEL_MATH`,
  `ACCEL_REDUCE`, and `ACCEL_TENSOR` operation IDs.
- The ONNX GPU converter continues to emit raw `__gpu func`; changing it to use
  managed programs was not part of this implementation.

The next coherent managed extension is an implicit one-element device result
for direct use of a DOSUM accumulator symbol by a later DOALL.  It should reuse
the existing result-buffer binding and must not introduce host scalar readback
between steps.
