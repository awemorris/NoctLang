# ONNX to specialized raw Noct GPU source generation

Status: mandatory OpenGL implementation complete through Stage J on 2026-08-12;
optional Stages K/L not started.  The exact locked Tiny YOLOv2 artifact remains
the documented fixed-shape-v1 model blocker because it declares symbolic batch
`None`; conversion rejects it without publishing an output root.

This document is the sole authority for the ONNX-to-Noct work described here.
It supersedes the CPU/DNN-Plan architecture in
`17-onnx-dnn-codegen.md` and the ONNX/DNN portions of
`handoff-2026-08-11-onnx-dnn.md`.  Design 16 remains authoritative for the
existing `__gpu func`, `_ptr`, launch, resource, event, OpenGL, and retained
Vulkan semantics.  Designs 15 and 12 remain authoritative for `require` and
multi-source `.nap` applications.

The implementing model must not combine the discarded design 17 architecture
with this one.  In particular, this plan does not introduce `dnn func`, a
runtime `DNN.*` tensor API, DPL1, Tensor View runtime handles, generated CPU
models, or managed `__accel func` model kernels.

## 1. Goal and fixed product shape

Write a converter in Noct which reads a fixed-shape float32 ONNX protobuf and
emits a GPU-only Noct model.  The converter selects a checked kernel-family
template for every compute node, specializes it with static shape/stride/layout
and operator attributes, and emits actual Noct `__gpu func` source.  The existing
Noct raw-GPU compiler then validates and compiles that source.

The required output for one model is:

```text
OUT/
  model.weights
  gpu/
    model.noct
    main.noct
  manifest.json
```

There is no `cpu/` output directory and no CPU model implementation.  The
converter itself is a normal Noct program and runs on the CPU; only its emitted
model is GPU-only.

Version 1 is complete when:

1. the production converter parses supported ONNX without Python;
2. conversion is deterministic and either emits a complete committed artifact
   or a precise error without replacing an existing artifact;
3. the emitted `model.noct` contains reusable, shape-specialized raw
   `__gpu func` kernels and persistent accelerator resources;
4. the sample uploads input once, keeps weights and intermediates on the GPU,
   synchronously produces one output, and downloads only the final output;
5. OpenGL output on a hardware renderer agrees with pinned ONNX Runtime output
   within the operator/model tolerance and no CPU fallback is possible;
6. micro fixtures, MNIST-12, the project CIFAR topology, and SqueezeNet 1.1
   pass in that order; the exact locked Tiny YOLOv2 artifact is either replaced
   by an explicitly owner-approved static artifact or remains a checked hard
   rejection under the blocker already recorded in `models.lock`.

The implementation target is native Linux.  The current test machine exposes
Mesa Intel Iris Xe OpenGL 4.6 and EGL 1.5.  Headless SSH execution uses
`EGL_PLATFORM=surfaceless`.  D3D12 is not present and must never be a required
renderer, build dependency, diagnostic recommendation, or acceptance string.
Vulkan is a secondary later target for the same generated Noct source; it must
not be claimed until the raw `__gpu func` Vulkan path has its own explicit gate.

## 2. Decisions that are closed

The following decisions came from the project owner and are not open for
reinterpretation by the implementing model.

1. Generated compute kernels are raw Noct `__gpu func`, not `__accel func`.
2. Do not add a `dnn` keyword or `NOCT_FUNC_DNN` function kind.
3. Do not generate a CPU model or implement CPU tensor operators.
4. Do not add a runtime `DNN` namespace or calls such as `DNN.conv2d`.
5. Do not add DPL1, accelerator descriptor v3 for DNN, serialized Tensor Views,
   or a DNN Plan interpreter.
6. Do not add `ACCEL_MATH`, `ACCEL_REDUCE`, or `ACCEL_TENSOR` to the ordinary VM
   bytecode enum.  They are GPU-compiler operation classes only.
7. `Accel.sigmoid(x)` and related names are GPU-only compiler-recognized calls.
   They are not runtime cfuncs, ordinary VM intrinsics, first-class function
   values, or mutable dictionary entries.
8. Version 1 implements only the `ACCEL_MATH` source-call class.  The numeric
   class tags for `ACCEL_REDUCE` and `ACCEL_TENSOR` may be reserved, but no
   whole-tensor or whole-reduction pseudo-call is implemented.  The converter
   emits their indexing, loops, loads, stores, and synchronization explicitly
   in Noct `__gpu func` source.
9. Kernel source is selected from reviewed converter-owned kernel families and
   specialized with static values.  Never emit GLSL or SPIR-V directly from
   the ONNX converter.
10. The production converter is written in Noct.  Python, NumPy, `onnx`, and
    ONNX Runtime are test inventory/oracle dependencies only.
11. Version 1 has one graph input, one graph output, batch one, float32 compute,
    ranks 1 through 8, positive static dimensions, and immutable weights.
12. Generated model execution is GPU-required.  Disabled, unavailable, or
    unsupported GPU execution is an error; there is no hidden CPU replay.
13. OpenGL is the first execution gate.  Vulkan work begins only after the
    complete OpenGL model ladder is correct unless the owner explicitly changes
    the order.
14. No inference server, HTTP API, web UI, daemon, preprocessing, postprocessing,
    image decoder, YOLO decoder, NMS, training, or dynamic-shape work is in
    scope.
15. The original review-pause instruction was superseded by the owner's
    2026-08-12 direction to finish without review pauses and checkpoint the
    `accel` branch using the commit message `WIP`.

## 3. Baseline and current gaps

The planning baseline is branch `accel` at
`96fde9ad6d09a25e0baccc29de0c527382859650` (`Add accel evaluation`).  Before a
stage starts, record the actual local HEAD, `origin/accel`, and dirty files; do
not silently reset or discard owner changes.

The baseline already provides:

- raw `__gpu func`, one-dimensional grid/block geometry, `_ptr` arguments,
  persistent typed `accel var` resources, synchronous triple-chevron launch,
  `Accel.dispatchAsync`, events, copies, shared memory, and `syncthreads()`;
- int32, uint32, and float32 raw buffer access and scalar parameters;
- a headless EGL/OpenGL 4.3+ backend whose current native-Linux test succeeds
  on the Intel renderer when `EGL_PLATFORM=surfaceless` is set;
- raw kernel metadata persistence in `.nb` and `.nap`;
- hand-expanded CNN and CIFAR-shaped raw GPU evaluations.

The baseline raw source compiler in `src/core/hir_gpu.c` currently emits GLSL
while walking raw AST and is deliberately narrow.  In particular, the initial
audit shows no ranged-for emission, no reassignment of a scalar local after its
declaration, and no expression calls other than the statement-only
`syncthreads()`.  These gaps must be closed before generated Conv, pooling, or
reduction kernels are practical.  Do not work around them by emitting millions
of unrolled multiply-add statements.

The existing Vulkan backend is retained but does not yet constitute a raw-GPU
model execution gate.  Hardware availability on the machine does not prove the
Noct backend path.

No ONNX reader, model normalizer, kernel-template generator, NWT1 loader, or
GPU-only model generator exists at this checkpoint.

## 4. End-to-end architecture

The only production path is:

```text
ONNX bytes
  -> bounded Noct protobuf reader
  -> validated ONNX graph with exact opset handlers
  -> backend-independent normalized static graph
  -> converter-only tensor/view and storage records
  -> storage/liveness plan
  -> kernel-family selection and specialization
  -> deterministic Noct __gpu func source
  -> existing Noct raw-GPU compiler
  -> raw kernel descriptor and backend shader/pipeline
  -> OpenGL execution (Vulkan later)
```

There is no ONNX-to-GLSL shortcut.  There is no DNN Plan between normalized
graph and generated source.  There is no managed `__accel func` in generated
model source.

The converter owns high-level tensor metadata only while converting.  Generated
runtime source sees persistent flat float32 resources, scalar parameters where
needed, and constants embedded in raw kernel source.

## 5. GPU-only `Accel.*` math pseudo-intrinsics

### 5.1 Meaning and representation

Reserve three GPU compiler operation-class values:

```text
ACCEL_MATH   = 1
ACCEL_REDUCE = 2   # reserved, not source-callable in v1
ACCEL_TENSOR = 3   # reserved, not source-callable in v1
```

Each class has an independent function-ID space.  A call such as:

```noct
let y: float = Accel.sigmoid(x);
```

inside a raw `__gpu func` resolves to:

```text
class       = ACCEL_MATH
function_id = SIGMOID
```

This operation exists only while the compiler validates/emits a raw GPU
kernel.  It is not an entry in `src/core/bytecode.h`; it is not executed by the
interpreter, a CPU JIT, or the C backend; and it does not need a DPL or other
wire format.  Current raw descriptors persist the resulting shader template.
If design 16 later introduces a common transient Kernel Accel IR, preserve the
same class/function IDs when moving this operation into that IR.

Create `src/core/accel_ops.def` as the one reviewed registry source, with
explicit class and numeric function IDs, source spelling, arity, accepted scalar
types, backend capability, lowering helper, exceptional-value policy, and test
tolerance.  Never derive an ID from enum or table order.  Never reuse a removed
ID.

The initial MATH IDs remain:

| ID | Name | Source spelling | Arity |
|---:|---|---|---:|
| 1 | ABS | `Accel.abs` | 1 |
| 2 | NEG | `Accel.neg` | 1 |
| 3 | ADD | `Accel.add` | 2 |
| 4 | SUB | `Accel.sub` | 2 |
| 5 | MUL | `Accel.mul` | 2 |
| 6 | DIV | `Accel.div` | 2 |
| 7 | MIN | `Accel.min` | 2 |
| 8 | MAX | `Accel.max` | 2 |
| 9 | CLIP | `Accel.clip` | 3 |
| 10 | SIGMOID | `Accel.sigmoid` | 1 |
| 11 | RELU | `Accel.relu` | 1 |
| 12 | LEAKY_RELU | `Accel.leakyRelu` | 2 |
| 13 | TANH | `Accel.tanh` | 1 |
| 14 | EXP | `Accel.exp` | 1 |
| 15 | LOG | `Accel.log` | 1 |
| 16 | SQRT | `Accel.sqrt` | 1 |
| 17 | POW | `Accel.pow` | 2 |
| 18 | FMA | `Accel.fma` | 3 |
| 19 | SOFTPLUS | `Accel.softplus` | 1 |
| 20 | SILU | `Accel.silu` | 1 |
| 21 | GELU_ERF | `Accel.geluErf` | 1 |
| 22 | GELU_TANH | `Accel.geluTanh` | 1 |
| 23 | ERF | `Accel.erf` | 1 |

An ID can be present but have its OpenGL/Vulkan support bit clear.  Calling an
unsupported operation in `__gpu func` is a stable compile error.  Do not add an
untested approximation merely to make an ID appear supported.  Implement only
the subset required by the next locked model, while maintaining registry and
diagnostic tests for reserved entries.

Ordinary `+`, `-`, `*`, and `/` remain the preferred spelling for ordinary
arithmetic in generated kernels.  Their MATH IDs are retained for registry
stability and possible future IR builders; the converter need not emit
`Accel.add` for every addition.

### 5.2 Source recognition and context rules

Within `__gpu func`, `Accel` is a reserved compiler namespace.  A parameter or
local may not shadow it.  Recognize only a direct call whose callee is the exact
two-part source form `Accel.<registered-name>`.  Do not recognize
`obj.Accel.sigmoid`, computed properties, aliases, or values extracted from the
dictionary.

The following are errors at compile time:

```noct
func f(x) { return Accel.sigmoid(x); }       /* GPU-only call */
__accel func f(x: float): void { ... }         /* GPU-only call */
__gpu func f(Accel: float): void { ... }       /* reserved name */
__gpu func f(x: float): void {
    let op = Accel.sigmoid;                  /* not first-class */
}
```

Do not register `sigmoid` or the other GPU math names in the runtime `Accel`
dictionary.  Existing runtime entries such as `Accel.copyToAccel`, resource
constructors, dispatch, and join remain unchanged.

Add an optimizer-independent semantic check so a direct GPU-only name outside
`__gpu func` is rejected even at optimization level 0 and when the optional
optimizer is disabled.  A missing dictionary key at runtime is not an
acceptable substitute for this compile-time rule.

In `src/core/hir_gpu.c`, extend expression handling to recognize the call,
check exact arity and float32 scalar operands, and emit through a registry
lowering helper.  Do not paste a second name/arity switch into multiple files.

### 5.3 Numeric policy

Version 1 math operates on float32 values.  Do not enable fast-math.  Each
implemented registry entry must define and test NaN, positive/negative
infinity, both signed zeros, overflow, and domain errors where relevant.

SIGMOID is the stable overflow-safe form:

```text
x >= 0: 1 / (1 + exp(-x))
x <  0: exp(x) / (1 + exp(x))
```

It maps `-Inf` to 0, `+Inf` to 1, both signed zeros to 0.5, and propagates NaN.
MIN, MAX, RELU, and CLIP must implement their documented NaN/signed-zero policy
explicitly rather than inherit driver-specific GLSL behavior.  ERF and GELU_ERF
have no portable core GLSL primitive; leave their backend capability clear
until a reviewed approximation and tolerance suite exists.

Add a separate GPU-only constant constructor if exact float32 source constants
cannot be emitted portably:

```noct
Accel.float32FromBits(bits)
```

It accepts a checked 32-bit bit pattern and lowers to `uintBitsToFloat`.  It is
not an `ACCEL_MATH` function ID and is not a runtime function.  Generated
operator attributes must not depend on locale-specific decimal formatting.

## 6. Required raw `__gpu func` language hardening

Generated tensor kernels need bounded loops and scalar accumulators.  Extend
the existing raw-GPU subset narrowly; do not attempt to make arbitrary Noct
code GPU-callable.

Required additions:

1. Nested ranged-for statements of the existing `for (i in start..stop)` AST
   form.  Version 1 accepts only compile-time integer start/stop values, a
   positive unit step, and no collection iteration.  Emit a structured GLSL
   `for` loop with an `int` induction variable and lexical scope.
2. Reassignment of a declared scalar `int` or `float` local, including an
   accumulator such as `sum = sum + value`.  Continue to reject assignment to
   undeclared or wrong-typed locals.
3. Lexically scoped local and induction-variable metadata in the raw emitter.
   Do not emit arbitrary source symbols without checking that they resolve to a
   parameter, declared local, induction variable, shared object, or reserved
   GPU name.
4. Structured `if`/`else if`/`else` if the AST representation requires it for
   generated templates.  Preserve barrier uniformity validation across every
   branch.
5. Direct GPU MATH pseudo-calls as expressions.

Still reject:

- `while`, collection `for`, dynamic loop bounds, negative/variable steps,
  `break`, and `continue` in version 1;
- allocation, dictionaries, arrays, strings, ordinary function calls, I/O,
  exceptions, closures, and recursion;
- long/double and unsupported Packed element types;
- return values;
- a barrier in nonuniform control flow or any early return in a kernel that
  contains a barrier.

Loop bounds are shape-specialization constants and must be checked by the
converter before source emission.  Also impose a compiler limit on nesting and
generated source size with stable errors.  A model capability failure is not an
out-of-memory strategy.

Add focused raw-GPU tests before any ONNX generator depends on these features:

- nested constant loops and accumulator reassignment;
- loop local shadowing and out-of-scope rejection;
- nonconstant/negative/overflowing bounds rejection;
- MATH calls nested in arithmetic;
- wrong name, arity, type, context, and first-class use rejection;
- barrier validation inside/around loops and conditionals;
- source, `.nb`, `.nap`, interpreter/JIT host launch paths, and OpenGL output.

## 7. Converter-only tensor and storage model

Tensor metadata is a Noct data structure used only by the converter.  Do not
add these fields to `rt_packed`, accelerator resources, raw kernel descriptors,
or VM values.

Each normalized tensor record contains:

```text
tensor_id
diagnostic_name
storage_id
role                 # input, output, initializer, temporary, alias
dtype                # float32 compute/storage in v1
rank                 # 1..8
shape[rank]           # positive checked integers
stride[rank]          # nonnegative element strides; zero allowed for reads
offset                # nonnegative element offset
layout                # STRIDED/C/NCHW/NHWC/OIHW/HWIO/NC
producer_node
consumer_nodes
first_use/last_use
```

All count, offset, range, and byte calculations use checked signed-64-bit logic
and reject before conversion on overflow.  Compute the reachable interval as:

```text
[offset, offset + sum((shape[d] - 1) * stride[d]) + 1)
```

Writes are contiguous, injective, and non-broadcast in version 1.  Read-only
broadcast axes may have stride zero.  Negative strides and zero-sized tensors
are unsupported.

Identity, inference Dropout, legal Reshape/Flatten/Squeeze/Unsqueeze, and
Transpose normally change tensor metadata without launching a kernel.
Materialize with a generated COPY/TRANSPOSE kernel only when a consuming kernel
family cannot represent the view.

One raw kernel parameter binds one unique underlying `storage_id`.  If an
operation reads two views of the same storage, pass the resource once and emit
both indexing formulas against that parameter.  Never pass the same accelerator
resource through two `_ptr` parameters; that violates the existing raw launch
alias contract.  In-place output/input aliasing is unsupported in version 1.

The first planner may allocate one persistent accelerator resource per material
tensor.  Liveness reuse is Stage L optimization and must not complicate the
correctness baseline.

## 8. Kernel-family generator

### 8.1 Definition and selection

A kernel family is reviewed converter code that emits a bounded Noct raw GPU
algorithm for an exact capability regime.  It is not a stored GLSL string and
not a full literal copy for every possible dimension tuple.

Example family keys include:

```text
pointwise-unary/contiguous
pointwise-binary/broadcast-static
copy/strided-read-contiguous-write
conv2d/reference/nchw-oihw/group1-dilation1
conv2d/reference/depthwise
gemm/reference/rank2
pool2d/reference/nchw
reduce/reference/serial-inner
softmax/reference/stable-serial-axis
concat/reference/static-axis
```

Selection is deterministic and exact.  A family declares accepted dtype,
rank/layout, attribute restrictions, device-independent source limits, and
launch strategy.  If no family accepts a node, conversion fails with model,
node ID/name, domain/opset/op type, shapes, attributes, and the first unmet
capability.  Never silently substitute approximate semantics or CPU code.

Specialize shapes, strides, offsets, axes, kernels, pads, dilations, group,
and other static attributes as checked constants.  Keep weights and storage
identities as `_ptr` parameters.  Do not completely unroll a large contraction;
use the newly supported constant ranged-for loops.

A correctness-first generated Conv family should have the following source
shape (illustrative names and dimensions only):

```noct
__gpu func k_conv_nchw_oihw_001(
    input: rpackedfloat_ptr,
    weight: rpackedfloat_ptr,
    bias: rpackedfloat_ptr,
    output: rpackedfloat_ptr
): void {
    let oi: int = globalIdx.x;
    if (oi < 2304) {
        let oc: int = oi / 144;
        let spatial: int = oi % 144;
        let oy: int = spatial / 12;
        let ox: int = spatial % 12;
        var sum: float = bias[oc];
        for (ic in 0..16) {
            for (ky in 0..3) {
                for (kx in 0..3) {
                    sum = sum + input[ic * 196 + (oy + ky) * 14 + ox + kx] *
                                weight[oc * 144 + ic * 9 + ky * 3 + kx];
                }
            }
        }
        output[oi] = Accel.relu(sum);
    }
}
```

The real emitter derives every index and bound from checked normalized metadata.
A kernel without bias omits the bias parameter rather than binding a dummy or
aliased resource.

### 8.2 Specialization reuse

Emit one `__gpu func` per unique specialization, not automatically one per ONNX
node.  Compute a canonical specialization signature containing:

```text
family ID and family version
operator/opset semantics
dtype
all logical shapes/strides/offsets/layouts
canonical attributes
indexing/materialization strategy
block size and shared-memory shape
```

Do not include ONNX names, node IDs, storage IDs, resource names, weight bytes,
or absolute paths.  Nodes with the same signature reuse the same emitted
function and backend pipeline.  Resolve the extremely unlikely hash collision
by comparing complete canonical signature bytes; a short hash alone is not an
identity.

Generated private names use a stable readable prefix plus a collision-checked
digest or ordinal.  Kernel order is first-use order with a stable signature
tie-breaker.

### 8.3 Correctness-first launch model

Initially assign one invocation to one output element.  Flatten the output
index and reconstruct coordinates with constant divisors/moduli.  Every kernel
guards `globalIdx.x < output_element_count`.  Use one-dimensional geometry and
block size 64 unless the family has a separately tested reason to choose
another value:

```text
grid = ceil(output_element_count / block_size)
```

Conv, Gemm, MatMul, pooling, and reductions initially use constant nested loops
inside the owning invocation.  This may be slow but must be bounded, compact,
and debuggable.  Shared-memory tiling, subgroup operations, fusion, and
autotuning are deferred until after the complete correctness ladder.

Use synchronous triple-chevron launches for the first implementation.  This
allows one fence per compute node but ensures resource lifetime and error
behavior are simple.  A later stage may use FIFO `dispatchAsync` and one final
join only after event/resource/error tests prove it correct.

### 8.4 Initial operator mapping

Every handler is keyed by exact imported opset, not merely operator spelling.
The initial capability target is:

| ONNX operator | Generated result | Initial restriction |
|---|---|---|
| Identity, Dropout | metadata alias | inference; optional mask absent |
| Reshape, Flatten, Squeeze, Unsqueeze | metadata reshape | static axes/shape, contiguous when required |
| Transpose | metadata strides or COPY | static full permutation |
| Conv | CONV2D kernel family | float32 NCHW/OIHW; begin group 1, dilation 1 |
| Add/Sub/Mul/Div | pointwise binary | static multidirectional broadcast |
| Relu/LeakyRelu/Sigmoid/Tanh | pointwise unary with `Accel.*` | exact opset defaults |
| Exp/Log/Sqrt/Pow/Clip | pointwise MATH | only after GPU math capability test |
| MaxPool | pool family | no indices; begin ceil 0, dilation 1 |
| AveragePool | pool family | exact count/include-pad rule |
| GlobalAveragePool | reduction family | NCHW |
| Gemm | GEMM family | rank 2, static alpha/beta/transposition |
| MatMul | MATMUL family | rank 2 initially |
| Concat | CONCAT family | static axis and bounded input count |
| BatchNormalization | fold or kernel family | inference, constant parameters |
| ReduceSum/Mean/Max/Min | reduction family | static axes/keepdims |
| Softmax/LogSoftmax | stable reduction family | opset-specific axis |
| Shape/Gather/Cast/Constant | converter-only fold | fully constant result |

Accept an optional ONNX output only when it is empty unless its exact semantics
are implemented.  Reject MaxPool indices, Dropout mask, or any other nonempty
extra output even when apparently dead.

## 9. Binary foundation and NWT1

The binary and immutable sidecar contract is fully specified here; do not look
to the superseded design 17 for missing behavior.  None of these APIs live
under a `DNN` runtime namespace.

Required generic APIs include:

```text
File.readExact(file, byteCount) -> Packed.uint8
File.writeAll(file, bytes, byteOffset, byteCount) -> void
FileUtil.makeDirectoryExclusive(path) -> void
Binary.readVarintUnsigned(bytes, offset, limit)
Binary.readVarintSigned(bytes, offset, limit)
Binary.skipVarint(bytes, offset, limit)
Binary.readU32LE(bytes, offset)
Binary.readI64LE(bytes, offset)
Binary.writeU32LE(bytes, offset, value)
Binary.writeU64LE(bytes, offset, value)
Binary.readFloat32LE(path, elementCount) -> Packed.float32
Binary.writeFloat32LE(path, packed) -> void
Hash.sha256(bytes) -> lowercase hex
Hash.sha256Bytes(bytes) -> 32 raw digest bytes
```

The varint read functions return the decoded 64-bit bit pattern as a Noct
`long`; `readVarintSigned` applies protobuf `int64` two's-complement
interpretation, not zigzag decoding.  `skipVarint` returns the first offset
after the canonical encoding.  `readU32LE` returns a nonnegative `long` and
`readI64LE` returns the signed 64-bit value.

Implement exact/short-read distinction, checked nonnegative size conversion,
ten-byte varint validation, signed int64 conversion, endian independence,
native-handle kind/owner/closed checks, and C89 portability.  Never use
unaligned pointer casts or NUL-terminated assumptions for binary data.

Both converter and generated model use one immutable canonical little-endian
NWT1 `model.weights` file.  Never dump a C struct directly.  Header fields are:

```text
magic[8]             = "NOCTWGT\0"
major:u16            = 1
minor:u16            = 0
header_bytes:u32      = 104 in v1
flags:u32            = 0 in v1
entry_count:u32
directory_bytes:u64
payload_bytes:u64
model_sha256[32]     # raw digest of input ONNX bytes
payload_sha256[32]   # raw digest of the payload section
```

Each variable-length directory entry is:

```text
entry_bytes:u32
name_bytes:u16
dtype:u8             # 1 = float32, the only runtime weight dtype in v1
rank:u8              # 1..8
flags:u32            = 0 in v1
payload_offset:u64   # relative to payload-section start
byte_length:u64
dimensions[rank]:u64
utf8_name[name_bytes]
zero padding to an 8-byte boundary
```

The payload section begins on a 64-byte boundary and every tensor payload is
64-byte aligned.  Entries are ordered by UTF-8 bytewise initializer name, with
original initializer order as the documented tie breaker.  Reject duplicate or
empty names.  Every dimension is positive, the checked element product is at
most `INT_MAX`, and `byte_length` is exactly product times four.  Shape-only
int32/int64 initializers are consumed by the converter and omitted from NWT1.

The reader validates complete file SHA-256 against the generated manifest
argument, magic/version/header size, all reserved fields, section sizes,
zero padding, entry length/name encoding, directory order, alignment, checked
ranges, nonoverlap, payload hash, dtype/shape/count, and absence of trailing
bytes before returning a handle.  The complete pack hash authenticates the
header's `model_sha256` field; the converter must populate that field from the
input ONNX bytes and publish the same identity in the later manifest.  A
malformed entry is rejected before any weight is exposed or GPU resource is
changed.

Expose weight loading through the frozen namespace `Weights`:

```text
Weights.open(path, expectedPackSha256)
Weights.loadFloat32(handle, entryIndex, expectedName, expectedShape)
Weights.close(handle)
```

`Weights.open` verifies the complete pack hash and returns an opaque frozen
handle.  `Weights.loadFloat32` verifies index, expected name, and expected shape,
then allocates a host `Packed.float32` and decodes IEEE-754 bytes with `memcpy`
plus host-endian conversion.  `Weights.close` is idempotent.  Handles are
type-tagged, owner-VM checked, GC-safe, and registered for VM teardown.  Their
finalizer is idempotent, non-allocating, and performs no GPU call.  Do not
serialize handles or weights into `.nb`/`.nap`.

The converter validates all ONNX and generated content in memory before
creating output.  It creates a new exclusive output root, never overwrites, and
writes `manifest.json` last as the commit marker.

## 10. ONNX reader and normalized graph

Implement the production converter under `tools/onnx2noct/` in Noct.  Do not
vendor a general protobuf runtime.  Use a bounded cursor supporting wire types
0, 1, 2, and 5; reject groups, field zero, overlong varints, child-cursor
overrun, malformed lengths, and configured resource limits.

The fixed version-1 limits are:

| Resource | Limit |
|---|---:|
| ONNX file bytes | 512 MiB |
| message nesting | 32 |
| graph nodes | 100,000 |
| initializers | 100,000 |
| attributes per node | 256 |
| rank | 8 |
| one name/string | 1 MiB |
| total decoded objects | 1,000,000 |
| logical elements in one tensor | `INT_MAX` |

Define official field numbers explicitly in `onnx_schema.noct`; do not infer
them from encoded samples.  Decode at least:

| Message | Required fields |
|---|---|
| ModelProto | `ir_version=1`, `graph=7`, `opset_import=8`; inspect/reject `training_info=20`, `functions=25` |
| GraphProto | `node=1`, `initializer=5`, `input=11`, `output=12`, `value_info=13`; reject `sparse_initializer=15` |
| NodeProto | `input=1`, `output=2`, `name=3`, `op_type=4`, `attribute=5`, `domain=7` |
| AttributeProto | `name=1`, `f=2`, `i=3`, `s=4`, `t=5`, `g=6`, `floats=7`, `ints=8`, `strings=9`, `tensors=10`, `graphs=11`, `type=20` |
| TensorProto | `dims=1`, `data_type=2`, `float_data=4`, `int32_data=5`, `int64_data=7`, `name=8`, `raw_data=9`, `double_data=10`, `uint64_data=11`, `external_data=13`, `data_location=14` |
| ValueInfoProto | `name=1`, `type=2` |
| TypeProto | `tensor_type=1`; nested tensor `elem_type=1`, `shape=2` |
| TensorShapeProto | `dim=1`; Dimension `dim_value=1`, `dim_param=2` |
| OperatorSetIdProto | `domain=1`, `version=2` |

Support packed and unpacked repeated scalars.  A length-delimited field uses a
bounded child cursor which cannot read into its parent.  Unknown fields of
supported wire types are skipped exactly.  A varint has at most ten bytes and
the tenth byte is validated.  Preserve TensorProto `raw_data` and packed float
fixed32 values bit-for-bit.  Decode int32/int64 initializer values only for
static shape/axis computation.  Reject competing tensor data fields, segments,
wrong byte counts, undefined/string/double/runtime-integer dtypes, and external
data.

Structural validation, before normalization, must:

1. require `ir_version`, exactly one GraphProto, and a default-domain opset;
2. accept only default domain spellings `""` and `"ai.onnx"`;
3. resolve every node by domain, op type, and exact imported opset handler;
4. require one non-initializer graph input and one graph output, both float32,
   fixed positive shape, and batch one;
5. require unique nonempty initializer/value names and normalize the legal old
   IR case where an initializer also appears in graph inputs;
6. require every node input to be graph input, initializer, or earlier output;
   reject duplicate outputs, cycles, missing values, and undefined output;
7. validate input/output counts, optional-empty positions, attribute names,
   types, defaults, and schema-generation semantics;
8. infer every output shape and compare present value-info/output declarations;
9. validate initializer element and byte counts with checked arithmetic;
10. reject dynamic/symbolic/zero dimensions, external/sparse/training/function/
    control-flow data, custom domains, runtime integer tensors, multiple I/O,
    non-float compute, and every unsupported attribute with node/opset context.

Normalize to stable node/tensor/storage IDs, exact registry/operator identity,
canonical attributes using integer or float32-bit representation, static
shapes/strides/layouts, and original provenance for diagnostics.  Topological
order preserves original node order when multiple nodes are ready.  ONNX names
are escaped into comments/JSON and never copied as executable source.

Initial imported opsets are exactly 7, 8, and 12.  Do not implement acceptance
as `opset <= 12`; use explicit handler generations and defaults.

## 11. Generated model and public ABI

The converter command is:

```sh
noct --path=tools/onnx2noct tools/onnx2noct/main.noct \
  --output=build/generated/mnist mnist-12.onnx
```

Supported options are required `--output=DIR` and optional
`--emit-main=yes|no` (default yes).  There is no `--target=cpu` or
`--target=both`; if compatibility parsing is temporarily retained, those
values must be rejected with a message directing the user to the GPU-only
design.  Unknown, duplicate, or conflicting options are errors.

Generated `model.noct` exports the same stable orchestration API:

```text
modelInfo() -> dict
modelInitialize(weightsPath) -> void
modelWarmup() -> void
modelInfer(input, output) -> void
modelShutdown() -> void
```

Behavior:

- `modelInitialize` validates NWT1 before model-state mutation, allocates or
  activates persistent resources, loads host weights, uploads each weight once,
  and rejects a second initialization until shutdown;
- `modelWarmup` uses deterministic zero input and private output to compile all
  required raw pipelines; it never reloads weights;
- `modelInfer` is synchronous, non-reentrant, and not thread-safe; it validates
  exact distinct host `Packed.float32` input/output before changing output,
  uploads input once, launches the planned kernel sequence, and downloads only
  final output;
- a missing/disabled/unsupported GPU or failed raw dispatch is an error, never
  CPU fallback or replay;
- `modelShutdown` is idempotent and makes the model unusable; persistent top-level
  accelerator resources may remain until VM teardown if no explicit destroy API
  exists;
- `modelInfo` reports target `gpu`, hashes, opsets, exact I/O metadata, kernel
  family versions, and raw-output meaning.

The existing `accel var` mechanism constructs resource objects during module
initialization.  `modelInitialize` therefore does not promise that no resource
object exists before weight validation; it promises that the model initialized
flag is false, no weight/input bytes have been uploaded, and no kernel has been
submitted until the complete NWT1 validation succeeds.

Use one top-level typed `accel var` per initially planned storage:

```noct
accel var model_input = Accel.float32(784);
accel var weight_000 = Accel.float32(800);
accel var temp_003 = Accel.float32(2304);
accel var model_output = Accel.float32(10);
```

The converter emits checked decimal element-count literals in these
constructors.  It may also emit ordinary metadata constants for reporting, but
resource construction must not depend on runtime shape evaluation.

Generated raw kernels accept only scalar `int`/`float` and typed `_ptr`
parameters permitted by design 16.  Every call passes distinct storage
resources once.  Generated host wrappers never subscript accelerator resources.

`gpu/main.noct` uses `require model;`, defines the only public `main(args)`,
and accepts exactly:

```text
<model.weights> <input.f32le> <output.f32le>
```

It reads exact little-endian float32 input, allocates exact output, initializes,
warms only when explicitly requested by generated policy (not timed), performs
one inference, writes the entire output, prints bounded metadata/first values,
and shuts down.  Backend selection is exclusively the Noct CLI.

Native-Linux source execution is exemplified by:

```sh
cmake -S . -B build-onnx-opengl \
  -DNOCT_ENABLE_ACCEL_OPENGL=ON \
  -DNOCT_ENABLE_API_FILE=ON \
  -DNOCT_ENABLE_MODEL_WEIGHTS=ON
cmake --build build-onnx-opengl -j

EGL_PLATFORM=surfaceless ./build-onnx-opengl/noct --accel=opengl \
  --path=build/generated/mnist/gpu \
  build/generated/mnist/gpu/main.noct \
  build/generated/mnist/model.weights input.f32le output.f32le
```

Do not include a D3D12 environment variable or renderer expectation in the
generated source, manifest, or generic tests.

## 12. Deterministic source and manifest rules

For identical converter build, ONNX bytes, and options, every artifact byte is
identical.  Emit UTF-8 with LF, no timestamp, no absolute path, stable ordering,
canonical JSON key order, exact float32 attribute bits, and collision-safe
ASCII private identifiers.  Generate source from arrays/streams of lines; do
not repeatedly concatenate one multi-megabyte immutable string.

`manifest.json` records:

- manifest/converter version;
- source model SHA-256, IR version, exact opset imports, source/license lock ID;
- NWT1 hash and file hash;
- exact input/output dtype, shape, layout, and byte counts;
- normalized operator class/IDs and opset provenance;
- emitted kernel specialization signatures, family versions, launch sizes, and
  source file hashes;
- numeric tolerances and raw-output meaning;
- no source absolute path, timestamp, driver name, device ID, native handle, or
  pipeline cache.

Sanitize names to `[A-Za-z0-9_]`, prefix a leading digit, escape other UTF-8
bytes as `_hh`, and append stable IDs for private model symbols.  Never paste
untrusted ONNX strings into code outside escaped comments.

## 13. File-level implementation map

### 13.1 Core/raw GPU compiler

| File | Required change |
|---|---|
| `src/core/accel_ops.def` | explicit GPU op classes/MATH IDs, spellings, arity, type/capability metadata |
| `src/core/accel_ops.h/.c` | C89 registry lookup/validation and lowering metadata |
| `src/core/hir_gpu.c` | ranged-for, scalar reassignment/scope, direct `Accel.*` MATH recognition, stable emission/errors |
| `src/core/hir.c` | optimizer-independent rejection of GPU-only calls outside raw `__gpu func`; preserve existing function-kind rules |
| `src/core/hir.h` | declarations only if required; do not add DNN Plan or runtime Tensor View fields |
| `src/core/accel.h` | operation-class declarations if shared; do not add ordinary bytecode or a DNN function kind |
| `src/api/accel.c` | normally no MATH cfuncs; retain resource/dispatch APIs and frozen `Accel` runtime dict |
| `src/api/accel_opengl.c` | no ONNX/DNN lowering; only generic raw pipeline fixes proved necessary by tests |
| `src/api/accel_vulkan.c` | keep building; later raw-GPU execution stage only |

Do not change lexer/parser syntax for `Accel.sigmoid`; it uses existing dot/call
syntax.  Parser regeneration is needed only if the ranged-for syntax itself is
changed, which this plan does not request.

### 13.2 Generic binary/weight runtime

| File | Required change |
|---|---|
| `src/api/api-file.c` | exact read/write, exclusive directory, native File kind checks |
| `src/api/api-binary.c` | checked endian/varint/float tensor file helpers |
| `src/api/api-weights.c` | frozen NWT1 handle/open/load/close implementation |
| `src/core/sha256.h/.c` | C89 streaming SHA-256 shared by converter-facing APIs |
| `src/core/intrinsics.c` | register Binary/Hash/Weights only under correct feature gates |
| `src/core/runtime.h/.c` | native handle registry/teardown only if common infrastructure requires it |
| `src/core/gc.c` | edit only when tests prove generic handle rooting/finalization needs collector support |

Add `NOCT_ENABLE_MODEL_WEIGHTS` with default OFF.  Enabling it requires
`NOCT_ENABLE_API_FILE=ON` and builds/registers NWT1 `Weights` support.  Checked
File/Binary/Hash primitives used by the converter follow the existing File API
gate.  GPU math/raw compilation belongs with the existing accelerator compiler
and is independent of the weight gate.  Do not add `NOCT_ENABLE_DNN`; there is
no DNN runtime.  DNN-off is not a meaningful build state in this design.

### 13.3 Converter

```text
tools/onnx2noct/main.noct
tools/onnx2noct/protobuf.noct
tools/onnx2noct/onnx_schema.noct
tools/onnx2noct/onnx_ir.noct
tools/onnx2noct/operators.noct
tools/onnx2noct/shape.noct
tools/onnx2noct/normalize.noct
tools/onnx2noct/planner.noct
tools/onnx2noct/weights.noct
tools/onnx2noct/kernel_registry.noct
tools/onnx2noct/kernel_common.noct
tools/onnx2noct/kernel_pointwise.noct
tools/onnx2noct/kernel_conv.noct
tools/onnx2noct/kernel_pool.noct
tools/onnx2noct/kernel_reduce.noct
tools/onnx2noct/kernel_tensor.noct
tools/onnx2noct/emit_model.noct
tools/onnx2noct/emit_main.noct
tools/onnx2noct/emit_manifest.noct
```

Only `main.noct` defines the converter's public main when bundled.  Keep module
helpers static where supported.  Kernel modules generate Noct source only; they
must not depend on OpenGL headers, shader syntax, or a live GPU.

### 13.4 Tests

```text
tests/accel/gpu-math-*.noct
tests/accel/gpu-loop-*.noct
tests/dnn/models.lock
tests/onnx2noct/fixtures/
tests/onnx2noct/golden/
tests/onnx2noct/oracle/
tests/generated-models/
tests/run-onnx2noct.sh
tests/run-onnx-gpu-opengl.sh
tests/run-onnx-gpu-vulkan.sh      # add only in the later Vulkan stage
```

The name `tests/dnn/models.lock` may be retained for compatibility even though
there is no DNN runtime; do not interpret the directory name as permission to
reintroduce `DNN.*`.

## 14. Ordered implementation stages

One lower-model task implements one stage only.  Do not begin the next stage
while the current gate is red.  The original review-pause convention was
superseded by decision 15 for this implementation run.  Every handoff names
the first incomplete gate precisely.

### Stage A — replace plan, lock models, and record native-Linux baseline

- land this design as the new authority and mark design 17/old handoff
  superseded;
- create `tests/dnn/models.lock` with exact URL, SHA-256, byte size,
  license/source notice, IR, opsets, I/O, operators, and attributes for
  MNIST-12, project CIFAR opset 12, SqueezeNet 1.1 opset 7, and Tiny YOLOv2
  opset 8;
- create tiny project-owned ONNX fixtures and fixed `.f32le` inputs/outputs;
- add a pinned Python inventory/oracle environment under tests (required for
  model gates, never a production dependency);
- record `run-accel.sh`, native Intel OpenGL `run-accel-opengl.sh`, and a short
  CIFAR smoke result;
- make the OpenGL test harness platform-neutral: accept externally selected
  EGL/Gallium environment and validate hardware versus software renderer rather
  than hard-code D3D12.

Gate: documentation/model-lock/fixture review; existing accelerator suites pass
with `EGL_PLATFORM=surfaceless`; renderer evidence names Mesa Intel; software
rejection uses `LIBGL_ALWAYS_SOFTWARE=1`; no ONNX implementation claim.

### Stage B — raw GPU loops, locals, and MATH pseudo-intrinsics

- add the explicit operation registry and MATH lookup;
- implement nested constant ranged-for and scalar reassignment/scopes in
  `hir_gpu.c`;
- implement direct GPU-only `Accel.*` calls and outside-context rejection;
- implement only the MATH subset required by micro/MNIST inventory, with exact
  policy tests;
- preserve raw descriptor source/bytecode/app roundtrip.

Gate: all focused valid/error tests in section 6, CPU/static builds rejecting
GPU-only calls deterministically, hardware OpenGL differential tests, existing
accelerator tests, optimizer levels 0/1/2 and optimizer-off build, `.nb/.nap`,
ASan, MinGW, and OpenWatcom compile.

### Stage C — checked Binary/Hash/Weights and NWT1

- implement exact File/Binary/Hash APIs and safe native type checks;
- implement exclusive output-root policy;
- implement deterministic NWT1 writer/parser/load and float32 LE helpers;
- implement frozen handle owner/close/finalizer/teardown behavior.

Gate: EOF/short I/O, endian golden, signed/unsigned ten-byte varint, every size
overflow boundary, malformed/truncated/overlap/padding/hash NWT1, wrong handle
kind/VM, GC movement, repeated VM lifecycle, DNN namespace absent, feature-off,
MinGW/OpenWatcom builds.

### Stage D — Noct protobuf and structural ONNX reader

- implement bounded protobuf cursor and required schema readers;
- decode initializers and static shape metadata without generating source;
- implement topology/domain/opset/name/dtype/fixed-I/O validation;
- reject every explicit non-goal.

Gate: Noct reader passes checked fixtures and malformed corpus including
truncation at every byte, overlong varint, bad wire type/length/depth, unknown
field skip, duplicates, cycle, external/sparse/function/training/dynamic shape.
Python reader success is not a gate substitute.

### Stage E — exact operator schemas and normalized converter graph

- implement opset 7/8/12 handler tables and exact defaults;
- implement shape/layout/stride inference and converter-only view folding;
- implement stable graph dump, name sanitation, storage identity, and canonical
  float-bit attributes;
- map each candidate model inventory to exact supported/missing capabilities.

Gate: byte-identical normalized graph goldens, inferred/declaration mismatch
errors, broadcast/view/range/overflow tests, and a precise capability list for
each locked real model.  No GPU source generation yet.

### Stage F — deterministic kernel framework and pointwise micro models

- implement kernel registry/signature/deduplication and line-stream writer;
- implement persistent storage planner with one resource per material tensor;
- implement COPY, metadata views, unary pointwise, and binary broadcast
  families;
- emit a minimal GPU-only `model.noct` for checked micro fixtures.

Gate: two conversions produce identical source bytes; generated source contains
only allowed raw GPU constructs; duplicate specializations reuse one function;
same-storage reads bind once; source and `.nap` execute on hardware OpenGL and
match oracle; disabled GPU is an error; no `__accel func`, `DNN.*`, DPL1, CPU
model, direct GLSL, timestamp, or absolute path appears.

### Stage G — contraction, pooling, concat, and reduction families

Implement in dependency order, with a separate review/gate for each family:

1. reference Conv2D group 1/dilation 1;
2. rank-2 Gemm/MatMul;
3. MaxPool/AveragePool/GlobalAveragePool;
4. Concat and required materializing copy/transpose;
5. ReduceSum/Mean/Max/Min and stable Softmax/LogSoftmax;
6. BatchNorm reference or proven Conv fold;
7. grouped/depthwise/dilation only when a locked model proves it necessary.

Gate per family: hand-computable tensors, boundary/padding/broadcast cases,
wrong attributes rejected during conversion, deterministic generated golden,
hardware OpenGL versus ONNX Runtime, no output OOB under sanitizer, and all
earlier families/models remain green.

### Stage H — complete GPU package, NWT1, manifest, and sample main

- integrate deterministic weight extraction and manifest-last publication;
- generate complete persistent resources, initialization/uploads, kernel
  sequence, final download, shutdown, common ABI, and sample main;
- compile source into `.nap` with explicit external weight path.

Gate: artifact hashes repeat across two clean conversions; corrupt/missing
weights and short/wrong input fail before caller output commit; only one input
upload/one final output download occurs; weights upload once; source and `.nap`
work after artifact relocation with explicit paths; GPU unavailable is a stable
error.

### Stage I — model ladder

Bring up one locked model at a time and stop after each for review:

1. MNIST-12, output `1x10`;
2. project CIFAR opset 12, output `1x10`;
3. SqueezeNet 1.1 opset 7, classification logits;
4. Tiny YOLOv2 opset 8, raw `1x125x13x13` tensor.

Add only operator/schema/kernel regimes required by the current exact locked
model.  Do not infer requirements from a README or a differently hashed model.

Gate per model: verified download lock, ONNX Runtime input/output oracle,
generated file hashes, source and `.nap`, hardware OpenGL interpreter/JIT host
paths, exact output shape, maximum absolute/relative error, tolerance and
argmax where meaningful, no software renderer, no CPU fallback/replay, and
bounded warmup/steady-state timing recorded separately.

### Stage J — persistence, portability, and regression hardening

- test source/bytecode/app relocation and malformed raw descriptors;
- run static/debug/OpenGL/OpenGL sanitizer, optimizer off, MinGW, OpenWatcom,
  and relevant cross-JIT compile gates;
- define C/Elisp/Scheme behavior explicitly as unsupported for raw GPU model
  execution rather than accidental emission;
- update syntax/library/vmspec/API docs and current handoff;
- run existing syntax/typing/ABCE/SIMD/CSE/app/remacs/accelerator suites.

Gate: all required builds/tests green or documented pre-existing baseline,
disabled builds contain no OpenGL headers/unresolved symbols, artifacts contain
no pointer/path/driver cache, and the old DNN architecture is absent.

### Stage K — optional Vulkan raw-model execution

Begin only after Stage J and explicit owner confirmation.  Validate and repair
the existing raw `__gpu func` Vulkan resource binding, launch variants, `_ptr`,
shared/barrier, MATH lowering, events, copies, and error paths.  The converter
and generated source must remain unchanged; only backend capability/manifest
claims and Vulkan tests are added.

Gate: every raw language and micro operator differential first, then each model
in ladder order; no staging behavior that violates public `_ptr` semantics; no
claim based only on `build-vulkan` success.

### Stage L — optional performance after correctness

Only after the selected backend/model gates are green:

- fold BatchNorm/constants with oracle proof;
- reuse persistent temporaries by liveness;
- add tiled/shared Conv and MatMul families;
- add shared/tree reductions and subgroup variants behind capability checks;
- deduplicate or fuse compatible pointwise kernels;
- replace per-node synchronous launch with validated FIFO submission and one
  final join;
- add layout conversion, cost model, and stable family versioning;
- benchmark after warmup with pipeline compilation excluded.

Performance changes must not alter the public model ABI, NWT1 bytes, normalized
ONNX semantics, stable MATH IDs, or deterministic converter output except for
an explicitly versioned kernel-family selection change.

## 15. Tests, tolerances, and failure rules

Every malformed input, unsupported model feature, raw GPU source error, missing
backend capability, weight mismatch, and range/alias error is a hard error.
Do not warn and continue with different semantics.  Because generated kernels
are raw `__gpu func`, backend failure never selects CPU execution.

Initial comparison guidance:

- exact for IDs, shapes, metadata, copies, and argmax tie behavior;
- simple pointwise arithmetic: `abs <= 1e-6 + 1e-6*abs(reference)` where exact
  comparison is not practical;
- transcendentals: `abs <= 2e-6 + 2e-5*abs(reference)`;
- Conv/Gemm/reductions: begin with
  `abs <= 1e-4 + 2e-5*abs(reference)`;
- NaN must match NaN, infinity sign must match, and a finite/nonfinite mismatch
  always fails;
- classification argmax must match in addition to tensor tolerance.

These are starting limits, not permission to loosen a failure.  Record actual
maximum errors for each locked vector and review changes.  ONNX Runtime is the
mandatory model oracle now that no Noct CPU model is generated, but it remains
tests-only and absent from production converter/model dependencies.

Fuzz/bound the C NWT1 loader and Noct protobuf cursor.  A malformed input must
not assert, crash, overrun, wrap, allocate without limit, partially publish a
manifest, partially commit caller output before preflight, or leak a native
handle.

## 16. Instructions for lower-level implementing models

Before editing:

1. read this file completely;
2. read the relevant design-16 sections for raw GPU semantics;
3. inspect current files named in the stage; do not rely only on this plan's
   baseline audit if HEAD changed;
4. record `git status --short`, preserve all unrelated/owner edits, and do not
   reset or commit;
5. implement only the currently assigned stage and gate.

While editing:

- use C89 in `src/core/` and preserve all build feature guards;
- initialize every new enum/state field explicitly; zero must not accidentally
  mean a valid proven state;
- update clone/free/dump/serialization paths only for data actually added;
- do not create a new VM opcode for GPU MATH;
- do not register runtime `Accel.sigmoid` cfuncs;
- do not add DNN Plan/Tensor View/CPU model infrastructure opportunistically;
- do not broaden raw GPU source beyond the subset required by this plan;
- do not edit generated lexer/parser sources unless authoritative grammar
  changed and regeneration is verified;
- never bypass `_ptr` alias checks or pass one resource twice;
- never hide a failing GPU operator behind CPU behavior;
- never embed third-party models, large generated weights, credentials, caches,
  timestamps, or absolute workstation paths in git.

Before handing off:

- run the exact stage gate and relevant existing regression suites;
- run `git diff --check` and inspect the complete diff/stat;
- state the exact HEAD/branch, dirty files, completed stage, first incomplete
  gate, renderer/backend evidence, and whether failures are code/model/environment;
- state explicitly that production has no Python dependency, no CPU model, no
  DNN runtime, no DPL1, and no D3D12 assumption;
- do not claim Vulkan model execution until Stage K passes.

## 17. Explicitly deferred work

- generated CPU inference and CPU fallback;
- `dnn func`, public/runtime Tensor Views, `DNN.*`, DPL1, or descriptor-v3 DNN
  programs;
- source-callable `ACCEL_REDUCE` and `ACCEL_TENSOR` whole-tensor operations;
- dynamic/symbolic/zero shapes, multiple I/O, non-float32 and quantized models;
- public `_ptr` subviews, negative strides, in-place compute, training;
- external/sharded/memory-mapped weights and files over the current limit;
- custom domains, FunctionProto, control flow, sparse/sequence/map/optional;
- preprocessing, image/audio codecs, tokenizers, YOLO decoding, NMS, labels;
- fusion, autotuning, asynchronous public inference, reentrant model instances;
- web/server/daemon/UI work.

Any proposal to restore one of these items requires an owner decision and a
new design revision; a lower implementing model must not infer permission from
an adjacent capability.
