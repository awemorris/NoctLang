# Noct App (`.nap`) 複数ファイルbytecodeの実装計画

Status: implemented; updated 2026-08-11

Implementation note: the backend collects per-source LIR, checks duplicate
public functions and top-level bindings, renames file initializers, builds the
aggregate initializer through AST/HIR/LIR, and atomically publishes an
owner-executable shebang-prefixed bundle.  `tests/run-app.sh` covers init order,
static isolation, interpreter/JIT loading, duplicate inputs/symbols, relative
path rejection, and preservation of an existing output on compile failure.

この文書は、複数の`.noct` source fileを一つの実行可能なNoct bytecode
applicationへまとめる`.nap` compiler modeを定義する。実装者は、既存`.nb`
format、単一source compile、source loaderの意味を壊さず、この文書の順序と
invariantに従うこと。

依存する設計:

- [11-static-inline-static-scope-void.md](11-static-inline-static-scope-void.md)
  のfile declaration table、static symbol mangling、logical relative path、
  void、static inline module pass。
- [13-jit-build-dependencies.md](13-jit-build-dependencies.md)の増分build修正と
  [14-dot-member-thiscall.md](14-dot-member-thiscall.md)の確定済み
  `OP_THISCALL` layout。`.nap` format goldenを旧JIT objectや旧layoutで作らない。
- 現行bytecode仕様は[../vmspec.md](../vmspec.md)を正本とする。

## 1. 固定した仕様

1. CLIは次の形に固定する。

   ```text
   noct --compile --app <output.nap> <input1.noct> [input2.noct ...]
   ```

2. compiler optionは`--compile`と`--app`の二段構成であり、独立した
   `--compile-app` commandは追加しない。
3. 一つ以上の`.noct` inputを、一つの`.nap` outputへcompile/linkする。
4. outputの先頭は、LFで終わる次の一行とする。

   ```text
   #!/usr/bin/noct
   ```

5. shebangの直後は既存の`Noct Bytecode 1.0`形式そのものとする。
   bytecode versionを上げず、module sectionや新しいfunction metadataを
   追加しない。
6. bytecode headerと`Number Of Functions`は一度だけ出力し、全inputの
   function blockを同じfunction listへ並べる。`.nb`を単純連結しない。
7. 各input fileのtop-level declarationは、そのfile専用initializerへ
   まとめる。
8. `.nap`全体を統括するinitializerを一つ生成し、各file initializerを
   command lineのinput順に明示的にcallする。
9. loaderは全functionを登録した後、aggregate initializerだけを一度呼ぶ。
   file initializerをloaderから直接自動実行しない。
10. initializer終了後の通常のCLI動作は既存どおりpublic `main()`を呼ぶ。
11. input/outputのmodule identityには、command line/APIへ指定された
    logical relative pathだけを使う。absolute path、`realpath()`、home
    directoryをmanglingやbytecodeへ追加しない。
12. public symbolが複数inputで重複した場合は`.nap` link errorとする。
    source load順による上書きは`.nap` modeでは許可しない。
13. `.nb` compilerとloaderは既存互換を維持する。

## 2. CLI

### 2.1 正規形

```sh
noct --compile --app app.nap main.noct image.noct
noct --compile --app --optimize-level=2 app.nap main.noct image.noct
noct --compile --optimize-level=2 --simd-info --app app.nap main.noct image.noct
```

`--app`、`--optimize-level=N`、`--simd-info`は、最初のoperandより前なら
順不同で一度ずつ指定できる。documentation上の推奨順は
`--compile --app [compiler-options] output inputs...`とする。

既存modeは維持する。

```sh
noct --compile [compiler-options] one.noct two.noct
```

これは従来どおり`one.nb`と`two.nb`を別々に生成する。

### 2.2 argument検査

app modeでは、option parse後のoperandを次のように解釈する。

```text
operand 0: output `.nap`
operand 1..N: input `.noct`
```

次をcommand errorとする。

- outputがない。
- inputが一つもない。
- `--app`が複数回指定された。
- output suffixが`.nap`でない。
- input suffixが`.noct`でない。
- outputまたはいずれかのinput logical pathがabsolute。
- 同じnormalized logical input pathが二度指定された。
- output logical pathとinput logical pathが同じ。
- unknown compiler optionがある。

pathの検査・normalizationはdesign 11 §5を共通helperとして再利用する。
filesystemでcanonicalizeしない。`\`を`/`へ変換する字句的normalizationは
よいが、current directoryやhome directoryを前置しない。

POSIX absolute path (`/x`)、Windows drive path (`C:/x`, `C:\x`)および
UNC path (`//server/x`, `\\server\x`)を、host OSにかかわらず検出する。
cross compile時にもWindows absolute pathを見逃してはならない。

### 2.3 mainの検査

`.nap`はapplicationなので、全inputを通じてpublic `main`をちょうど一つ
要求する。`static func main`はentry pointではない。

- public `main`がない: link error。
- public `main`が複数: duplicate public symbol error。
- parameter数は0または1だけを許可する。既存CLIは0なら引数なし、1なら
  command line argument arrayを渡すためである。
- return annotationは指定なし、scalar、voidのいずれでもよい。CLIは戻り値を
  現状どおりprocess exit codeへ変換しない。

## 3. `.nap` file layout

出力例:

```text
#!/usr/bin/noct
Noct Bytecode 1.0
Source
apps/editor.nap
Number Of Functions
7
Begin Function
Name
main
...
End Function
Begin Function
Name
helper
...
End Function
Begin Function
Name
__noct_nap_file_init_<hex path 1>
...
End Function
Begin Function
Name
__noct_nap_file_init_<hex path 2>
...
End Function
Begin Function
Name
$init.__noct_nap_<hex output path>
...
End Function
```

shebangはbytecode payloadの一部ではなく、transport prefixとして扱う。
shebangを読み飛ばした後の最初のbyteは`N` (`Noct Bytecode 1.0`)である。
shebangとheaderの間に空行、BOM、追加metadataを入れない。

`Source`にはcommand lineで指定されたnormalized relative output pathを入れる。
既存formatはfile-level `Source`を一つしか持たないため、runtime errorのfile名は
全functionについて`.nap` pathになる。元`.noct`ごとのline numberはbytecodeに
残るが、元file名はruntimeでは失われる。この制約は初回仕様として受け入れる。

compile-time diagnosticは、各inputをcompileしている間は元のrelative `.noct`
pathとlineを表示する。

## 4. initializerの名前と実行意味

### 4.1 file initializer

各sourceにtop-level `var`、`let`、`class`、`static var`、`static let`が一つでも
あればfile initializerを生成する。何もなければ生成せず、aggregate
initializerのcall listにも加えない。

app modeのfile initializer名はloaderの自動実行prefix `$init.`を使わない。

```text
__noct_nap_file_init_<hex(normalized relative input path UTF-8 bytes)>
```

この名前はreserved internal namespaceとし、source-level top declarationで
`__noct_nap_` prefixを使った場合はcompile errorにする。hex encodingはhashを
使わずcollision-freeにする。

file initializer bodyのstatement順は、そのsourceのtop-level declaration順を
そのまま保持する。public/staticの区別で並べ替えない。

### 4.2 aggregate initializer

aggregate initializer名は、現行loaderが一つだけ自動実行できるように
`$init.`で始める。

```text
$init.__noct_nap_<hex(normalized relative output path UTF-8 bytes)>
```

app modeで`$init.` prefixを持つfunctionは、このaggregate initializer一つだけ
でなければならない。file initializerは必ず§4.1の名前へ変更する。

bodyは、initializerを持つinputだけをcommand line順にcallする。

```noct
/* conceptual source; actual name is internal */
func aggregate_initializer(): void {
    __noct_nap_file_init_<input1>();
    __noct_nap_file_init_<input2>();
    __noct_nap_file_init_<input3>();
    return;
}
```

各callの戻り値は捨てる。callがruntime errorを返した場合、通常のOP_CALL
semanticsに従ってaggregate initializerも失敗し、後続initializerと`main`を
実行しない。既に完了したinitializerのside effectはrollbackしない。

### 4.3 登録と実行順

runtime loaderの順序は現状を利用する。

```text
parse bytecode header
  -> register every function record
  -> remember the only `$init.` aggregate function
  -> call aggregate initializer
  -> return from noct_register_bytecode
  -> CLI validates and calls main
```

全functionを先に登録するため、早いinputのinitializerから、後ろのinputで
定義されたpublic functionを呼べる。これは`.nap`固有のlink semanticsとする。

## 5. public symbolとmodule semantics

design 11のfile declaration tableを、app linkerが全input分集約する。
同一namespaceとして最低限、次をduplicate検査する。

- public named function。
- top-level public var。
- top-level public let。
- top-level class（public letと同じbinding）。

functionとvariableのkindが異なっても同名ならduplicateとする。diagnosticには
両方のrelative pathとsource lineを表示する。

```text
Duplicate public symbol "foo":
  first declared at a.noct:3
  redeclared at b.noct:8
```

static symbolはsource nameが同じでも、input relative pathを含むlink nameが
異なるため衝突しない。各fileのstatic name resolutionは、他fileのHIRを混ぜる
前にfile単位で完了させる。static inlineもfile単位module passであり、別fileの
calleeを見てはならない。

`.nap` link時にpublic symbolの上書きを許可しないことで、「全function登録後に
全initializer実行」というsemanticsでも、後のfileによる予期しないcallee
置換を防ぐ。

## 6. compiler/backend architecture

### 6.1 既存backendを壊さない

`noct_bcback_start()` / `translate()` / `finalize()`の`.nb` behaviorを維持する。
app mode用APIは別に追加するか、内部modeを明示する。推奨interface:

```c
bool noct_bcback_app_start(const char *output_logical_path);
bool noct_bcback_app_add_source(const char *input_logical_path,
                               const char *source_data);
bool noct_bcback_app_finalize(void);
void noct_bcback_app_abort(void);
```

public API名を最終的に変える場合も、次のstate machineを守る。

```text
IDLE
  -> app_start -> COLLECTING
  -> zero or more successful add_source
  -> finalize -> IDLE

COLLECTING
  -> any error -> abort -> IDLE
```

`finalize`はinputが0なら失敗する。error pathは必ずAST/HIR/LIR、source buffer、
aggregate arrays、output handleを解放し、backend global stateをIDLEへ戻す。

### 6.2 sourceごとのcompile

`app_add_source()`は各sourceについて次を行う。

```text
validate logical relative path
  -> ast_build(path, data)
  -> collect/export declaration summary
  -> hir_build()
  -> file-static resolution and design 11 module inline pass
  -> for each HIR function:
       existing per-function optimizer
       lir_build()
       transfer owned lir_func to app aggregate
  -> rename/capture file initializer for app mode
  -> hir_cleanup()
  -> ast_cleanup()
```

全inputで同じoptimize levelと`simd_info`を使う。あるsourceでcompile errorが
起きたら、後続sourceを処理せずapp全体を失敗させる。

### 6.3 function recordの集約

既存`bcback.c`のfunction record出力を、次のprivate helperへ分離する。

```c
static bool bcback_write_header(FILE *fp,
                                const char *source,
                                uint32_t function_count);
static bool bcback_write_function(FILE *fp,
                                  const struct lir_func *func);
```

`.nb` pathもこのhelperを使い、format差分が生じないようにする。app finalizerは、
shebangを書いた後にheaderを一回書き、収集した全LIRとaggregate initializerを
順に`bcback_write_function()`へ渡す。

`Number Of Functions`は次を合計した値である。

```text
all normal/static/source functions
+ all file initializer functions that exist
+ exactly one aggregate initializer
```

合計時は`uint32_t` overflowを検査する。

### 6.4 LIR ownership

app modeは全sourceのcompile成功後に一度だけoutputを書く。従って、各
`struct lir_func`とそのstring/bytecodeをfinalizeまで所有するdynamic arrayが
必要になる。

```c
struct bcback_app_func {
    struct lir_func *lir;
    struct bcback_app_func *next;
};

struct bcback_app_init {
    char *link_name;
    struct bcback_app_init *next;
};
```

linked listまたはoverflow-checked dynamic arrayのどちらでもよい。C89制約を
守り、VLAやC99 declarationを使わない。

現行`lir_cleanup()`はownershipを再監査すること。`lir_build()`は
`func_name`、`param_name[]`、`bytecode`、`file_name`を複製しているため、
cleanupはそれらと`struct lir_func`自身をちょうど一度解放しなければならない。
app aggregateから参照中のLIRをper-source cleanupで解放してはならない。

既存callerが`lir_cleanup()`後のpointerを使っていないことをgrepで確認し、
不足している`file_name`/struct解放を修正する場合は単独testを通す。double-free
を避けるため、ownership修正とapp collectionを同じ曖昧な状態で進めない。

## 7. aggregate initializerの生成

bytecode opcodeを手作業で組み立てず、HIR/LIRの通常経路を使う。実装方法は
次のいずれかとし、推奨はAである。

### A. internal AST/HIR builder（推奨）

- compiler内部だけが呼べるhelperで、0 parameterのfunction ASTを作る。
- 各file initializer名をcallee symbolとするexpression statementを順に作る。
- 最後にbare returnを置く。
- design 11のvoidが実装済みならreturn typeをvoidにする。
- functionのsource nameは診断用internal名、link nameは§4.2の`$init.`名とする。
- 通常の`hir_build`/optimizer/`lir_build`を通す。

### B. synthetic source text

reserved ASCII-safe file initializer名だけからsourceを生成してparserへ渡す。
buffer sizeをoverflow-checkして動的確保する。文字列連結のescapingを誤る可能性
があるためAより優先度を下げる。

手作業のLIR/bytecode生成は、opcode layout、temporary frame、line info、CALL
error propagationを複製するため採用しない。

aggregate initializerはfunction listの最後に出力する。現在のloaderは最後に
見つけた`$init.`名を保持するが、この順序だけに安全性を依存せず、app出力に
`$init.` functionが一つしかないことをfinalize時にassert/検査する。

## 8. shebang-aware loading

現在の`src/cli/cli-run.c`はfile先頭を`NOCT_BYTECODE_HEADER`と比較し、それ以外を
sourceとして扱う。このままでは`.nap`をsource parserへ渡して失敗する。

共有判定を次の意味で追加する。

```c
#define NOCT_APP_SHEBANG "#!/usr/bin/noct\n"

bool rt_get_bytecode_payload(const uint8_t *data,
                             size_t size,
                             size_t *offset);
```

意味:

- offset 0に`Noct Bytecode`がある: bytecode、offset=0。
- exact shebangの直後に`Noct Bytecode`がある: bytecode、offset=shebang length。
- その他: bytecodeではない。

CLIはこの判定でsource/bytecodeを分ける。`noct_register_bytecode()`もfull `.nap`
bufferを直接受け取れるよう、core loader側で同じexact shebangを認識する。
CLIだけでpointerを進める実装にしてpublic APIを不整合にしてはならない。

次を拒否する。

- `#!/usr/bin/noct`の後にLFがない。
- CRLF shebang。
- shebangとheaderの間に空行がある。
- 別のinterpreterを指定した`#!`。
- shebang後のheaderが不正。

POSIX kernelのshebang規則に合わせ、compilerは常にexact LFを出力する。
通常`.noct` sourceの先頭`#!`対応は今回追加しない。

`rt_register_bytecode()`では、payload offsetを初期`pos`として既存parserを
呼ぶ。raw bytecode中にNULやnewlineがあっても、先頭offset以外をscanして
shebangを探してはならない。

## 9. executable permission

app outputを完全に書き、`fclose()`が成功した後、POSIXでは既存modeに
owner execute bitを追加する。

```c
stat(output, &st);
chmod(output, st.st_mode | S_IXUSR);
```

- `<sys/stat.h>`はPOSIX guard内だけでincludeする。
- group/other execute bitを勝手に追加しない。
- Windowsではpermission変更を行わないが、shebangはformatの一部として出す。
- `stat`/`chmod`失敗はcompile failureとし、不完全な新規outputを削除する。
- 既存outputを上書きする場合の失敗で、以前の正常なfileを失わないようにする。

最後の点を満たすため、推奨手順は「全LIRをmemoryへ収集してcompileを完了し、
同じdirectoryのtemporary outputへwrite、close、chmod、最後にatomic rename」で
ある。temporary名は固定一個にせず、衝突しない方法で作る。Windowsのreplace
semanticsは専用helperで扱い、shell commandへ委譲しない。

初回実装でatomic replaceを分離する場合も、少なくともcompile/parse errorが
既存outputをtruncateしないことをcompletion条件とする。

## 10. relative pathと情報漏洩

`.nap`ではfile initializerが必ずinput identityを使うため、static宣言の有無に
かかわらず全input/output logical pathをrelativeに限定する。

禁止:

```text
/home/alice/project/main.noct
C:\Users\alice\project\main.noct
\\server\share\main.noct
```

許可例:

```text
main.noct
src/main.noct
../shared/math.noct
```

`..`を許可する場合もfilesystemで解決せず、指定されたrelative spellingの
一部として扱う。異なるspellingは異なるmodule identityになり得ることを
仕様とする。separator normalization以外のcanonicalizationを勝手に追加しない。

bytecode security testでは、output全体をbinary-safeに走査し、test環境のhome
path文字列とabsolute input pathが含まれないことを確認する。ただしsource code
自身のstring literalに同じ文字列が書かれているfixtureは使用しない。

`Source`行、file initializer名、static symbol名、aggregate initializer名の
すべてを監査する。一箇所だけrelativeにして他からabsolute pathが漏れる状態を
完了としない。

## 11. error handling

app compilerは最初のerrorで停止し、次を保証する。

- diagnosticは可能なら元input relative pathとlineを持つ。
- AST/HIR global stateをcleanupし、次のcompile requestが可能。
- 収集済みLIR、initializer名、declaration summary、source bufferを解放する。
- backend stateをIDLEへ戻す。
- 新規のpartial `.nap`を残さない。
- 既存の正常なoutputをcompile errorで破壊しない。

想定diagnostic:

```text
--app requires an output .nap file and at least one input .noct file.
Noct App paths must be relative: /home/alice/main.noct
Duplicate public symbol "foo": a.noct:3 and b.noct:8.
Noct App requires exactly one public main() function.
Noct App main() must take zero or one parameter.
Failed to make app.nap executable.
```

write error、close error、permission errorを無視して成功を返してはならない。
`fprintf`、`fwrite`、`fclose`のreturnをapp pathでは検査する。既存`.nb` writerの
error検査改善は可能なら共通helper化に含める。

## 12. 変更箇所ガイド

実装前に最新treeを再確認すること。現時点の主な変更箇所は次である。

| File | 必要な変更 |
| --- | --- |
| `src/cli/cli-compile.c` | `--app` parse、operand/path検査、複数source読込、app backend呼出し、permission/error表示 |
| `src/cli/cli-main.c` | usageへ`noct --compile --app <out.nap> <in-files>`追加。READMEの古いcompile例も合わせる |
| `include/noct/backend.h` | app backend lifecycle API追加。既存`.nb` APIを維持 |
| `src/backend/bcback.c` | compile-unit helper、LIR collection、重複link、aggregate initializer、single header/function count、shebang出力 |
| `src/core/ast.c`, `ast.h` | design 11のtop-level declaration保持、app internal initializer mode/builder |
| `src/core/hir.c`, `hir.h` | file symbol summary、internal aggregate function、static inline module passとの接続 |
| `src/core/lir.c`, `lir.h` | LIR ownership/cleanup監査。新opcodeは不要 |
| `src/core/runtime.c` | optional exact shebang offset、aggregate initの既存single-init実行確認 |
| `src/core/noct.c` | public `noct_register_bytecode()`へfull `.nap`を渡せることを保証 |
| `src/cli/cli-run.c` | shebang付きbytecode判定。sourceとしてparseしない |
| `docs/vmspec.md` | `.nap` transport prefix、single Source制約、aggregate init順序を追記 |
| `docs/napi.md` | `noct_register_bytecode()`が`.nb`と`.nap`を受けることを追記 |
| `README.md` | 正しい`.nb`例と`--compile --app`例、直接実行例 |
| `tests/app/` | compile/link/load/init/order/path/security/error tests |

JIT backend、interpreter opcode、C backendへの新opcode追加は不要である。通常の
CALLと既存function registrationを使う。opcodeを追加し始めた場合は設計から
逸脱している可能性が高いので立ち止まること。

## 13. 実装フェーズと依存関係

一phase一コミットを目安にし、各phase終了時に既存`.nb` testを通す。

### Phase A: golden baseline

- design 13とdesign 14を完了し、全arch JIT objectとbytecode decoderが同じ
  `OP_THISCALL` layoutを使っていることを確認する。
- 現在の`.nb` compile/load、top-level init、複数inputが別`.nb`になる挙動を
  golden testとして固定する。
- loaderが一つの`init_func_name[256]`を保持する現状をtestで可視化する。
- READMEの実際のCLIと一致しない古い例を記録する。

依存: design 13、design 14。

### Phase B: design 11のmodule identityとdeclaration summary

- relative logical path helperを実装・単体testする。
- top-level public/static declaration summaryをsource cleanup後もapp linkerが
  所有できる形で返す。
- initializerのsource/link nameを分離する。

依存: design 11 Phase 1--3。

### Phase C: bytecode writer refactor

- header writerとfunction writerをprivate helperへ分離する。
- `.nb`出力のbyte比較またはload/run結果がrefactor前と一致することを確認する。
- LIR cleanup/ownershipを修正し、memory sanitizerまたはvalgrindで単一compileを
  確認する。

依存: Phase A。

### Phase D: app collection/link

- app backend stateとlifecycle APIを追加する。
- 複数sourceをcompileし、LIRとdeclaration summaryを収集する。
- duplicate public symbol、main、function count overflowを検査する。
- file initializerをnon-auto internal名へする。

依存: Phase B、C。

### Phase E: aggregate initializer

- input順のfile initializer callを持つinternal AST/HIRを生成する。
- 通常のLIR CALL loweringを使う。
- aggregate initializerだけに`$init.` prefixを付け、function list最後へ追加する。
- 全function登録後に全initializerが一度ずつ順番に動くtestを通す。

依存: Phase D、design 11のvoidを利用できれば利用する。void未完でも内部unit
returnで先行実装可能だが、最終的にはvoidへ揃える。

### Phase F: `.nap` writerとCLI

- exact shebang、single header、total function countを出力する。
- `noct --compile --app` argument parseとusageを追加する。
- relative path、suffix、input countを検査する。
- compile failure時に既存outputを壊さないwrite strategyを実装する。

依存: Phase D、E。

### Phase G: loaderとexecutable

- CLI/coreでexact shebangを認識する。
- `.nap` bufferを`noct_register_bytecode()`へそのまま渡せるようにする。
- POSIX owner execute bitを安全に付ける。
- `noct app.nap`、可能な環境では`./app.nap`を検証する。

依存: Phase F。

### Phase H: documentationとfull regression

- README、vmspec、napi、design statusを更新する。
- native全suite、`.nb` roundtrip、`.nap` interpreter/JITを通す。
- MinGW/OpenWatcomでcompile guardとC89を確認する。

依存: Phase G。

## 14. test matrix

### 14.1 happy path

三つのsourceを用意する。

```text
a.noct: initializerがlogへAを追加
b.noct: initializerがlogへBを追加し、c.noctのfunctionを呼ぶ
c.noct: initializerがlogへCを追加、public mainがABCを検査
```

期待:

- 全functionがinitializerより先に登録される。
- initializer順がA、B、C。
- 各initializerは一度だけ。
- B initializerから後続Cのfunctionを呼べる。
- `main`はすべてのinitializer後に実行される。
- O0/O2、interpreter/JITで同じ結果。

### 14.2 file構成

- top-level declarationなしのfileはinit callを生成しない。
- 一fileだけの`.nap`も動く。
- static func/var/letの同名を別fileに置いても衝突しない。
- static inlineは同一fileだけ展開される。
- public cross-file callは通常CALLで動く。

### 14.3 error

- no output、no input、wrong suffix。
- absolute POSIX/Windows/UNC path。
- duplicate input path。
- duplicate public func、func対var、var対class。
- mainなし、複数main、2 parameter main。
- source compile errorが2番目のfileにある。
- initializer Bがruntime errorになりC/mainが走らない。
- output write/close/chmod failure。
- failure後に同processでもう一度app compileできる。

### 14.4 format/security

- byte 0からexact shebang。
- shebang直後からexact`Noct Bytecode 1.0`。
- bytecode headerは一つだけ。
- `Number Of Functions`が実function block数と一致。
- `$init.` functionがaggregate一つだけ。
- input順のfile init callが存在。
- output中にtest host home pathとabsolute pathがない。
- malformed shebang、CRLF、別interpreterをreject。
- existing `.nb`を引き続きload/runできる。

### 14.5 executable

- POSIXでowner execute bitが立つ。
- `/usr/bin/noct`が存在するintegration環境では`./app.nap args...`を実行する。
- 通常CIで`/usr/bin/noct`がない場合は、shebang文字列とmodeを検査し、build
  treeの`noct app.nap`で機能を検証する。system directoryをtestが変更しては
  ならない。
- Windowsでは生成と`noct app.nap`を検証し、POSIX permissionを要求しない。

### 14.6 portability/regression

- Linux native debug/release。
- x86_64 interpreter/JIT O0/O2。
- existing syntax/typing/typedop/ABCE/CSE/SIMD/ctrans suite。
- `.nb` return metadata/SIMD metadata roundtrip。
- MinGW build。
- OpenWatcom/MS-DOS buildではPOSIX APIがcompileされないこと。
- 必要に応じqemu-user targetで`.nap` bytecode loadとmain実行。architecture固有
  opcode変更はないため、全targetで同じbytecode loader semanticsを確認する。

## 15. 完了条件

次をすべて満たしたときだけ完了とする。

1. 指定CLIで複数`.noct`から一つの`.nap`を生成できる。
2. outputがexact shebangを持ち、POSIXでowner-executableになる。
3. shebang後は既存`Noct Bytecode 1.0`parserで読める単一function listである。
4. 全function登録後、file initializerがinput順に各一度、aggregate
   initializerから明示callされる。
5. initializer成功後にmainが実行される。
6. static symbolがfile間で衝突せず、duplicate public symbolがlink errorになる。
7. outputへcompiler home/absolute pathが追加されない。
8. `.nb` compile/load behaviorが回帰しない。
9. error pathでpartial output、stale compiler state、LIR ownership leakを残さない。
10. docsとtestsが実装に一致する。

## 16. 初回実装の対象外

- bytecode format 2.0、module section、per-function Source metadata。
- `.nap`内のpublic symbol override/weak symbol。
- archive、dynamic library、incremental linker。
- compression、signature、encryption。
- embedded source code/debug source map。
- public/cross-file inlineとVM HIR template。
- `.nap`から別`.nap`をlinkすること。
- shebang pathの設定option。初回はexact `#!/usr/bin/noct`のみ。
- initializer rollbackまたはtransactional global state。

将来per-function source名やmodule metadataが必要になった場合はbytecode 2.0を
別設計として追加する。初回`.nap`のsimple container semanticsへ互換性のない
optional fieldを継ぎ足さない。
