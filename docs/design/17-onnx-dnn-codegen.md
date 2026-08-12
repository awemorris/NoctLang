# ONNX to Noct DNN code generation (superseded)

Status: rejected architecture; do not implement

This document number is retained as a tombstone so old links do not silently
resolve to an apparently active plan.  The project owner rejected the design
which generated CPU and managed-accelerator libraries through runtime `DNN.*`
Tensor Views and a serialized DPL1 program.

The sole current ONNX authority is
[18-onnx-gpu-source-codegen.md](18-onnx-gpu-source-codegen.md).  It specifies a
GPU-only converter written in Noct which emits specialized raw Noct `__gpu func`
source, NWT1 weights, a manifest, and a GPU sample application.

Do not recover or implement any of the discarded items from git history or an
older handoff:

- `dnn func` or `NOCT_FUNC_DNN`;
- runtime `DNN.*` tensor operations;
- runtime Tensor View handles;
- DPL1 or a DNN accelerator descriptor version;
- generated CPU model source or CPU tensor executor;
- managed `__accel func` kernels for ONNX nodes;
- direct ONNX-to-GLSL generation.

Design 16 remains authoritative for existing low-level accelerator and raw
`__gpu func` semantics.  Designs 15 and 12 remain authoritative for modules and
`.nap` applications.
