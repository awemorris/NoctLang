# Scalar Packed-loop unrolling

After ABCE and SIMD admission, an eligible scalar Packed loop can be split
into a factor-4 bulk loop and a scalar tail. The HIR pass preserves source
iteration order and rewrites Packed indices to `index + lane`. LIR retains
the existing PLOAD/PSTORE operand format and identifies the bulk loop with
`PLOOP_UNROLL4`; its `INC` and `SUBJNZ` immediates are both four.

The common x86_64/ARM64 PLOOP scanner propagates constant index offsets and
records a signed element displacement for every Packed access LPC. Address-
only ASSIGN, ICONST, IADD, and ISUB instructions are omitted after complete
use accounting. Repeated-load keys include the displacement.

- x86_64 folds the checked byte displacement into the SIB disp8/disp32.
- ARM64 factor-4 loops keep each Packed base at the current four-element
  group and use scaled or unscaled immediate loads/stores. The latch advances
  each live base by `4 * element_size` before `subs`/`b.ne`.
- Other backends execute the expanded portable bytecode normally.

## Profitability policy

x86_64 and ARM64 enable the transform by default. Final uint16 scalar-loop
measurements improved from 2.051 ms to 1.465 ms on Apple M5 (1.400x), and from
15.773 ms to 14.941 ms on the shared x86_64 host (1.056x). Exact definition-LPC
use accounting was important: it removed constants whose only consumers were
elided lane-address expressions.

Architectures without offset-aware native lowering leave the transform off by
default. `NOCT_UNROLL_ENABLE=1` is their developer/portable-bytecode override.

Variable DIV/MOD loops are also rejected by the initial profitability model.
Four sequential copies raised register pressure and were much slower on both
x86_64 and ARM64. `NOCT_UNROLL_EXPENSIVE=1` enables this case for allocator and
scheduling experiments. `NOCT_UNROLL_DISABLE=1` disables the pass on every
architecture.

These environment variables are developer controls, not public CLI options.
Enabling variable-division unrolling by default requires a lane-aware
scheduling/register-allocation improvement and before/after native measurements.
