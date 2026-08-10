# JIT architecture sourceの増分ビルド依存修正計画

Status: implemented; updated 2026-08-11

Implementation note: architecture JIT sources are separate CMake translation
units instead of textual includes from `jit.c`, so normal compiler dependency
tracking rebuilds the changed backend.  Native x86_64 incremental/full builds
pass; the current host lacks its former `i686-linux-gnu-gcc`, so x86-32 was not
rebuilt in this session.

この文書は、アーキテクチャ別JIT sourceを更新しても古い`jit.o`が再利用される
問題を修正する。これはopcode実装漏れではない。現行sourceのx86-32 JITに、
今回問題となった未実装opcodeは存在しない。

## 1. 調査結果と訂正

- `src/core/jit.c`はpreprocessorで`jit-x86.c`などを直接includeしている。
- 問題が起きたbuildでは、includeされたアーキテクチャ別`.c`が`jit.o`の依存先に
  入っていなかった。
- そのためNoctを更新しても古いopcode tableを持つ`jit.o`が残り、新しいbytecodeを
  読むとdispatchの`default`へ入り`JIT_OP_NOT_IMPLEMENTED`になった。
- `jit.o`を強制再buildすると、同じG2A programはx86-32/QEMUで完走した。
- よって「x86-32に未実装命令を追加する」という作業は撤回する。opcodeや
  bytecode formatをこの問題のために変更してはならない。

強制再build後の`build-x86`ではcompiler生成の`.d` fileに`jit-x86.c`が現れるが、
今回の再現は、この暗黙のinclude scanner依存がbuild generator、既存dependency
database、またはbuild directoryの履歴によって保証されないことを示している。
修正はdependency scannerの偶然に依存しない構造にする。

## 2. 固定した実装方針

アーキテクチャ別JITを`jit.c`へtextual includeする構造を廃止し、CMake targetの
通常のtranslation unitとして列挙する。

```text
src/core/jit.c          common memory mapping / cache commit / no-JIT stub
src/core/jit-x86.c      x86 implementation
src/core/jit-x86_64.c   x86_64 implementation
src/core/jit-arm32.c    ARMv7 implementation
...
```

各arch fileには既に`NOCT_ARCH_* && NOCT_USE_JIT` guardがあるため、全arch fileを
`NOCT_BASE_SOURCE`へ列挙してよい。選択対象だけがsymbolを定義し、他は空の
translation unitになる。この方式ならarch source自身がbuild graphのinputであり、
`.c` includeをheader dependencyとして発見できるかに依存しない。

`OBJECT_DEPENDS`だけを追加する案は採用しない。generatorごとの扱いを増やし、
textual includeという原因を残すためである。

## 3. `jit.c`の分離

1. `jit.c`から10個の`#include "jit-ARCH.c"`を削除する。
2. recognized JIT archかを判定し、該当しないtargetだけ`NOCT_USE_JIT`をundefして
   stubを出す現行の意味は保持する。
3. recognized archでは`jit.c`は`jit_map_memory_region()`、
   `jit_unmap_memory_region()`、`jit_map_writable()`、`jit_map_executable()`など
   common symbolだけを定義する。
4. `jit_build()`、`jit_commit()`、`jit_free()`は選択されたarch translation unitが
   定義する。
5. `jit.h`を両translation unitのinterface境界とし、暗黙宣言やinclude順依存を
   許さない。不足するprototype/typeがあれば`jit.h`へ移す。
6. 各arch file末尾のguard commentも実際の`NOCT_ARCH_*`名へ直す。

## 4. CMake変更

`CMakeLists.txt`に`NOCT_JIT_ARCH_SOURCE`を作り、10 backendを明示列挙して
`noct` targetへ加える。

```cmake
set(NOCT_JIT_ARCH_SOURCE
  src/core/jit-x86.c
  src/core/jit-x86_64.c
  src/core/jit-arm32.c
  src/core/jit-arm64.c
  src/core/jit-mips32.c
  src/core/jit-mips64.c
  src/core/jit-ppc32.c
  src/core/jit-ppc64.c
  src/core/jit-riscv32.c
  src/core/jit-riscv64.c
)
```

このlistはJIT disabled buildにも入れてよい。guardにより空になる。targetやpreset
ごとにarch sourceを選択するCMake分岐は、`CMAKE_SYSTEM_PROCESSOR`の表記差と
toolchain preset差を新たなfailure sourceにするため導入しない。

## 5. 増分ビルド回帰検査

最低限、次を自動検査する。

1. fresh `build-x86`をconfigure/buildする。
2. `compile_commands.json`またはverbose build logで`jit-x86.c`が独立したcompile
   inputであることを確認する。
3. test用の一時source copyで`jit-x86.c`だけのmtimeを更新し、incremental buildで
   `jit-x86.c.o`と最終library/executableが更新されることを確認する。
4. `jit.c`だけを更新した場合はcommon objectと最終binaryが更新されることを確認する。
5. no-op rebuildではJIT objectが再compileされないことも確認する。

source treeのmtimeをtest後に戻す方式は使わない。testは`mktemp`配下のcopy、または
CI専用checkoutで行い、開発者のworking treeを変更しない。

## 6. functional acceptance

増分build後のprogram outputが正しいだけでは不十分である。JIT build failure時は
interpreter fallbackでも同じoutputになり得るため、`NOCT_JIT_DEBUG=1`を使って
対象functionに`compiled`が出たことをassertする。

- x86-32: QEMUで問題を再現したG2A programを`--force-jit`で実行し完走する。
- x86-32: typed-op、SIMD scalar/SSE2/SSE3/SSE4.1 testを実行する。
- x86_64: native full JIT regressionを実行する。
- ARM32/64、PPC32/64、MIPS32/64、RISC-V32/64: cross buildを行い、利用可能な
  qemu-user targetはsmoke testまで行う。
- JIT disabled、MinGW、OpenWatcom/Boots object buildもsymbol重複なくbuildする。

## 7. 実装順

### Phase A: baseline

- 現在のforced-clean x86-32 buildでG2Aが完走することを保存する。
- stale object時の`JIT_OP_NOT_IMPLEMENTED`はopcode goldenにせず、incident noteだけにする。

### Phase B: translation unit分離

- `jit.c`のinclude構造を除去する。
- CMake source listへ全arch JITを追加する。
- prototype/include/guardを整理する。

### Phase C: incremental build test

- temporary treeでarch source単独変更、common source単独変更、no-op buildを検査する。
- JITが実際にcompileされたことを`NOCT_JIT_DEBUG`で確認する。

### Phase D: portability gate

- native、cross、qemu-user、JIT-disabled build matrixを通す。
- 全backendでduplicate/missing symbolがないことを確認する。

## 8. 他計画との依存

この修正を、bytecode decoderまたは全JIT backendを変更する作業より先に行う。
特に[14-dot-member-thiscall.md](14-dot-member-thiscall.md)は`OP_THISCALL` layoutを
全backendで変更するため、この計画の完了をhard prerequisiteとする。

[12-noct-app-nap.md](12-noct-app-nap.md)は新opcodeを追加しないが、永続bytecodeを
配布する前にbuild artifactとsourceの一致を保証するため、最終portability gateは
この修正後のbinaryで行う。

## 9. 完了条件

- アーキテクチャ別`.c`を`jit.c`からincludeしていない。
- 各arch JIT sourceがCMake targetの直接sourceである。
- x86-32のarch sourceだけを更新したincremental buildで該当objectが必ず更新される。
- G2AがQEMU x86-32 JITでfallbackせず完走する。
- opcode未実装という誤ったtaskが残っていない。
- pushは行わない。
