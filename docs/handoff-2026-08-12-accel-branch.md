# `accel` branch comprehensive handoff 窶・2026-08-12

## 1. Repository snapshot

- Repository: NoctLang
- Branch: `accel`
- `main` / merge base: `c5dfd7a3ba90bfd1473826055af94d40a43799b1`
- Implementation HEAD before this handoff commit:
  `761addbf20b1210ae6962917fc8afc814bc09dbe`
- Working tree was clean before this document was added.
- This handoff document itself is committed with message `WIP`.

The branch is currently a straight descendant of `main`; no unmerged `main`
commit exists at the snapshot above.  The branch contains roughly 40,000 added
lines across compiler, runtime, tests, documentation, GPU support, and the ONNX
converter.  Most implementation checkpoints intentionally use the commit
message `WIP`.

This document is the top-level handoff for the whole branch.  The more focused
documents remain useful as detailed evidence:

- [`design/16-accel-vulkan.md`](design/16-accel-vulkan.md): raw and managed
  accelerator language/runtime contract;
- [`design/18-onnx-gpu-source-codegen.md`](design/18-onnx-gpu-source-codegen.md):
  ONNX-to-raw-GPU architecture and stage gates;
- [`design/19-accel-multikernel-doall-dosum.md`](design/19-accel-multikernel-doall-dosum.md):
  common loop analysis and multi-kernel managed execution;
- [`handoff-2026-08-12-accel-multikernel.md`](handoff-2026-08-12-accel-multikernel.md):
  completed managed `__accel func` subset;
- [`handoff-2026-08-12-onnx-gpu.md`](handoff-2026-08-12-onnx-gpu.md):
  completed ONNX Stage J state and locked model results;
- [`../plan/fast-func-exact-shape.md`](../plan/fast-func-exact-shape.md):
  original `__fast func` plan.  Observable behavior is implemented, with the
  internal representation notes recorded in section 6 below.

## 2. Executive summary

The branch adds four related capabilities.

1. Raw `__gpu func` kernels can be written in Noct, serialized, and dispatched
   through accelerator resources.  OpenGL compute is the validated backend.
2. Managed `__accel func` is now a synchronous GPU-only multi-kernel
   orchestrator.  Multiple DOALL and additive DOSUM loops are compiled to
   ordered internal kernels, while local Packed intermediates remain in device
   memory.
3. A production converter written in Noct reads a bounded static ONNX subset
   and emits a deterministic GPU-only package containing specialized raw
   `__gpu func` source and NWT1 weights.
4. CPU-side `__fast func` adds strong primitive/restricted-Packed contracts,
   exact multidimensional shapes, checked C-row-major indexing, a restricted
   intrinsic set, cross-module prototype resolution, and SIMD-friendly bounds
   facts.  Multicore automatic parallelization is not implemented yet.

These are separate execution paths.  The ONNX converter emits raw
`__gpu func`, not managed `__accel func`.  `__fast func` executes on the CPU and
does not dispatch GPU work.

## 3. Source spelling and compatibility

The current source spellings are:

```noct
__gpu func kernel(...): void { ... }
__accel func pipeline(...): void { ... }
__fast func calculate(...): int { ... }
static inline __fast func helper(...): float { ... }
```

Legacy `gpu func` and `accel func` are no longer aliases.  They produce a
migration diagnostic directing the user to `__gpu func` and `__accel func`.
Top-level persistent resources use `__accel var` or `__accel let`.

Function-kind numbers are persistence ABI and must not be renumbered:

```text
NOCT_FUNC_NORMAL = 0
NOCT_FUNC_ACCEL  = 1
NOCT_FUNC_GPU    = 2
NOCT_FUNC_FAST   = 3
```

The tracked parser/lexer outputs are large.  Edit `parser.y` / `lexer.l` as
the sources of truth, regenerate deliberately with compatible Bison/Flex, and
inspect generated diffs.  Do not hand-renumber tokens or function kinds.

## 4. Raw GPU and accelerator runtime

### 4.1 Raw `__gpu func`

Raw GPU functions are compiler-validated kernels, not CPU-callable functions.
The implemented frontend includes:

- typed `_in`, `_out`, and `_ptr` buffer parameters;
- one-dimensional dispatch geometry;
- scalar locals, assignment, lexical scopes, and bounded constant nested
  ranged loops used by generated tensor kernels;
- shared workgroup storage and `Accel.syncthreads()` validation;
- direct compiler-owned `Accel.*` float32 math operations and
  `Accel.float32FromBits()`;
- deterministic typed GPU IR and GLSL generation;
- raw kernel descriptors preserved through `.nb` and `.nap`;
- synchronous launch syntax and asynchronous dispatch/event APIs.

The `Accel.*` math names used inside `__gpu func` are compiler operations.  They
are not runtime functions or first-class values.  Their stable registry is in
`src/core/accel_ops.def`; keep `ACCEL_MATH`, `ACCEL_REDUCE`,
`ACCEL_TENSOR`, and their IDs stable.

### 4.2 Resources, copies, and events

The public runtime supports typed persistent resources such as
`Accel.float32(n)`, host/device copies, raw dispatch, and events.  Relevant
entry points are documented in [`library.md`](library.md):

```noct
__accel var state = Accel.float32(1024);
event = Accel.dispatchAsync(kernel, grid, block, ...);
Accel.join(event);
Accel.copyToAccel(...);
Accel.copyFromAccel(...);
```

OpenGL transfers and dispatches can be queued asynchronously.  Resource
coherence, pending events, pinned Packed objects, failure cleanup, and VM
teardown are covered by `tests/testcases/accel/async*.noct` and
the `tests/test.sh accel-opengl` suite.

### 4.3 Backend status

- **OpenGL:** implemented and hardware-validated.  This is the production
  backend for this branch.
- **Vulkan:** synchronous managed execution with multiple ordered DOALL and
  additive DOSUM steps, local intermediates, and persistent `_ptr` resources
  is hardware-validated on Linux. Raw kernels and generated ONNX models are
  not yet Vulkan claims.
- **D3D12:** not implemented or required.  Do not introduce a D3D12 assumption
  into Linux paths.

Feature-off builds must remain free of OpenGL/Vulkan headers and unresolved
backend symbols.  Backend descriptors must not serialize GL/Vulkan handles,
driver binaries, pointers, or absolute paths.

## 5. Managed GPU-only `__accel func`

`__accel func` is no longer a CPU function with an optional GPU fast path.  It
is a synchronous GPU-only program invoked through `Accel.call()`.

The compiler performs mandatory source-HIR analysis at every optimization
level and when `NOCT_ENABLE_OPTIMIZER=OFF`.  It recognizes supported top-level
loops, classifies them independently as DOALL or additive DOSUM, and creates a
versioned backend-neutral `accel_program` descriptor containing:

- checked scalar/length/dispatch expressions;
- host-backed formal buffers and persistent `_ptr` resources;
- call-local device buffers for typed local Packed declarations;
- compiler-generated internal GPU kernels and bindings;
- ordered dispatch/reduction steps.

At runtime, inputs are uploaded at the managed boundary, steps execute in
source order, intermediates stay in device buffers, and only required outputs
are downloaded at the end.  There is no hidden CPU replay or serial GPU
fallback if analysis or backend selection fails.

Currently accepted important cases:

- multiple zero-based, unit-step top-level DOALL loops;
- multiple canonical additive DOSUM reductions for `int32`, `uint32`, and
  `float32`;
- DOALL竊奪OSUM竊奪OALL and interleaved multiple reductions;
- local `Packed.int32/uint32/float32(length)` device intermediates;
- structured supported branches and proven independent affine accesses;
- reduction publication through an immediately following `_out[0]` or
  `_ptr[0]` store.

Important deferred/rejected cases:

- general nested source loops, scans, atomics, non-additive reductions, or
  multiple accumulators in one DOSUM;
- arbitrary calls, GPU-produced allocation/dispatch sizes, and general host
  control flow;
- managed int8/int16/int64/float64 arithmetic;
- asynchronous managed `Accel.call`, queue scheduling, overlap, CPU fallback,
  or simultaneous CPU/GPU execution;
- direct later use of an unpublished scalar reduction accumulator.

Unsupported programs must remain deterministic compile errors.

### 5.1 Common analysis infrastructure

The target-neutral base compiler modules are:

```text
src/core/hir_parallel.h
src/core/hir_loop_analyze.c
src/core/hir_doall.c
src/core/hir_dosum.c
```

They own declaration metadata, memory catalogs, affine access summaries,
scalar dataflow, dependence reasons, and reduction recognition.  Keep them in
`NOCT_BASE_SOURCE`; moving them under `NOCT_ENABLE_OPTIMIZER` breaks `-O0` and
optimizer-off accelerator semantics.

The normal CPU SIMD pass still has its own mature analysis.  Do not assume it
has been migrated wholesale to this common DOALL/DOSUM infrastructure.

## 6. CPU `__fast func`

`__fast func` is a constrained CPU function intended to make static analysis,
SIMD, and future multicore parallelization reliable.

### 6.1 Contract

- Every parameter, explicit local, and return type is annotated.
- Primitive types are exactly `int`, `long`, `float`, and `double`; return may
  also be `void`.
- Packed parameters must be restricted `rpacked*` types with an exact shape.
- Shapes have rank 1 through 8; extents are positive literals or `int`/`long`
  parameters.
- `(100)` means exactly 100 elements.  `(10,5,2)` is a distinct rank-3 view of
  the same one-dimensional Packed storage.
- Multidimensional indices are allowed only in FAST bodies, are zero-based,
  and use C row-major order with the final axis contiguous.
- Different restricted Packed formal parameters must receive distinct Packed
  objects.  Backing-address interval comparison is intentionally not done.
- Globals, closures, dynamic function values, object/method calls, normal
  functions, GPU functions, and accel functions are unavailable in a FAST
  body.
- Direct same-unit, preloaded, required-module, and `static inline` FAST calls
  are supported.  Direct and mutual recursion are rejected.

Caller-side preflight checks exact primitive tags, Packed element kinds,
positive dynamic extents, checked shape products, exact element counts, and
restricted identity before a frame is created.  A validated FAST-to-FAST call
reuses the caller contract and does not repeat the runtime preflight.  Normal
and external callers always take the complete checked path.

The initial compiler-owned intrinsic set is:

```text
min max abs sqrt sin cos tan asin acos atan atan2
exp ln log2 log10
int long float double    # explicit conversions
```

No implicit fast-math, reassociation, NaN ignoring, or precision relaxation is
enabled.

### 6.2 Bounds and SIMD behavior

- Provable out-of-bounds accesses in simple literal ranged-loop affine forms
  are compile errors at all optimization levels.
- Unknown accesses remain checked and fail safely at runtime.
- Exact FAST shape/type facts let ABCE omit ordinary `PCHECK`/`PLEN` guards.
- FAST restrict contracts let SIMD omit pairwise backing-range alias guards.
- Rank-1 contiguous loops and statically proven rank-2/3 final-axis row-major
  loops can use the existing CPU SIMD pass.
- Optimizer-off builds retain correct checked scalar execution.

The mandatory affine recognizer is intentionally conservative.  It handles
constants, loop counters, `i+C`, `C+i`, and `i-C`, plus known nested loop
domains.  Complex path proofs and arbitrary symbolic arithmetic fall back to
checked execution; they must never be treated as safe by default.

### 6.3 Current internal representation

The implementation deliberately reuses existing VM machinery rather than
adding a parallel FAST bytecode VM:

- shapes are canonical annotation strings parsed into a versioned
  `fast_signature` descriptor;
- comma indices are retained as an AST index list;
- unknown multidimensional accesses lower through checked internal
  `$Fast.indexN` helpers that evaluate every index once and check every axis;
- statically safe constant-shape accesses lower directly to a row-major HIR
  expression for ABCE/SIMD;
- calls use the existing call opcode plus a side-effect-free FAST prototype
  registry and runtime function registry;
- FAST signatures are serialized in versioned `.nb`/`.nap` sections and
  emitted by the C backend.

Do not assume the plan's suggested `OP_FAST_LOAD/STORE/CALL` names exist.
Changing to dedicated opcodes would be a new ABI project, not a cleanup.

Required-module prototype scanning parses only exported names and contracts.
It does not register functions, assign globals, or run initializers.  The
Noct App compiler scans all explicit inputs before compiling bodies, making
FAST resolution independent of input order.  `static inline __fast func`
remains file-local.

Multicore automatic parallelization, FAST DOALL/DOSUM lowering, software
pipeline scheduling, slices, partial indices, transpose/broadcast views,
column-major order, and local Packed values are not implemented.

## 7. ONNX GPU converter and model-weight foundation

### 7.1 Production command and artifacts

The converter is written in Noct.  Production conversion does not require the
Python `onnx` package or ONNX Runtime.

```sh
./build/noct --path=tools/onnx2noct \
  tools/onnx2noct/main.noct --output=OUT MODEL.onnx
```

It publishes the output directory transactionally, with `manifest.json`
written last:

```text
OUT/model.weights
OUT/gpu/model.noct
OUT/gpu/main.noct       # omitted by --emit-main=no
OUT/manifest.json
```

`gpu/model.noct` contains shape-specialized raw `__gpu func` kernels selected
from converter-owned families.  It is not GLSL, `dnn func`, `DNN.*`, or
managed `__accel func`.  The sample main accepts exactly:

```text
MODEL.weights INPUT.f32le OUTPUT.f32le
```

### 7.2 Supported subset

The reviewed static batch-one float32 subset includes:

- metadata views and COPY;
- Relu and Sigmoid;
- Add/Sub/Mul/Div static broadcasting;
- Conv2D with group 1 and dilation 1;
- rank-2 Gemm and MatMul;
- MaxPool, AveragePool, and GlobalAveragePool;
- Concat;
- static ReduceSum/Mean/Max/Min;
- stable Softmax and LogSoftmax;
- inference BatchNormalization.

Input/output are currently one fixed static graph interface.  There is no
dynamic/symbolic production shape, multi-I/O, quantization, generated CPU
model, or CPU fallback.

Locked model status at the completed Stage J checkpoint:

| Model | Status |
|---|---|
| MNIST-12 | Intel OpenGL source/`.nap`, interpreter/JIT pass |
| project CIFAR opset 12 | pass |
| SqueezeNet 1.1 opset 7 | pass |
| Tiny YOLOv2 opset 8 | intentionally blocked by symbolic batch `None` |

Do not silently rewrite Tiny YOLOv2's symbolic batch or replace the locked
artifact.  Supporting it needs a new approved dynamic-shape policy or a new
approved static artifact.

### 7.3 Generic binary and immutable-weight APIs

The branch also adds checked `File.readExact` / `File.writeAll`, endian and
varint `Binary.*` operations, streaming SHA-256, and feature-gated immutable
NWT1 `Weights.*` handles.  `NOCT_ENABLE_MODEL_WEIGHTS=ON` requires
`NOCT_ENABLE_API_FILE=ON`.

NWT1 loading authenticates the full pack and validates its versioned header,
canonical directory, names, shape/range metadata, alignment, zero padding,
non-overlap, payload hashes, exact end, handle ownership, and closed state.
These APIs are generic foundations; do not weaken validation for a particular
model.

Python is used only by deterministic test fixture/oracle tools.  The extended
locked-model oracle environment is pinned in
`tests/testcases/onnx2noct/oracle/requirements.txt`.

## 8. Build dependencies and suggested configurations

The exact package names vary by distribution.  On Debian/Ubuntu, the practical
native development set is approximately:

```sh
sudo apt install build-essential cmake pkg-config \
  bison flex libfl-dev \
  libegl-dev libgles-dev mesa-utils \
  python3
```

Bison/Flex are needed when regenerating grammar outputs; ordinary builds can
use the tracked generated sources.  Python 3 is needed for the deterministic
test fixture builders, not for production ONNX conversion.

Vulkan compilation needs the distribution equivalents of Vulkan
headers/loader and Shaderc (`libvulkan-dev` and `libshaderc-dev`).  The Vulkan
gate also uses `glslc`, `spirv-val`, `vulkan-tools`, and the Khronos validation
layer; on Debian these are provided by `glslc`, `spirv-tools`, `vulkan-tools`,
and `vulkan-validationlayers` in addition to a hardware Vulkan driver.

A representative full OpenGL ES build is:

```sh
cmake -S . -B build-linux-opengl \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNOCT_ENABLE_CLI=ON \
  -DNOCT_ENABLE_STATIC=ON \
  -DNOCT_ENABLE_JIT=ON \
  -DNOCT_ENABLE_OPTIMIZER=ON \
  -DNOCT_ENABLE_API=ON \
  -DNOCT_ENABLE_API_SYSTEM=ON \
  -DNOCT_ENABLE_API_CONSOLE=ON \
  -DNOCT_ENABLE_API_FILE=ON \
  -DNOCT_ENABLE_MODEL_WEIGHTS=ON \
  -DNOCT_ENABLE_BCBACKEND=ON \
  -DNOCT_ENABLE_CBACKEND=ON \
  -DNOCT_ENABLE_REPL=ON \
  -DNOCT_ENABLE_ACCEL_OPENGL=ON \
  -DNOCT_ENABLE_ACCEL_VULKAN=OFF
cmake --build build-linux-opengl -j2
```

The checked-in Linux Vulkan preset and its hardware gate are:

```sh
cmake --preset linux-vulkan
cmake --build build-linux-vulkan -j2
NOCT="$PWD/build-linux-vulkan/noct" sh tests/test.sh accel-vulkan
```

Create `build-static` from the same configuration with both accelerator
options set to `OFF`; it is the correct binary for the backend-disabled
compiler/serialization runner.  Keep `NOCT_ENABLE_OPTIMIZER=ON` in that build.

Also keep an optimizer-off/backend-off build for mandatory-semantics testing:

```sh
cmake -S . -B build-noopt \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNOCT_ENABLE_CLI=ON \
  -DNOCT_ENABLE_STATIC=ON \
  -DNOCT_ENABLE_API=ON \
  -DNOCT_ENABLE_API_FILE=ON \
  -DNOCT_ENABLE_BCBACKEND=ON \
  -DNOCT_ENABLE_OPTIMIZER=OFF \
  -DNOCT_ENABLE_ACCEL_OPENGL=OFF \
  -DNOCT_ENABLE_ACCEL_VULKAN=OFF
cmake --build build-noopt -j2
```

If `NOCT_ENABLE_API=ON` is used, ensure at least one API component is enabled.
The repository's defaults and platform gates in `CMakeLists.txt` remain the
authority.

## 9. Validation entry points

Use absolute `NOCT` paths for runners that change into temporary directories.

### 9.1 CPU/compiler regression

```sh
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh syntax
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh typing
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh scoping
FAST_EXPECT_SIMD=1 NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh fast
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh simd
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh app
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh ctrans build-linux-opengl
```

The FAST suite covers source/JIT/interpreter, bytecode, Noct App prototype
scanning, required-module initializer ordering, exact shape failures,
restricted aliasing, checked dynamic fallback, affine diagnostics, and rank-1
plus multidimensional final-axis SIMD.

### 9.2 Target-neutral accelerator tests

Run these from the repository root:

```sh
NOCT="$PWD/build-static/noct" sh tests/test.sh accel
NOCT="$PWD/build-noopt/noct" sh tests/test.sh accel-analysis
```

The `accel` command is a backend-disabled/static compiler suite and intentionally
checks that `--accel=opengl` is rejected.  Do not use an OpenGL-enabled binary
for that runner.  Use `tests/test.sh accel-opengl` for hardware execution.

### 9.3 OpenGL hardware tests

On a headless Mesa system, `EGL_PLATFORM=surfaceless` is usually sufficient.
On a desktop session, expose the appropriate display/session environment.

```sh
EGL_PLATFORM=surfaceless \
NOCT="$PWD/build-linux-opengl/noct" sh tests/test.sh accel-opengl
```

The runner rejects llvmpipe, softpipe, and other software renderers.  It covers
managed multi-kernel programs, raw kernels, shared memory, compiler math,
resources, copies, asynchronous events, serialization, and JIT/interpreter.

### 9.4 ONNX converter and generated GPU tests

```sh
NOCT="$PWD/build-static/noct" sh tests/test.sh onnx2noct

EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-gpu
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-conv
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-contraction
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-pool
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-concat
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-reduce
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-batchnorm
EGL_PLATFORM=surfaceless NOCT="$PWD/build-linux-opengl/noct" \
  sh tests/test.sh onnx-package
```

The core Stage D窶滴 converter suite uses Python's standard library for fixture
generation.  ONNX Runtime is needed only for the optional independent oracle
and locked-model comparisons.

### 9.5 Sanitizers

FAST, target-neutral accel analysis, OpenGL accelerator execution, and ONNX
packages have dedicated sanitizer-compatible runners.  A typical environment
is:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
NOCT="$PWD/build-asan/noct" sh tests/test.sh fast
```

Use an OpenGL-enabled sanitizer build for `tests/test.sh accel-opengl` and the ONNX
hardware runners.

## 10. Validation evidence at handoff

The following passed at or immediately before this handoff HEAD:

- optimizer-on, optimizer-off, JIT-off, and ASan/UBSan FAST suites;
- syntax, typing, scoping, Noct App, C translation, and complete SIMD suites;
- target-neutral/static accelerator compiler tests;
- hardware OpenGL accelerator suite;
- ONNX Stage D through Stage H deterministic converter/package tests;
- OpenGL generated-model families F, G1, G2, G3, G4, G5, and G6;
- complete Stage H OpenGL package test.

The hardware used for the validated GPU path was Mesa Intel Iris Xe
(Alder Lake GT2), OpenGL 4.6 with EGL 1.5.  Hardware names and driver versions
are evidence, not a hard-coded requirement.  The tests require compute-shader
and SSBO capability and reject software rendering.

Earlier accelerator/ONNX checkpoints also completed MinGW x86-64 and
OpenWatcom DOS4GW feature-off builds.  Those cross-builds were not rerun after
the final FAST commits, so treat them as historical evidence rather than a
final-HEAD guarantee.

## 11. File map

### Compiler and runtime

| Area | Files |
|---|---|
| source syntax | `src/core/lexer.l`, `parser.y`, `ast.h`, `ast.c` |
| accelerator operations | `src/core/accel_ops.def`, `accel_ops.[ch]` |
| GPU IR / GLSL | `src/core/gpu_ir.[ch]`, `gpu_glsl.c`, `hir_gpu.c` |
| common loop analysis | `hir_parallel.h`, `hir_loop_analyze.c`, `hir_doall.c`, `hir_dosum.c` |
| managed programs | `accel_program.c`, `hir_opt_accel.c` |
| common accel runtime | `src/api/accel.c` |
| OpenGL backend | `src/api/accel_opengl.c` |
| Vulkan backend | `src/api/accel_vulkan.c` |
| FAST contracts | `src/core/fast.[ch]`, FAST sections in `hir.c` and `runtime.c` |
| FAST optimization bridge | `hir_opt_abce.c`, small guarded changes in `hir_opt_simd.c` |
| persistence/AOT | `lir.[ch]`, `runtime.[ch]`, `bcback.c`, `cback.c`, `include/noct/aot.h` |
| binary/weights | `api-binary.c`, `api-weights.c`, `sha256.[ch]`, `api-file.c` |

### ONNX converter

`tools/onnx2noct/` contains the production reader, normalization, planner,
kernel registry/families, model emitter, and package emitter.  The production
entry point is `tools/onnx2noct/main.noct`.

`tests/testcases/onnx2noct/` contains deterministic fixtures, goldens, package
verification, oracle tooling, and locked model metadata.  Performance and
correctness baselines are under `docs/bench/`.

## 12. Maintenance boundaries

Preserve all of the following unless a new design explicitly replaces them:

- existing function-kind, bytecode opcode, accelerator operation, descriptor,
  and serialization version numbers;
- mandatory accelerator/FAST validation outside optional optimizer gates;
- GPU-only failure semantics for `__accel func` and generated ONNX models;
- raw `__gpu func` as the ONNX converter target;
- exact FAST shape and caller-side restrict contract;
- one-dimensional Packed storage beneath FAST multidimensional views;
- axis-by-axis checks for unknown multidimensional accesses;
- normal `func` SIMD behavior and heuristics;
- feature-off builds without backend or model-weight dependencies;
- loader validation of counts, enums, references, ownership, hashes, and
  arithmetic overflow;
- deterministic converter output and manifest-last publication.

Do not add or infer:

- `dnn func`, `DNN.*`, a runtime tensor object, or DPL1;
- generated CPU models or hidden CPU fallback;
- D3D12/HLSL support;
- symbolic shape normalization by silently choosing batch one;
- backing-memory interval checks for distinct restricted Packed objects;
- unsafe FAST vectorization/parallelization from an unproved access;
- serialization of compiler pointers, backend handles, or absolute paths.

## 13. Recommended next work

Choose one bounded project and add a new plan/gate before changing semantics.
The currently coherent options are:

1. validate raw Vulkan kernels as a separate gate before considering generated
   models; managed multi-kernel DOALL/DOSUM programs are now validated;
2. extend managed DOSUM publication so a later DOALL may consume an implicit
   one-element device result without host readback;
3. design FAST multicore automatic parallelization on top of the common loop
   analysis, without changing normal `func` multicore eligibility;
4. extend FAST bounds/path analysis while preserving checked fallback;
5. approve a static Tiny YOLO artifact or design real dynamic-shape support;
6. begin ONNX performance work only after selecting a backend/model gate and
   version every changed kernel family.

For any follow-up, first run the focused suite plus syntax/typing/SIMD and a
feature-off build.  GPU/compiler failures, unsupported model capability, and
environment/driver failures should be reported separately rather than hidden
behind fallback behavior.
