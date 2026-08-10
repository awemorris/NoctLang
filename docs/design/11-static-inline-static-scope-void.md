# static scope、static HIR inline、void 戻り値の実装計画

Status: implemented (conservative expression inliner); updated 2026-08-11

Implementation note: `static var`/`static let`/`static func` use collision-free
hex mangling of the specified relative source path.  `static inline` is an O2
HIR pass and currently expands a single value-return expression when each
parameter is used exactly once and every actual argument is side-effect-free;
more complex CFGs deliberately remain normal calls.  `void` return validation
is performed only at level 2, as specified.

Related plan: [12-noct-app-nap.md](12-noct-app-nap.md) uses this plan's
file-symbol table, logical relative path validation, initializer naming, and
safe mangling to link several source files into one executable `.nap` file.
Before implementing call handling, complete
[13-jit-build-dependencies.md](13-jit-build-dependencies.md), then stabilize
the dot member-call semantics in
[14-dot-member-thiscall.md](14-dot-member-thiscall.md).  The first inliner does
not devirtualize or expand `HIR_EXPR_THISCALL`.

この文書は、ファイルローカルな関数・変数、同一ファイル内だけで完結する
HIR inline 展開、および `void` 戻り値注釈の実装計画を定義する。初回実装では
VM に inline 用 HIR template/cache を保持せず、public またはファイル間の
inline 展開を行わない。

## 1. 固定した判断

1. inline 展開をサポートする構文は、初回実装では
   `static inline func` だけとする。
2. `inline func` と `inline static func` は受理しない。正規の修飾子順序は
   `static inline func` とする。
3. static inline の callee は、現在コンパイル中のファイルの宣言/HIR table
   から解決する。VM の登録済み関数表は参照しない。
4. VM の `rt_func` に HIR template、inline cache、function version guard を
   追加しない。
5. public function、別ファイルの関数、method/thiscall、lambda、関数値経由の
   indirect call は inline 展開しない。
6. top-level の public `var` と `let` は既に実装済みであり、互換性を保つ。
   新たに `static var` と `static let` を追加する。
7. static func/var/let の link name は、コンパイラに指定された論理的な
   **相対ソースパス**と宣言名だけから生成する。
8. mangler は `realpath()`、カレントディレクトリとの結合、ホーム
   ディレクトリ、絶対パスを使用しない。bytecode にコンパイル環境の
   home directory 名を混入させない。
9. `void` は戻り値注釈専用の compiler type とし、runtime の新しい
   `NoctValue` tag は追加しない。
10. 既存方針どおり、戻り値契約と void の値利用検査は optimize level 2
    で有効にする。

## 2. 対象構文

```noct
static func private_helper(): int {
    return 1;
}

static inline func make_pixel(r: int, g: int, b: int): int {
    return (r << 16) | (g << 8) | b;
}

static var file_counter = 0;
static let pixel_mask = 0x00ffffff;

func procedure(): void {
    file_counter++;
    return;
}
```

`static` は top-level 宣言にだけ使用できる。関数内の static local storage は
導入しない。`static class` は今回の対象外とし、必要なら将来
`static let Name = class {...};` に相当する仕様として別途検討する。

次は初回実装では compile error とする。

```noct
inline func public_inline() { }
inline static func wrong_order() { }
static var no_initializer;
static let no_initializer;
```

## 3. 現状と必要な変更

現在の `ast_build()` は top-level `var`/`let` を直ちに `$init.<file>` の
statement へloweringする。`let` は代入後に `Global.markConst("name")` を
呼ぶ。関数については `hir_build()` が同一ファイルの全関数を構築した後、
runtime が関数ごとに HIR optimization、LIR生成、VM登録を繰り返す。

static宣言の先行解決には、top-level宣言を即時loweringする前に宣言情報を
保持し、ファイル単位のsymbol tableを構築する必要がある。また、通常の
per-function optimizerを順番に動かすとinline calleeのHIRが先に変形される
可能性があるため、static inline展開はper-function optimizerより前の
module passとして一度だけ行う。

```text
parse AST
  -> collect file declarations and assign link names
  -> build all HIR functions
  -> resolve file-static symbols
  -> HIR module inline pass (static inline only)
  -> existing per-function passes
       typed -> ABCE -> SIMD -> CSE -> typed
  -> LIR generation and existing VM registration
  -> execute $init
```

VM登録順はinlineの可否に影響しない。module inline passはVMを参照しないため、
現在の「一関数ずつLIR生成・登録」という後段処理は初回実装では変更不要で
ある。

## 4. ファイルsymbol table

ファイルごとに、少なくとも次の情報を持つ。

```text
source_name
link_name
kind              (func / var / let)
is_static
is_inline
parameter signature
return signature
AST/HIR declaration pointer
source line
```

すべての宣言をbody構築前に登録するため、static func/var/letの名前は同一
ファイル全体で認識できる。値の初期化順は従来どおりソース順であり、名前が
解決できることは初期化済みであることを意味しない。

symbol参照の解決順序は次とする。

1. parameterまたはlexical local。
2. 同一ファイルのstatic symbol。
3. 従来のVM global symbol。

static symbolはcall positionだけでなく、すべてのsymbol expressionで
link nameへ置換する。従って関数値としての参照もファイルローカルになる。

```noct
static func helper(): int { return 1; }

func f(): int {
    let p = helper;
    return p();
}
```

この例の`helper`参照はmangled functionをロードするが、`p()`はindirect
callなのでinlineしない。

同一ファイル内で、kindまたはvisibilityにかかわらず同じsource nameを
二度宣言した場合はcompile errorとする。local/parameterによるshadowingは
既存のlexical scope規則に従って許可する。

## 5. 安全なmangling

### 5.1 source pathの信頼境界

`noct_register_source(env, file_name, source_text)`、bytecode/C backendなどへ
指定されたsource file nameのうち、static宣言のmodule identityに使う値を
logical source pathと呼ぶ。

- logical source pathは相対パスでなければならない。
- manglerは渡された文字列を絶対パスへ変換しない。
- filesystemのcanonicalization、`realpath()`、home directory参照をしない。
- path separatorは`/`へ字句的に統一できるが、filesystemへ問い合わせない。
- static宣言を含むsourceに絶対パスが指定された場合はcompile errorとする。
- CLIはユーザーが指定した相対パスをそのままcompilerへ渡す。

将来、物理ファイルを絶対パスで開きながら別のlogical pathを指定する必要が
生じた場合は、physical pathとsource identityをAPI上で分離する。初回実装で
absolute pathからbasenameを推測したり、home prefixを自動削除したりしない。
そのような推測はcollisionと再現性低下を招くためである。

### 5.2 link nameの形式

link nameはC backendでも安全なASCII identifierだけで構成する。推奨形式は
次である。

```text
__noct_static_<hex(relative-path UTF-8 bytes)>_<hex(source-name UTF-8 bytes)>
```

例:

```text
logical path: tests/simd/pixel.noct
source name:  make_pixel

link name:
__noct_static_74657374732f73696d642f706978656c2e6e6f6374_6d616b655f706978656c
```

hex encodingはhashと異なりcollisionを生まず、元の相対パスと宣言名だけを
可逆に表す。bytecodeにはproject内の指定相対パスは現れ得るが、compilerが
探索・補完したhome directoryや絶対パスは現れない。

HIR function/global metadataには診断用の`source_name`と、code generation用の
`link_name`を分けて保持する。error message、`--inline-info`、source line表示
にはsource_nameを使う。

## 6. static func/var/letのruntime表現

static functionも、inlineできないcall、O0、関数値参照に備えて、mangled
link nameで通常のruntime functionとして生成・登録する。初回実装では、
すべてのcallが展開された場合のdead function除去は行わない。

static var/letは既存の`$init`へ、mangled global nameを使うstatementとして
loweringする。

```text
__noct_static_<path>_<name> = initializer
Global.markConst("__noct_static_<path>_<name>")  // static letのみ
```

- static var: bindingを書き換え可能。
- static let: 初期代入後にbindingをconstant化。
- initializerは必須。
- 初期化順はpublic/staticを含むtop-level宣言のソース順。
- 別ファイルからsource nameでは参照できない。
- mangled名を知るnative APIからのアクセスを防ぐsecurity boundaryではない。
  staticはlanguage visibilityでありsandbox機構ではない。

## 7. void戻り値

`void`はreturn annotationでだけ有効なcompiler typeとする。parameter、local、
packed element typeには使用できない。

level 2では次を検査する。

- `return;`およびfunction末尾へのfallthroughを許可する。
- `return expr;`をcompile errorにする。
- 戻り値がvoidと確定したdirect callはexpression statementで使用できる。
- assignment、return operand、operator operand、argument valueなど、値が必要な
  contextでvoid callを使うとcompile errorにする。
- indirect callまたはsignature不明のcallは従来どおりUNKNOWNとする。

現在はbare `return;`をAST構築時に`return 1;`へ変換している。これをやめ、
bare return情報をAST/HIRへ保持する。runtime ABIと既存OP_CALLは有効な
`NoctValue` destinationを要求するため、void functionも内部unit valueを返して
よいが、その値を型伝播へ公開してはならない。新しいruntime tagは追加しない。

static inline void functionは、expression statementとして呼ばれた場合だけ
展開できる。result tempは作らず、calleeのreturn edgeをcallerのcontinuationへ
接続する。

## 8. static inlineの適格性

最初のinline passは、次をすべて満たすdirect callだけを展開する。

- calleeが同一module tableの`static inline func`。
- callee symbolがlocal/parameterにshadowされていない。
- parameter数が一致する。
- calleeが再帰call graphのSCCに含まれない。
- callee bodyが単一basic blockで、末尾に単一returnを持つ。
- break、continue、LOOP、while、if、lambdaを含まない。
- non-void returnの型契約をearly type passで証明できる。
- callerごとのinline node/statement budgetを超えない。

不適格な場合でもcompileを失敗させず、mangled functionへの通常CALLを残す。
`static inline`は「O2で強く要求する最適化指定」だが、再帰や未対応CFGで
program semanticsを変更してはならない。拒否理由は`--inline-info`で表示する。

O0/1ではinline展開しないが、static name resolutionとmanglingは行う。
従ってoptimization levelによって参照先が変化しない。

## 9. HIR変形

calleeのparameterとlocalはcall siteごとにalpha-renamingする。

```noct
static inline func make_pixel(r: int, g: int, b: int): int {
    return (r << 16) | (g << 8) | b;
}
```

概念的な展開結果:

```text
$inl1.r = arg0
$inl1.g = arg1
$inl1.b = arg2
$inl1.result = ($inl1.r << 16) | ($inl1.g << 8) | $inl1.b
```

argumentは元のOP_CALLと同じ順序で、一度だけ評価する。inline callを含む
expressionの一部だけを先にhoistすると、他のsubexpressionとの副作用順序を
変える可能性がある。初回実装では次のいずれかを採用する。

1. expression全体を既存評価順どおりtemporary列へlinearizeする。
2. 安全性を証明できるcall positionだけを展開し、その他を拒否する。

実装量が小さい2から開始してよいが、`make_pixel(local, local, local)`のような
pure term argumentを持つRHS/return callは必ず対象に含める。

calleeのreturn contractをinlineによって消してはならない。初回はreturn
operandの型が宣言型と静的に一致するcalleeだけを展開する。UNKNOWN returnに
対するruntime checkをinline bodyへ複製する一般化は後続作業とする。

## 10. pass interface

module passを追加する。

```c
bool hir_opt_inline_module(int level, bool inline_info);
```

または同等のmodule contextを明示引数で渡す。passは`hir_func_tbl`とfile symbol
tableのimmutableなcallee bodyを参照し、全callerを変形してから既存の
`hir_optimize_func()`を開始する。VM environmentや`rt_func`を参照しては
ならない。

callee自身が別のstatic inline functionを呼ぶ場合は、call graphを作って
非再帰部分をcallee-firstに処理するか、depth/budget付き再帰展開を行う。
自己再帰・相互再帰SCCは展開しない。

## 11. 実装フェーズ

### Phase 0: baselineと仕様テスト

- design 13を完了し、design 14の`OP_THISCALL` layoutとdot-call semanticsを
  baselineにする。stale JIT objectまたは旧arrow syntaxをfixtureへ持ち込まない。
- 現在のtop-level public var/let、return annotation、O0/O2を固定する。
- function/globalのsource orderと再定義挙動を記録する。
- bytecodeに絶対home pathを追加しないsecurity testのfixtureを用意する。

### Phase 1: lexer/parser/AST metadata

- `TOKEN_STATIC`と`TOKEN_INLINE`を追加する。
- `static func`、`static inline func`、`static var`、`static let`をparseする。
- public `inline func`とmodifier順序違反を明示的にrejectする。
- AST func/top-level declarationにvisibility、inline、source lineを保持する。
- bare returnを値1へ変換せず保持する。

### Phase 2: file symbol tableとmangling

- body構築前に全top-level宣言を収集する。
- duplicate、kind conflict、visibility conflictを診断する。
- logical relative source pathを検証する。
- ASCII-safeでcollision-freeなlink nameを生成する。
- symbol referenceをlocal -> static -> globalの順で解決する。

### Phase 3: static code generationと初期化

- static functionをmangled名でHIR/LIR/runtimeへ運ぶ。
- static var/letをmangled名で`$init`へloweringする。
- static letの`Global.markConst`へlink nameを渡す。
- bytecode、C backend、Emacs Lisp/backend固有の名前生成を監査する。
- O0/1/2で同じvisibilityと初期化semanticsを確認する。
- このphaseのfile declaration table、relative logical path、initializer
  link nameをdesign 12の`.nap` app linkerから再利用可能なinterfaceにする。

### Phase 4: void contract

- return signatureにcompiler-only VOIDを追加する。
- bare/value/fallthrough returnを区別して検査する。
- direct void callのvalue-context検査を追加する。
- LIR/runtime ABIでは既存の有効なNoctValue return slotを維持する。

### Phase 5: resolved static callee

- direct static callにfile symbol entryまたはcallee identityを付ける。
- first-class参照とindirect callはmangleのみ行い、inline候補にしない。
- static inline call graphとrecursive SCCを構築する。

### Phase 6: simple HIR inline

- module inline driverをper-function optimizerより前へ追加する。
- parameter/local/resultをalpha-renamingする。
- argument評価回数と順序を保持する。
- single-basic-block non-void/void calleeを展開する。
- member call (`HIR_EXPR_THISCALL`)はcallee identityがdynamicなので展開しない。
  inlineするcallee body内に存在するmember-call subtreeは意味を変えずcloneする。
- `--inline-info`にsource line、callee名、成功/拒否理由を出す。
- 展開後にtyped/ABCE/SIMD/CSEを実行する。

### Phase 7: 制御フローとコストの拡張

- multiple return、if、LOOPを持つcalleeをCFG clone + joinへ拡張する。
- node/statement/depth budgetを調整する。
- 全callが展開されたstatic functionのdead strippingを検討する。

## 12. テスト

最低限、次を追加する。

- 同じsource nameのstatic func/var/letを持つ二ファイルが衝突しない。
- 別ファイルからsource nameでstatic symbolを参照できない。
- local shadowingがstatic symbolより優先される。
- static varは変更でき、static letは初期化後に変更できない。
- public/staticを混在させてもtop-level初期化順が変わらない。
- 相対pathが異なる同名ファイルは異なるlink nameになる。
- generated bytecodeにcompiler hostのhome directory、drive letter、絶対pathが
  新たに混入しない。
- static宣言を含むsourceへ絶対logical pathを渡すと明示的に失敗する。
- O0はmangled CALL、O2はeligible callにCALLを残さない。
- argumentを一度だけ、左から右へ評価する。
- recursive、indirect、unsupported CFGは通常CALLへfallbackする。
- inline後にtyped ops、ABCE、SIMDが発火する。
- voidのbare return/fallthrough/value return/value contextを網羅する。
- source、bytecode、C backend、および全JIT targetの既存suiteを回帰確認する。

## 13. 初回実装の対象外

- public `inline func`。
- VMに保持するinline HIR template/cache。
- cross-file inline、LTO、bytecodeからのHIR復元。
- function identity/version guardとdeoptimization。
- method、thiscall、lambda、closureのinline。
- 自動inline、profile-guided inline。
- static local variable、static class。
- 完全なgeneral CFG inlineをPhase 6の完了条件にすること。

将来public/cross-file inlineを再検討するときは、この初回実装を変更せず、
VM inline templateとbinding guardを独立した設計として追加する。
