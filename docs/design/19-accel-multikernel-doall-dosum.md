# GPU-only `__accel func`: common loop analysis, DOALL/DOSUM, and multi-kernel execution plan

Status: initial OpenGL implementation complete on branch `accel` (2026-08-12)

Implementation baseline: branch `accel`, commit
`7e8d803616986690db0a7845b508fe6f09953c97`

Implemented checkpoints begin at `db4e3a4` and continue through `8c40643`.
They add target-neutral analysis, versioned accelerator programs, GPU-only
execution, multiple ordered DOALL and additive DOSUM steps, local logical
Packed buffers, bytecode persistence, and the common GPU-IR
validation/emission boundary.  The final handoff records the exact test matrix
and hardware result.

Validated target: native Linux, Mesa Intel OpenGL 4.3+ compute through headless
EGL. OpenGL is the only execution backend in scope. Vulkan remains build-only
and unvalidated. D3D12 must not be introduced as an assumption.

This document is the authoritative delta to
`docs/design/16-accel-vulkan.md` for managed `__accel func`. It replaces design
16's single-loop, CPU-fallback, serial-GPU managed-function contract. Public
raw `__gpu func`, the design-18 ONNX converter, persistent top-level `accel var`,
raw launches, copy APIs, and events remain unless stated otherwise here.

The implementing model must execute the stages in order without waiting for
review between stages. On branch `accel`, checkpoint commits use the message
exactly `WIP`; do not push.

### Implemented subset and remaining extensions

The initial implementation deliberately accepts a narrower, diagnosed subset
of the full architecture described below:

- any supported number of top-level DOALL loops are emitted in source order;
- canonical additive DOSUM loops may occur before, between, or after DOALL
  loops, each with its own result and scratch buffers;
- each DOSUM result is published by the immediately following
  `_out[0] = accumulator` or `_ptr[0] = accumulator` store;
- a later DOALL can consume an `_ptr[0]` result without an intermediate host
  transfer;
- int32, uint32, and float32 are supported; reduction block size is 64;
- local `Packed.int32/uint32/float32(length)` declarations are device-only
  intermediates whose length is a scalar parameter or integer constant.

An implicit hidden buffer for direct use of an accumulator symbol by a later
DOALL, non-additive reductions, and general scalar prologue computation remain
extensions.  They must be rejected at compilation rather than falling back to
CPU execution.

## 1. Goal

Turn `__accel func` into a synchronous, GPU-only orchestration function:

1. analyze all supported top-level ranged loops with a new target-neutral loop
   analysis infrastructure;
2. classify every loop as DOALL or DOSUM;
3. lower each DOALL loop to an internal generated `__gpu func` kernel;
4. lower each DOSUM loop to an internal generated reduction kernel family;
5. create call-local device buffers for host Packed arguments and local Packed
   temporaries;
6. upload required host input ranges once before the first dispatch;
7. execute generated steps in source order with intermediate data on the GPU;
8. download required host output ranges once after the last dispatch; and
9. release all call-local GPU allocations on success and every failure path.

There is no CPU execution of an `__accel func`, simultaneous CPU/GPU work,
automatic CPU parallelization, or asynchronous overlap in this plan.

The loop analysis is deliberately reusable. A later project may make SIMD,
CPU-PE parallelization, and software pipelining consume it. This plan does not
modify the current SIMD implementation.

## 2. Fixed semantic decisions

These are implementation requirements, not suggestions.

1. `__accel func` is GPU-only. Source HIR exists for analysis and extraction but
   has no executable CPU fallback body.
2. `Accel.call(accel_function, ...)` remains synchronous and returns after all
   kernels and final downloads complete.
3. Ordinary calls to `__accel func` remain errors. Public `__gpu func` call/launch
   rules remain unchanged.
4. Compilation succeeds only if the complete observable computation fits the
   supported accelerator program. Unsupported statements, unsupported GPU
   expressions, and loops that are neither DOALL nor supported DOSUM are
   compile errors.
5. Remove the current `ACCEL_PARALLEL_SERIAL` whole-loop shader fallback. A
   true recurrence that is not DOSUM is rejected.
6. Disabled/unavailable OpenGL and backend failures are runtime errors, never
   reasons to execute bytecode on the CPU.
7. Failure after submission is reported and never replayed.
8. Source loop order is kernel order. OpenGL queue order plus explicit storage
   barriers establishes inter-kernel visibility.
9. Version 1 uses one queue/context and synchronous completion. It does not use
   events, multiple queues, or overlap for `Accel.call`.
10. `_in` and `_out` accept host Packed arguments. `_ptr` accepts an existing
    persistent `Accel.*` resource.
11. `_in` is read-only and `_out` write-only inside `__accel func`. Do not add a
    host `_inout` transport; use `_ptr` for persistent read/write data.
12. Restricted arguments are pairwise non-aliasing. Validate host backing byte
    intervals, not only wrapper identity. `_ptr` identity is sufficient while
    resource subviews do not exist.
13. Reject direct top-level `accel var` access from `__accel func`; require an
    explicit typed `_ptr` parameter.
14. Call-local intermediate arrays use existing pure-prologue syntax:

    ```noct
    let tmp = Packed.float32(n);
    ```

    In a GPU-only `__accel func`, a supported local `Packed.<kind>(length)` is a
    logical device-buffer declaration, not a CPU allocation.
15. Do not add function-local `accel var` syntax. Existing top-level
    `accel var name = Accel.<kind>(constant);` remains persistent.
16. Initial GPU computation types are int32, uint32, and float32, matching the
    working OpenGL/raw-GPU backend.
17. Compile `__accel func` at `-O0` through `-O3` and with
    `NOCT_ENABLE_OPTIMIZER=OFF`. Optimization level cannot remove its only
    execution path.
18. Generated kernels are internal `NOCT_FUNC_GPU` descriptors. They are not
    registered as source globals and have deterministic names derived from the
    owning function and loop ordinal.
19. Public raw `__gpu func` semantics and ABI remain compatible. It may migrate
    to a shared Kernel IR/emitter, but existing positive and negative behavior
    must remain.
20. Do not edit the ONNX converter or generated model kernels.
21. Source calls such as `Accel.sigmoid()` remain legal only inside explicit or
    compiler-generated `__gpu func` code. Do not make the `Accel.*` GPU math
    namespace directly callable from `__accel func` source as part of this plan.

## 3. Initial accepted source subset

### 3.1 Pure prologue

Before the first loop, allow only:

- scalar declarations with explicit `int` or `float` annotations and pure
  initializers over constants/scalar parameters;
- pure scalar aliases used in bounds or kernel expressions;
- `Packed.int32(expr)`, `Packed.uint32(expr)`, or `Packed.float32(expr)` local
  logical-buffer declarations;
- a DOSUM accumulator initialized to exact typed zero; and
- empty statements or a final bare `return;` where already legal.

Lengths must use the owned expression format in section 8, be non-negative at
runtime, and not depend on a GPU-produced value. Reject ordinary calls,
object/container/string work, globals, I/O, exceptions, `while`, and other
side effects in the prologue.

### 3.2 Top-level computation

Accept an ordered sequence of ranged loops:

```noct
for (i in 0..upper) {
    // supported body
}
```

The step is one and `upper` is an exclusive, loop-invariant owned expression.
Non-zero starts, strides, collection loops, and nested source loops are
deferred.

A DOALL body may contain typed private scalars, logical-buffer loads/stores,
supported int32/uint32/float32 operations and conversions, structured branches,
and the counter as an ordinary integer. `Accel.*` GPU math remains a `__gpu func`
source facility; an ordinary Noct call in `__accel func` is rejected unless a
separate, explicitly documented kernelizable intrinsic is added later.

A DOSUM body follows section 6. Source barriers, shared declarations, raw GPU
built-ins, launches, `break`, `continue`, and early `return` are rejected.

### 3.3 Device-result epilogue

After loops, allow only a direct store of a DOSUM result to one statically
indexed `_out`/local element, later device-side use of that result in a DOALL
body, and a final bare return. The direct store is a device copy or one-thread
kernel, never host scalar readback.

A GPU-produced scalar may not control allocation length, dispatch bounds, or a
top-level branch because that would force intermediate GPU/CPU synchronization.

### 3.4 Representative target

```noct
__accel func normalize(
    input: rpackedfloat_in,
    output: rpackedfloat_out,
    n: int
): void {
    let tmp = Packed.float32(n);
    for (i in 0..n) {
        tmp[i] = input[i] * input[i];
    }
    var total: float = 0.0;
    for (i in 0..n) {
        total = total + tmp[i];
    }
    for (i in 0..n) {
        output[i] = tmp[i] / total;
    }
}
```

This produces two DOALL kernels, one DOSUM program, call-local input/output/tmp,
reduction result and scratch buffers, one upload phase, ordered GPU-only work,
and one download phase.

## 4. Target-neutral loop-analysis infrastructure

### 4.1 New files and responsibilities

Add base compiler modules, independent of `NOCT_ENABLE_OPTIMIZER`:

```text
src/core/hir_parallel.h
    Shared compiler-only structures, reason enums, and APIs.

src/core/hir_loop_analyze.c
    CFG walk, scalar def/use, logical memory accesses, affine-index
    normalization, path-domain facts, alias classes, and dependencies.

src/core/hir_doall.c
    Target-neutral DOALL classification.

src/core/hir_dosum.c
    Target-neutral canonical sum-reduction recognition.
```

Do not create `hir_softpipeline.c` now. Preserve dependence kind and distance
so a later pass can consume them. Add the C files to `NOCT_BASE_SOURCE`, not
the optional optimizer list, and keep them C89/OpenWatcom compatible.

### 4.2 API boundary

Exact names may follow convention, but provide the equivalent of:

```c
bool hir_loop_analyze(
    struct hir_block *func,
    struct hir_block *loop,
    const struct hir_memory_catalog *catalog,
    struct hir_loop_summary **summary);

bool hir_doall_classify(
    const struct hir_loop_summary *summary,
    struct hir_doall_result *result);

bool hir_dosum_classify(
    const struct hir_loop_summary *summary,
    struct hir_dosum_result *result);

void hir_loop_summary_free(struct hir_loop_summary *summary);
```

Compiler summaries may point to live HIR, but no AST/HIR pointer may enter LIR,
runtime descriptors, bytecode, or bundles. All lists need checked limits and
explicit init/free.

Initial named limits:

- 32 top-level loops per accel function;
- 64 logical buffers per accel program;
- 256 memory accesses per loop;
- 64 internal kernels per accel program;
- 128 program steps;
- `NOCT_ARG_MAX` parameters/bindings per kernel, additionally checked against
  the backend device limit.

Never truncate at a limit; diagnose it.

### 4.3 Logical memory catalog

The analyzer must not hard-code `_in`, `_out`, OpenGL, SSBO, VRAM, or Packed
constructors. Its caller maps symbols to logical objects containing:

- stable object id, element kind/width, source symbol/line;
- storage and alias class;
- readable/writable contract;
- unique allocation or may-alias set; and
- optional length-expression identity.

The accel caller catalogs every buffer parameter and accepted local Packed.
Local allocations are unique; restricted parameters are runtime-checked
noalias. Unknown/global identities fail before extraction. Reduction buffers
are added after classification.

A future SIMD caller may construct a different catalog. No SIMD-specific rule
belongs in this API.

### 4.4 Collected information

For each loop record:

- shape, counter, source line, start and exclusive stop;
- invariant inputs;
- scalar definitions/reads/scope/definite-assignment/live-in/live-out and
  read-before-definition;
- each memory READ/WRITE with object id, type, line, normalized index, and
  dominating path restriction;
- calls and explicit purity classification;
- RAW, WAR, and WAW dependencies;
- known signed dependence distance where representable;
- unknown reasons without guessing; and
- canonical reduction candidates without a GPU decision.

Initial affine forms are `i`, `i + signed constant`, `i - non-negative
constant`, and normalized `constant + i`. Keep the structure extensible but
reject unsupported forms initially. Path facts may prove bounds such as a
guarded `i - 1`; they do not erase same-object cross-iteration dependence.

### 4.5 Independent results

Do not use one eligibility Boolean. Preserve:

```text
structural analysis: COMPLETE / UNKNOWN(reason)
parallel class:       DOALL / DOSUM / DEPENDENT / UNKNOWN
GPU lowering:         LOWERABLE / UNSUPPORTED(reason)
range summary:        COMPLETE / UNAVAILABLE(reason)
```

GPU-only accel compilation requires complete ranges, DOALL or supported DOSUM,
and lowerable operations. Stable reason enums have text from one formatter.

## 5. DOALL classifier

`hir_doall.c` proves iteration independence. It does not check GLSL support or
generate kernels.

Scalar rules:

- immutable parameters/prologue locals are invariant;
- loop-local variables are private when assigned before every read;
- an outer scalar read then written is carried;
- an outer scalar written by multiple iterations is an output/lastprivate
  dependence even if not read in-loop;
- the counter is a safe induction;
- a canonical sum accumulator is offered to DOSUM, not accepted as DOALL; and
- calls without explicit pure classification remain unknown.

Keep classifications rich enough for future private/firstprivate/lastprivate,
induction, and reduction users. GPU v1 uses only invariant/private.

For every same/may-alias access pair with at least one write:

1. identical `i + c` maps are injective and safe across distinct iterations;
2. different constant offsets imply dependence when domains may overlap;
3. invariant/constant writes are WAW unless the loop is statically at most one
   iteration;
4. unknown/non-affine writes are not DOALL;
5. distinct unique/noalias objects do not depend;
6. may-alias objects need an explicit checked noalias contract;
7. multiple writes to the same `object[i]` in one iteration do not alone add a
   cross-iteration dependency; and
8. path-domain disjointness may be conservative initially, but never guessed.

Examples:

```text
dst[i] = src[i - 1]       DOALL when dst/src are noalias and bounds are proven
data[i] = data[i] + 1     DOALL
data[i] = data[i - 1]     DEPENDENT, distance 1
data[i] = data[i + 1]     DEPENDENT anti/flow ordering
data[0] = src[i]          DEPENDENT WAW
```

The accel builder accepts only DOALL plus complete ranges/lowerability and
generates one invocation per iteration. Initial block size is 64 and group
count is `ceil(trip_count/64)`. Zero count submits nothing. Validate counts and
device limits before upload/allocation.

## 6. DOSUM classifier and reduction contract

There is no reusable DOSUM in current SIMD. Build it from the common summary.

Recognize one typed outer accumulator initialized immediately before the loop:

```noct
var sum: float = 0.0;
for (i in 0..n) {
    sum = sum + expression;
}
```

Also recognize `expression + sum` and normalized `+=`. Require one update on
every path; an expression independent of the accumulator; read-only buffer
effects; no other non-private scalar write; no store, side-effect, nested loop,
early exit, or second accumulator; known int32/uint32/float32 type; and exact
typed zero identity.

Defer conditional reductions, nonzero identities, min/max/product, scans,
atomics, mixed DOALL stores plus reduction, and multiple accumulators. Other
recurrences are DEPENDENT, not serial kernels.

The DOSUM result records operator/type, accumulator/identity, trip expression,
pure mapped expression, logical reads/ranges, line, and post-loop uses. The
compiler materializes a one-element device result. Later kernels read it as a
buffer, not a host push constant.

Numeric contract:

- uint32 add is modulo 32-bit;
- int32 positive tests initially avoid overflow and retain documented raw GPU
  int32 behavior rather than widening to Noct long;
- float32 explicitly permits reassociation into the deterministic reduction
  tree and uses documented absolute/relative tolerance;
- NaN/signed-zero policy must be designed before other reduction operators.

Hierarchical lowering uses block size 64:

1. first-pass map/reduce evaluates the source expression, reduces shared[64],
   and writes one partial per workgroup;
2. a fold kernel reduces partials;
3. repeat the fold until one value remains;
4. copy it device-to-device to the stable result buffer.

All threads reach every barrier. Out-of-range lanes contribute identity. Zero
input produces identity without a zero-group dispatch. Allocate two checked
call-local partial buffers and ping-pong them; never read partial values to the
host.

## 7. Shared internal GPU Kernel IR

Current code directly emits shader text in two places:

- `hir_gpu.c` validates/emits public raw `__gpu func` from AST;
- `hir_opt_accel.c` analyzes/emits one managed kernel from HIR.

Do not extend both independently. Introduce:

```text
src/core/gpu_ir.h
src/core/gpu_ir.c
    Typed structured backend-neutral kernel IR, ownership, and validation.

src/core/gpu_glsl.c
    Deterministic GLSL 450 emission from validated IR only.

src/core/gpu_ir_ast.c (or a separated section in hir_gpu.c)
    Existing public raw __gpu func AST -> GPU IR.

src/core/gpu_ir_hir.c
    Analyzed accel loop/reduction -> GPU IR.
```

IR requirements:

- typed scalar constants, parameters, and locals;
- `_ptr` buffers with typed load/store and binding identity;
- int32/uint32/float32 arithmetic, conversion, comparison, and integer bitwise
  operations;
- structured selection and registered `ACCEL_MATH` calls;
- x-only invocation/workgroup built-ins;
- shared arrays and uniform workgroup barriers for DOSUM;
- bounded constant internal loops for reduction trees; and
- early return only where barrier-uniformity permits it.

Validate IR before emission. The emitter must not infer dependencies, ranges,
aliasing, or reduction semantics from shader text.

Migrate public raw building through this IR before generated reductions rely on
it. Preserve raw binding/push layout, local-size placeholder, diagnostics,
math helpers, source-size limit, shared/barrier rules, and hashing. Harmless
whitespace may change, but raw source/bytecode/`.nap` and ONNX model tests must
pass.

Each generated kernel owns a normal `struct accel_kernel` with
`func_kind = NOCT_FUNC_GPU`. Its parameters are typed `_ptr` buffers and scalar
push values only. `_in`/`_out` exist only at the outer program boundary.

## 8. Accelerator program descriptor

### 8.1 Ownership model

Add a versioned multi-kernel descriptor equivalent to:

```text
struct accel_program
    descriptor version
    owning function/source identity
    logical buffer descriptors
    internal accel_kernel descriptors
    ordered program steps
    aggregate argument effects/ranges
    stable diagnostics
```

After migration:

- public/internal `NOCT_FUNC_GPU` owns one `accel_kernel`;
- `NOCT_FUNC_ACCEL` owns one `accel_program`;
- the program owns its generated kernels;
- runtime caches are VM-owned and keyed by kernel content and every
  shader-affecting property; and
- no descriptor owns AST/HIR pointers, OpenGL objects, contexts, device ids, or
  absolute source paths.

Add `accel_program_clone()` and `accel_program_free()`. Update HIR, LIR,
runtime, cleanup, dump, bytecode, and app-bundle paths in one structural stage.
Never leave ambiguous double ownership.

### 8.2 Owned checked expressions

Lengths, ranges, and dispatch counts use a small owned, serializable program:

```text
CONST(non-negative int64)
SCALAR_ARG(argument index)
BUFFER_LENGTH(argument/logical-buffer index)
ADD_CONST(expression, signed int64)
MUL_CONST(expression, non-negative int64)
MIN(expression, expression)
MAX(expression, expression)
CEIL_DIV_CONST(expression, positive int64)
```

Evaluate with checked signed arithmetic. Validate references, non-negativity,
`size_t` conversion, element-width multiplication, and backend integer limits.
Overflow/negative results fail preflight before GPU side effects.

### 8.3 Logical buffers

Each logical buffer records:

- stable id/debug name and source line;
- origin: HOST_IN, HOST_OUT, DEVICE_PTR, LOCAL, REDUCTION_RESULT, or SCRATCH;
- outer parameter index or none;
- element kind and width;
- length expression;
- merged READ and WRITE interval expressions;
- first/last program-step use; and
- initially-defined/upload/download flags.

Transfers use element intervals converted to checked byte ranges. Initially
allocate a full argument-sized device buffer so indices remain unchanged, but
transfer only a sound complete summarized range. Conservative over-transfer is
allowed; under-transfer is a bug.

For local buffers, require conservative cross-kernel definite initialization.
Version 1 accepts a prior whole `[0,length)` write before any read. Reject
partial/unknown coverage rather than reading uninitialized VRAM.

### 8.4 Ordered steps

Step kinds:

```text
DOALL_DISPATCH
DOSUM_REDUCTION
DEVICE_COPY
```

A DOALL step stores kernel index, trip expression, fixed block size, and maps
kernel parameters to logical buffers/owned scalars.

A DOSUM step stores first/fold kernel indices, trip count, result/scratch ids,
block size, operator, and type. The runtime expands it to dispatches.

A DEVICE_COPY step is a checked same-device range used to stabilize or publish
a reduction result. No backend command object is serialized.

## 9. Compiler pipeline and pass boundaries

Required logical order:

```text
frontend validation
 -> HIR construction with declaration metadata
 -> logical-buffer catalog and pure-prologue validation
 -> common loop analysis
 -> DOALL/DOSUM classification
 -> range and cross-kernel dataflow validation
 -> GPU Kernel IR construction/validation
 -> accel_program construction
 -> deterministic GLSL/hash generation
 -> LIR/runtime propagation
```

### 9.1 Mandatory compilation

Replace optional `hir_opt_accel_func()` responsibility with mandatory
`hir_accel_build_program()` (exact name flexible) in base sources. Invoke it
for `NOCT_FUNC_ACCEL` after HIR has the needed declaration/type information and
before optional optimization.

After building the program, do not run inline/ABCE/SIMD/CSE over an accel
source body. It is extraction input, not CPU code. Register a function shell
and owned program descriptor.

For normal functions, preserve existing optimizer order. For public GPU
functions, preserve mandatory raw compilation.

### 9.2 HIR declaration metadata

Extend `struct hir_local` or an owned declaration table with:

- let versus var;
- declared scalar type and Packed element type;
- storage class: scalar, logical local buffer, or reduction scalar; and
- declaration/initializer source line.

Initialize fields at the single allocation site; zero must not accidentally
mean a valid type. This metadata change must not alter SIMD behavior.

### 9.3 GPU-only runtime shell

LIR/JIT/interpreter must not execute the original accel body. Emit a minimal
unreachable shell or retain body data only for diagnostics, and make `rt_call`
reject CPU execution of `NOCT_FUNC_ACCEL` under every dispatch-depth state.

Remove `accel_dispatch_depth` if unused. Do not retain a hidden fallback route.
C, Emacs Lisp, and Scheme backends reject GPU-only accel functions with a
stable unsupported-backend diagnostic rather than emitting the old CPU body.

## 10. Runtime and OpenGL execution

### 10.1 Preflight order

`Accel.call` checks, before allocation/upload/pipeline/dispatch:

1. evaluate function and source arguments once, left-to-right;
2. validate kind, arity, scalar types, and exact buffer element kinds;
3. validate host Packed versus `_ptr` storage class;
4. validate host byte-interval and `_ptr` identity noalias contracts;
5. evaluate all length, trip, range, scratch, and dispatch expressions;
6. validate intervals against actual logical lengths;
7. validate program/kernel/binding counts and OpenGL limits;
8. resolve/compile required pipeline variants if policy does so pre-submit;
9. only then allocate call-local buffers and upload.

Failures before step 9 cause no GPU-visible mutation. Backend absence is an
error, not fallback.

### 10.2 Synchronous executor

Refactor single-kernel `accel_gl_dispatch_internal()` to execute a program:

```text
create or borrow physical buffers
upload every HOST_IN read interval once
for each program step:
    bind buffers and scalar push data
    dispatch DOALL or DOSUM sequence
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                    GL_BUFFER_UPDATE_BARRIER_BIT)
perform ordered device copies
download every HOST_OUT write interval once
complete/check errors
commit persistent resource generations
release owned buffers and push objects
```

Do not `glFinish` between kernels. Queue order and barriers sequence device
work. Keep a checked final completion point before freeing resources.

### 10.3 Physical-buffer rules

- HOST_IN/HOST_OUT/LOCAL/REDUCTION/SCRATCH buffers are call-owned;
- DEVICE_PTR borrows an existing `accel_gl_resource`;
- one logical buffer maps to one stable physical buffer except explicit DOSUM
  ping-pong scratch;
- later kernels bind the same intermediate without host staging;
- validate SSBO bindings, group counts/sizes, and shared bytes; and
- pipeline lookup takes an `accel_kernel *` plus local size instead of assuming
  the outer `rt_func` owns one shader.

### 10.4 Cleanup, GC, and coherence

Use one cleanup path with submitted/not-submitted state. Delete every owned
buffer/UBO/fence and uncached pipeline object; release roots; never delete
borrowed `_ptr`; never download/mark output current after incomplete work; and
never replay on CPU.

Arguments remain GC-safe through compilation/allocation failures. Do not spend
one fixed global-pin slot per internal buffer. For `_ptr` writes, update
generation state once after successful program completion; read-only versions
remain unchanged.

## 11. Persistence and compatibility

Use accelerator-program descriptor version 3 unless a newer version exists at
implementation time. Serialize checked counts for outer contracts, owned
expressions, buffers/ranges, steps/bindings, and internal kernels/GLSL sizes and
hashes.

Loader validation covers enum ranges, count limits, indices/references,
expression stack/depth, byte lengths, hashes, binding limits, and descriptor
consistency. Reject truncation/overflow before allocation.

Compatibility policy:

- ordinary old bytecode remains compatible;
- public raw-GPU v2 remains loadable where currently supported;
- old single-kernel managed accel v2 is rejected with a stable “recompile
  GPU-only __accel func” error rather than guessed into new semantics;
- new accel functions always emit a multi-kernel descriptor; and
- bundles contain no pointers, absolute paths, driver binaries, GL objects, or
  device identifiers.

Complete clone/free/validation before enabling the writer. Sanitizer-test
successful and failed load/teardown at truncated field boundaries.

## 12. Diagnostics

`--accel-info` reports source-order classifications and program summary, for
example:

```text
ACCEL: file.noct:12: loop 0 DOALL -> function$accel$k00 block=64
ACCEL: file.noct:18: loop 1 DOSUM(add,float32) -> first/fold block=64
ACCEL: function: buffers=6 kernels=4 steps=3 upload=1 download=1
```

Dependency rejection includes kind/distance/object:

```text
ACCEL: file.noct:20: loop 2 dependent RAW distance=1 on 'data'
Error: GPU-only __accel func 'prefix' contains a loop that is neither DOALL nor supported DOSUM.
```

Stable reason enums cover unsupported shape, non-affine access, scalar
recurrence/lastprivate, RAW/WAR/WAW, malformed DOSUM, incomplete range,
read-before-defined local, unsupported type/call, limits, and descriptor/legacy
incompatibility.

`NOCT_ACCEL_DEBUG` may dump summaries, expressions, GPU IR, steps, transfers,
GLSL, and lifetimes. Golden output excludes absolute paths and driver text.

## 13. Implementation stages

Each stage ends with its gate green and a `WIP` checkpoint. Continue without
waiting for review. If a gate exposes an unrelated baseline failure, record
exact evidence; do not weaken the gate.

### Stage 0: plan and baseline lock

Work:

- add this document only;
- record branch/baseline and OpenGL/Vulkan boundary;
- change no compiler, runtime, test, ONNX, or SIMD code.

Gate:

- branch `accel`;
- documentation-only diff before its `WIP` commit;
- baseline remains merge commit `7e8d803`.

### Stage 1: declaration metadata and common summary model

Primary files:

- `CMakeLists.txt`;
- `src/core/hir.h`, `hir.c`, dump/free helpers;
- new `hir_parallel.h`, `hir_loop_analyze.c`;
- focused compiler/static tests.

Work:

- preserve local declaration type/storage/line metadata;
- add memory catalog and loop-summary ownership;
- implement recursive expression/CFG effect collection without classification;
- add init/free/count/OOM handling;
- build optimizer-off/backend-off;
- expose developer summary diagnostics without changing execution semantics.

Do not edit `hir_opt_simd.c` or normal optimizer order.

Gate:

- summaries for simple, conditional, shifted, local-buffer, scalar-carried,
  and side-effect loops;
- unknown forms reported, never omitted;
- ASan/UBSan allocation/free;
- full existing static suite including SIMD.

### Stage 2: DOALL classifier

Primary files:

- new `src/core/hir_doall.c`;
- common reason/result definitions;
- analysis goldens.

Work:

- implement private/invariant/scalar-carried rules;
- compute RAW/WAR/WAW and constant dependence distances;
- consume noalias/unique catalog classes;
- classify independently of OpenGL/GLSL support;
- report DOALL/DEPENDENT/UNKNOWN deterministically.

Gate:

- distinct input/output, same-index in-place, shifted distinct read: DOALL;
- same-buffer neighbor and constant/different-offset writes: dependent;
- private locals, branches, scalar recurrence/outer output, unknown calls and
  indices;
- identical results at every optimization level.

### Stage 3: DOSUM classifier

Primary files:

- new `src/core/hir_dosum.c`;
- HIR declaration metadata;
- analysis tests only.

Work:

- recognize canonical, commuted, and normalized `+=` additive reduction;
- validate identity/type/definite update/pure expression/read-only effects;
- record post-loop device-result uses;
- reject two accumulators, missing-path updates, nonzero identity, stores,
  calls, and other recurrences;
- complete init/free for DOSUM results.

Gate:

- int32/uint32/float32 positives and zero/one/general trip expressions;
- all negative patterns have stable reasons;
- no runtime behavior change yet.

### Stage 4: versioned accel-program representation

Primary files:

- `src/core/accel.h` and descriptor implementation;
- `hir.h`, `lir.h`, `lir.c`, `runtime.h`, `runtime.c`;
- `src/backend/bcback.c` and loader;
- `gc.c`/VM teardown where ownership requires it;
- descriptor hardening tests.

Work:

- add owned expressions, buffers, steps, bindings, and `accel_program`;
- add clone/free/validate/dump;
- propagate through HIR/LIR/runtime;
- temporarily adapt one current managed kernel as one program step;
- serialize v3 and reject old managed v2 deterministically;
- retain ordinary/raw compatibility.

Gate:

- source, `.nb`, `.nap` one-step round trip;
- relocation/truncation and invalid reference/count tests;
- sanitizer success/failure teardown;
- no path/handle leakage;
- raw GPU and SIMD unchanged.

### Stage 5: shared GPU Kernel IR and emitter

Primary files:

- new `gpu_ir.*`, `gpu_glsl.c`, AST/HIR adapters;
- `src/core/hir_gpu.c`, `accel_ops.*`;
- raw static/OpenGL and ONNX regression tests.

Work:

- define typed structured IR and validator;
- emit deterministic GLSL only from IR;
- migrate public raw AST validation/emission without semantic expansion;
- retain raw shared memory/barriers;
- add HIR-to-IR for the current one-loop managed subset;
- preserve binding/push/local-size placeholder contracts.

Gate:

- raw positive/negative tests at all optimization levels and optimizer-off;
- raw `.nb`/`.nap` hardening;
- OpenGL raw launch/shared/math;
- ONNX generated source/package/model runners;
- feature-off, MinGW, OpenWatcom builds;
- no SIMD file changes.

### Stage 6: mandatory single-DOALL accel program

Primary files:

- new `src/core/hir_accel_program.c` or a staged replacement for
  `hir_opt_accel.c`;
- `hir.c` pass boundaries;
- `src/api/accel.c`, `accel_opengl.c`;
- managed diagnostics/tests.

Work:

- compile accel semantics at all optimization levels and optimizer-off;
- use common DOALL, not `shifted_read[]/written[]`;
- build one internal GPU kernel/program step;
- execute it through an initial OpenGL program executor;
- retain temporary comparison fallback only within this stage, not as final
  public semantics;
- make dependency/lowering diagnostics deterministic.

Gate:

- current positive one-loop cases use the new program;
- same-index RMW and distinct-buffer neighbor pass;
- current prefix recurrence is classified dependent;
- transfers and `_ptr` coherence remain correct;
- source/bytecode and interpreter/JIT host entry call the same program.

### Stage 7: multiple DOALL loops and call-local buffers

Primary files:

- accel program builder plus cross-kernel dataflow/liveness;
- HIR local-constructor recognition;
- OpenGL executor/pipeline cache;
- multi-loop tests.

Work:

- accept checked loop/program limits;
- recognize local supported `Packed.*(length)`;
- generate one kernel/step per DOALL loop in source order;
- allocate physical buffers once per call;
- upload HOST_IN once, retain intermediates, barrier, download HOST_OUT once;
- implement whole-range local initialization proof;
- map per-step bindings and pipeline variants;
- clean up all injected allocation/pipeline/pre/post-submit failures.

Gate:

- two/three-kernel pipelines;
- intermediate local with no host transfer;
- output and `_ptr` used across kernels with correct ordering/generation;
- empty loop among nonempty loops;
- local read-before-write, partial definition, length/binding/step overflow;
- debug counters prove one transfer phase, not one per kernel.

### Stage 8: hierarchical DOSUM

Primary files:

- DOSUM HIR-to-IR builder;
- reduction step descriptors;
- OpenGL reduction/scratch executor;
- numeric tests.

Work:

- generate first/fold kernels with uniform barriers;
- handle zero/one/general lengths;
- ping-pong scratch without host readback;
- materialize stable device result;
- bind it into later kernels/final output;
- support multiple sequential reductions;
- validate shared/workgroup limits before submission.

Gate:

- lengths 0, 1, 63, 64, 65, multi-workgroup, and large;
- int32/uint32 non-overflow and uint modulo cases;
- float32 documented tolerance;
- DOSUM followed by DOALL normalization;
- two reductions and independent scratch lifetimes;
- error cleanup with zero leaked GL objects;
- source/`.nb`/`.nap` parity.

### Stage 9: GPU-only semantic cutover

Primary files:

- `src/api/accel.c`;
- runtime/LIR/JIT/interpreter guards;
- `hir.c` optimizer dispatch;
- C/Elisp/Scheme backends;
- fallback/non-DOALL tests and docs.

Work:

- remove `Accel.call` CPU fallback and managed `accel_backend_sync_cpu` use;
- remove managed sequential GPU lowering and old eligibility path;
- make non-DOALL/non-DOSUM and unsupported accel bodies compile errors;
- make backend absence/failure runtime errors;
- prevent CPU body execution through dispatch depth;
- remove obsolete fields/functions after all references/formats migrate;
- reject accel in non-GPU source backends;
- convert fallback/non-DOALL tests to stable negatives or explicit raw examples.

Gate:

- deliberate CPU-only side effects in accel cannot compile/execute;
- disabled backend never mutates output and reports error;
- prefix and malformed reduction loops fail compilation;
- DOALL/DOSUM source and bundles pass on Intel OpenGL;
- no `ACCEL_DISPATCH_FALLBACK` route remains for `Accel.call`.

### Stage 10: hardening and documentation sync

Work:

- update design 16 fixed decisions, analysis/range/pass/runtime sections, error
  table, deferred list, and tests;
- update `docs/syntax.md`, `docs/library.md`, examples, and handoff;
- document local Packed device semantics and float reassociation;
- audit counts, OOM, GC roots, teardown, and backend cleanup;
- run full OpenGL/ONNX/cross-target regression;
- retain Vulkan compile-only status.

Gate:

- complete section-14 matrix;
- clean ASan/UBSan stderr;
- no path/backend handle in artifacts;
- clean worktree after final `WIP`;
- no push.

## 14. Test matrix

Keep `tests/run-accel.sh` and `tests/run-accel-opengl.sh` authoritative; add
focused runners where useful.

| Area | Required cases |
| --- | --- |
| analysis | CFG/path facts, affine offsets, scalar def/use, unknown effects |
| DOALL | distinct, in-place same index, shifted distinct read, RAW/WAR/WAW, constant write, scalar carried |
| DOSUM | canonical/commuted add, bad identity, missing update, two accumulators, store/call/other recurrence |
| local buffers | dynamic length, whole definition, read-before-write, mismatch, overflow, lifetime |
| multi-kernel | 2/3+ order, one upload/download, no intermediate host copy, empty step |
| reduction | 0/1/63/64/65/large, folds, float tolerance, uint wrap, downstream use |
| arguments | exact types/classes, overlapping host ranges, duplicate `_ptr`, short buffers |
| errors | backend absent, pipeline/binding/shared/workgroup failure, no CPU replay |
| cleanup | OOM injection, pre/post-submit failure, repeat calls, teardown, generations |
| persistence | source/`.nb`/`.nap`, relocation, truncation, bad enum/count/ref/hash, legacy rejection |
| levels | `-O0` through `-O3`, optimizer-disabled |
| host | interpreter and forced JIT host code call the same GPU program |
| source backends | stable C/Elisp/Scheme rejection |
| regressions | syntax, typing, ABCE, CSE, SIMD, class, scoping, app, raw GPU, weights, ONNX |
| platforms | Intel OpenGL execute; feature-off; MinGW/OpenWatcom compile; Vulkan compile-only |

Recurring commands, adjusted to actual build directories:

```sh
NOCT=/path/to/noct sh tests/run-accel.sh

EGL_PLATFORM=surfaceless \
NOCT_OPENGL_RENDERER_PATTERN=Intel \
NOCT=/path/to/opengl/noct \
sh tests/run-accel-opengl.sh

NOCT=/path/to/full/noct \
NOCT_META=/path/to/full/noct \
sh tests/test.sh all

sh tests/run-accel-serialization-hardening.sh
sh tests/run-onnx-package-opengl.sh
sh tests/run-onnx-mnist-opengl.sh
```

Run CIFAR/SqueezeNet gates at the final stage when locked assets are available.
Do not run or claim Vulkan execution.

## 15. Expected file changes

Expected additions/changes include:

```text
CMakeLists.txt
src/core/hir_parallel.h
src/core/hir_loop_analyze.c
src/core/hir_doall.c
src/core/hir_dosum.c
src/core/gpu_ir.h
src/core/gpu_ir.c
src/core/gpu_glsl.c
src/core/gpu_ir_ast.c or hir_gpu.c
src/core/gpu_ir_hir.c
src/core/hir_accel_program.c

src/core/accel.h
src/core/hir.h
src/core/hir.c
src/core/hir_opt.h only when removing the old entry
src/core/lir.h
src/core/lir.c
src/core/runtime.h
src/core/runtime.c
src/core/gc.c where ownership/rooting requires it
src/core/hir_gpu.c during common-IR migration

src/api/accel.c
src/api/accel_opengl.c
src/backend/bcback.c
src/backend/cback.c
src/backend/elback.c
src/backend/scmback.c

tests/accel/*
tests/run-accel.sh
tests/run-accel-opengl.sh
serialization/relocation hardening runners

docs/design/16-accel-vulkan.md at final sync
docs/syntax.md
docs/library.md
handoff documentation
```

Delete `hir_opt_accel.c` only after all mandatory responsibilities move and no
optional-optimizer dependency remains. Preserve bisectable intermediate stages.

## 16. Explicit non-goals and files not to change

### 16.1 SIMD boundary

Do not edit:

```text
src/core/hir_opt_simd.c
SIMD vector lowering in src/core/lir.c
SIMD opcodes/interpreter/JIT implementations
existing SIMD test expectations
```

Do not move/rename/reinterpret `abce_fast` or `is_vector`. Neutral HIR fields
may be added only when fully initialized and behavior-preserving. Run SIMD as a
regression. Future SIMD adoption of this analyzer is a separate project.

### 16.2 Other deferred work

Do not implement:

- CPU PE parallelization, `--cpu`, or simultaneous CPU/GPU work;
- async accel calls, overlap, multiple queues, or dependency scheduling;
- `hir_softpipeline.c` or software pipelining;
- fusion, cost models, autotuning, or `-O9` policy;
- non-zero starts, strides, nested source loops, polyhedral analysis, scans,
  atomics, min/max/product, or multiple accumulators;
- 8/16/64-bit or float64 GPU arithmetic;
- local source `accel var`, subviews, or host `_inout`;
- 3-D/subgroup/warp features;
- ONNX changes or new model support;
- Vulkan execution, D3D12, HLSL, Metal, or other backends.

Do not preserve CPU fallback behind a debug flag; that violates the contract.

## 17. Repository and implementation rules

1. Keep `src/core` C89/OpenWatcom-compatible: no VLAs, designated initializers,
   C99 loop declarations, or mixed declarations after statements.
2. Check counts and sizes before allocation, conversion, width multiplication,
   or OpenGL casts.
3. Every owned descriptor/IR/expression has init, clone where needed,
   validation, serialization where needed, and free.
4. Never retain AST/HIR pointers at runtime.
5. Do not leak GPU headers/symbols into feature-off builds.
6. This plan adds no syntax; do not edit generated parser/lexer files.
7. Preserve argument evaluation order and preflight before side effects.
8. Alias violations are runtime contract errors, not fallback.
9. Kernel/copy steps are optimizer side-effect barriers.
10. Internal names, descriptor order, GLSL, hashes, and diagnostics are
    deterministic.
11. No absolute paths in artifacts/tests/diagnostics.
12. Do not push; commit each green stage as exactly `WIP`.
13. Continue without review unless a genuinely new semantic choice outside
    this document is unavoidable.

## 18. Definition of done

Done means:

- common analysis/DOALL/DOSUM exists outside the optional optimizer with no
  SIMD/OpenGL assumptions;
- SIMD implementation is untouched and its full suite passes;
- supported accel functions compile to versioned multi-kernel programs at all
  levels and optimizer-off;
- every loop is DOALL/supported DOSUM; dependent/unknown loops fail compile;
- local intermediates and reduction scratch remain on GPU;
- each host `_in` uploads at most once and `_out` downloads at most once per
  synchronous call;
- Intel OpenGL executes ordered kernels/reductions correctly;
- backend absence/failure never runs a CPU body;
- source, bytecode, and `.nap` agree;
- raw GPU and ONNX generated models remain correct;
- sanitizer, feature-off, cross-build, and full regressions pass;
- public docs no longer claim CPU fallback, one-loop-only, or serial recurrence;
- final branch is clean, consists of `WIP` checkpoints, and is not pushed.
