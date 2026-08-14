# Fixed tmpvar types and Packed-loop register allocation

## Bytecode contract

`OP_TMPVAR_TYPE(tmp, tag)` is append-only metadata emitted at the beginning of
an optimized function.  `tag` is one of the primitive `NOCT_VALUE_*` tags, or
`TMPVAR_TYPE_DYNAMIC`; bit `TMPVAR_TYPE_COMPILER_TEMP` identifies a
compiler-owned temporary slot independently of whether its runtime tag is
fixed.

The declaration has no interpreter or C-backend semantics.  Every bytecode
consumer must still validate and consume it.  JIT backends may use a fixed tag
only to omit a tag store; payload semantics and entry `OP_CHECKTYPE` checks are
unchanged.  Parameters are not considered frame-tag-known until their entry
check succeeds.  Fresh non-parameter fixed-int slots are known because
`rt_enter_frame()` zeroes the frame and `NOCT_VALUE_INT` is zero.

LIR determines a slot's fixed type by meeting every definition assigned to
that slot.  Unknown and conflicting definitions yield `DYNAMIC`.  Since LIR
temporary indices are reused, this is a whole-function property, not a
per-expression promise.  Prepending metadata shifts absolute bytecode PCs;
block relocations, loop backedges, fused latches, and logical short-circuit
patches are all recorded and adjusted by the prefix size.

## Scalar Packed-loop allocation

The x86_64 and ARM64 `OP_PLOOP_HINT` fast paths use deterministic next-use
allocation:

- take an empty register first;
- otherwise prefer a value with no future use;
- otherwise evict the value whose next use is farthest away;
- retain a cached Packed load until a matching load can reuse it;
- reuse a dead source register for a destination when the ISA form permits;
- x86_64 keeps integer constants as rematerializable facts and uses immediate
  arithmetic where possible.

Dirty compiler temporaries with no future use may be discarded instead of
spilled in non-vector functions.  Named locals are always preserved because
they may be loop live-outs.  SIMD functions retain conservative scalar-tail
spilling; this avoids treating a vector/remainder live-out as a dead compiler
temporary.

The loop cache stays register-canonical across the backedge.  Live-out spills
are emitted after the conditional backward branch, so they execute only on
the loop's fall-through exit.  A fallback to a generic VM-frame visitor uses a
forced flush and, on x86_64, publishes rematerialized constants first.

## Current generated shape

For one `multi-doall` stage on x86_64, the hot loop is 17 instructions:
one Packed load, two value-preserving copies, seven arithmetic instructions,
the four-instruction signed division sequence, one Packed store, and the
two-instruction latch.  VM tag/payload spills are outside the backedge.

The remaining two-copy gap to the 15-instruction target is the explicit
`EAX/EDX` signed-division convention and preservation of the shared input
value.  A future transient fixed-register descriptor for `EAX` can address
that gap; this change deliberately does not introduce a general allocator or
AVX2-width expansion.

## Validation

Required regression gates are `typing`, `typedop`, `packed-loop`, `simd`, and
`multiarch`.  The `multiarch` suite verifies that interpreter and every JIT
consumer recognize the append-only metadata, including optimized bytecode
round trips under QEMU.
