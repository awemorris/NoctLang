# ARM64 Packed-loop最適化 実装・評価レポート

## 概要

2026-08-14に、幅1のinteger Packed-loop向けJIT最適化をARM64へ実装し、
Apple M5で動作・性能を確認した。

実装前のARM64は`OP_PLOOP_HINT`からremainingをx21へ保持するだけで、Packed
base/index、中間tmpvar、型タグを反復ごとにmemoryへ読み書きしていた。実装後は
次の構成になった。

- x86_64ローカルだったPLOOP grammar/alias/base scannerをtarget-neutral化。
- x19/x20/x22へ最大3個の調整済みPacked baseを保持。
- w21へ負のelement cursorを保持。
- PLOAD/PSTOREを`[xBase,w21,sxtw #scale]`の1命令へlowering。
- latchを`adds w21,w21,#1`と`b.ne`の2命令へ短縮。
- w23--w28の6本でint32 tmpvar payloadをキャッシュ。
- AArch64 3-address integer ALU、signed division、moduloを直接emit。
- repeated PLOADを同一反復で再利用。
- signed range proofで安全なchecked divisionを直接`sdiv`化。
- ARM64ではscalar loop-carried valueをscannerで拒否し、memory-canonical fallbackを維持。x86_64は従来どおりlatchでキャッシュをflushするため、この制限を適用しない。
- DOALL型regionではGPR spillをbackedge前からloop exitへ移動。

bytecode opcode、Packed ABI、HIR SIMD判定、既存NEON vector opcodeの意味は変更して
いない。optimizerなしruntimeでも最適化済みPLOOP bytecodeを実行できる。

## ARM64 register契約

| register | 用途 |
|---|---|
| x0 | `rt_env *` |
| x1 | tmpvar array base |
| x19/x20/x22 | Packed base slot 0/1/2 |
| w21 | negative cursor兼loop condition |
| w23--w28 | int32 tmpvar cache 6 slots |
| x2--x8 | generic/cold-path scratch |

`NOCT_JIT_GPR_LIMIT`はARM64では0--6へclampされる。limit 0はcursor-only、limit 1は
強制spill試験、未指定は6-register cacheである。

## M5性能評価

対象は`bench/multi-doall.noct`の`md_single_cpu`である。

```text
elements: 4,194,304
loops:    8 ordered Packed loops
work:     33,554,432 element iterations
division: variable signed integer divisor 1..8
samples:  51
timing:   source registration/JIT/setup/verifyを除く1関数call区間
host:     Apple M5, arm64
compiler: Apple clang 17 -O3
```

| mode | best ms | median ms | worst ms | 最終Noct比 |
|---|---:|---:|---:|---:|
| 従来ARM64 PLOOP（regcache無効） | 377.373 | 378.093 | 389.118 | 12.47倍遅い |
| cursor-only (`NOCT_JIT_GPR_LIMIT=0`) | 243.799 | 243.904 | 251.160 | 8.04倍遅い |
| ARM64 6-GPR cache | 30.240 | 30.318 | 35.531 | 1.00 |
| clang C `-O3` | 16.621 | 16.791 | 16.984 | Noctより1.81倍高速 |

最終Noctは従来ARM64比12.47倍高速、cursor-only比8.04倍高速である。計画の
「clang比2倍以内」を満たした。

## hot loop命令監査

Noct第1loopの主要部は次の形になった。

```asm
ldr     w23, [x19, w21, sxtw #2]
mov     x24, #90
eor     w25, w23, w24
add     w26, w25, w23
mov     x25, #3
mul     w27, w26, w25
mov     x24, #7
and     w25, w23, w24
mov     x23, #1
add     w26, w25, w23
sdiv    w28, w27, w26
...
and     w24, w23, w28
str     w24, [x20, w21, sxtw #2]
adds    w21, w21, #1
b.ne    loop
```

base/index tmpvar load、per-op tag/payload store、bounds helper、normal hot-path helperは
消えている。register pressureで発生する一部spillはloop bodyに残るが、全dirty値の
writebackはfall-through exitだけである。

clangは同じvariable division loopを4反復unrollし、4本の`sdiv`を独立に発行して
division latencyを隠している。Noctとの差1.81倍の主因は次である。

- Noctはscalar 1反復、clangは4反復unroll。
- Noctは定数90/3/7/1/maskを一部GPRへmaterializeする。
- 6-register上限により、Noctには少数のloop-body spillが残る。
- clangは`*3`をshifted-add、low-bit maskをlogical immediateとして選択する。

次の性能改善候補は新opcodeではなくARM64 instruction selectionである。

1. `*3`を`add wD,wS,wS,lsl #1`へする。
2. `&7`、`&0x00ffffff`をlogical immediate/UBFMへする。
3. `+1`をadd-immediateへする。
4. next-use based victim選択でloop-body spillを減らす。
5. variable division loopの2-way/4-way scalar unrollを別計画で検討する。

## 併せて修正したARM64 NEON問題

multiarch回帰で`blend-add`の上位2 laneだけ結果が異なる問題を検出した。
`OP_VMINS32X4/OP_VMAXS32X4`のNEON encodingでQ bitが欠け、`.4s`ではなく`.2s`
として実行されていた。

```text
誤: 0x4e206c00 / 0x4e206400
正: 0x4ea06c00 / 0x4ea06400
```

修正後、QEMU ARM64のscalar/NEON tierと`blend-add`が通った。

## テスト結果

- packed-loop suite: PASS（limit 0/1/full、roundtrip、3 base、非0 start）
- ARM64 loop-carried scalar rejection / x86_64 compatibility: PASS
- syntax interpreter/JIT/-O2: PASS
- CLI/typing/typed-op/ABCE/CSE: PASS
- SIMD/drawimage blend suite: PASS
- parallel-analysis: PASS
- ASan packed-loop suite: PASS
- QEMU ARM64 SIMD scalar/NEON: PASS
- 全multiarch: PASS
  - i386/x86_64/ARM32/ARM64/RISC-V64/PPC32/PPC64/MIPS32/MIPS64
- M5 native packed-loop suite: PASS
- M5 native `blend-add`: PASS

## 追加した評価資産

- `bench/packed-loop-cpu-bench.c`: NoctのJIT済み1関数callを測定するdriver。
- `bench/multi-doall-c.c`: `multi-doall.noct`と同じ8-loop処理のclang比較用C実装。
- `tests/testcases/packed-loop/three-base.noct`: 3 base、非0 start、spill fixture。
- `NOCT_JIT_DUMP_DIR`: standard JIT backendでもraw native codeを保存可能。
- `NOCT_JIT_CODEGEN_DEBUG`: standard backendでもcode size/PC/branch数を表示。
