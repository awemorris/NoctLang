# ONNX raw-GPU source-generation handoff — 2026-08-11

Status: design-18 Stage C complete; owner review gate pending

## Repository checkpoint

```text
repository:   git@github.com:awemorris/NoctLang.git
branch:       accel
local HEAD:   f44a2504be60a3e1d3e0bb1287905a002acfd29a
origin/accel: 96fde9ad6d09a25e0baccc29de0c527382859650
```

The implementing model must record the actual local/origin HEAD and dirty files
before editing.  Do not reset owner changes and do not commit.

## Authority order

1. [design 18](design/18-onnx-gpu-source-codegen.md) — sole authority for ONNX,
   kernel templates, GPU-only artifacts, `ACCEL_MATH`, NWT1, stage order, and
   acceptance gates;
2. [design 16](design/16-accel-vulkan.md) — existing raw `__gpu func`, `_ptr`,
   launch, resource, event, OpenGL, alias, and retained Vulkan semantics;
3. [design 15](design/15-require-modules.md) — module lookup;
4. [design 12](design/12-noct-app-nap.md) — multi-source `.nap` applications;
5. [design 17](design/17-onnx-dnn-codegen.md) — rejected-plan tombstone only.

## Owner decisions

The converter is written in Noct and emits only:

```text
OUT/model.weights
OUT/gpu/model.noct
OUT/gpu/main.noct
OUT/manifest.json
```

Generated compute is actual shape-specialized Noct `__gpu func` source selected
from converter-owned kernel families.  It is not `__accel func` and not direct
GLSL.  The model has no CPU implementation or CPU fallback.

Inside raw `__gpu func`, direct calls such as `Accel.sigmoid(x)` are compiler-only
float32 GPU operations.  They lower as `ACCEL_MATH` plus an explicit stable
function ID.  They are not ordinary bytecode, runtime cfuncs, first-class
values, or dictionary entries.  `ACCEL_REDUCE` and `ACCEL_TENSOR` class numbers
are reserved but not source-callable in version 1; the converter emits explicit
bounded loops and memory operations for reductions and tensor kernels.

Do not add `dnn func`, `DNN.*`, runtime Tensor Views, DPL1, a DNN descriptor,
CPU model generation, or CPU tensor handlers.

## Environment

This is native Linux.  The hardware renderer is Mesa Intel Iris Xe OpenGL 4.6
with EGL 1.5.  Headless SSH execution uses:

```sh
EGL_PLATFORM=surfaceless
```

D3D12 does not exist on this machine and is not a requirement.  Software
renderer rejection can be exercised with `LIBGL_ALWAYS_SOFTWARE=1`.  OpenGL is
the first execution gate.  Vulkan is available on the machine but generated
model support is not claimed until design-18 Stage K validates the Noct raw
Vulkan path.

## Completed stage and next gate

Design-18 Stages A, B, and C are complete.  Stage A locked the model inventory,
fixtures, oracle data, and native Intel OpenGL baseline.  The exact Tiny
YOLOv2 opset-8 artifact still declares symbolic batch `None`; it remains
incompatible with the version-1 static-shape contract and must not be silently
rewritten.

Stage B adds the reviewed compiler-only operation registry with fixed class
numbers and MATH IDs, raw `__gpu func` nested constant ranged loops, scalar
locals/reassignment and lexical scopes, and exact direct GPU-only
`Accel.sigmoid()` / `Accel.relu()` lowering.  Reserved registry entries remain
unsupported rather than receiving untested approximations.  No runtime cfunc,
ordinary bytecode operation, DNN namespace, CPU model, converter, DPL1, or
Vulkan execution path was added.

The focused valid/error suites, optimization levels 0/1/2, optimizer-off
build, `.nb` and `.nap` roundtrips, existing accelerator coverage, Intel Mesa
OpenGL execution, MinGW cross-build, and OpenWatcom MS-DOS build pass.  A
sanitized interpreter/OpenGL run of the new Stage-B kernels passes.  The full
sanitizer suite reaches an existing x86-64 JIT misaligned store in
`src/core/jit-x86_64.c`; the same issue is outside the Stage-B raw GPU path and
is recorded rather than hidden.  Exact commands and other baseline test
limitations are in `docs/bench/onnx-stage-b-baseline-2026-08-11.md`.

Stage C adds the checked `File.readExact`, `File.writeAll`, exclusive output
directory, Binary endian/varint/float32 helpers, streaming SHA-256, and the
feature-gated frozen `Weights` namespace.  The NWT1 reader validates the full
pack hash, fixed v1 header, canonical directory, UTF-8 names, checked shapes and
ranges, zero padding, nonoverlapping 64-byte-aligned payloads, payload hash,
and exact file end before exposing a handle.  Handles enforce kind, owner VM,
closed state, idempotent close/finalization, GC movement, and VM teardown.

The dedicated corpus includes a deterministic Noct-written 200-byte pack, a
Python test-only independent fixture writer, 200 truncation points and 30
additional malformed cases, interpreter/JIT, `.nb`/`.nap`, feature-off,
optimizer-off, repeated VM teardown, and wrong-kind/owner tests.  It passes
under ASan+UBSan with leak detection.  The full existing accelerator suites,
Intel Mesa OpenGL execution, MinGW, OpenWatcom, syntax, typing, class, and app
regressions also pass.  Exact commands are recorded in
`docs/bench/onnx-stage-c-baseline-2026-08-11.md`.

The first incomplete gate is owner review of the uncommitted Stage-C source,
tests, diagnostics, and baseline report.  Do not begin Stage D until that gate
is accepted.  There is still no ONNX conversion script: Stage D first adds the
bounded Noct protobuf/structural ONNX reader, without source generation.

At every handoff state the completed stage, first incomplete gate, exact model
hashes where applicable, hardware backend evidence, test commands/results, and
whether a failure is code, model capability, or environment.
