# 16 — ARM64 vector-loop code generation improvement

Status: **implemented and validated on Apple M5**
(2026-08-11).

This plan turns the `tests/simd/blend2.noct` arm64 inner vector loop from the
simulated PBASE/INC result of 113 instructions into the 42-instruction shape
already produced by Apple Clang 17.  Read [06-simd.md](06-simd.md),
[05-cse.md](05-cse.md), and the measured
[blend2 arm64 assembly report](../bench-blend2-arm64-assembly.md) first.

## Implementation record (2026-08-11)

The implementation now includes:

- `OP_INC` with an explicit step on every interpreter/JIT/backend;
- PBASE virtual IDs, with ARM64 mappings for IDs 0/1;
- `OP_VINDEX_HINT`, portable `OP_SUBJNZ`, and ARM64 index suppression;
- a vector-local structural value cache with backedge-live constants and
  invariants kept in unique logical vregs;
- 16 logical vregs on arm64, mapping logical 8..15 to caller-saved
  physical v16..v23 while deliberately avoiding callee-saved v8..v15;
- source/destination pixel-load CSE, `pix_a`/inverse-alpha CSE;
- removal of the redundant alpha mask after logical shift by 24;
- portable `OP_VORI32X4I`, with single-instruction ARM64 opaque-alpha OR;
- ARM64 `x19`/`x20` cursors selected by a guarded bytecode pre-scan; and
- direct short backward `subs`/`b.ne` latch emission, with long fallback.

The final `blend2.noct` recurrent LIR contains 40 one-word NEON operations
(including exactly two loads and one store).  The accepted hint suppresses
the semantic `OP_INC` and adds the two-word `subs`/`b.ne` latch, giving the
planned **42 recurrent ARM64 instructions**.  Native Apple M5 disassembly
confirmed the exact count, including v16/v17 scratch allocation, and a
multi-group regression confirmed that constants remain valid after the first
four lanes.

Validation completed on x86_64: full SIMD suite (interpreter, JIT, forced
scalar, SSE2/SSE3/SSE4.1 ceilings, bytecode round trip), syntax suite, typing,
ABCE, typed-op, C-backend translation, and forced-long-branch tests.  ARM64 and
the portable JIT additions were also compiled through architecture-forced
syntax translation units on the source host.  Apple M5 native, interpreter,
and forced-scalar modes agree on the 13-pixel multi-group regression.

A 50-sample M5 benchmark over 100 calls of a 1280x720 blend, excluding JIT
time, produced 49.041312 ms Clang and 49.022042 ms Noct medians when Clang was
built with `-ffp-contract=off`; the full-frame hashes were identical.  Default
contracting Clang used FMA and measured 45.093938 ms versus Noct 48.956479 ms,
but its repeated-blend hash intentionally differs because contraction changes
rounding.

The implementation must preserve the portable bytecode/interpreter contract.
The new register declarations are hints: refusing a hint may reduce performance
but must never change behavior or make compilation fail.  All backends must
decode every new operand/opcode before LIR starts emitting it.  The optimized
native implementation in this phase is arm64; other targets use correct
fallbacks until independently audited.

---

## 1. Target and non-goals

The recurrent four-pixel loop must reach this static accounting on arm64:

| Category | Target instructions |
|---|---:|
| countdown and back edge | 2 |
| one destination load, one source load, one destination store | 3 |
| channel extraction and result packing | 16 |
| integer/float conversions | 10 |
| floating-point blend arithmetic | 11 |
| **total** | **42** |

The zero-trip guard, initial cursor/count setup, scalar-tail state writeback,
and outer row loop are outside this recurrent count.  They still require
correct code and tests.

This work does not introduce a general-purpose register allocator, arbitrary
loop dependence analysis, wider vectors, FMA contraction, or reordering of
non-vectorizable loops.  It specializes the already-proven vector strip loop.

---

## 2. Authoritative decisions

### D-A64VL1: hints have no runtime semantics

The loop-index declaration opcode emits no machine instruction by itself.
Interpreter/C-backend behavior is a no-op after operand consumption.  The JIT
uses a bytecode pre-scan to decide whether it can honor the declaration.  All
ordinary semantic operations remain in bytecode so declining the hint is safe.

### D-A64VL2: virtual IDs are not physical registers

PBASE and loop-index operands carry small virtual allocation IDs.  The arm64
JIT maps accepted PBASE IDs to `x19`, `x20`, ... and loop state to another
reserved register.  Bytecode never contains `x19` or another target register
number.  `0xff` means unallocated/fallback.

### D-A64VL3: two-instruction control uses cursors plus remaining count

An ascending semantic index cannot in general be updated and tested in two
instructions while also addressing forward data.  The accepted arm64 hint
represents the same state as:

- advancing source/destination cursor registers; and
- one remaining-element register decremented by four.

The semantic index update stays in bytecode for fallback execution but is
suppressed in the recurrent arm64 loop when cursor-only legality is proven.  On
vector-loop exit the JIT writes the known vector stop index back once for the
scalar tail.

### D-A64VL4: vector CSE is loop-local and memory-aware

`hir_opt_cse.c` deliberately skips vector loops because `HIR_EXPR_CAPTURE`
breaks the current vector grammar.  Do not simply remove that guard.  Add a
vector-local DAG/value-numbering and liveness stage to the vector LIR planner.
It may reuse pure expressions and packed loads within one vector body, while
respecting stores and restrict/alias facts.

### D-A64VL5: preserve loop-live homes and use arm64 caller-saved capacity

Constants and invariants are live across the loop backedge even when their last
use in one iteration precedes a scratch expression.  Their homes must never be
reused unless they are rematerialized before the next use.  The arm64 planner
may use logical vregs 0..15; its emitter maps logical 0..7 to v0..v7 and 8..15
to caller-saved v16..v23.  Other targets retain an eight-vreg plan and discard
low-value cache candidates until the original HIR budget fits.  Portable
execution uses the existing 16-slot `env->vreg` array.

### D-A64VL6: exact arithmetic is unchanged

Keep `-ffp-contract=off`-equivalent behavior: no FMA introduction and no
algebraic reassociation.  CSE reuses values that the source program already
defines; it does not reorder floating operations.

---

## 3. Bytecode and LIR contracts

Opcode numbers below are symbolic.  Place new opcodes without violating the
contiguous typed/vector helper-table ranges in `src/core/bytecode.h`, then
update every range check and table in the same change.

### 3.1 Extend OP_PBASE

Change the encoding from:

```text
OP_PBASE dst:u16, packed:u16
```

to:

```text
OP_PBASE dst:u16, packed:u16, base_id:u8
```

Semantics remain `dst = packed payload address`.  `base_id` is only a register
allocation hint.  Every interpreter/JIT/backend must consume the new byte even
when it ignores it.

Before bytecode emission, add a small HIR/LIR prewalk that finds PBASE-derived
locals used as bases by vector loops, counts static PLOAD/PSTORE uses, and
assigns IDs in descending use-count order.  This avoids patching an OP_PBASE
that may be emitted before its vector loop.  Unproven bases receive `0xff`.

The prewalk must enforce:

1. the PBASE definition dominates every hinted vector use;
2. its destination local is not reassigned in the region;
3. the packed buffer cannot be replaced by an allowed body operation; and
4. reused IDs never have overlapping live vector-loop regions.

For the first arm64 implementation, accepting IDs 0 and 1 is sufficient for
blend (`dst` and `src`).  Additional IDs may map to `x21`/`x22` only after the
loop-state allocation below is fixed and clobber-audited.

### 3.2 Extend OP_INC with a step

Retain the previously agreed format change:

```text
OP_INC dst:u16, step:u8       # dst.val.i += step
```

Ordinary loops emit one; the vector semantic index emits four.  This operation
is still required for interpreter and fallback JIT correctness even though an
accepted cursor hint suppresses its recurrent arm64 machine instruction.

### 3.3 Add the non-emitting vector-index declaration

Add:

```text
OP_VINDEX_HINT
    index_tmp:u16
    stop_tmp:u16
    remaining_tmp:u16
    index_id:u8
    lanes:u8
    flags:u8
```

Initial flags:

```text
VINDEX_CURSOR_ONLY    0x01  # every index use is a bare packed element access
VINDEX_WRITEBACK_STOP 0x02  # write stop_tmp to index_tmp on vector exit
```

Place the hint after preheader initialization and immediately before the first
body opcode.  It produces zero native words.  Nested hinted vector loops are
not supported in this phase; emit `index_id=0xff` or fall back if encountered.

The JIT pre-scan validates that:

- every PLOAD/PSTORE in the region uses `index_tmp` as its bare element index;
- the only update of `index_tmp` is the matching `OP_INC step=lanes`;
- the matching latch is `OP_SUBJNZ remaining_tmp, lanes, body_lpc`;
- the body has no helper call, break, return, or non-whitelisted opcode; and
- the branch target is exactly the first opcode after `OP_VINDEX_HINT`.

If validation fails, the hint is ignored and all semantic bytecode is emitted.

### 3.4 Add subtract-and-branch

Add the semantic operation:

```text
OP_SUBJNZ value:u16, decrement:u8, target:u32
```

Its portable meaning is:

```text
value.val.i -= decrement
if value.val.i != 0: pc = target
```

The vector LIR preheader computes `remaining = vector_stop - start` and retains
the existing zero-trip guard outside the hot loop.  The latch becomes:

```text
OP_INC     index_tmp, lanes
OP_SUBJNZ  remaining_tmp, lanes, body_lpc
```

Interpreter and non-optimizing backends execute both operations.  With an
accepted cursor-only hint, arm64 emits no recurrent word for the OP_INC and
emits exactly:

```asm
subs xLoopRemaining, xLoopRemaining, #4
b.ne vector_body
```

On fallthrough only, emit the required tmpvar writebacks after the conditional
branch; these words are outside the loop back-edge range.  Conditional branch
range failure must use an inverted short condition plus an unconditional long
branch when representable, otherwise use the existing clean JIT-failure path.

### 3.5 Add vector OR-immediate

Add a portable vector opcode:

```text
OP_VORI32X4I vd:u8, vs:u8, imm8:u8, shift:u8
# vd.lane = vs.lane | ((uint32_t)imm8 << shift)
# shift is one of 0, 8, 16, 24
```

The LIR allocator must choose `vd == vs` for the opaque-alpha combine so arm64
emits one instruction.  Add the portable `env->vreg` helper and backend
fallbacks before emitting this opcode from LIR.

For `imm8=0xff, shift=24`, arm64 must emit the verified word:

```text
4f0777e3  orr v3.4s, #0xff, lsl #24   # register field varies with vd
```

Do not implement this as an arm64-only multi-op peephole; preserving the intent
in LIR makes the count deterministic and gives every backend a correct fallback.

---

## 4. Vector-local CSE and vreg allocation

### 4.1 Why the existing CSE is insufficient

`cse_walk_for()` in `src/core/hir_opt_cse.c` explicitly skips
`is_vector` loops.  The current vector LIR visitor recursively emits each
expression occurrence, so blend produces nine source vector loads, three
destination loads, six alpha conversions, six `pix_a` computations, and three
inverse-alpha computations.

Leave the general HIR CSE rule intact and add the following stage inside the
vector planner shared conceptually by `hir_opt_simd.c` and `lir.c`.

### 4.2 Build a per-iteration value DAG

Walk the single eligible basic block in source evaluation order.  Intern pure
nodes using keys containing:

- opcode/expression kind;
- value-number IDs of operands (canonical order for commutative operations);
- immediate value/shift and lane type;
- for PLOAD, `(load kind, PBASE local, index local, memory version)`; and
- for conversion intrinsics, the intrinsic ID and operand value number.

Map each HIR temporary symbol to its value node.  A repeated symbol read uses
the existing node; do not recursively rebuild the defining expression.

PSTORE handling:

1. evaluate and retain the RHS node first;
2. emit the store in original order;
3. increment the memory version of the stored base;
4. if restrict proves distinct bases, invalidate only that base; otherwise
   invalidate all cached PLOAD nodes.

Calls and other effects are already illegal in the vector grammar.  If that
grammar expands later, an effect must conservatively invalidate memory CSE.

### 4.3 Compute liveness across the backedge

Count uses and record each node's last use.  Emit nodes in statement order, but
classify constants and invariants as backedge-live.  They retain unique homes
for the complete strip loop.  Per-iteration cache values and expression
scratch may be freed after their last use in an iteration.

The blend schedule must retain:

- one source-pixel vector and one destination-pixel vector;
- `pix_a` and `pix_inv_a`;
- the mask only until the final channel extraction;
- one result accumulator; and
- at most two channel scratch vectors.

The optimized blend needs two additional logical scratch registers.  On arm64
they become v16/v17, preserving the 42-instruction schedule without a per-
iteration re-splat.  On eight-register targets the planner drops cache entries
until its computed expression peak fits; it never aliases a backedge-live home.

If allocation fails:

1. discard the DAG emission for that loop;
2. restore the bytecode output position and planner state; and
3. re-run the existing recursive vector lowering unchanged.

Never turn register pressure into a compilation failure or scalarize a loop
that currently vectorizes without first trying the existing lowering.

### 4.4 Required CSE result for blend

Before moving to cursor codegen, the LIR dump must show exactly:

- one VLOAD for `src_pix`;
- one VLOAD for `dst_pix`;
- one source-alpha extraction/conversion;
- one `pix_a` multiply;
- one `pix_inv_a` subtract; and
- one VSTORE of the result.

The RGB source/destination conversions and blend operations remain once per
channel.  No floating reassociation is allowed.

---

## 5. arm64 register/cursor implementation

### 5.1 Register reservation

The current arm64 prologue already saves/restores `x19` through `x28`, and
`x19`/`x20` are otherwise unused.  Initial mapping:

```text
PBASE id 0 -> x19
PBASE id 1 -> x20
remaining  -> x21
scratch    -> existing x2/x3/x4 convention
frame      -> existing x1
```

Audit all arm64 emitters and reserve accepted registers globally.  Normal and
exception epilogues already restore them; tests must cover both paths.

At OP_PBASE, continue writing the raw pointer tmpvar and additionally place it
in its accepted physical register.  At a hinted vector-loop preheader, reset
the physical register from the raw tmpvar if a previous loop may have advanced
it, then adjust it once for a nonzero start.  These setup instructions are
outside the recurrent loop.

### 5.2 Cursor memory selection

The arm64 JIT pre-scan records the last vector memory use of each accepted base
within the loop.  Emit earlier uses without writeback and the last use with a
post-index of 16 bytes:

```asm
ldr qDst, [x19]          # dst is also stored later, so do not advance yet
ldr qSrc, [x20], #16     # last src use
...
str qDst, [x19], #16     # last dst use
```

These exact forms have been assembled successfully.  If the base has an
unsupported offset pattern, multiple element indices, or cannot be assigned,
use the existing fixed-base/index path and do not suppress semantic OP_INC.

The alternative `ldr/str q, [base, index, lsl #4]` encoding is valid for a
16-byte vector ordinal, but cursor post-index is the required first
implementation because it naturally supports the two-instruction countdown.

### 5.3 Loop latch and exit

Initialize `x21` from the precomputed positive remaining count before the body.
At OP_SUBJNZ emit the verified `subs`/`b.ne` pair.  On the non-taken path only:

- write zero to `remaining_tmp` if it is live after the loop;
- write `stop_tmp` to `index_tmp` when `VINDEX_WRITEBACK_STOP` is set; and
- close the active hint/cursor mapping before scalar-tail emission.

No vector arithmetic opcode in the accepted grammar may clobber integer NZCV
between `subs` and the immediately following branch.

---

## 6. Alpha simplification and immediate packing

### 6.1 Remove the redundant alpha mask

In the vector-expression canonicalizer, recognize only the proven unsigned
lane pattern:

```text
(x >> 24) & 0xff  ->  x >> 24
```

Require `x` to be a uint32 packed/vector value and the shift to lower as logical
`VSHRI32X4`.  Do not apply it to a signed arithmetic shift.  Test alpha values
`0x00`, `0x7f`, `0x80`, and `0xff` to cover the sign bit.

This changes six repeated two-op alpha extractions to one `ushr` after CSE and
is responsible for the 44-to-43 instruction step.

### 6.2 Emit opaque alpha with OR-immediate

Canonicalize the final result so the RGB vector is accumulated first and
opaque alpha is applied last with `OP_VORI32X4I(..., 0xff, 24)`.  Ensure the
allocator uses the same source/destination vreg.  This removes the separate
alpha-vector shift and is responsible for the 43-to-42 step.

---

## 7. Required source changes

The implementing agent must audit at least these locations; do not stop after
the arm64 switch compiles:

| Area | Files / functions |
|---|---|
| Opcode definitions and operand comments | `src/core/bytecode.h` |
| Vector planning, prewalk, DAG/CSE, liveness, emission, LIR dump/walker | `src/core/lir.c` |
| SIMD legality, unsigned lane facts, mirrored vreg-budget calculation | `src/core/hir_opt_simd.c` |
| Existing vector-loop CSE exclusion/comment | `src/core/hir_opt_cse.c`, `docs/design/05-cse.md` |
| Portable semantics/new vector helper | `src/core/interpreter.c`, `src/core/execution.c`, declarations in internal headers |
| C backend | `src/backend/cback.c` |
| Optimized target, pre-scan, register maps, post-index loads, SUBJNZ, OR immediate | `src/core/jit-arm64.c` |
| Correct consume/fallback for changed/new opcodes | every `src/core/jit-*.c` backend |
| Architecture include dependencies | build files covered by `docs/design/13-jit-build-dependencies.md` |
| Design and measured output | `docs/design/06-simd.md`, this plan, `docs/bench-blend2-arm64-assembly.md` |

Search every opcode switch and every `OP_VLOADI32X4..OP_*` range/table with
`rg` after changing the enum.  A stale operand-size walker will misalign all
following bytecode and can appear as `JIT_OP_NOT_IMPLEMENTED` rather than a
local failure.

Core sources are C89.  Keep declarations at block starts and preserve the
project's tab/formatting conventions.

---

## 8. Dependency-ordered implementation phases

### Phase 0 — Freeze the baseline

1. Build and run the existing interpreter and arm64 JIT SIMD tests.
2. Preserve the current blend output hashes and the 156-instruction raw dump.
3. Add a script/check that extracts and counts the recurrent loop categories.

Exit: baseline results in the assembly report are reproducible.

### Phase 1 — Portable opcode contracts, no optimized emission

1. Extend OP_PBASE and OP_INC encodings.
2. Add OP_VINDEX_HINT, OP_SUBJNZ, and OP_VORI32X4I.
3. Update the interpreter, execution helper, C backend, LIR dumper/walker, and
   every JIT decoder/fallback.
4. Do not emit hints/new vector OR from normal LIR yet.

Exit: all targets build; focused hand-encoded bytecode tests prove decoding and
portable semantics; existing tests are unchanged.

### Phase 2 — Vector-local CSE and backedge-correct vreg liveness

1. Add the per-iteration DAG, memory versions, restrict-aware invalidation, use
   counts, unique loop-live homes, and per-iteration scratch reuse.
2. Retain transactional fallback to the existing recursive lowering.
3. Emit the new plan for blend and verify the required CSE result in the LIR
   dump before touching arm64 cursors.

Exit: blend has two vector loads and one store in LIR; alpha/pix_a/inverse are
single computations; L0/L2/interpreter/JIT outputs remain identical.

### Phase 3 — PBASE/index hints and semantic countdown LIR

1. Add the PBASE-use prewalk and base ID operands.
2. Change vector-loop lowering to precompute remaining count, emit the no-op
   index hint, one semantic OP_INC step four, and OP_SUBJNZ.
3. Keep all backends on fallback behavior initially.

Exit: interpreter and non-arm64 JITs run the new loop correctly for zero trip,
exact vector trips, and scalar remainders.

### Phase 4 — arm64 accepted hints, cursors, and two-word latch

1. Add the arm64 pre-scan/validation and register reservations.
2. Materialize hinted PBASEs, initialize cursors/count once, select last-use
   post-index loads/stores, suppress the recurrent semantic OP_INC, and emit
   SUBS/B.NE.
3. Emit exit writeback outside the back edge and retain complete fallback paths.

Exit: recurrent counts are control=2 and data/address=3; there are no frame
loads, scalar index loads, or explicit address adds in the loop.

### Phase 5 — Alpha and opaque-output instruction selection

1. Apply the unsigned alpha mask simplification.
2. Select OP_VORI32X4I and emit arm64 modified-immediate OR.

Exit: extraction+packing=16 and total recurrent loop=42 instructions.

### Phase 6 — Cross-target regression and M5 validation

Run the test matrix below, capture a new branch-patched M5 dump, compare it
against Clang, and append measured results to the assembly report.  Performance
measurement is evidence, not a correctness gate; functional qemu-user results
are sufficient for non-M5 targets.

---

## 9. Test matrix

### 9.1 Opcode and fallback tests

- OP_PBASE with `base_id=0xff`, accepted IDs, duplicate/conflicting IDs, and
  more hinted bases than target registers.
- OP_INC steps one and four.
- OP_VINDEX_HINT ignored by interpreter/C backend and rejected safely by a JIT.
- OP_SUBJNZ taken, not taken, zero-trip preguard, and branch patching.
- OP_VORI32X4I for shifts 0/8/16/24 and all four lanes.

### 9.2 Loop-shape tests

- lengths 0, 1, 3, 4, 5, 7, 8, and a large non-multiple of four;
- nonzero range start;
- one packed input, two packed buffers, and more bases than available hints;
- read-only, write-only, and read/write bases so the correct last memory use is
  post-indexed;
- scalar tail observes the correct writeback index;
- any currently legal `continue` path targets the latch; and
- a deliberately unsupported body rejects the hint and matches fallback.

### 9.3 CSE/alias tests

- repeated PLOAD from one restrict base is loaded once;
- src/dst restrict bases remain independent across the destination store;
- a store to a possibly aliasing/nonrestrict base invalidates cached loads;
- repeated conversions and arithmetic are reused without reassociation;
- vreg-pressure overflow transactionally selects old lowering; and
- `--simd-info` reports CSE hits, max live vregs, and accepted/rejected hints.

### 9.4 Alpha and numerical tests

- source alpha bytes 0x00, 0x7f, 0x80, and 0xff;
- channel endpoints 0 and 255;
- representative alpha parameters including 0, 1, 128, 254, and 255;
- byte-for-byte equality between interpreter L0, interpreter L2, scalar JIT,
  optimized arm64 JIT, and the existing C reference; and
- preserve `-ffp-contract=off` operation ordering.

### 9.5 Cross-architecture build/run

At minimum build every configured JIT backend after the operand changes.  Run
native x86_64 tests and qemu-user arm32/arm64/PPC/RISC-V/MIPS tests available in
the repository.  A target without optimized hints must execute the portable
fallback, never report an unknown opcode.

---

## 10. Final arm64 acceptance checklist

With `NOCT_JIT_ARM64_DUMP=blend`, the branch-patched recurrent loop must contain:

- exactly 42 instructions per four pixels;
- `ldr qDst,[xDst]`, `ldr qSrc,[xSrc],#16`, and
  `str qDst,[xDst],#16` exactly once each;
- no tmpvar/frame load in the recurrent loop;
- no explicit packed address-generation add in the recurrent loop;
- one source-alpha `ushr #24` and no following alpha mask;
- seven int-to-float and three float-to-int conversions;
- seven FP multiplies, one FP subtract, and three FP adds;
- one opaque-alpha modified-immediate OR;
- one `subs #4` immediately followed by one `b.ne`; and
- no recurrent OP_INC machine sequence or unconditional back-edge branch.

Then run the existing 50-sample, 100-call M5 benchmark with JIT time excluded.
Record best/median Noct time, C time, slowdown ratio, output hash/equality, exact
source revision, compiler version, and the new disassembly in
`docs/bench-blend2-arm64-assembly.md`.

The work is complete only when all portable fallbacks pass and the M5 dump
matches the static accounting.  Reaching 42 instructions without identical
output is a failure.
