# 戻り値型契約と mixed SIMD の実装計画

Status: implemented on `dev`; native and QEMU functional tests complete

Implementation note (2026-08-10): Phases 1--7 and 9--12 are implemented.
Phase 8 uses safe single-use temporary folding and physical-register reuse;
loops that still exceed the eight-register portable budget take the scalar
path instead of generating spills.  This is an intentional deviation from the
initial spill proposal: it preserves the existing zero-spill backend contract
and is sufficient for `blend2.noct`.  The profitability guard estimates body
work and rounds its minimum trip count to a complete four-lane group.

The mixed conversion path was executed with scalar and native ceilings on
x86-32 (SSE2/SSE3/SSE4.1), ARMv7 (NEON), ARM64 (ASIMD), PPC32 and PPC64
(AltiVec), plus the MIPS32/64 and RISC-V32/64 scalar JITs under qemu-user.
Native x86_64 regression suites also pass.  An M5 native functional run and
short performance sample are recorded in design 08; POWER8 measurement
remains optional performance work, not a correctness gate.

この文書は、`tests/simd/blend2.noct` を正しくベクトル化できる状態までの作業と、
既存 SIMD/JIT 実装の移植性・堅牢性を仕上げる作業を、依存関係順に定義する。
実装は `dev` ブランチで小さなコミットに分け、レビュー後の `main` への squash merge
は手動で行う。

## 1. 対象と完了条件

主な対象は次の通りである。

1. `func foo(param: type): type { ... }` 形式の戻り値型注釈。
2. optimize level 2 に限定した戻り値チェックと、呼び出し側での戻り値型の信頼。
3. `Float.from()` と `Int.from()` の関数全体での型認識と、変換命令への特殊化。
4. 整数 packed 入力、浮動小数点の中間値、整数 packed 出力が混在するループの SIMD 化。
5. `blend2.noct` の内側ループを SIMD 化し、スカラ実行と同じ結果を得ること。
6. 全 JIT アーキテクチャでの命令能力判定、ネイティブ命令またはスカラ fallback。
7. SIMD レジスタの ABI 保存規則と、分岐到達距離超過の監査・対策。

次は今回の範囲外とする。

- Hindley–Milner 型推論や、プログラム全体の一般的な型推論。
- 任意のユーザー関数を純粋関数とみなして SIMD ループ内へ取り込むこと。
- bytecode を敵対的入力として検証すること。bytecode は内部仕様であり信頼する。
- Boots、プロセス、タスクスケジューラの作業。このロードマップとは別に扱う。
- 実機性能測定をマージ条件にすること。クロスビルドと qemu-user の機能確認を
  必須とし、M5 Mac と POWER8 の測定は非ブロッキングとする。

## 2. 現在地

既存実装で、以下は完了済みの土台として扱う。

- `packedint8/16/32/64`、`packeduint8/16/32/64` と、それぞれの `rpacked...`。
- `packedfloat`、`packeddouble`。
- `rpacked` 引数による restrict/非 alias 契約。
- i32x4 と f32x4 の基本 vector opcode。
- x86/x86_64、ARM32/ARM64、PPC32/PPC64 の SIMD capability と、その他を含む
  scalar fallback の基本構造。
- x86 32-bit JIT の SSE2 経路。
- `--simd-info` による、ベクトル化されたループのソース位置表示。
- runtime の vector backing store は 16 個 x 16 byte。

一方、現在の optimizer は物理 vector register 数を 8 個として扱い、SIMD の
適格性解析は基本的に「単一 lane 型の単純な式」を前提にしている。
`blend2.noct` を SIMD 化するには、単に変換 intrinsic の型を付けるだけでは不足する。

現時点で確認した主要 blocker は以下である。

| blocker | 必要な対策 |
| --- | --- |
| call の結果が SIMD 解析時に UNKNOWN | 型伝播を SIMD 解析より前へ置く |
| `Float.from` / `Int.from` が通常の mutable call | intrinsic の同一性を保証し、変換 node に特殊化する |
| `y * 1280 + x` を連続アクセスと認識できない | loop-invariant 部分を hoist/canonicalize する |
| uint32 packed と float 演算が同一 loop にある | mixed scalar type と mixed lane IR を導入する |
| 多数の一時値が 8 vector register を超える | DCE、寿命解析、再利用、必要なら spill を実装する |
| `int / float` 等の昇格規則が不足 | 限定的な数値昇格を明文化して実装する |
| float-to-int の端値が backend ごとに異なり得る | 言語意味と fallback 条件を固定する |

## 3. 固定する言語仕様

### 3.1 戻り値型の構文

```
func foo(p1: int, p2: packeduint32): float {
    return Float.from(p1) + Float.from(p2[0]);
}
```

戻り値型は省略可能で、既存コードとの互換性を保つ。型名の解決は引数型注釈と
同じ resolver を使う。`rpacked...` は引数の alias 契約なので戻り値型としては
compile error にする。`packed...` は戻り値型として許可する。

これは一般的な型推論ではなく、明示された signature と局所データフローに基づく
`signature-directed type propagation` と位置付ける。

### 3.2 level ごとの意味

| optimize level | 戻り値注釈の実行時効果 | 呼び出し結果の型として信頼 |
| --- | --- | --- |
| 0 / 1 | なし。構文・metadata は保持する | しない |
| 2 | return edge で契約を保証する | 保証済みの呼び出しだけ信頼する |

level 2 では return operand を一度だけ評価し、次の規則を適用する。

| HIR 上の証明 | 動作 |
| --- | --- |
| 注釈型と一致 | check を挿入しない |
| 型が UNKNOWN | `OP_CHECKRETURN` を return transfer の直前に挿入 |
| 注釈型と不一致 | level-2 compile error |

複数の `return`、bare `return`、末尾への fallthrough をすべて return edge として扱う。
暗黙の戻り値が注釈に適合しなければ同じ規則で失敗させる。

### 3.3 戻り値型の厳密性

呼び出し側が check なしで利用できるよう、戻り値契約は runtime tag と packed element
kind の厳密一致とする。引数で許されている `int` から `long`、`float` から `double`
のような受け入れ拡張を、戻り値には適用しない。

- sized integer alias は既存の storage tag へ正規化する。
- packed は packed tag と element kind の両方を確認する。
- diagnostic は `<function>(): return type mismatch (expected <type>)` の形にする。

### 3.4 bytecode の信頼境界

bytecode loader は return contract を再検証しない。level 2 の bytecode が「戻り値型を
信頼可能」と表明する場合、compiler が unknown return edge に必要な check を必ず生成した、
という内部 invariant を置く。異なる level/契約で生成した object の混在は build pipeline
側で禁止する。

### 3.5 変換 intrinsic の意味

- `Float.from(int)` は i32-to-f32 変換として特殊化する。
- `Int.from(float)` は、有限かつ int32 範囲内なら 0 方向への丸めとする。
- NaN、無限大、範囲外の値について、native SIMD 命令の偶然の結果を言語仕様にしない。
  範囲が証明できない場合は generic intrinsic を維持するか、範囲 guard と scalar slow path
  を生成する。最初の実装は「証明できた場合だけ SIMD 特殊化」を採用する。
- optimizer が intrinsic として認識する `Float.from` / `Int.from` は上書き不能にする。
  名前だけを見て mutable な一般関数を intrinsic とみなしてはならない。

## 4. 型 metadata と伝播の設計

### 4.1 metadata の経路

次の順に、戻り値型 metadata を欠落なく運ぶ。

```
parser -> AST -> HIR function -> LIR function -> bytecode -> runtime function
```

parser の生成物も repository 管理対象なので、grammar 更新後は既存の再生成手順で更新する。
bytecode section は後方互換な optional section とし、旧 bytecode は「注釈なし」と読む。

### 4.2 型証明の表現

型証明は `UNKNOWN` と実型を明確に分離する。runtime の `INT` tag が 0 であっても、
`memset(0)` された解析領域を誤って int と解釈しない表現にする。推奨は次のいずれかである。

1. proof enum の 0 を UNKNOWN とし、runtime tag は `tag + 1` で格納する。
2. `known` bit と type payload を分ける。

全 node を手作業で UNKNOWN 初期化する方式は、新 node 追加時に unsoundness を生みやすいため
採用しない。

### 4.3 pass の順序

型伝播を一回だけ後段で実行するのではなく、用途を分ける。

1. signature collection: 同一 compilation unit の function signature を body より先に収集。
2. early type propagation: SIMD/ABCE より前に parameter、constant、arithmetic、intrinsic
   conversion の型を付ける。
3. transformation: intrinsic specialization、index canonicalization、SIMD 化。
4. final validation: return contract と変換後 HIR の整合性を再確認。

再帰関数でも signature 自体は body 解析前に利用できる。ただし、Noct の function/global binding
は再定義可能なので、名前が一致するだけの一般 call を安全とみなさない。

- resolver が binding の安定性を証明できる direct call だけ、宣言された戻り値を伝播する。
- mutable、first-class、method、動的 call は UNKNOWN のままにする。
- `Float.from` / `Int.from` は上書き不能な intrinsic identity によって識別する。

戻り値型が既知でも、任意のユーザー関数は pure/non-allocating とは限らない。従って型注釈だけで
一般 call を vector loop 内に許可しない。今回は intrinsic descriptor に限り result type、purity、
non-allocation、変換種別を持たせる。

## 5. 実装フェーズと依存関係

以下を原則として一フェーズ一コミットにし、各コミット単独で build/test を通す。

### Phase 0: baseline と golden semantics の固定

- 現在の level 0/1/2、interpreter/JIT、`--simd-info` の出力を記録する。
- `blend2.noct` の期待出力を scalar reference として用意する。
- 入力を固定 seed で初期化し、全 pixel または checksum を比較可能にする。
- 現在のサンプルに見える `src_pix = dst[...]` と `255.0 - a` が意図通りか確認し、
  意図した alpha blend の式を golden test として固定する。ここは推測で修正しない。

依存: なし。

### Phase 1: return annotation の構文と metadata

- lexer/parser、AST、HIR/LIR/runtime function 構造体へ optional return type を追加。
- bytecode writer/reader に optional return-type section を追加。
- pretty printer、dump、diagnostic の型名表示を更新。
- level 0/1 では実行意味を変えない。

依存: Phase 0。

### Phase 2: level-2 return contract

- HIR 型 proof を UNKNOWN-safe な表現へ変更。
- `OP_CHECKRETURN` を HIR、LIR、bytecode、interpreter/JIT 共通 semantics に追加。
- known match、unknown check、known mismatch の三分岐を実装。
- explicit/bare/multiple/fallthrough return を網羅。
- level-2 runtime function に「checked return signature」metadata を付ける。

依存: Phase 1。

### Phase 3: intrinsic identity と早期型伝播

- `Float.from` / `Int.from` を名前比較でなく intrinsic ID で表す。
- object model の single-thread/multi-thread 両実装で、対象 member の上書きを拒否する。
- early type propagation pass を SIMD 解析より前に配置する。
- signature の先行収集と、安定性を証明できる direct call の限定伝播を実装。
- optimizer debug dump に proof と intrinsic specialization 理由を表示できるようにする。

依存: Phase 2。return contract が caller trust の根拠になるため、順序を逆にしない。

### Phase 4: scalar conversion と限定的な数値昇格

- scalar HIR/LIR に i32-to-f32、f32-to-i32 conversion node/opcode を追加。
- `int op float` で必要な箇所に明示 conversion を挿入し、結果型を float とする。
- `Int.from(float)` は範囲証明または guard がある場合だけ特殊化する。
- interpreter を portable reference semantics とし、各 JIT の scalar lowering を一致させる。

依存: Phase 3。

### Phase 5: loop-invariant index の正規化

- `y * width + x` の invariant 部分を loop 外で一度計算する。
- packed access を `base + induction + constant` の canonical form にする。
- overflow、負 index、bounds-check の意味を変えない。
- ABCE が canonical form を認識し、vector main loop と scalar tail の双方で安全性を維持する。

依存: Phase 0。実装順としては Phase 4 後に統合し、mixed expression の debug を容易にする。

### Phase 6: mixed-type ABCE と SIMD 適格性解析

- loop 全体を packed element type 一種類と仮定せず、value ごとに scalar/lane type を持たせる。
- uint32 load、shift/mask、i32-to-f32、f32 arithmetic、f32-to-i32、shift/or、uint32 store
  の chain を許可する。
- side effect、alias、throw、allocation、未知 call が残れば安全側で reject する。
- reject reason を debug と `--simd-info` の optional verbose mode で追跡可能にする。

依存: Phase 4 と Phase 5。

### Phase 7: mixed vector IR と portable execution

- vector opcode に `VCVTI32F32X4` と `VCVTF32I32X4` を追加。
- opcode ごとに input/output lane type を検証し、暗黙の bit reinterpret を禁止。
- vector interpreter/scalar-vector executor を最初の参照実装にする。
- vector main loop と scalar tail の結果を bit/許容誤差の仕様に従い比較する。

依存: Phase 6。

### Phase 8: vector register 寿命管理

- dead code elimination と single-use temporary の縮約を先に行う。
- vector value の live interval を計算し、終了した物理 register を再利用する。
- logical vector ID と backend physical register ID を分離する。
- 8 本で収まらない場合だけ spill/reload を生成し、runtime の 16 x 16-byte backing slot を
  spill home として利用する。
- `SIMD_VREG_MAX` を単純に増やして x86 32-bit 等の実レジスタ数を超える実装は禁止する。

依存: Phase 7。`blend2` の高い register pressure を native backend へ渡す前に解決する。

### Phase 9: backend lowering と capability fallback

各 conversion opcode について、backend ごとに次の順で実装する。

1. capability があり、言語 semantics を満たす native SIMD sequence。
2. 複数命令の安全な emulation sequence。
3. lane-wise scalar fallback。

対象は x86/x86_64、ARMv7、ARM64、PPC32/PPC64、MIPS、RISC-V の既存 JIT module 全て。
ARMv7 は NEON、PPC は AltiVec を runtime capability が示す場合だけ emit する。
qemu-user では対応 CPU model/feature を明示して命令を実行し、SIGILL がないことと結果一致を
確認する。実機 M5/POWER8 の性能値は補助資料とする。

x86/x86_64 は capability tier を SSE2、SSE3、SSE4.1（既存名が SSE4.2 なら実使用命令に
合わせて整理）として明示する。SSE3 固有命令が今回の dataflow に利益を与えない場合、
無理に別 sequence を作らず SSE2 sequence を選ぶ。その場合も「SSE3 CPU で SSE4.1 命令を
emit しない」ことを test する。優先度は他 backend の正しさより低い。

依存: Phase 8。

### Phase 10: profitability guard

SIMD 化可能でも、短い loop には適用しない。

- `body_cost * trip_count` の概算値を使う。
- static bound は compile time に判断する。
- dynamic bound は overflow しない比較（必要 work から最小 trip count を算出）で guard し、
  小さい場合は scalar path へ分岐する。
- threshold は magic number として散在させず、一箇所の設定値にする。
- x86_64 benchmark script で overhead の交点を測り、既定値を決める。共有 server 上の測定は
  ユーザーが実行できるよう script のみ repository に置く。

依存: Phase 9。

### Phase 11: `blend2.noct` 統合と回帰評価

- golden semantics を確定した `blend2.noct` の内側 loop を level 2 で vectorize する。
- `--simd-info` が正しいファイル名・行番号を一度だけ報告する。
- no-SIMD、scalar-vector、各 native tier の出力を比較する。
- alpha blend、f32 affine、u32 inplace、u32 3-buffer の既存 benchmark を再実行可能にする。
- benchmark CSV を読む report script は median 基準の speedup と min/max を表示する。

依存: Phase 10。

### Phase 12: ABI と branch-range の architecture audit

このフェーズは correctness の横断監査であり、Phase 9 と並行可能だが、最終完了条件には含める。

#### SIMD register の prologue/epilogue

- 各 ABI で volatile/callee-saved な SIMD register を表にする。
- JIT が volatile register だけを使うなら退避を入れない。
- Windows x64 の XMM6–XMM15 など callee-saved register を allocator が使う場合のみ、使用集合を
  prologue/epilogue で保存・復元する。
- x86 32-bit、SysV x86_64、AArch64、ARM EABI、PPC ELF ABI を個別に test する。
- 全 SIMD register を無条件保存する実装は、frame 増大と性能低下のため避ける。

#### 相対 branch の到達距離

- fixup に「必要距離が encoding に収まるか」の検査を必須化し、silent truncation を禁止する。
- x86 の short branch は near branch へ relaxation し、rel32 を超える外部 target は absolute
  sequence/thunk を使う。単一 JIT allocation 内の rel32 超過は明示的 compile error でもよい。
- ARM/AArch64、PPC、MIPS、RISC-V は backend-local veneer/trampoline または long-branch sequence
  を定義する。
- conditional branch は、条件反転 + long unconditional branch で範囲を伸ばせる。
- literal pool/constant island が必要な backend は同じ layout pass で扱う。
- 実装コストが過大な稀な距離については、理由と最大 code size を diagnostic に含めた
  clean compile failure を許容する。
- 巨大な synthetic HIR で short/near/long の境界値を test する。

依存: register 監査は Phase 8/9、branch 監査は各 backend emitter の layout 情報。

## 6. テスト計画

### 6.1 戻り値型

- 構文あり/なし、全 scalar 型、全 packed 型。
- `rpacked...` 戻り値の compile error。
- known match、known mismatch、unknown runtime mismatch。
- multiple return、bare return、fallthrough、再帰。
- level 0/1 で注釈が実行結果を変えず、level 2 だけ check/trust すること。
- bytecode round-trip と旧 bytecode の読み込み。
- mutable/dynamic call の戻り値を誤って信頼しないこと。

### 6.2 intrinsic と変換

- `Float.from` / `Int.from` を loop 外と loop 内の両方で認識。
- intrinsic member の上書き試行を明示的に拒否。
- i32 の境界、f32 の ±0、端数、NaN、±Inf、int32 範囲外。
- interpreter、JIT scalar、SIMD native、SIMD fallback の結果一致。

### 6.3 mixed SIMD

- uint32 の channel extract/pack。
- invariant row base + induction variable の連続アクセス。
- alias する packed 引数では reject、`rpacked` では許可。
- trip count 0..vector width 周辺、scalar tail、動的 bound。
- register pressure が 8 未満、ちょうど 8、8 超のケース。
- `blend2.noct` の全 pixel/checksum 比較。

### 6.4 architecture matrix

| target | build | functional | native capability | fallback |
| --- | --- | --- | --- | --- |
| x86_64 | native | native | SSE2/SSE3/SSE4.1 tier | forced-disable |
| x86 | cross/native | qemu/native | SSE2/SSE3/SSE4.1 tier | forced-disable |
| ARMv7 | cross | qemu-user | NEON on | NEON off |
| ARM64 | cross | qemu-user | ASIMD on | forced-disable |
| PPC32/64 | cross | qemu-user | AltiVec on | AltiVec off |
| MIPS/RISC-V | cross | qemu-user | 対応済みの場合のみ | scalar-vector |

qemu-user は SIMD 命令を実行できるが、選択した QEMU CPU model が機能を公開している必要がある。
従って build 成功だけでなく、feature-on/off の両方を明示して実行する。性能評価には使わない。

## 7. 推奨コミット列

1. `docs: plan return types and mixed SIMD pipeline`
2. `parser: carry function return annotations`
3. `optimizer: enforce level-2 return contracts`
4. `runtime: identify immutable conversion intrinsics`
5. `optimizer: propagate signature and intrinsic result types`
6. `ir: add checked scalar numeric conversions`
7. `optimizer: canonicalize loop-invariant packed indices`
8. `optimizer: accept mixed numeric SIMD dataflow`
9. `ir: add mixed-lane vector conversions`
10. `optimizer: reuse and spill vector temporaries`
11. `jit: lower mixed conversions with capability fallbacks`
12. `optimizer: add SIMD profitability guards`
13. `tests: vectorize and benchmark alpha blending`
14. `jit: audit SIMD ABI preservation and long branches`

各コミットで関係のない user file、benchmark 結果、backup file を stage しない。push は行わない。

## 8. レビュー checkpoint

次の checkpoint で設計または結果をレビューする。

1. Phase 2 後: level-2 return trust が bytecode invariant を含めて sound か。
2. Phase 4 後: `Int.from(float)` の端値と guard 方針が言語仕様として適切か。
3. Phase 7 後: portable executor で `blend2` の mixed dataflow が表現できるか。
4. Phase 8 後: x86 32-bit の 8 register 制約で spill/reuse が正しいか。
5. Phase 11 後: `--simd-info`、結果一致、benchmark script が受け入れ条件を満たすか。
6. Phase 12 後: architecture ごとの未対応 long branch が clean failure として明記されているか。

最終的な `main` への squash merge と push はこのロードマップの自動作業に含めない。
