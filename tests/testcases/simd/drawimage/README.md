# Draw-image SIMD essence cases

These files reduce the inner pixel loops from `/home/awe/drawimage.h` to
small Noct programs.  They are optimizer probes, not production-compatible
image APIs.  Clipping, row strides, scan conversion, notifications, and the
PC-98/PC/AT alpha-table specialization are deliberately outside the cases.

Source snapshot: SHA-256
`75dbc7ed68a92309116b8ea4335bd3ab9cbbd2586f7425fd4791bbbf1df43ee3`
(2026-08-12).

| Case | Source kernel represented |
|---|---|
| `blend-copy.noct` | `DRAW_IMAGE_COPY` |
| `blend-dim.noct` | `DRAW_IMAGE_DIM`; arithmetic part of `3D_DIM` |
| `blend-glyph.noct` | Common non-PC98 body of `GLYPH` and `EMOJI` |
| `blend-add.noct` | `DRAW_IMAGE_ADD`; arithmetic part of `3D_ADD` |
| `blend-sub.noct` | `DRAW_IMAGE_SUB`; arithmetic part of `3D_SUB` |
| `blend-rule.noct` | `DRAW_IMAGE_RULE` conditional store |
| `blend-melt.noct` | `DRAW_IMAGE_MELT` clamp and blend |
| `blend-cross.noct` | `DRAW_IMAGE_CROSS` after bounds are pre-clipped |
| `blend-3d-alpha.noct` | Affine coordinates plus one texture gather |
| `blend-3d-cross.noct` | Two rasterized texture gathers and cross blend |

Inspect the current optimizer decision with, for example:

```sh
NOCT_SIMD_DEBUG=1 build-static/noct -j0 -O2 \
  tests/testcases/simd/drawimage/blend-dim.noct
```

## Measured vectorization (2026-08-12)

- Noct: x86_64, `-O2` and `-O3` (same admission result).
- GCC 14.2: x86_64, `-O3 -msse4.1 -mno-avx`.
- GCC fast: same target with `-Ofast`.
- Apple Clang 17: Apple Arm64, `-O3 -mcpu=native`.

| Case | Noct | GCC O3 | GCC Ofast | Clang O3 |
|---|---|---|---|---|
| copy | x4 | `memcpy` | `memcpy` | `memcpy` |
| dim | E8 vreg budget | x4 | x4 | x4 |
| glyph | E2 body shape | scalar | x4 | x4 |
| add | E2 body shape | x4 | x4 | x4 |
| sub | E2 body shape | x4 | x4 | x4 |
| rule | E2 body shape | scalar | scalar | scalar (cost model) |
| melt | E2 body shape | scalar | x4 | x4 |
| cross | E8 vreg budget | x4 | x4 | x4 |
| 3d-alpha | indexed/induction loop not admitted | scalar | x4 | scalar |
| 3d-cross | indexed gather loop not admitted | x4 | x4 | x4 |

Representative compiler lowerings are `pminud`/`umin.4s` for add clamp,
`pmaxsd`/`smax.4s` for subtract clamp, `pblendvb` or `fcmgt.4s` plus
`bsl.16b` for glyph selection, and `minps`/`maxps` or compare plus
`bit.16b` for melt clamp.  SSE constructs a four-lane gather with `pinsrd`
and `punpcklqdq`; NEON uses scalar-lane `ld1.s` loads.

The Noct copy hot loop is already minimal: one `movdqu` load, one `movdqu`
store, `add index, 4`, and the back-edge `jne`.

## Source questions noticed during extraction

- The PC-98/PC/AT `DRAW_IMAGE_GLYPH` path appears to use `dst_a` as an
  `alphatable` index before assigning it.
- The PC-98/PC/AT ALPHA and GLYPH bodies do not appear to apply the function's
  global `alpha` argument after `check_draw_image`.
- `DRAW_IMAGE_CROSS` computes `src2_a` from source 1 alpha, while
  `DRAW_IMAGE_3D_CROSS` uses source 2 alpha.  The Noct cross case preserves
  the non-3D C behavior so this discrepancy remains visible.
