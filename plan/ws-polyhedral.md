# WS-POLY: Full polyhedral model with bounded scheduling

Status: Planned; architecture review required before implementation

## Goal

Implement a complete polyhedral representation and exact legality framework
for affine static-control regions while keeping optimization time practical by
constraining schedule generation, candidate search, and code growth.

The mathematical model should be general within the supported affine domain.
The optimizer is deliberately not required to enumerate every legal schedule.

## Design principles

- Separate full modeling and legality from bounded profitability search.
- Preserve existing optimizers as conservative fallbacks.
- Prefer deterministic operation and candidate budgets over wall-clock-only
  timeouts.
- Use different search budgets for latency-sensitive JIT compilation and AOT.
- Reject or guard transformations when integer overflow, aliasing, calls, or
  floating-point reassociation cannot be proven safe.
- Obtain expert review of the representation, dependence model, scheduler, and
  code generator before their interfaces become stable.

## Polyhedral representation

The planned representation covers:

- multidimensional iteration domains and symbolic parameters;
- affine and supported quasi-affine expressions, including constant
  floor/ceiling division and modulo;
- statement instances and read/write access relations;
- RAW, WAR, WAW, and reduction dependences;
- original and transformed schedules represented as schedule trees;
- exact schedule-legality checks;
- runtime contexts for bounds and no-alias versioning; and
- reconstruction of structured HIR from a legal schedule.

Existing memory catalogs, loop summaries, alias classifications, and
DOALL/DOSUM results are inputs to this model. Existing ABCE, SIMD, unroll, and
accelerator passes may consume its facts after transformation or analysis.

## Bounded scheduler and search

Search should proceed in stages rather than taking the Cartesian product of
all transformations:

1. Expose legal outer parallel dimensions.
2. Consider a bounded set of loop permutations.
3. Consider adjacent fusion or fission opportunities.
4. Consider skewing with small bounded coefficients.
5. Select from a small target-specific tile-size set.
6. Place a contiguous, dependence-free dimension innermost for SIMD.
7. Leave remaining eligible scalar loops to the unroll pass.

Initial deterministic limits should cover:

- solver operations;
- candidates visited and retained beam width;
- loop-nest depth, statements, accesses, and parameters;
- schedule coefficient magnitude;
- generated HIR nodes and estimated bytecode growth;
- runtime versions per region; and
- one final wall-clock safety limit that does not normally select the result.

Every exhausted budget falls back to the unchanged HIR or an already-proven
candidate. Budget exhaustion is not a compilation error.

## Compilation modes

- JIT: small fixed search budget and immediate fallback.
- Normal AOT: balanced bounded search.
- Deep AOT or research mode: wider search and optional profile-guided or
  empirical evaluation.

Polyhedral support should be an optional build feature independent of the
basic optimizer and SIMD auto-vectorizer.

## Milestones

1. Write the integer, overflow, alias, effect, and floating-point semantics for
   affine transformations; review them with polyhedral experts.
2. Implement model extraction and stable diagnostics without transformations.
3. Implement an independently testable dependence and legality checker.
4. Regenerate the original schedule and prove semantic round trips.
5. Add one transformation, initially loop interchange, behind strict budgets.
6. Add fusion/fission, skewing, and tiling incrementally.
7. Connect legality/profitability facts to ABCE, SIMD, unroll, and accelerator
   planning.
8. Tune target cost models only after correctness and compile-time budgets are
   stable.

## Validation strategy

- Differential execution of original and transformed HIR.
- Property-based generation of small affine loop nests.
- Negative tests for aliasing, carried dependences, overflow, calls, and
  floating-point reductions.
- An optional external oracle during development, such as isl or another
  established polyhedral tool, without requiring it in production builds.
- Compile-time, generated-code-size, and runtime-performance budgets tracked
  separately.

## Expert-review checkpoints

Seek review before committing to:

- integer-set and relation representation;
- handling of parameters, division, modulo, and overflow;
- dependence construction and legality proofs;
- schedule-tree structure and code generation;
- objective functions and search-space restrictions; and
- benchmark and correctness methodology.

## Acceptance criteria for the first usable release

- All supported affine regions have a precise model or a deterministic reason
  for rejection.
- Every applied schedule passes an independent legality check.
- Compilation remains within configured operation and code-growth budgets.
- Budget exhaustion and unsupported regions preserve current behavior.
- At least interchange, fusion or fission, tiling, and parallel/SIMD placement
  are represented, even when the bounded search chooses not to apply them.
- JIT latency and binary footprint are measured before enabling the feature by
  default in any preset.

## References

- MLIR Affine dialect: https://mlir.llvm.org/docs/Dialects/Affine/
- Integer Set Library manual: https://libisl.sourceforge.io/manual.pdf
- Pluto: https://www.ece.lsu.edu/jxr/pluto/
- Polly architecture: https://polly.llvm.org/docs/Architecture.html
