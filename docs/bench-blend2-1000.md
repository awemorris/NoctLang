# blend2 1000-pixel call benchmark

Date: 2026-08-10

`tests/simd/blend2.noct` の修正版と同じアルファブレンド式を、連続する
1000 pixel に適用する一回の関数呼び出しについて測定した。

## 測定境界

- JIT は threshold 0 で source 登録時に生成する。
- 最初に 1-pixel の呼び出しを行い、JIT code の commit を完了する。
- 続いて測定対象と同じ 1000-pixel 呼び出しを5回 warmup する。
- 入力バッファの初期化と各回の復元は測定区間外とする。
- 単調時計を `blend_bench()` の呼び出し直前と復帰直後だけで読む。
- O0 と O2 をそれぞれ50回測り、最速値と通常の偶数標本中央値
  （25番目と26番目の平均）を求める。
- x86_64 はCPU 63へ固定した。M5はmacOS上で通常実行した。
- 両アーキでO0/O2のchecksumは `7225778` で一致した。
- O2では同じloopが `i32x4` mixed conversion loopとしてvectorizeされた。

これは「720p frameを1000回処理」ではなく、「同じalpha-blend kernelを
1000 pixel反復する一回の関数呼び出し」の測定である。

## 結果

| architecture | level | fastest | median | O0からの高速化（fastest） | O0からの高速化（median） |
|---|---:|---:|---:|---:|---:|
| Intel Xeon Gold 6130 x86_64 | O0 | 1.136730 ms | 1.166826 ms | 1.00x | 1.00x |
| Intel Xeon Gold 6130 x86_64 | O2 | 0.004029 ms | 0.004124 ms | 282.14x | 282.94x |
| Apple M5 arm64 | O0 | 0.496500 ms | 0.765479 ms | 1.00x | 1.00x |
| Apple M5 arm64 | O2 | 0.002458 ms | 0.002542 ms | 201.99x | 301.13x |

O0は動的なpacked access、数値演算、`Float.from` / `Int.from` の処理を
各pixelで行う。O2ではABCE、typed operation、invariant index normalization、
mixed i32/f32 SIMDが組み合わさるため、倍率は単なるSIMD幅4より大幅に大きい。

## 再現用ファイル

- `tests/bench/blend2-call-bench.noct`: 修正版alpha kernel。
- `tests/bench/blend2-call-bench.c`: VM/JIT warmup、関数境界計時、50標本集計。

Linux x86_64ではrelease buildの`libnoct.a`/`libnoctapi.a`へリンクし、
macOS arm64では`macos-arm64` presetへ`NOCT_ENABLE_OPTIMIZER=ON`を追加した
release buildへリンクした。
