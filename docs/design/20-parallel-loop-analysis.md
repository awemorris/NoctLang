# Target-neutral parallel loop analysis

## Purpose

`hir_loop_analyze.c`, `hir_doall.c`, and `hir_dosum.c` provide the common,
target-neutral facts used by parallel execution consumers.  The analysis is
shared by accelerator kernel formation and CPU SIMD vectorization.  It does
not choose an execution target, a vector width, a register assignment, or a
runtime scheduling policy.

The central separation is:

- common analysis describes loop memory accesses, scalar effects, calls,
  affine indexes, dependences, reductions, and unresolved alias obligations;
- a consumer applies its own legality and profitability rules;
- lowering and the JIT remain target-specific.

This lets future CPU DOALL/DOSUM and accelerator passes reuse one conservative
dependence model without forcing SIMD-only restrictions onto GPU kernels, or
GPU-only restrictions onto SIMD loops.

## Input levels

The collector accepts both source-like HIR and the lowered packed-memory HIR
seen after ABCE.  In particular it recognizes packed subscript expressions as
well as `PLOAD*`, `PSTORE*`, `PMASKSTORE32`, and `PGATHER32`.  Consumers supply
a `hir_memory_catalog` mapping symbols to stable object IDs, storage classes,
element widths, and alias classes.

The normal-function catalog is deliberately permissive about non-affine reads.
A gather-like read can still be DOALL when no write can conflict with it.  A
non-affine write remains unknown.  Accelerator catalog behavior is unchanged
and remains strict where its existing contract requires it.

## Affine normalization

`hir_parallel_normalize_index()` canonicalizes the forms currently required by
the consumers:

- `i`
- `i + C`, `C + i`, and `i - C`
- `i + u`, `u + i`, and `i - u`, where `u` is loop invariant

The result records the loop coefficient, constant offset, and optional signed
invariant symbol.  `hir_parallel_affine_equal()` compares canonical forms.
Expressions outside this intentionally small grammar are reported as
non-affine rather than guessed.  After collection, every symbolic offset is
checked against the scalar-effect summary; a symbol written anywhere in the
loop is not invariant and makes the index non-affine.

## Classification results

The common result distinguishes:

- `DOALL`: no carried memory or scalar dependence was found;
- `DOSUM`: a supported scalar sum reduction was found;
- `DEPENDENT`: a definite RAW, WAR, WAW, or scalar-carried dependence exists;
- `UNKNOWN`: analysis is incomplete, a call is unresolved, an index is not
  safely classified, or runtime alias information is required.

Potential overlap between distinct memory objects is represented by deduplicated
`alias_requirement` pairs.  It is not silently treated as disjoint.  A consumer
may reject the loop, prove the pair disjoint from a stronger contract, or emit
a runtime guard.

`hir_doall_classify_memory()` exposes the memory-only part of classification.
This is used by SIMD because SIMD has additional scalar rules that do not belong
in the generic DOALL definition.

## SIMD consumer policy

The SIMD pass keeps responsibility for all vector-specific constraints:

- supported packed element types and operation grammar;
- 4-lane strip formation, scalar remainder, minimum trip count, and frame/vreg
  budgets;
- float induction handling and live-out extraction;
- adjusted packed bases and runtime byte-range alias guards;
- target feature selection and opcode emission.

After its existing expression grammar and environment checks, SIMD builds a
catalog for the ABCE `PBASE` symbols and invokes the common loop collector.
Definite common memory dependences reject vectorization.  May-alias object pairs
become the existing runtime disjointness guards.  Scalars read before their
first write reject vectorization except for the explicitly recognized floating
induction form.  Scalars written before any read remain SIMD-private or
lastprivate and are handled by the existing SIMD planner.

This integration does not introduce vector reduction lowering.  A sum loop can
be classified as DOSUM by the generic analysis while still being rejected by
the present SIMD consumer.

## Diagnostics and tests

`NOCT_PARALLEL_DEBUG=1` prints one stable `parallel-analysis` line for each loop
in a normal function.  This is a developer diagnostic and has no effect when
unset.  `NOCT_SIMD_ANALYSIS_COMPARE=1` prints the SIMD front-end decision beside
the common-analysis decision and lists scalar effects; it is intended for
shadow comparison and regression work.

The `parallel-analysis` test suite checks optimization-level invariance,
DOALL/DOSUM/dependent/unknown classification, symbolic affine forms,
non-affine read/write policy, alias requirements, and the SIMD integration.
The existing accelerator-analysis golden output remains unchanged.

## Non-goals

This layer does not perform scheduling, choose PE counts, generate guards,
rewrite loops, emit SIMD/GPU code, infer arbitrary points-to relationships, or
solve general symbolic arithmetic.  Extending those areas must preserve the
facts/consumer boundary above and add conservative tests before changing an
existing classification.
