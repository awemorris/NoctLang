# dot member callとconditional `this`の実装計画

Status: implemented; updated 2026-08-11

Implementation note: direct `a.b(...)` preserves member association, evaluates
the receiver once and resolves the callee before arguments.  Receiver injection
is conditional on the callee's first parameter being named exactly `this`.
Parenthesized `(a.b)(...)` remains an ordinary call and the old `->` token has
been removed.  The interpreter, C backend and every JIT decoder share the new
temporary-callee bytecode layout.

この文書は、`obj.foo(...)`をmember callとしてloweringし、calleeの第一引数名が
exactly `this`の場合だけreceiver `obj`を暗黙の第一引数へ積む仕様を定義する。
旧`obj->foo(...)`演算子は削除する。

## 1. 固定した言語仕様

1. `a.b(args...)`はmember callである。member `b`を取得し、functionなら呼ぶ。
2. calleeの第一引数名がASCIIでexactly `this`の場合だけ、receiver `a`を暗黙の
   第一引数にする。
3. 第一引数名が`this`でない、引数が0個、またはparam metadataがないcalleeには
   receiverを追加しない。
4. 通常の`f(args...)`は、第一引数名が`this`でもreceiverを追加しない。
5. `(a.b)(args...)`も通常callでありreceiverを追加しない。parenthesesはmember-call
   associationを意図的に切るescape hatchとする。
6. `var f = a.b; f(args...)`、`a["b"](args...)`も通常callである。
7. `a->b(args...)`はsyntax errorとする。互換modeは設けない。
8. methodという宣言kindや新しいfunction typeは追加しない。`this`というparameter
   nameがmethod opt-in metadataである。

例:

```noct
var obj = {
    method: (this, x) => { return this.base + x; },
    plain:  (x) => { return x + 1; }
};

obj.method(3);       // method(obj, 3)
obj.plain(3);        // plain(3)
(obj.method)(obj, 3); // ordinary call; explicit receiver
```

## 2. 現状

- parserは`a.b`を`AST_EXPR_DOT`、その後の`()`を`AST_EXPR_CALL`にするため、
  `a.b()`は既に`CALL(DOT(a,b), args)`というshapeになる。
- `(a.b)()`は`CALL(PAR(DOT(a,b)), args)`となり、direct shapeと区別できる。
- `a->b()`だけが専用`AST_EXPR_THISCALL`を作る。
- 現行`OP_THISCALL`は`receiver + member name + args`を持ち、runtime helper内で
  memberを検索してreceiverを常に追加する。
- `rt_func`とserialized bytecodeはparameter nameを既に保持している。

現行`parser.y`をBison 3の`-Wall -Wcounterexamples`で監査した時点では
shift/reduce、reduce/reduce conflictは0件である。今回も新しいcall grammar productionを
追加しないため、`a.b()`と`(a.b)()`のためのLR conflictは発生させない。

## 3. parser/AST方針

### 3.1 新productionを追加しない

次のような重複productionは追加しない。

```text
expr : expr DOT SYMBOL LPAR ... RPAR
```

既存の`expr DOT SYMBOL`と`expr LPAR ... RPAR`でparseし、HIR変換時にAST shapeを
見る。これによりprecedenceを増やさず、parenthesesの有無もASTに残る。

### 3.2 HIR正規化

`hir_visit_call_expr()`の入口でcallee ASTを検査する。

- calleeがdirect `AST_EXPR_DOT`なら`HIR_EXPR_THISCALL`を構築する。
- receiverはdotの`obj`、member nameはdotの`symbol`、explicit argsはcallのargsを使う。
- calleeが`AST_EXPR_PAR`なら内側がDOTでもunwrapしない。通常`HIR_EXPR_CALL`にする。
- nested caseはouter shapeだけを見る。

例:

| Source | HIR |
|---|---|
| `a.b()` | `THISCALL(a, "b", [])` |
| `(a.b)()` | `CALL(PAR(DOT(a,"b")), [])` |
| `a.b()()` | `CALL(THISCALL(a,"b",[]), [])` |
| `a.b.c()` | `THISCALL(DOT(a,"b"), "c", [])` |
| `a.b().c()` | `THISCALL(THISCALL(a,"b",[]), "c", [])` |

### 3.3 arrow削除

- lexerから`"->"`と`TOKEN_ARROW`を削除する。
- parserからprecedence、`thiscall_expr` nonterminal、prototypeを削除する。
- `AST_EXPR_THISCALL`とそのunion member、constructor、copy/free caseを削除する。
- `HIR_EXPR_THISCALL`はdot-call正規形として残す。
- checked-in `lexer.yy.c`、`parser.tab.c`、`parser.tab.h`を正規generatorで再生成する。
- source、tests、apps、README、docs内の実コード例を`.`へ移行する。説明文中の一般的な
  矢印記号や`=>` lambdaは変更しない。

## 4. 評価順と一回評価

現行`a.b(args)`の通常callは次の順である。

1. `a`を評価する。
2. そのobjectから`b`を取得してcallee valueを固定する。
3. explicit argsを左から右へ評価する。
4. 固定済みcalleeを呼ぶ。

新仕様でもこの順序を変えてはならず、receiver expressionも一度だけ評価する。
現行`OP_THISCALL`のままhelper内でmember lookupすると、step 2がargs評価後になり、
次のprogramで呼ぶfunctionが変わる。

```noct
obj.f(change_obj_f());
```

従ってLIRはreceiverとcalleeの両方をtemporaryへ保存してからargsを評価する。
calleeとreceiverはframe temporaryなので、args評価中にGCが起きてもrootとして
追跡される。

## 5. `OP_THISCALL`の新layout

bytecodeは内部仕様であるため、旧layoutとの互換decoderは作らない。

旧:

```text
THISCALL dst:u16, obj:u16, name:string, argc:u8, argv:u16...
```

新:

```text
THISCALL dst:u16, obj:u16, callee:u16, argc:u8, argv:u16...
```

LIR loweringは次の順にする。

1. receiver tmpをallocateし、receiver expressionを評価する。
2. callee tmpをallocateし、`LOADDOT callee, receiver, member-name`をemitする。
3. explicit argument tmpをsource orderで評価する。
4. 新layoutの`THISCALL`をemitする。
5. args、callee、receiverの逆順でtemporaryを解放する。

`LOADDOT`を再利用することで、dictionary validation、missing member、hash lookupの
error semanticsを通常のdot accessと一致させる。

## 6. runtime semantics

interpreter/JIT helperは、name lookupをせず、`callee` tmpのfunction valueを使う。

```text
inject_this = callee.param_count > 0
              && callee.param_name[0] != NULL
              && strcmp(callee.param_name[0], "this") == 0
call_argc = explicit_argc + (inject_this ? 1 : 0)
```

- `inject_this`なら`arg_val[0] = receiver`、explicit argsはindex 1からcopyする。
- そうでなければexplicit argsをindex 0からcopyする。
- final count checkは既存`rt_call()`に任せ、通常callと同じargument mismatch診断にする。
- calleeがfunctionでなければ通常callと同じ`Not a function.` errorにする。
- receiverはcallee lookup時にはdictionaryでなければならないが、callee呼び出し時に
  改めてdictionary typeを要求しない。lookup後の値をreceiverとしてそのまま渡す。
- `param_name[0]`の比較はtype annotation、visibility、lambda/functionの別に依存しない。
  Noct function、lambda、registered C functionを同じ規則にする。

parameter nameがruntime dispatch semanticsを持つため、bytecode writer、loader、
`.nap` linkerは第一parameter名をstripまたはrenameしてはならない。将来parameter名を
削除したい場合は、bytecodeに明示的な`accepts_this` flagを追加する別設計が必要である。

## 7. argument上限

receiverを追加するかはruntimeまで未確定なので、member callのexplicit arg数は
`NOCT_ARG_MAX - 1`以下に制限する。これで`this` injection後も`arg_val`を超えない。
通常callは従来どおり`NOCT_ARG_MAX`まで許可できる。

AST/HIR/LIRの境界検査は`<`と`<=`を混在させず、次をtestする。

- member call explicit `NOCT_ARG_MAX - 1`: parse/lower可能。
- 同数かつ`this` callee: receiver込みでちょうど上限。
- member call explicit `NOCT_ARG_MAX`: compile error。
- normal call `NOCT_ARG_MAX`: 従来どおり可能。

## 8. interpreter/JIT/backend変更

新layoutを読む全箇所を同じphaseで変更する。

- `src/core/bytecode.h`: comment/layout contract。
- `src/core/lir.c`: emitter、operand validation、dump/reader。
- `src/core/interpreter.c`: decoder。
- `src/core/execution.c`とheader: helper signature/conditional injection。
- `src/core/jit-x86.c`、`jit-x86_64.c`、ARM32/64、PPC32/64、MIPS32/64、
  RISC-V32/64: decoderとhelper-call argument setup。
- `src/backend/cback.c`、`elback.c`: HIR/LIR consumerを監査し、該当表現があれば更新。
- `docs/vmspec.md`: new operand layoutとconditional receiver rule。

この変更の前に[13-jit-build-dependencies.md](13-jit-build-dependencies.md)を完了し、
各arch sourceの更新が確実にobjectへ反映される状態にする。

## 9. static inlineとの関係

[11-static-inline-static-scope-void.md](11-static-inline-static-scope-void.md)の初回
inlinerは、従来どおり`HIR_EXPR_THISCALL`をinline対象にしない。calleeはdictionary
内容によりruntimeで決まるため、parameter名だけを根拠にdevirtualizeしてはならない。

direct static symbol callだけをinlineし、member callのdevirtualizationはpoints-to解析を
導入するfuture workへ送る。inline済みcallee body内にmember callがある場合は、
`HIR_EXPR_THISCALL` subtreeを通常どおりcloneする。

## 10. `.nap`との関係

[12-noct-app-nap.md](12-noct-app-nap.md)より先にこの変更を完了する。`.nap`は永続
bytecodeを生成するため、`OP_THISCALL` layoutを確定してからformat goldenを作る。

複数file linkでもparameter nameを保存し、file間で呼ばれたfunction/lambdaに同じ
conditional-this規則を適用する。aggregate initializer callはdirect CALLであり影響しない。

## 11. 実装フェーズ

### Phase 0: build dependency gate

- design 13を実装し、全arch JIT sourceが直接build inputになるようにする。
- x86-32/QEMUでJIT compiledをassertする。

### Phase 1: semantic baseline

- 現行`a.b()`のcallee-before-args、left-to-right args、receiver一回評価をtest化する。
- `a->b()` method、`a.b()` plain member call、`(a.b)()` ordinary callを別fixtureにする。
- parser generator conflict countを記録する。

### Phase 2: HIR dot-call normalization

- direct `CALL(DOT)`を`HIR_EXPR_THISCALL`へ変換する。
- parentheses、subscript、function value経由を通常CALLのままにする。
- HIR dump testでshapeをassertする。

このphaseではarrowをまだ残し、old/new syntaxを同じHIRへloweringできる状態で比較してよい。

### Phase 3: bytecode/runtime変更

- callee-before-argsのLIRをemitする。
- `OP_THISCALL` layoutとhelper signatureを変更する。
- interpreterと全JIT backendを同時更新する。
- conditional `this`とarg上限を実装する。

### Phase 4: arrow migration/removal

- repository内の実コードを`.` syntaxへ移行する。
- lexer/parser/ASTからarrow専用surface構造を削除する。
- generated lexer/parserを再生成する。
- `a->b()` negative syntax testを追加する。

### Phase 5: full regression

- syntax、class、webapp、thread、REmacsをinterpreter/JITのO0/O2で通す。
- x86_64 nativeとx86-32 QEMUを必須にする。
- 他archはbuildし、qemu-user利用可能targetでmember-call smokeを行う。
- `.nb` roundtripでnew THISCALL layoutとparameter name保持を確認する。

## 12. test matrix

### Dispatch

- `(this)` lambda fieldへreceiverが入る。
- named `func(this, x)`をfieldへ格納した場合もreceiverが入る。
- first paramが`self`、`obj`、`This`なら入らない。
- zero-param functionへ入らない。
- registered C functionもfirst param nameで同じ判断をする。

### Syntax shape

- `a.b()`はmember call。
- `(a.b)()`、`f = a.b; f()`、`a["b"]()`はordinary call。
- chained dot/callを正しくassociateする。
- `->`はsyntax error、`=>` lambdaは引き続きparseできる。

### Evaluation

- receiver expressionは一回だけ評価される。
- memberはargsより前に取得される。
- argsは左から右へ一回だけ評価される。
- argsがmemberを書き換えても取得済みcalleeを呼ぶ。
- args評価中のGC後もreceiver/calleeが有効である。

### Errors

- receiverがdictでない、memberなし、memberがfunctionでない。
- conditional injection後のargument count mismatch。
- member callのexplicit arg上限超過。

### Execution modes

- interpreter/JIT、O0/O2で同一結果。
- bytecode compile/load roundtrip。
- x86-32では`NOCT_JIT_DEBUG=1`でfallbackでないことをassertする。

## 13. 完了条件

- repositoryのNoct sourceに実行用`->` callが残っていない。
- direct `a.b()`とparenthesized `(a.b)()`の意味がtestで固定されている。
- first param名`this`だけがreceiver injectionを有効にする。
- callee-before-args、一回評価、GC safetyを保持する。
- `OP_THISCALL` new layoutをinterpreterと全JIT backendが処理する。
- Bison conflictが増えていない。
- static inlineと`.nap` planの依存順が更新されている。
- pushは行わない。
