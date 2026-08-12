# ONNX raw-GPU converter handoff — 2026-08-12

## Current state

Branch: `accel`

Mandatory design-18 work is complete through Stage J.  Checkpoints were
committed with the owner-requested message `WIP`; the last pre-Stage-J
checkpoint is `16e5032fcefd22f259265101a04d821cf3c5158d`.  OpenGL is the only
validated generated-model backend.  Optional Stage K Vulkan execution and
Stage L performance work have not started.

The production converter is:

```sh
./build-stage-c/noct --path=tools/onnx2noct \
  tools/onnx2noct/main.noct --output=OUT MODEL.onnx
```

It is implemented in Noct and has no production Python/ONNX Runtime
dependency.  It reads bounded protobuf, validates exact opset 7/8/12 schemas,
normalizes one fixed batch-one float32 input/output graph, plans persistent
storage, specializes converter-owned kernel families, and publishes canonical
NWT1 plus raw `__gpu func` source.  `manifest.json` is written last.  The bound
production path is `OUT/gpu/model.noct`; `OUT/gpu/main.noct` accepts exactly
`MODEL.weights INPUT.f32le OUTPUT.f32le`.  `--emit-main=no` omits the sample
main.  No root compatibility source is emitted; Stage F–G source goldens track
the bound `gpu/model.noct` artifact directly.

Supported reviewed families are COPY/metadata views, Relu/Sigmoid,
Add/Sub/Mul/Div static broadcasting, Conv2D group 1/dilation 1, rank-2
Gemm/MatMul, MaxPool/AveragePool/GlobalAveragePool, Concat, static
ReduceSum/Mean/Max/Min, stable Softmax/LogSoftmax, and inference BatchNorm.
Grouped/depthwise/dilated Conv was not added because no convertible locked
model requires it.  GPU-only scalar math uses stable `Accel.*` compiler
operations; it is not a runtime DNN namespace or new VM opcode.

## Locked model results

| Model | Result | Output | Max abs | Argmax | Representative warm / steady |
|---|---|---:|---:|---:|---:|
| MNIST-12 | Intel OpenGL source/`.nap`, interpreter/JIT pass | `1x10` | `2.86102295e-06` | 4 | 47.2 ms / 3.43 ms |
| project CIFAR opset 12 | pass | `1x10` | `0` | 9 | 52.1 ms / 1.09 ms |
| SqueezeNet 1.1 opset 7 | pass | `1x1000` | `3.33786011e-06` | 904 | 224.9 ms / 65.4 ms |
| Tiny YOLOv2 opset 8 | checked blocker | symbolic batch `None` | n/a | n/a | n/a |

Tiny YOLOv2 matches its locked hash and ONNX Runtime oracle when the test
explicitly supplies batch one, but production conversion correctly rejects its
symbolic declaration before creating an output root.  Do not silently rewrite
it or substitute a differently hashed artifact.  Future enablement needs an
owner-approved static artifact or a new dynamic-shape design.

## Verification evidence

- Mesa Intel Iris Xe, OpenGL 4.6/EGL 1.5, `EGL_PLATFORM=surfaceless`;
- all Stage F–G family hardware runners, complete-package relocation, and the
  three convertible locked models pass source/`.nap` and interpreter/JIT;
- normal typing, ABCE, CSE, SIMD, syntax, app, model-weight, accelerator, and
  raw `.nb`/`.nap` relocation/truncation tests pass;
- the complete ONNX/OpenGL sequence passes ASan/UBSan with zero stderr;
- optimizer-disabled source generation passes;
- static feature-off build contains no unresolved OpenGL/Vulkan symbols;
- MinGW x86_64 builds; OpenWatcom DOS4GW builds with its pre-existing missing
  optional `wstub.exe` warning after successful link.

The legacy `tests/testcases/run-elisp.sh` golden comparison is a documented pre-Stage-J
baseline: the current transpiler output differs from its checked-in legacy
goldens, while the Stage J diff changes neither the transpiler nor those
goldens.  Raw `__gpu func` rejection for C/Elisp/Scheme is covered and passes in
the accelerator suite; repairing unrelated generic Elisp golden drift is not
part of this converter plan.

The larger model ladder exposed unaligned x86-64 JIT accesses to embedded call
argument tables.  They now use `memcpy` for both emission and helper reads,
preserving the public AOT ABI and passing sanitizer.

## Boundaries that remain fixed

There is no generated CPU model, CPU fallback/replay, `dnn func`, `DNN.*`,
DPL1, DNN Plan, dynamic/symbolic production shape, multi-I/O, quantization,
D3D12 assumption, or Vulkan model-execution claim.  C/Elisp/Scheme transpilers
explicitly reject raw `__gpu func` model source.  Python and ONNX Runtime remain
tests-only.

The first optional next gate is Stage K and requires explicit owner
confirmation.  It must validate the existing Vulkan raw path without changing
converter output.  Stage L optimization follows only after a selected backend
and model gate are correct and must version any kernel-family change.
