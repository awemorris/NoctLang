# Accelerator execution model: managed `__accel func` and raw `__gpu func`

> **Current managed-function semantics:** design 19 supersedes this document's
> CPU-fallback, single-loop, and serial managed-`__accel func` rules.  On branch
> `accel`, `__accel func` is GPU-only and lowers to a versioned ordered program
> of DOALL/DOSUM kernels.  A disabled or unavailable backend is an error and
> never executes the source body on the CPU.  This document remains
> authoritative for raw `__gpu func`, accelerator resources/copies/events, and
> backend details not replaced by
> [design 19](19-accel-multikernel-doall-dosum.md).

Status: managed/raw language and OpenGL runtime baseline implemented on branch
`accel`; full contract hardening and reusable Kernel IR remain incomplete

This document is the authoritative implementation contract for Noct accelerator
work. It replaces the original Vulkan-first plan with the two-function model
approved after the OpenGL milestone:

- `__accel func` is a managed, CPU-executable function with optional GPU lowering
  and automatic single-loop DOALL parallelization.
- `__gpu func` is a raw GPU kernel. It has no CPU implementation and is launched
  only through the raw dispatch interfaces.

If an older commit message, test name, or design note conflicts with this file,
this file wins. The general CPU multigrain research in
[10-future-multigrain-parallelization.md](10-future-multigrain-parallelization.md)
is not activated by this GPU-only DOALL plan.

High-level ONNX work is specified separately in
[18-onnx-gpu-source-codegen.md](18-onnx-gpu-source-codegen.md).  It emits actual
raw Noct `__gpu func` source and consumes this document's raw function, `_ptr`,
launch, resource, and alias semantics.  It does not add a DNN function kind or
change public `__accel func` semantics.

## 1. Baseline and implementation audit

The original planning baseline was branch `accel` at commit `e529952`.  The
language/runtime implementation landed at
`9b46d6927055d92c4d70e537a3f66f0f9b6ecf41`, and the current audited baseline
is `96fde9ad6d09a25e0baccc29de0c527382859650`.  The latter adds the small CNN
and CIFAR-shaped CPU/GPU evaluations.  Preserve all of the following:

- `__accel func` syntax, mandatory `void` return, ordinary-call rejection, and
  CPU fallback;
- synchronous managed `Accel.call()`, raw `Accel.dispatchAsync()`, synchronous
  triple-chevron launch, and the removal of managed `Accel.callAsync()`;
- synchronous and asynchronous byte-range copies plus generation-safe events;
- top-level typed resources declared as
  `accel var data = Accel.uint32(LENGTH);`;
- constructors for Packed element kinds `int8`, `int16`, `int32`, `int64`,
  `uint8`, `uint16`, `uint32`, `uint64`, `float32`, and `float64`;
- a backend-neutral common runtime, a retained Vulkan prototype, and a
  headless EGL/OpenGL 4.3 compute backend;
- OpenGL FIFO ordering, `GLsync` events, persistent SSBO resources, host/device
  generation coherence, CPU fallback synchronization, and stale-event
  protection;
- current GLSL lowering for the 32-bit `int32`, `uint32`, and `float32`
  operation subset, including managed parallel/sequential strategies and raw
  GPU IDs/shared memory;
- source, bytecode, `.nap`, interpreter, JIT, C-translation, sanitizer, and
  OpenGL test coverage already present under `tests/accel`.

The validated native-Linux headless execution command is:

```sh
EGL_PLATFORM=surfaceless noct --accel=opengl -O2 program.noct
```

The audited native renderer is Mesa Intel Iris Xe OpenGL 4.6/EGL 1.5.  WSL2
may separately select Mesa D3D12, but D3D12 is not a general requirement.
Every platform must report a hardware-backed renderer.  Software renderers such
as llvmpipe remain an error for explicit `--accel=opengl`.
The Vulkan backend is retained but was not validated during the OpenGL work.
Do not run Vulkan execution tests while implementing this plan unless the
project owner explicitly reopens Vulkan validation.

The implementation is a functional baseline, not proof that every acceptance
detail below is complete.  The audit at `96fde9a` is:

| Area | Audit status |
|---|---|
| function kind, transports/effects, `_ptr` | implemented baseline |
| managed range metadata | implemented as a restricted count-plus-offset summary; the general owned expression program below is incomplete |
| managed DOALL/serial lowering | implemented restricted dependence test and both strategies; the full analysis contract below is incomplete |
| raw `__gpu func`, IDs, dispatch, launch | implemented baseline |
| shared storage and barrier validation | implemented baseline |
| events/resource lifetime/pipeline variants | implemented baseline with existing tests; keep auditing maximum-arity GC and all error unwinds |
| descriptor persistence | implemented experimental metadata; complete versioning/legacy hardening remains |
| Kernel Accel IR | incomplete: managed and raw paths still construct GLSL while walking HIR/AST instead of first building the validated reusable IR required by section 6 |
| Vulkan | retained and buildable; execution untested |

Do not layer a second incompatible shader generator over these shortcuts.  New
DNN work must first close or deliberately reuse the common Kernel IR boundary.

## 2. Fixed language and runtime decisions

The following decisions are settled for the first implementation. Do not
silently reinterpret them while coding.

1. There are three function kinds: ordinary, managed accelerator, and raw GPU.
   Use one enum throughout AST, HIR, LIR, runtime functions, bytecode metadata,
   dumps, and source backends. Do not add independent `is_accel` and `is_gpu`
   booleans that can form an invalid combination.
2. Both `__accel func` and `__gpu func` must explicitly return `void`. Neither may
   be called with the ordinary call convention or used as a first-class value.
3. `__accel func` always keeps its ordinary CPU body and can execute on the CPU.
   GPU rejection or backend unavailability may select that CPU body.
4. `__gpu func` has no CPU body. Unsupported raw-GPU source is a compile error.
   A disabled, unavailable, or failed GPU backend is a runtime error, never a
   CPU fallback.
5. `Accel.call(accel_function, arguments...)` is the only managed-kernel call.
   It is synchronous and returns `void`.
6. Remove the public `Accel.callAsync()` API. Do this only after the replacement
   raw async dispatch and its event lifetime rules are working.
7. `Accel.dispatchAsync(gpu_function, grid_count, block_size, arguments...)` is
   the only asynchronous GPU-kernel launch. It returns the existing opaque
   accelerator event-ID kind.
8. `gpu_function<<<grid_count, block_size>>>(arguments...);` is Noct-specific
   synchronous syntax. Unlike a CUDA launch expression, it waits for
   completion. Its semantic expansion is raw async dispatch followed by
   `Accel.join()`, although lowering need not literally build two source calls.
9. `grid_count` means the number of one-dimensional workgroups. `block_size`
   means local invocations per workgroup. Both are positive `int` values.
10. Version 1 launch geometry is one-dimensional. Three-dimensional
    `Accel.dim3` and y/z source components are deferred.
11. Raw GPU source exposes `threadIdx.x`, `blockIdx.x`, `blockDim.x`,
    `gridDim.x`, and `globalIdx.x`. They have Noct type `int` and are
    non-negative. The runtime rejects launch geometry that cannot be
    represented safely as `int`.
12. `globalIdx.x` is equivalent to
    `blockIdx.x * blockDim.x + threadIdx.x`. The emitter may map that canonical
    expression directly to `gl_GlobalInvocationID.x`.
13. `_in` and `_out` are managed `__accel func` transports for ordinary host
    Packed objects. `_ptr` is a typed view of an existing `Accel.*` resource.
14. A `__gpu func` may have scalar parameters and `_ptr` buffer parameters. It
    may not have `_in` or `_out` parameters.
15. `_ptr` does not allocate or copy a call-specific argument buffer. It aliases
    the existing accelerator resource storage. Coherence synchronization or an
    internal device/host transfer required for managed CPU fallback is still
    permitted; it is not source-level argument copying.
16. All restricted Packed buffer arguments of one kernel call are pairwise
    non-aliasing and are also disjoint from any implicitly captured managed
    `accel var`. Passing the same storage through two names is a program error,
    not a reason to disable DOALL or fall back to the CPU.
17. Check aliasing by storage class. For ordinary `_in`/`_out` Packed arguments,
    compare backing byte intervals because the C API can create distinct
    wrappers over overlapping memory. For `_ptr` and implicit accelerator
    resources, version 1 has no subviews, so resource identity is sufficient.
    Reject a statically obvious duplicate at compile time and any remaining
    overlap during call preflight, before queue or buffer side effects.
18. Every managed function admitted to the canonical GPU-lowerable subset owns
    a complete required-range summary. Its call validates those ranges before
    selecting a backend, including when that invocation later uses the CPU.
    A failed or overflowing range check is a runtime error and must not retry
    on the CPU. A managed function whose accesses cannot be summarized is
    CPU-only and retains ordinary per-access CPU bounds checks; diagnostics
    must state that no call-level range summary was available.
19. A managed function contains exactly one top-level ranged-for loop when it
    is considered for GPU lowering. It may have a restricted pure-local
    prologue and no observable epilogue.
20. GPU-lowerability and DOALL are separate results. A GPU-lowerable DOALL loop
    runs in parallel. A GPU-lowerable non-DOALL loop runs sequentially in
    `<<<1,1>>>`. A function outside the GPU-lowerable subset uses its CPU body.
21. Direct access to a top-level `accel var` in a managed loop is legal but
    prevents DOALL. Pass that resource through a typed `_ptr` parameter when
    parallelization is intended.
22. Direct top-level `accel var` access is forbidden in `__gpu func`. Raw kernels
    receive every resource explicitly through `_ptr`.
23. Version 1 shared memory exists only in `__gpu func`, is per workgroup,
    uninitialized, statically sized, and synchronized with `syncthreads()`.
24. Automatic DOSUM, atomics, reductions, scans, multiple loops, and general
    recurrence transformations remain deferred.

## 3. Surface language

### 3.1 Persistent typed resources

The existing declaration remains:

```noct
accel var data = Accel.uint32(1048576);
```

Rules already implemented and retained:

- the declaration is top-level only;
- the initializer is exactly `Accel.<element-kind>(element_count)`;
- `element_count` is an element count, not a byte count;
- it must be positive and `element_width * element_count` must not overflow;
- host code cannot subscript the resource;
- host/device movement uses the existing bulk copy APIs;
- the runtime value is an opaque typed accelerator-resource handle.

Although the declaration uses `var`, the binding denotes one compiler-owned
logical resource and cannot be reassigned after initialization. Its contents
remain mutable through copies and kernels. Implicit managed captures therefore
store a stable logical resource ID; call preflight still validates the resolved
resource kind and identity.

The ten constructor names and element widths are fixed:

| Constructor | Packed element kind | Bytes |
| --- | --- | ---: |
| `Accel.int8` | `INT8` | 1 |
| `Accel.int16` | `INT16` | 2 |
| `Accel.int32` | `INT32` | 4 |
| `Accel.int64` | `INT64` | 8 |
| `Accel.uint8` | `UINT8` | 1 |
| `Accel.uint16` | `UINT16` | 2 |
| `Accel.uint32` | `UINT32` | 4 |
| `Accel.uint64` | `UINT64` | 8 |
| `Accel.float32` | `FLOAT32` | 4 |
| `Accel.float64` | `FLOAT64` | 8 |

Allocation and copies support all ten. Initial GPU arithmetic and shared-memory
lowering remain limited to `int32`, `uint32`, and `float32`. For a managed
function, use of another element kind keeps CPU execution available. For a raw
GPU function, an unsupported element kind is a compile error because there is
no CPU implementation.

### 3.2 Buffer parameter spellings

Managed parameters may use the current copy transports:

```text
rpackedint32_in       rpackedint32_out
rpackeduint32_in      rpackeduint32_out
rpackedfloat_in       rpackedfloat_out
```

Add the following persistent-resource transports:

```text
rpackedint8_ptr       rpackeduint8_ptr
rpackedint16_ptr      rpackeduint16_ptr
rpackedint32_ptr      rpackeduint32_ptr
rpackedint64_ptr      rpackeduint64_ptr
rpackedfloat_ptr      rpackeddouble_ptr
```

The float spellings follow the existing Noct Packed type names:
`rpackedfloat` maps to `Accel.float32` and `rpackeddouble` maps to
`Accel.float64`.

Keep declaration transport separate from inferred kernel effects:

| Declaration | Legal function | Actual argument | Declared effect |
| --- | --- | --- | --- |
| `_in` | `__accel func` | ordinary typed Packed | read-only |
| `_out` | `__accel func` | ordinary typed Packed | write-only |
| `_ptr` | `__accel func`, `__gpu func` | same-kind `Accel.*` handle | inferred read/write |
| scalar | both | supported scalar value | value copy |

Version 1 GPU scalar parameters are exactly `int` and `float`. A managed
function using `long`, `double`, or another scalar kind remains CPU-only. The
same unsupported scalar in `__gpu func` is a compile error. GPU locals must have
an explicit supported annotation or a type proved uniquely by the mandatory
base GPU type checker; raw compilation must not depend on the O2 typed
optimizer.

The compiler rejects writes through `_in` and reads through `_out`. It infers
READ and WRITE bits independently for every `_ptr`. Do not reuse the transport
enum as the effect bitset; the runtime needs both pieces of information.

A GPU-lowerable managed function may have zero or one transient `_out` in
version 1. Zero is required for pointer-only managed kernels. More than one
`_out` keeps the function on its CPU body until multi-output transient staging
is designed; multiple WRITE `_ptr` parameters are supported.

For the GPU path, every element in the committed `_out` write interval must be
definitely written on every control-flow path of its loop iteration. Failure of
this must-write proof makes the managed function CPU-only. The runtime commits
only that proven write interval and preserves destination elements outside it.
Use `_ptr` for sparse or conditional updates that must retain prior device
contents.

Passing an `Accel.*` resource to `_in`/`_out` is an error. Passing an ordinary
Packed to `_ptr` is an error. Element kinds must match exactly; no implicit
reinterpretation is allowed.

The exact source spelling for an element count remains
`Packed.size(buffer_parameter)`. It is compiler-recognized inside accelerator
functions for `_in`, `_out`, and `_ptr` and returns `int`. For `_ptr`, lower it
from the resource descriptor/count supplied with the binding; do not require a
host copy or rely on driver-specific unsized-array behavior.
`Packed.size(shared_array)` is its compile-time constant.

### 3.3 Managed accelerator functions

Example:

```noct
__accel func add(
    src: rpackeduint32_ptr,
    dst: rpackeduint32_ptr,
    n: int
): void {
    let bias: int = 1;
    for (i in 0 .. n) {
        dst[i] = src[i] + bias;
    }
}

Accel.call(add, device_src, device_dst, n);
```

Managed rules:

- `void` is mandatory;
- a direct `add(...)` call is a compile error;
- `Accel.call` requires a directly resolved `__accel func` symbol;
- arguments are evaluated once, left to right;
- `Accel.call` does all preflight checks before queue synchronization,
  backend selection, CPU/GPU execution, or output mutation;
- the call is synchronous on every path;
- the original CPU HIR, bytecode, and JIT body are retained.

The GPU-lowerable shape is:

```text
pure scalar-local prologue
exactly one top-level ranged-for
empty epilogue or one final return;
```

The prologue may contain:

- scalar `let` or `var` declarations;
- literals, scalar parameters, and earlier prologue locals;
- side-effect-free arithmetic, comparison, Boolean, bitwise, and supported
  `Int.from`/`Float.from` conversions.

The prologue may not contain:

- Packed, `_ptr`, or implicit `accel var` reads or writes;
- global reads or writes;
- allocation, I/O, ordinary calls, dispatch, copies, or join;
- loop control or an early return.

For a parallel lowering the prologue runs independently in every invocation.
For a sequential `<<<1,1>>>` lowering it runs once. Because it is pure and
local, this difference is unobservable outside local values.

The loop body may use supported scalar locals, buffer accesses, and structured
`if`/`elif`/`else`. Nested loops, `while`, `break`, `continue`, ordinary calls,
I/O, allocation, exceptions, and early returns are outside the first
GPU-lowerable subset. Such a managed function still has its CPU implementation.

The epilogue is empty or contains only `return;`. A second loop or observable
post-loop operation prevents GPU lowering rather than being replicated across
threads.

### 3.4 Raw GPU functions

Example:

```noct
__gpu func fill(
    dst: rpackeduint32_ptr,
    value: int,
    n: int
): void {
    let i: int = globalIdx.x;
    if (i < n) {
        dst[i] = value;
    }
}

let event = Accel.dispatchAsync(fill, 100, 256, device_dst, 7, n);
Accel.join(event);

fill<<<100, 256>>>(device_dst, 7, n);  // synchronous in Noct
```

Raw rules:

- `__gpu func` and `static __gpu func` are accepted; inline and nested forms are
  rejected in version 1;
- return type must be `void`;
- ordinary calls, `Accel.call`, and use as a first-class value are errors;
- `_in` and `_out` are errors;
- scalar and typed `_ptr` parameters are allowed;
- direct global and direct top-level `accel var` access are errors;
- supported expressions, structured conditions, raw intrinsics, shared
  declarations, `syncthreads()`, and `return;` form the initial body subset;
- if a kernel contains `syncthreads()`, no early return may occur before any
  barrier; version 1 simplifies this to one optional final top-level `return;`;
- unsupported raw source is a compile error with a stable reason;
- raw validation and kernel generation run at every optimization level and in
  builds where the optional optimizer is disabled.

A raw GPU function must not silently flow through normal CPU bytecode or JIT
lowering. Runtime metadata may use a minimal trap stub so the ordinary runtime
function table remains structurally valid, but any attempted CPU entry reports
an internal/runtime error. Semantic analysis should normally make that stub
unreachable.

### 3.5 GPU intrinsics and one-dimensional geometry

Version 1 recognizes these reserved values only inside `__gpu func` and
compiler-generated managed GPU IR:

| Noct source | Vulkan GLSL | OpenGL compute GLSL |
| --- | --- | --- |
| `threadIdx.x` | `gl_LocalInvocationID.x` | `gl_LocalInvocationID.x` |
| `blockIdx.x` | `gl_WorkGroupID.x` | `gl_WorkGroupID.x` |
| `blockDim.x` | `gl_WorkGroupSize.x` | `gl_WorkGroupSize.x` |
| `gridDim.x` | `gl_NumWorkGroups.x` | `gl_NumWorkGroups.x` |
| `globalIdx.x` | `gl_GlobalInvocationID.x` | `gl_GlobalInvocationID.x` |

GLSL built-ins are unsigned. The emitter converts them explicitly to Noct
`int`. Launch preflight ensures that each value and the global-index domain fit
the Noct `int` contract. User code cannot assign to, shadow, take the address
of, or pass an intrinsic object as a value.

Only `.x` is valid in version 1. Access to `.y` or `.z` is a compile error with
a diagnostic that three-dimensional launch is deferred.

### 3.6 Shared workgroup memory

Initial syntax:

```noct
__gpu func tiled(
    src: rpackeduint32_ptr,
    dst: rpackeduint32_ptr,
    n: int
): void {
    __shared let tile = Accel.uint32(256);

    let lane: int = threadIdx.x;
    let i: int = globalIdx.x;
    if (i < n) {
        tile[lane] = src[i];
    }
    syncthreads();
    if (i < n) {
        dst[i] = tile[lane];
    }
}
```

Rules:

- `__shared let` and `__shared var` are legal only in `__gpu func`;
- initial declarations must be at function scope before executable statements;
- the initializer is `Accel.int32(CONST)`, `Accel.uint32(CONST)`, or
  `Accel.float32(CONST)`;
- in this context the constructor is a compiler storage marker, not a runtime
  persistent-resource allocation;
- `CONST` is a compile-time-folded positive element count;
- checked multiplication computes the shared byte size;
- storage is allocated once per workgroup and lives for that workgroup launch;
- contents are uninitialized on entry and are not automatically cleared;
- each workgroup has independent storage;
- a `let` binding cannot be rebound, but its elements are writable;
- version 1 also treats a `var` shared-array binding as non-reallocatable; the
  spelling is retained for language consistency and future shared scalars;
- the runtime checks aggregate shared bytes against the selected device limit;
- shared memory is not an `Accel.*` persistent resource and cannot be passed,
  returned, copied, or retained after launch.

`syncthreads();` is a statement-only raw intrinsic. Initial implementation
accepts it only at top-level uniform control positions, not inside a conditional
or loop. A barrier-containing function also rejects every early return before
the final barrier; initially, only one optional final top-level `return;` is
accepted. These two rules ensure all workgroup invocations reach every barrier
without requiring a general uniformity analysis. Emit the backend-equivalent
shared-memory ordering plus workgroup barrier; for the GLSL path use the
equivalent of:

```glsl
memoryBarrierShared();
barrier();
```

Atomics and shared memory in managed `__accel func` are deferred.

### 3.7 Launch and event APIs

The final public kernel-call surface is:

| API/syntax | Function kind | Completion | CPU execution |
| --- | --- | --- | --- |
| `Accel.call(f, ...)` | `__accel func` | synchronous | allowed |
| `Accel.dispatchAsync(f, g, b, ...)` | `__gpu func` | event | impossible |
| `f<<<g, b>>>(...)` | `__gpu func` | synchronous | impossible |

The existing APIs below remain:

- `Accel.join(event)`;
- `Accel.copyToAccel` and `Accel.copyFromAccel`;
- `Accel.copyToAccelAsync` and `Accel.copyFromAccelAsync`.

`Accel.callAsync` is absent from the final source namespace.

For both raw launch forms:

- the function operand must be directly resolvable;
- evaluate grid, block, then kernel arguments once from left to right;
- validate function kind, arity, scalar types, buffer storage, element kinds,
  non-alias, geometry, shared bytes, and backend limits before submission;
- `grid_count > 0` and `block_size > 0`;
- `grid_count` is passed to `glDispatchCompute(grid_count, 1, 1)`;
- an asynchronous event retains the kernel and all resource arguments until
  join or VM teardown;
- a write-effect `_ptr` advances its device generation;
- no ordinary Packed output snapshot or deferred host commit is created for a
  raw dispatch;
- `join` waits, reports failure, releases retained objects, and consumes the ID.

The synchronous triple-chevron form uses the same preflight and submission
implementation, then consumes its event before returning. Do not create a
second independent sync dispatch path.

Commands share the existing per-VM FIFO with asynchronous copies. For example:

```noct
let u = Accel.copyToAccelAsync(host, 0, device, 0, bytes);
let k = Accel.dispatchAsync(kernel, groups, threads, device, n);
let d = Accel.copyFromAccelAsync(device, 0, host_out, 0, bytes);
Accel.join(d);
```

Joining `d` observes upload, raw kernel, and download in submission order.

Keep the current source argument ceiling explicit. Until launch operands are
carried outside the generic call frame, `Accel.call` permits at most
`NOCT_ARG_MAX - 1` user arguments and raw launch permits at most
`NOCT_ARG_MAX - 3`. Emit a compile-time diagnostic instead of overflowing a
frame. A later dedicated variable-arity launch ABI may restore 32 user kernel
arguments.

## 4. Managed DOALL contract

### 4.1 Two independent analysis results

Every managed function records independent range, lowering, and parallelism
states:

```text
Range summary:
  RANGE_COMPLETE
  RANGE_UNAVAILABLE(reason)
  RANGE_NOT_APPLICABLE

GPU lowering:
  LOWERABLE
  NOT_LOWERABLE(reason)

Loop parallelism:
  DOALL
  NON_DOALL(reason)
  NOT_ANALYZED
```

Do not use one `eligible` Boolean to encode both questions.

Range status is independent of the lowering reason. A function rejected from
GPU lowering for an unrelated operation may still have RANGE_COMPLETE and must
run its call-level preflight. RANGE_UNAVAILABLE skips only range steps 7-9 and
uses checked CPU subscripts. Raw GPU descriptors use RANGE_NOT_APPLICABLE.

A lowerable, DOALL loop selects a parallel kernel. A lowerable, NON_DOALL loop
selects a serial GPU kernel if the selected backend can execute it. A
NOT_LOWERABLE function selects the preserved CPU implementation.

The following are not interchangeable:

- an unsupported expression is a GPU-lowering limitation;
- a true loop-carried dependence is a NON_DOALL result;
- a call-time buffer that is too short is a runtime programming error;
- no GPU device is a backend-selection condition for managed code.

### 4.2 Canonical loop and effect collection

The first target accepts:

```noct
for (i in 0 .. upper_bound) {
    // body
}
```

`upper_bound` must be a loop-invariant pure integer expression representable by
the bounds-expression format in section 5. It may be a scalar parameter,
`Packed.size(buffer_parameter)`, a compile-time integer, an allowed constant
adjustment, or a pure prologue alias of one of those forms. The step is one and
the end is exclusive. Non-zero starts and general strides are deferred until the initial
metadata and guards are proven.

Collect a buffer effect for every access:

```text
resource identity
parameter or implicit accel-var identity
READ and/or WRITE
element kind
index expression
dominating path predicates
source line
```

Version 1 DOALL index reasoning accepts `i + signed_compile_time_constant`.
The existing dominating-condition proof may refine the valid iteration domain,
for example an `i > 0` guard around `src[i - 1]`. Unknown indices can make the
function not GPU-lowerable; never guess a bound or dependence.

### 4.3 Initial dependence rules

The initial conservative DOALL proof is:

- all restricted arguments have passed the non-alias contract;
- every write target is injective across iterations; version 1 requires write
  index exactly `i`;
- reads from a different restricted buffer may use a proven `i + constant`;
- a buffer read and write in the same function may share index `i`;
- a non-zero offset read of a buffer also written by the loop is NON_DOALL;
- two writes whose distinct-iteration disjointness is not proved are
  NON_DOALL;
- scalar locals created inside an iteration are private;
- loop-invariant scalar parameters and pure prologue locals are read-only;
- any loop assignment to a prologue local is NON_DOALL because the original
  CPU loop carries that value between iterations;
- loop-body locals must be definitely assigned before read on every path;
- reductions, scans, atomics, unknown calls, and recurrences are NON_DOALL or
  not lowerable according to whether their operations can be represented by
  the serial GPU IR;
- an implicit direct top-level `accel var` reference is always NON_DOALL, even
  when its current indices appear disjoint.

Examples:

```noct
// DOALL: distinct resources, one output element per iteration.
dst[i] = src[i - 1];  // only under a proven i > 0 path

// DOALL: same-iteration read/modify/write.
data[i] = data[i] + 1;

// NON_DOALL: a prior iteration can produce this iteration's input.
data[i] = data[i - 1] + 1;

// NON_DOALL: implicit global accelerator resource dependency.
global_table[i] = src[i];
```

The direct `accel var` rule deliberately avoids treating an implicit global as
a restrict parameter. Rewrite the final example to take
`global_table: rpacked..._ptr` and pass the resource explicitly when DOALL is
desired.

### 4.4 Parallel and sequential lowering

Parallel mode derives the loop variable from the invocation geometry:

```text
logical_id = blockIdx.x * blockDim.x + threadIdx.x
i = logical_id
if i >= upper_bound: return
execute one original loop iteration
```

The first automatic launch policy remains deterministic:

```text
block_size = 64
grid_count = ceil(upper_bound / block_size)
```

Validate against device limits and integer overflow. Zero iterations submit no
kernel and still complete the synchronous call successfully after preflight.
Profitability-based CPU/GPU choice is deferred.

If future device-count limits require it, a grid-stride form may be added only
with tests proving identical iteration coverage. Do not silently change to a
grid-stride loop in the first implementation.

Sequential GPU mode is a distinct lowering:

```text
launch <<<1,1>>>
run the pure prologue once
execute the original ranged-for loop in that one invocation
```

Merely launching the existing one-iteration SPMD shader with `<<<1,1>>>` would
execute only iteration zero and is incorrect. Accel IR and GLSL emission must
have explicit parallel and sequential forms.

For both forms, an out-of-bounds descriptor check is completed before
submission. The in-shader `i < upper_bound` guard protects excess parallel
invocations but does not replace call-time array-size validation.

## 5. Required-range metadata and preflight

### 5.1 Element intervals, not only an upper length

For each `_in`, `_out`, and `_ptr` parameter, record separate READ and WRITE
unions as half-open intervals:

```text
[required_min, required_max_exclusive)
```

Both ends are required. An upper-only check misses negative neighbor accesses.

Example:

```noct
for (i in 0 .. n) {
    dst[i] = src[i];
    if (i > 0) {
        prev[i] = history[i - 1];
    }
}
```

A correct summary is conceptually:

```text
dst:     WRITE [0, n)
src:     READ  [0, n)
prev:    WRITE [1, n)
history: READ  [0, max(n - 1, 0))
```

Required ranges use element counts in the parameter's own element kind, never
bytes. Existing copy API offsets and lengths remain byte-based.

An access `src[i + 1]` over `i in [0,n)` therefore records `[1,n + 1)` and may
remain GPU-lowerable when call preflight proves
`Packed.size(src) >= n + 1`. It is not an automatic CPU fallback merely because
the offset is positive.

### 5.2 Bounds-expression format

Do not retain AST or HIR pointers in a runtime descriptor. Store a small owned,
serializable, backend-neutral expression program. Version 1 needs these checked
operations:

```text
CONST(non-negative integer)
SCALAR_ARG(argument index)
BUFFER_LENGTH(argument index)
RESOURCE_LENGTH(implicit resource binding slot)
ADD_CONST(expression, signed constant)
MIN(expression, expression)
MAX(expression, expression)
```

Every operation is evaluated with checked signed range arithmetic. Conversion
to `size_t` occurs only after non-negativity is proven. Overflow, an invalid
argument reference, or a negative required lower bound is a preflight failure.

A parameter range descriptor owns:

```text
optional read_min/read_max_exclusive expressions
optional write_min/write_max_exclusive expressions
source lines for diagnostics
```

Multiple accesses merge separately by effect with `MIN` for the lower end and
`MAX` for the upper end. Path predicates may narrow each access before the
merge. The parameter READ/WRITE bitset is canonical and must exactly equal the
presence of these intervals for summarized managed code; the loader rejects an
inconsistent descriptor. Raw code has effects but no automatic intervals.
Conservative over-approximation is permitted; under-approximation is a compiler
bug.

Before translating `i + constant`, intersect its path iteration domain with
the loop domain and normalize an empty path. For symbolic path bounds use the
equivalent of:

```text
L = min(n, max(0, path_lower))
U = max(L, min(n, max(0, path_upper)))
access interval = [L + offset, U + offset)
```

Ignore an interval whose normalized `L == U` when merging. For example,
`if (i > K) p[i + c]` is empty when `n <= K`; it must not produce a malformed
range with minimum greater than maximum.

If the compiler cannot safely express a needed range, the managed function is
not GPU-lowerable and has no partial call-level range contract. It uses the
ordinary CPU implementation and its normal per-access bounds checks. Do not
omit the unknown access while claiming that the descriptor is complete.

### 5.3 Empty range

Evaluate the canonical upper bound before individual access intervals.

- `upper_bound < 0` is a runtime range error.
- `upper_bound == 0` means zero iterations and every loop-derived access
  requirement is empty, even if its syntactic offset is negative.
- `upper_bound > 0` evaluates the recorded interval normally.

This avoids falsely rejecting an empty loop containing `src[i - 1]` while still
rejecting any non-empty invocation whose lower requirement is negative.

### 5.4 Mandatory preflight order

`Accel.call` performs these steps in order:

1. evaluate the function and all source arguments once, left to right;
2. validate managed function kind and arity;
3. validate scalar runtime types;
4. validate Packed versus accelerator-resource transport and resolve every
   implicit slot to its registered fixed accelerator resource;
5. validate exact element kinds for explicit and implicit bindings;
6. validate pairwise non-alias across all explicit buffer arguments and all
   deduplicated implicit resource binding slots;
7. evaluate the loop upper bound with checked arithmetic;
8. evaluate every required interval;
9. verify `0 <= min <= max_exclusive <= actual_element_count`;
10. only then synchronize a queue, compile/select a pipeline, submit GPU work,
    or enter CPU fallback.

Steps 1 through 6 apply to every managed call, including a CPU-only function
whose range summary is unavailable. For a function with a complete summary,
steps 7 through 9 also apply even when acceleration
is disabled or the body will run on the CPU. Therefore its CPU and GPU paths
report the same precondition failure before any body side effect. A failed
preflight is a runtime error, not `ACCEL_DISPATCH_FALLBACK`. A CPU-only function
whose summary was statically unrepresentable is the explicit exception: it
skips only steps 7 through 9 and uses ordinary checked CPU accesses because no
sound call-level range preflight can be constructed.

Direct implicit `accel var` ranges use their resource descriptor and can be
checked statically when the count is constant or through the same runtime
expression machinery when necessary.

### 5.5 Raw GPU distinction

Raw `__gpu func` is intentionally not given automatic per-access range proofs.
`Accel.dispatchAsync` validates parameter types, resource identities,
non-alias, launch geometry, device limits, and shared-memory size. The raw
kernel author is responsible for guarding every buffer access, normally with a
scalar length or `Packed.size(ptr)`. Raw out-of-bounds access has the backend's
error/undefined behavior and is never replayed on the CPU.

## 6. Compiler representation and pass boundaries

### 6.1 Function and parameter descriptors

Replace the current coupled fields with concepts equivalent to:

```c
enum noct_func_kind {
    NOCT_FUNC_NORMAL,
    NOCT_FUNC_ACCEL,
    NOCT_FUNC_GPU
};

enum accel_param_transport {
    ACCEL_TRANSPORT_SCALAR,
    ACCEL_TRANSPORT_COPY_IN,
    ACCEL_TRANSPORT_COPY_OUT,
    ACCEL_TRANSPORT_DEVICE_PTR
};

#define ACCEL_EFFECT_READ  1
#define ACCEL_EFFECT_WRITE 2

enum accel_parallel_kind {
    ACCEL_PARALLEL_NOT_ANALYZED,
    ACCEL_PARALLEL_DOALL,
    ACCEL_PARALLEL_SERIAL
};
```

The exact names may follow project conventions, but the states and separation
are mandatory.

Each parameter descriptor records:

- Noct runtime type;
- Packed element kind;
- scalar layout if applicable;
- transport;
- READ/WRITE effect bits;
- optional required-range expressions;
- for transient `_out`, the must-write proof and exact commit interval;
- stable source parameter index and binding number.

The owned kernel descriptor records at least:

- descriptor format version;
- function kind, resolved link name, logical source name, and source line;
- GPU-lowerable status and stable rejection reason;
- RANGE_COMPLETE/RANGE_UNAVAILABLE/RANGE_NOT_APPLICABLE and reason;
- DOALL result and stable reason;
- parallel versus serial/raw code-generation mode;
- parameter descriptors;
- canonical loop-bound expression for managed functions;
- explicit/raw or automatic launch metadata;
- implicit resource descriptors for serial managed functions;
- shared declarations and checked aggregate byte count;
- deterministic GLSL source/template and content hash;
- a backend cache container, not a single untyped pipeline pointer.

Update clone, free, copy, dump, HIR-to-LIR, LIR-to-runtime, C backend, bytecode
writer/reader, and VM teardown together. Every owned expression tree, string,
IR block, and cache entry needs an explicit lifetime rule.

### 6.2 Frontend

Authoritative sources:

- `src/core/lexer.l`;
- `src/core/parser.y`;
- `src/core/ast.h` and `src/core/ast.c`.

Regenerate and check in:

- `src/core/lexer.yy.c`;
- `src/core/parser.tab.c`;
- `src/core/parser.tab.h`.

Add a `gpu` keyword while retaining it in the existing property-name escape
rules. Add dedicated longest-match tokens for `<<<` and `>>>` before the
existing shift/operator rules, so the lexer cannot split launch delimiters into
`<<` plus `<` or `>>` plus `>`.

Represent these explicitly in AST/HIR:

- function kind;
- parameter transport;
- synchronous raw launch statement;
- asynchronous raw dispatch intrinsic;
- raw GPU built-ins;
- shared declarations;
- `syncthreads`.

Do not leave launch or barriers as mutable dictionary/property calls. They are
side effects and control/synchronization barriers. If lowering uses
parser-inaccessible internal cfuncs, the typed HIR node must still carry enough
identity for every optimizer visitor to handle it explicitly.

After forward declarations, static mangling, and `require` resolution, perform
a module-wide function-kind use check. A resolved accelerator/GPU function
symbol is legal only as the direct first operand of its matching reserved API
or, for raw GPU, as the direct triple-chevron target. Any return, assignment,
container insertion, ordinary argument use, or mismatched call is a compile
error. Do not defer these first-class/context errors to the runtime.

### 6.3 Managed analysis versus mandatory raw compilation

The current `hir_opt_accel.c` is built with the optional optimizer and combines
shape validation, index proof, and direct GLSL emission. Split responsibilities
so that:

- managed declaration/transport validation, READ/WRITE effect collection, and
  construction of any complete required-range summary are non-transforming
  semantic work at every optimization level and when
  `NOCT_ENABLE_OPTIMIZER=OFF`;
- unknown or escaped managed resource effects are conservatively READ|WRITE;
- managed DOALL classification and GPU extraction remain optimize-level-2
  optimizations;
- the managed CPU body is always valid at levels 0 and 1;
- raw GPU validation and code generation are compiled and run at levels 0, 1,
  and 2 and when `NOCT_ENABLE_OPTIMIZER=OFF`;
- frontend semantic errors do not depend on optimizer availability.

Create or split out a base GPU type checker that uses declared parameter/local
types and conservative expression typing. It is always built and does not call
or assume `hir_opt_typed_func()`, which remains an optional O2 optimizer pass.
For raw GPU source, an expression whose supported `int`/`float` type is not
unique is a compile error. For managed source it prevents GPU lowering while
the CPU body remains valid.

A suitable boundary is:

```text
typed HIR
  -> common GPU semantic validation and effect/range collection
  -> managed DOALL classification (O2 only)
  -> validated Accel IR
  -> backend-neutral GLSL emitter
```

Do not emit shader text while walking arbitrary HIR. First build and validate
Accel IR, then emit from only that IR.

### 6.4 Accel IR additions

Extend the current conceptual IR to cover:

- scalar constants and locals;
- typed resource length, load, and store;
- invocation/workgroup built-ins;
- arithmetic, comparison, bitwise operations, and supported conversions;
- structured selection;
- a serial ranged-for node;
- shared declaration, load, and store;
- workgroup barrier;
- return.

The parallel managed IR contains a single guarded loop iteration. The serial
managed IR contains the original loop. Raw IR contains the source SPMD body.
All buffer operations carry parameter/resource identity, element kind, effect,
and source line.

Validate IR invariants before shader generation. The emitter must not infer
DOALL, aliasing, or bounds from shader text.

Accel IR is compiler-owned transient state. Free it after all required shader
templates and logical metadata are emitted. Runtime descriptors and bytecode
store those templates plus parameter/range/launch metadata, not compiler IR or
arena pointers. Adding an IR wire format is a separate future decision.

### 6.5 Local declaration metadata

Current HIR drops part of the source declaration form after validation. Preserve
enough metadata for raw/shared validation:

- declaration kind (`let` versus `var`);
- declared scalar/Packed element type;
- storage class (ordinary local versus shared);
- definite-assignment state needed by DOALL;
- source line.

Initialize every new field at the single allocation/construction site; do not
depend on zero values accidentally meaning a valid enum state.

### 6.6 Bytecode and bundles

Accelerator metadata is experimental on this branch. Introduce a versioned
descriptor section for the new function kinds, transports, effects, bound
programs, shared declarations, and raw launch metadata. Do not reinterpret old
field counts as the new format.

The initial migration policy is:

- ordinary existing bytecode remains compatible;
- load the current accelerator descriptor as legacy managed `__accel func` only
  when it can be mapped without guessing and its bytecode does not require the
  removed managed async API;
- newly emitted accelerator descriptors use the new explicit version;
- reject an incompatible raw/accelerator descriptor with a stable message;
- never execute a `__gpu func` from a bundle lacking its complete GPU descriptor;
- newly compiled source and bundles do not expose `Accel.callAsync`.

Experimental legacy accelerator bytecode that calls `Accel.callAsync` is not
compatible with the final API. Reject it with a stable incompatible-accelerator
metadata/API error. Do not retain a public or hidden legacy async-kernel helper;
users must rebuild that experimental bundle from source. Ordinary bytecode
compatibility is unaffected.

A descriptor owns logical data only. Never serialize absolute source paths,
raw pointers, OpenGL/Vulkan objects, device IDs, driver binaries, or a pipeline
cache.

### 6.7 Boundary with generated ONNX raw kernels

The rejected design 17 DNN Plan/DPL1/Tensor View boundary is not implemented.
Design 18 instead generates ordinary Noct source containing specialized raw
`__gpu func` declarations and launches.  The boundary is:

```text
ONNX normalized graph and converter-only tensor metadata (design 18)
  -> deterministic specialized Noct __gpu func source (design 18)
  -> raw GPU validation/emission and descriptors (this document)
  -> backend shader/pipeline (this document)
```

`ACCEL_MATH` is a GPU-compiler scalar operation class used for direct calls
such as `Accel.sigmoid()` inside `__gpu func`; it is not ordinary VM bytecode, a
runtime cfunc, or a DPL instruction.  Source-callable `ACCEL_REDUCE` and
`ACCEL_TENSOR` are deferred: the converter emits explicit bounded loops,
loads/stores, and synchronization for those kernel families.

Converter-only views do not enable public resource subviews.  Bind one
underlying storage once per raw kernel call and embed its view offsets/strides
in generated indexing.  Do not pass two aliases as separate `_ptr` arguments,
and do not weaken section 2's non-alias contract.

## 7. Common runtime contract

### 7.1 Managed `Accel.call`

The runtime receives a managed descriptor plus already evaluated arguments. It
performs section 5 preflight before backend selection.

GPU path:

- choose parallel or serial descriptor;
- synchronize earlier FIFO work;
- ensure `_ptr` and implicit resources have current device storage;
- marshal `_in` and scalar values;
- submit and wait;
- commit `_out` before returning;
- update device/host generations from parameter effect bits.

CPU path:

- drain earlier GPU work;
- with version 1 whole-resource generations, materialize the complete host
  shadow before the CPU body for every READ or WRITE `_ptr` and implicit
  resource;
- omit that download for a WRITE-only resource only when the descriptor proves
  that the CPU body overwrites the complete resource;
- expose a typed host-shadow view to the preserved CPU body;
- run the ordinary function synchronously;
- after any CPU WRITE, advance the host/logical generation and leave the device
  generation stale;
- upload later only when required by subsequent GPU work.

The CPU `_ptr` path may transfer for coherence, but it must not allocate a new
argument Packed or change the resource identity.

The compiler deduplicates repeated references to the same implicit global
symbol into one binding slot. Preflight compares every distinct implicit slot
against the other implicit slots and every explicit restricted buffer. If two
distinct names resolve to the same backing resource, the call is an alias
error. An incomplete CPU-only effect summary is treated as READ|WRITE so
coherence never depends on an optimizer proof.

Version 1 resource coherence uses three monotonic generation values:

```text
logical_generation  latest semantic contents
host_generation     contents represented by the complete host shadow
device_generation   contents represented by the FIFO-ordered device buffer
```

Required transitions are:

- after successful queue submission of any GPU WRITE, increment logical and
  set device to logical immediately; host remains stale. Do not wait until
  join to publish the ordered device generation;
- after a CPU WRITE, increment logical and set host to logical; device remains
  stale;
- before any device operation while host is newer, perform a full host-to-device
  synchronization before a partial copy or dispatch;
- before any CPU READ/partial WRITE while device is newer, perform the full
  device-to-host synchronization described above;
- a full upload/download sets the receiving side's generation to logical;
- a partial copy-to operation first resolves host-newer state, then advances
  logical/device and conservatively leaves the host shadow stale;
- a copy-from operation is a read and does not advance resource generation;
- after a post-submit failure, treat the device as potentially modified and the
  host as stale; report the error and never replay.

These transitions are the authoritative state machine whether implemented as
three counters or equivalent HOST_DIRTY/DEVICE_DIRTY/SYNCED states. Never mark
a mixed partial shadow as a complete current generation.

A lowerable NON_DOALL managed function prefers its serial `<<<1,1>>>` GPU
kernel when an enabled backend supports it. Backend unavailability, an
unsupported serial backend feature, or a pre-submit pipeline failure may use
the CPU body. No managed kernel is replayed after submission.

### 7.2 Generic event ownership

Generalize the current single-output event so it can retain:

- operation kind;
- backend fence/submission token;
- kernel/runtime function;
- every referenced accelerator resource;
- scalar/push storage owned by the submission;
- async copy staging and destination roots;
- per-resource write generations;
- deferred copy-from commit data;
- error state.

Raw dispatch events do not carry managed `_out` snapshots. Async copy events
keep the existing staging/snapshot and deferred-commit behavior.

The state machine remains generation-safe:

```text
FREE -> RESERVED -> SUBMITTED -> COMPLETE -> JOINED
                         \-----> FAILED ----> JOINED/error
```

A second join and a stale ID are runtime errors. VM teardown drains submissions
and releases every retained object. Do not consume one fixed global-pin slot
for every retained launch argument: `RT_GLOBAL_PIN_MAX` would make a few
maximum-arity events exhaust the VM-wide table. Store retained `rt_value`s in
each live event and make every single-threaded and multithreaded GC root walk
scan and update those values, including compacting collection. A dynamically
sized VM root set is also acceptable if it has the same lifetime and compaction
properties. No event may keep an untracked movable pointer.

`Accel.join()` consumes the event ID on both success and failure. Even if fence
wait, deferred commit, or backend status fails, release all roots, staging,
snapshots, and backend tokens before returning the runtime error. An internal
FIFO drain used before managed CPU execution waits outstanding work and marks
events COMPLETE/FAILED as appropriate, but it does not consume user-visible
event IDs; a later explicit join remains valid and performs final commit or
error cleanup.

### 7.3 Backend result semantics

Common backend outcomes must be interpreted by function kind:

| Outcome | Managed `__accel func` | Raw `__gpu func` |
| --- | --- | --- |
| backend disabled/unavailable | CPU fallback | runtime error |
| unsupported pre-submit feature | CPU fallback | runtime error |
| shader/pipeline failure before submit | CPU fallback/mark CPU-only | runtime error |
| range/type/non-alias programming error | runtime error | runtime error |
| failure after submit | runtime error, no replay | join/sync runtime error, no replay |

Do not return a generic FALLBACK result from raw dispatch and accidentally call
a nonexistent CPU body.

## 8. OpenGL and retained Vulkan backends

### 8.1 Backend-neutral shader contract

Continue emitting deterministic Vulkan-dialect GLSL 450 from validated Accel IR.
The OpenGL runtime may keep its deterministic translation to GLSL 430 core.
Backend-native objects do not enter shared compiler descriptors.

Bindings are stable by parameter order, followed by any implicit managed
resources in stable logical-ID order. `_ptr` binds the existing persistent
SSBO directly. Copy transports may keep staging behavior.

The compiler checks an explicit internal binding-count cap over all buffer
parameters plus deduplicated implicit resource slots. Backend preflight checks
the same total against the selected device's compute-SSBO binding limit.
Exceeding the compiler cap is a stable compile/CPU-only reason as appropriate;
exceeding a device limit causes managed CPU fallback or a raw runtime error.
Never validate only the number of user arguments.

### 8.2 OpenGL pipeline variants

OpenGL workgroup size is a shader compile/link property. Replace the current
single `kernel->backend_data` pipeline with a per-VM cache keyed by at least:

```text
kernel content hash
code-generation mode
block_size_x
relevant shared-memory layout/version
```

Emit a unique non-source sentinel for `local_size_x` in the validated GLSL
template. Variant creation replaces only that sentinel with the already
range-checked decimal block size, then performs the existing GLSL-430
translation. Managed sizes 64 and 1 use the same materializer. Never perform a
general textual substitution on user-derived shader text.

Do not use the current 32-bit content hash as sole identity. Key first by
descriptor identity and variant properties; when hashes match across distinct
descriptors, compare complete template bytes. Cache a compile/link failure per
variant key so every launch does not retry the same failed shader.

Managed parallel kernels initially use block size 64. Managed serial kernels
use local size 1. Raw kernels create/reuse a variant for their explicit block
size. Compiling a second block size must not destroy or overwrite the first
variant.

The first asynchronous dispatch of a variant may synchronously compile and
link that variant before submission. `Async` describes submitted GPU work, not
background shader compilation. Later launches reuse the cached variant.

Before submission query/validate:

- `GL_MAX_COMPUTE_WORK_GROUP_COUNT[0]`;
- `GL_MAX_COMPUTE_WORK_GROUP_SIZE[0]`;
- `GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS`;
- `GL_MAX_COMPUTE_SHARED_MEMORY_SIZE`;
- checked `grid * block` against the Noct `int` intrinsic domain.

For every write-effect `_ptr`, advance the device generation and leave its host
shadow stale. Read-only resources must not be marked written. Bind and retain
all pointer resources, not only one output.

After a compute dispatch, issue the OpenGL memory barriers required for later
SSBO reads/writes and buffer copy/readback operations before enqueuing the next
dependent command. The existing FIFO and fence do not replace shader-storage
visibility barriers.

### 8.3 Vulkan preservation boundary

Do not delete or opportunistically rewrite the existing Vulkan prototype.
Update common signatures, descriptor-version adapters, and compile guards so
`build-vulkan` continues to compile.

Until separately validated:

- do not claim Vulkan support for raw `__gpu func`, dynamic block variants,
  `_ptr` zero-copy, or shared memory;
- do not emulate `_ptr` by an undocumented staging copy;
- a managed call may use its CPU fallback when the retained Vulkan path cannot
  execute a new descriptor;
- a raw launch reports a clear unsupported-backend runtime error;
- shader-source/golden and compile-only checks are allowed;
- `tests/testcases/run-accel-vulkan.sh` execution remains intentionally unrun.

Hardware-backed OpenGL is the development and runtime debugging backend for
this plan.  Native Linux may use surfaceless EGL; WSL2 Mesa D3D12 remains an
alternative environment, not a required backend.

## 9. Diagnostics

`--accel-info` must report analysis per managed loop, not only a generic
generated/fallback line. Stable output includes:

- function kind and source line without an absolute path;
- GPU-lowerable yes/no and reason;
- DOALL yes/no and reason;
- each buffer parameter's inferred READ/WRITE effects;
- each required element interval;
- generated preflight guards;
- selected strategy: parallel automatic launch, serial `<<<1,1>>>`, or CPU;
- actual runtime backend and launch geometry when a GPU launch occurs.

Illustrative output:

```text
ACCEL: kernel add: GPU-lowerable yes
ACCEL: kernel add: loop 12 DOALL yes
ACCEL: kernel add: src_ptr READ required [0, n)
ACCEL: kernel add: dst_ptr WRITE required [0, n)
ACCEL: kernel add: guard 0 <= n <= Packed.size(src_ptr)
ACCEL: kernel add: strategy parallel auto <<<ceil(n/64),64>>>
```

For direct implicit resource access:

```text
ACCEL: kernel update: loop 8 DOALL no
ACCEL: kernel update: reason implicit accel var 'table'; pass it as _ptr
ACCEL: kernel update: strategy serial <<<1,1>>>
```

Raw diagnostics include the requested grid/block, selected pipeline variant,
shared bytes, and backend errors. There is no old warning that permits races or
waives CPU/GPU equality for implicit `accel var`. Direct resource dependence
now blocks DOALL deterministically.

Developer-only `NOCT_ACCEL_DEBUG` may include reason codes, IR, GLSL, cache
transitions, preflight expression evaluation, resource generations, and event
state. Stable tests assert reason codes/messages rather than driver-specific
logs.

## 10. Implementation stages from the current baseline

The stage text below remains the detailed acceptance contract and dependency
record.  It was written at the original `e529952` baseline.  Do not read a
stage heading as a claim that every bullet is complete; section 1's audit at
`96fde9a` is the current status.  Remaining hardening may be split into small
reviewable changes while preserving this order.  ONNX source-generation stages
live only in design 18.

Each stage is ordered by dependency and must be reviewable with its gate green.
Do not combine later syntax/runtime behavior into an earlier structural stage.
Do not commit or push a stage unless the project owner requests it.

### Stage 0: specification and baseline lock — this planning change

- replace the obsolete first-generation contract with this document;
- update the design index and the boundary note in design 10;
- record branch, baseline commit, validated OpenGL environment, and untested
  Vulkan boundary;
- make no compiler/runtime/test implementation changes.

Gate:

- documentation-only diff;
- branch remains `accel`;
- no commit;
- worktree contains only the reviewed plan files.

### Stage 1: descriptor version 2 and function-kind plumbing

Primary files:

- `src/core/accel.h`;
- `src/core/ast.h`/`ast.c`;
- `src/core/hir.h`/`hir.c`;
- `src/core/lir.h`/`lir.c`;
- `src/core/runtime.h`/`runtime.c`;
- `src/backend/bcback.c` and the bytecode loader;
- `src/backend/cback.c`;
- `src/api/accel.c`.

Work:

- replace `is_accel` with the three-state function kind;
- propagate resolved function kind through forward/static/required symbols and
  reject existing managed-function first-class/mismatched uses consistently;
- split parameter transport from READ/WRITE effect;
- add owned required-range, parallel-result, raw-launch, and shared metadata;
- make clone/free/dump/serialization paths complete;
- version the accelerator bytecode section and map the current legacy managed
  descriptor where exact;
- keep current source behavior unchanged.

Gate:

- existing accelerator CPU/OpenGL tests unchanged;
- source -> `.nb`/`.nap` round trip;
- interpreter and forced JIT;
- ASan/UBSan clone/free/teardown checks;
- all existing syntax, typing, ABCE, SIMD, CSE, app, and C-translation suites.

### Stage 2: managed `_ptr` transport and coherence

Primary files:

- `src/core/hir.c` and type tables;
- `src/core/hir_opt_accel.c` or its new analysis module;
- `src/api/accel.c`;
- `src/api/accel_opengl.c`;
- accelerator tests and runners.

Work:

- recognize all ten `rpacked..._ptr` spellings;
- restrict `_ptr` to `__accel func` initially;
- validate same-kind `Accel.*` arguments;
- reject wrong storage classes and duplicate restricted buffers;
- infer READ/WRITE bits;
- implement CPU host-shadow views and before/after coherence from effects;
- bind supported 32-bit pointer resources directly in OpenGL;
- keep unsupported GPU element operations on the managed CPU path.

Gate:

- pointer read, write, and read/write on CPU interpreter/JIT;
- GPU -> CPU -> GPU coherence chains;
- wrong Packed/Accel class and wrong element kind;
- static and dynamic duplicate-resource errors;
- no call-specific full-buffer copy for OpenGL `_ptr`;
- multiple write-resource generation tracking.

### Stage 3: range summaries and mandatory call preflight

Primary files:

- managed analysis and descriptor code;
- bytecode writer/loader;
- `src/api/accel.c`;
- OpenGL argument binding/dispatch guards;
- `--accel-info` diagnostics.

Work:

- canonicalize `for i in 0 .. upper`;
- summarize `i + constant` accesses with dominating path conditions;
- build owned min/max-exclusive expression programs;
- serialize/clone/free them;
- evaluate them overflow-safely before both CPU and GPU managed calls;
- distinguish an unrepresentable static summary from a failing runtime guard;
- implement empty-loop semantics;
- remove the current coarse single dispatch-count size assumption.

Gate:

- exact-fit, one-short, negative-lower, overflow, and empty cases;
- multiple buffers with different required ranges;
- guarded `i - 1` and unguarded failure;
- a guard whose path is empty because the runtime loop count is below its
  threshold;
- disabled-backend path reports the same preflight error;
- no output/resource mutation on failure;
- descriptor round trip preserves identical diagnostics.

### Stage 4: explicit DOALL analysis and dual managed lowering

Primary files:

- `src/core/hir_opt_accel.c`, split helpers if needed;
- backend-neutral Accel IR definitions;
- GLSL emitter;
- `src/api/accel.c` and `src/api/accel_opengl.c`;
- diagnostics and managed tests.

Work:

- validate pure-local prologue, one loop, and empty/return-only epilogue;
- collect effects and prove the initial RAW/WAR/WAW rules;
- record stable GPU-lowering and DOALL reasons separately;
- mark every direct implicit `accel var` reference NON_DOALL and suggest
  `_ptr`;
- emit distinct parallel-one-iteration and serial-whole-loop IR;
- dispatch managed DOALL as automatic `<<<ceil(n/64),64>>>`;
- dispatch lowerable NON_DOALL as `<<<1,1>>>`;
- retain CPU fallback only for not-lowerable or unsupported backend cases.

Gate:

- true DOALL, same-index read/write, neighbor distinct-buffer read;
- recurrence, reduction candidate, cross-iteration WAW (`p[i]` with
  `p[i + 1]` or `p[0]`), and implicit resource;
- two writes to the same `p[i]` within one iteration remain DOALL-safe;
- serial GPU output equals CPU output for dependence loops;
- tests prove `<<<1,1>>>` executes the entire loop;
- stable `--accel-info` golden output;
- level 0/1 CPU behavior remains unchanged.

### Stage 5: raw `__gpu func` frontend and mandatory GPU IR

Primary files:

- `src/core/lexer.l` and generated lexer;
- `src/core/parser.y` and generated parser;
- AST/HIR/function metadata;
- mandatory GPU validation/IR builder;
- every optimizer visitor and source backend.

Work:

- add `__gpu func` and `static __gpu func` declarations;
- add `_ptr` buffer parameters for raw functions;
- add x-only GPU built-ins and `globalIdx.x`;
- enforce raw `void`, no CPU call, no `_in`/`_out`, and no implicit globals;
- run the module-wide function-kind context check for raw symbols after
  forward/static/require resolution;
- run raw validation/codegen independently of O2 and optional optimizer;
- create an unreachable runtime trap stub rather than a CPU implementation;
- make C, Emacs Lisp, and Scheme source backends reject `__gpu func` with a
  stable unsupported-backend compile diagnostic at this stage; only the
  bytecode/backend-neutral GPU descriptor path may carry raw code;
- defer shared declarations to Stage 7 while issuing a precise not-yet-supported
  diagnostic.

Gate:

- positive parse/type/IR tests at optimize levels 0, 1, and 2;
- optimizer-disabled build;
- direct call, `Accel.call(gpu)`, wrong transport, y/z, global access, and
  unsupported operation diagnostics;
- checked-in generated parser files match their sources;
- all non-GPU targets still build with GPU code generation metadata only.

### Stage 6: raw dispatch, synchronous launch, and API cutover

Primary files:

- HIR/LIR launch nodes or parser-inaccessible internal helpers;
- `src/api/accel.c`;
- `src/api/accel_opengl.c`;
- runtime event and VM teardown structures;
- `src/core/gc.c` and every GC root-scanning path;
- public intrinsic registration and tests.

Work in this order:

1. generalize events to retain multiple resources and effects;
2. implement common raw preflight;
3. add `Accel.dispatchAsync` and OpenGL raw submission;
4. add block-size-keyed OpenGL pipeline variants;
5. add longest-match `<<<`/`>>>` lexer tokens, the statement AST/HIR node, and
   lowering of synchronous launch through dispatch plus join;
6. test FIFO/lifetime/error behavior;
7. remove public `Accel.callAsync` from compiler recognition, intrinsic
   registration, tests, and documentation.

Do not remove `callAsync` first; the current async-kernel tests are the baseline
for event behavior until the replacement works. Async copy APIs and
`Accel.join` remain.

Gate:

- raw async launch followed by explicit join;
- synchronous launch returns only after writes complete;
- IDs and all five x intrinsics across multiple groups;
- two block sizes create/reuse distinct pipeline variants;
- multiple read/write pointer resources remain alive through join;
- several maximum-arity outstanding events survive a compacting GC without
  exhausting the fixed global-pin table;
- async upload -> dispatch -> download -> join-last FIFO;
- total explicit-plus-implicit binding overflow and device binding limit;
- backend disabled/unavailable is a raw runtime error;
- managed/raw function-kind mixups are errors;
- `Accel.callAsync` is rejected while async copies still pass.

### Stage 7: shared memory and `syncthreads`

Primary files:

- lexer/parser/AST/HIR shared declarations;
- raw GPU IR and validator;
- GLSL emitter;
- OpenGL pipeline limit/preflight logic;
- negative and multi-workgroup tests.

Work:

- add function-scope `__shared let`/`__shared var` markers;
- constant-fold and overflow-check the three supported 32-bit constructors;
- emit deterministic per-workgroup shared declarations;
- add top-level-uniform `syncthreads()`;
- validate aggregate shared bytes against the backend;
- reject initialization assumptions, escape, dynamic length, unsupported type,
  managed use, divergent barriers, and every early return before a barrier.

Gate:

- per-workgroup isolation with at least two groups;
- communication within one group around a barrier;
- no cross-group visibility assumption;
- uninitialized storage is never used by a positive test;
- dynamic/zero/overflow size and excessive device-limit errors;
- divergent/nested barrier rejection;
- shader-source golden tests for OpenGL translation and Vulkan dialect.

### Stage 8: persistence, cross-target hardening, and documentation sync

Work:

- complete descriptor-v2 `.nb`/`.nap` round trips for both function kinds;
- define stable rejection for incomplete/legacy raw metadata;
- verify the Stage 5 C/Elisp/Scheme behavior: managed CPU only, raw GPU rejected
  with an explicit diagnostic rather than accidental CPU generation;
- run static, debug, OpenGL, OpenGL ASan, MinGW, and OpenWatcom/MS-DOS builds;
- run existing architecture JIT build checks with accelerator backends off;
- update `docs/syntax.md`, library/API documentation, examples, and handoff only
  after implementation semantics pass;
- keep Vulkan execution untested and record that fact.

Gate:

- full regression matrix below;
- no absolute path or backend handle in bundles;
- no Vulkan/OpenGL headers in backend-disabled builds;
- no implementation claim beyond tested OpenGL behavior.

## 11. Test matrix

Use:

- `tests/testcases/run-accel.sh` for syntax, compiler, CPU, bytecode, and fallback;
- `tests/testcases/run-accel-opengl.sh` for platform-selected hardware-backed OpenGL;
- do not invoke `tests/testcases/run-accel-vulkan.sh`: the existing script performs Vulkan
  execution. Until validation is explicitly reopened, the Vulkan gate is only
  `cmake --build build-vulkan`. Put backend-neutral GLSL/shader golden checks in
  a separate compiler/static runner.

Required new cases:

| Area | Required cases |
| --- | --- |
| function kinds | ordinary/accel/gpu accepted and every cross-kind call rejected |
| API | `call` managed-only, `dispatchAsync` raw-only, sync chevron, no `callAsync` |
| pointer typing | all spellings, wrong storage, wrong kind, unsupported GPU width |
| alias | static duplicate, overlapping Packed ranges, duplicate resources |
| bounds | exact, short by one, lower underflow, arithmetic overflow, empty loop |
| summary | multiple buffers, offsets, guarded neighbor, serialized expressions |
| DOALL | independent, same-index RMW, distinct neighbor, recurrence, WAW |
| implicit resource | direct `accel var` serial plus `_ptr` rewrite parallel |
| sequential GPU | one invocation executes every original iteration |
| raw IDs | local, group, block size, grid size, and global ID |
| geometry | zero/negative/too-large grid/block and integer-domain overflow |
| events | async raw, sync raw, stale/double join, teardown with outstanding work |
| FIFO | async upload -> raw dispatch -> async download -> join last |
| lifetime | all raw pointer resources and kernel retained to join |
| generations | several write pointers, read-only pointer unchanged |
| coherence | GPU-write/CPU-write transitions, partial copy after HOST_DIRTY |
| pipeline cache | same block reuse and different block variant |
| shared | isolation, barrier communication, limits, divergent/early-return rejection |
| fallback | managed disabled/no device/unsupported body uses CPU |
| raw errors | disabled/no device/shader/pre-submit failure never uses CPU |
| bundles | descriptor v2 round trip and path/handle leakage scan |
| legacy | legacy `callAsync` accelerator bundle gets stable incompatibility error |
| backends | OpenGL execute; Vulkan compile-only and explicitly untested |

For managed correctness compare:

```text
level 0 CPU
level 1 CPU
level 2 CPU with --disable-accel
level 2 OpenGL parallel or serial as diagnosed
interpreter and forced JIT CPU bodies
source and bytecode/.nap
```

For raw correctness compare against a separately written CPU reference in host
code; do not pretend the raw function itself has a CPU variant.

After every stage that touches core/HIR/runtime representation, run at least:

- syntax and typing;
- ABCE, SIMD, CSE, scoping, class, and app;
- accelerator static interpreter and forced JIT;
- C translation;
- relevant ASan/UBSan paths.

Before handing off implementation, run the full existing suite and record any
environmental baseline failure separately.

## 12. Error and fallback table

| Condition | Required result |
| --- | --- |
| invalid declaration, transport, or launch syntax | compile error |
| unsupported `__gpu func` body/type | compile error |
| ordinary call of accel/gpu function | compile error |
| managed body not GPU-lowerable | successful compile; CPU body |
| managed loop lowerable but NON_DOALL | serial GPU `<<<1,1>>>` when available |
| managed backend disabled/unavailable | CPU body |
| managed pre-submit pipeline limitation | CPU body |
| managed/raw arity, storage, kind, or non-alias violation | runtime error |
| managed required-range failure/overflow | runtime error before any side effect |
| raw backend disabled/unavailable | runtime error; no CPU fallback |
| raw launch/device-limit/shared-limit failure | runtime error; no submission |
| raw shader/pipeline failure | runtime error; no CPU fallback |
| invalid copy range or event ID | runtime error |
| failure after any GPU submission | runtime/join error; no CPU replay |
| unjoined event at VM destruction | drain and release; debug report only |

Optimizer inability is not a source error for `__accel func`. It is a source
error for `__gpu func` when the raw body cannot be represented, because no legal
execution remains.

## 13. Explicitly deferred work

The reductions and subviews deferred here are general source-level
`__accel func`/`__gpu func` features.  Design 18 may generate a bounded raw
reduction kernel and may use converter-only view metadata over one storage
binding.  This does not expose a whole-reduction pseudo-call, public `_ptr`
slice, or unchecked alias.

- three-dimensional launch and `Accel.dim3`;
- y/z intrinsic components;
- automatic DOSUM and floating-point reassociation modes;
- multiple managed loops and cross-dispatch global barriers;
- general loop starts, strides, and non-affine subscripts;
- raw automatic bounds checking;
- subviews/slices and general overlap analysis;
- 8/16/64-bit and float64 GPU arithmetic/shared operations;
- atomics, scans, reductions, and warp/subgroup intrinsics;
- shared memory in managed functions;
- divergent-barrier uniformity analysis;
- calls between GPU functions and reusable GPU helper functions;
- multiple queues and explicit dependency graphs;
- profitability/cost-based managed GPU selection;
- Vulkan validation of the new raw/pointer/shared features;
- DX12, Metal, HLSL, and MSL backends;
- disk pipeline caches and cross-process resource sharing.

## 14. Repository constraints for the implementing model

1. Code under `src/core` remains C89-compatible for OpenWatcom: declarations at
   block start, no C99 `for` declarations, no designated initializers, no VLAs,
   and project compatibility types.
2. Edit `lexer.l`/`parser.y` first and regenerate checked-in outputs. Never
   patch only generated parser files.
3. Vulkan/OpenGL headers remain excluded when their CMake options are off.
   DOS, PC-98, MinGW variants, and every cross-JIT target must configure and
   compile without a GPU SDK.
4. New HIR operations are explicit side effects or barriers in inline, typed,
   ABCE, SIMD, CSE, dump, copy, and free visitors. No default fall-through.
5. Preserve the CPU HIR only for managed functions. Never synthesize a silent
   CPU implementation for raw GPU code.
6. Every descriptor allocation has matching clone/free/serialization logic.
   Do not retain arena pointers in runtime or bundle metadata.
7. Evaluate and validate all launch/call operands before observable effects.
   Do not let backend choice change source argument evaluation.
8. Bounds and byte-size arithmetic is checked before signed/unsigned or
   element/byte conversion.
9. Restricted non-alias is a required source contract and a preflight guard,
   not a speculative optimizer heuristic or fallback condition.
10. Pipeline caches are per VM and keyed by every shader-affecting launch
    property. Do not store a block-specific pipeline as the only kernel cache.
11. Events are explicit GC root containers for every retained value and are
    scanned/updated by all collectors. Do not spend one fixed global-pin entry
    per argument. Release roots on join, failure, and VM teardown.
12. Accel calls, raw dispatch, launches, copies, joins, barriers, and resource
    generation transitions are side effects. Optimizers must not reorder or
    CSE them.
13. Backend failures before submission may use CPU only for managed functions.
    No operation may replay after GPU submission.
14. Keep diagnostics deterministic and free of absolute workspace/home paths.
15. Do not run or claim Vulkan execution validation during the OpenGL phase.
16. Do not push. Do not commit implementation stages unless the project owner
    explicitly asks for commits after review.
