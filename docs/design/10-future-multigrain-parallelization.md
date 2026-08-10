# Future work: 階層的マルチグレイン自動並列化

Status: exploratory discussion only; no implementation is scheduled or approved

この文書は、Noct にプログラム全域の階層的な自動並列化を導入するという
将来研究のアイデアを記録する。現時点では実装計画でも言語仕様でもなく、
既存の最適化や `NoctConfig` の契約を変更しない。再開時には、ここに記録した
前提を改めてレビューしてから詳細設計を行う。

## 1. 研究上の位置付け

基本となる考え方と用語は、笠原博徳先生らによる OSCAR のマルチグレイン
並列処理研究に由来する。Noct で検討する拡張は、弱い型情報、動的呼び出し、
インタプリタのプロファイル、JIT 再特殊化、および汎用スレッドプールを持つ
処理系で、この方法の適用範囲と有用性を示すことを目的とする。

原手法と Noct 固有の拡張を文書・実装・将来の発表で明確に区別する。
公表を検討する段階では、笠原先生に内容を提示してレビューを受けられる。

## 2. 用語

マクロタスクの基本分類は次のとおりとする。

| 種別 | 名称 | 意味 |
| --- | --- | --- |
| BB | Basic Block | 通常の基本ブロック |
| LOOP | Structured Loop | 構造化されたループ。DOALL/DOSUM の解析対象 |
| RB | Repetition Block | LOOP として構造化できず、DOALL/DOSUM にできない反復領域 |
| SB | Subroutine Block | 呼び出し先を静的に確定できるサブルーチン呼び出し |
| VSB | Virtual Subroutine Block | 呼び出し先を静的に確定できないサブルーチン呼び出し。Noct で追加する分類 |

DOALL、DOSUM、SIMD、およびスケジューリング方式はブロック種別ではなく、
LOOP の解析結果または実行属性とする。RB に DOALL/DOSUM 属性を付けては
ならない。

構造化された LOOP は、依存解析の結果により次のいずれかになる。

```text
LOOP
  +- DOALL
  +- DOSUM
  `- sequential LOOP
```

構造化されているがループ伝搬依存を除去できない場合は `sequential LOOP` で
あり、構造化に失敗した RB とは区別する。RB 全体を反復方向に分割できない
ことと、RB 内部に粗粒度の並列性が存在しないことは同義ではない。

## 3. 不完全な invocation graph

invocation graph の完全な構築を成功条件にしない。direct call または解析で
単一 target に解決できた call site は SB とし、解決不能または複数候補が
残る call site は VSB としてグラフ内に残す。VSB をグラフから脱落させたり、
その存在を理由に関数全体の解析を破棄したりしない。

初期状態の VSB はブラックボックスとして、任意の状態を読み書きし、並列
実行できないものと保守的に扱う。呼び出し先は既知だが、本体を解析できない
外部 API は VSB ではなく、effect summary を持つ opaque SB とする。

array/dict の alias と要素依存は最初の実装では解決不能として扱う。これらの
アクセスを新しいブロック種別にはせず、BB、LOOP、RB、SB の effect を
`World` にする。将来、restrict、定数伝搬、symbolic な伝搬、および簡単な
points-to 解析を配列領域やキー単位へ拡張する。

## 4. MFG と MTG

用語は OSCAR の体系に合わせる。

- Invocation Graph: 関数間の SB/VSB の関係。
- Macro Flow Graph (MFG): BB/LOOP/RB/SB/VSB 間の制御フローと
  データ依存を表す。
- Macro Task Graph (MTG): MFG から実行可能条件を解析して得る、
  並列スケジューリング用の階層グラフ。
- Processor Group (PG): ある階層へ割り当てる論理的な PE budget。
- Processor Element (PE): 実行資源の単位。

制御フローを無条件に捨ててデータ依存だけに変換するのではない。value、
RAW/WAR/WAW、predicate/control、reduction、barrier、I/O の順序を扱い、
最早実行可能条件を表現する MTG を作る。Noct には例外が存在しないため、
exception edge と unwind は考慮しない。

SB は nested task 並列化の主要な階層境界とする。解析可能な SB は callee の
内側に MTG を持てる。VSB は特殊化されるまで子 MTG を持たない。LOOP は
構造化された HIR と反復範囲を保持し、DOALL/DOSUM と SIMD の粒度を選択
できる。RB は反復単位では分割しないが、解析できる範囲で内部の階層 MTG を
持つ可能性を残す。

## 5. effect と I/O

API descriptor に最低限、pure、readonly、write、I/O の effect flag を持たせる。
未指定の API と VSB は unknown effect とする。

I/O を含むマクロタスクは、すべての変数およびメモリ操作と依存するとみなす。
実装では全変数との依存辺を列挙する代わりに、単一の仮想 `World` トークンを
読み書きさせる。unknown effect も初期実装では同様に扱う。この表現により、
I/O と不明な副作用の前後順序を保守的に直列化できる。

## 6. profile-guided de-blackboxing

インタプリタは将来、少なくとも次の情報を call site と LOOP ごとに収集する。

- VSB で観測した target と頻度。
- 引数と戻り値の型。
- LOOP の trip count。
- マクロタスクと LOOP の実測実行時間。
- 実行時に検証可能な shape、長さ、stride、およびアドレス範囲。

プロファイルは特殊化候補を選ぶヒントであって、安全性の証明には用いない。
callee identity、function version、型、レイアウト、非 alias なアドレス範囲
などを入口ガードで検証する。すべてのガードは原則として副作用の実行前に
確認し、途中 deoptimization と状態復元は初期実装の対象外とする。

ガード付き devirtualization の結果に、新しいマクロタスク種別を導入する
必要はない。

```text
BB: callee == foo && version == expected ?
  +- true  -> SB(target=foo, nested MTG available)
  `- false -> VSB(original fallback)
```

静的解析で target が一つに証明された場合は VSB を直接 SB に置換する。
プロファイル上で単相に見えるだけの場合は VSB のままとし、必ず上記の
guarded SB と fallback VSB を残す。この将来変形を profile-guided
de-blackboxing と呼ぶ。

## 7. thread pool と PE 割り当て

将来 `NoctConfig.auto_parallel = N` を導入する場合、N は worker 数ではなく
総 PE 数とする。

- N == 0: 自動並列化を無効化。
- N == 1: caller のみで逐次実行。
- N > 1: N-1 worker を事前起動し、caller を PE0 として参加させる。
- worker は VM の lifetime 中待機し、VM 終了時にのみ OS thread を join する。
- 各並列領域の終了では thread join ではなく completion barrier を用いる。
- nested task は親 PG の PE budget 内で動作し、oversubscription しない。

コンパイラは静的な階層 MTG と粗いコスト式を生成し、実行時 scheduler が
実際の入力、trip count、プールの空き、および実測時間から PE 数を決める。

初期のコストモデルは次の程度でよい。

```text
estimated_work = fixed_cost + iteration_cost * trip_count
desired_pe = clamp(1, available_pe,
                   ceil(estimated_work / minimum_grain))
```

実行コストの大きい LOOP に PE 割り当ての優先度を与え、small/medium/large
程度の粗い queue から始める。将来は実測値の移動平均、critical path、追加
PE による限界短縮量、cache locality、affinity をコストモデルへ加える。
PG は固定スレッド集合ではなく論理的な PE budget とし、空いている worker
を動的に借りる。ただしコンパイラが判断した階層境界と最大有効並列度は守る。

## 8. 弱い解析の将来候補

完全な型推論や完全な points-to 解析を目標にしない。次を組み合わせた有限で
保守的な解析を候補とする。

- 明示された型注釈と戻り値契約に基づく弱い型伝搬。
- restrict hint の packed から array/dict への将来的な拡張。
- 定数伝搬と限定的な symbolic propagation。
- Param、AllocationSite、Global、FiniteSet、Unknown 程度の points-to domain。
- 再帰呼び出しを invocation graph の SCC としてまとめ、初期状態では直列化。

array/dict は最初から同時に解決しようとせず、当面 `World` effect とする。
解析を追加した場合も、解決できた部分だけ依存を細分化し、未解決部分は
ブラックボックスのまま残す。

## 9. 段階的な研究案

実装を再検討する場合は、次の順序が依存関係上自然である。ただし、これは
承認済みロードマップではない。

1. BB/LOOP/RB/SB/VSB と effect summary を表す解析 IR。
2. `World` 依存を含む保守的な階層 MFG/MTG の可視化と検証。
3. packed restrict LOOP に限定した DOALL と永続 thread pool。
4. 整数加算など、順序と overflow semantics を固定できる DOSUM。
5. direct SB と LOOP/RB/SB 内部の階層グラフ化。
6. 粗い静的コスト式と実行時 PE budget。
7. インタプリタの target/trip-count/time profile。
8. VSB の入口ガード付き de-blackboxing。
9. array/dict の restrict/effect/points-to 解析。
10. critical path と実測値を利用する適応 scheduler。

## 10. 主な未解決点

- DOSUM の決定性。整数演算から始め、浮動小数点は再結合を許す明示的な
  モードなしには並列化しない。
- scheduler overhead を上回る minimum grain の決定。
- nested task の PE budget、優先順位逆転、starvation、cache locality。
- mutable function binding に対する version guard と invalidation。
- VSB の複数候補をどこまで multi-versioning するか。
- I/O/World による過剰な直列化と、将来の effect 精密化。
- profile 保存形式、JIT code cache、および再コンパイル契機。
- 実測 cost とコンパイル時 cost の乖離、および測定ノイズへの耐性。

最初の設計原則は「未解決部分があってもグラフ全体を捨てない」である。
VSB、opaque SB、および `World` effect を直列 island として残し、その周囲で
証明できた並列性だけを安全に利用する。将来、静的解析またはガード付き
プロファイル特殊化によって island を小さくしていく。
