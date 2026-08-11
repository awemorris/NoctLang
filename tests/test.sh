#!/bin/sh

set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case_dir="$test_dir/testcases"

show_help()
{
    cat <<'EOF'
Usage: tests/test.sh [command] [arguments]

Main suites:
  all              Run the normal host test suite (default).
  syntax           Parser, language syntax, interpreter and JIT tests.
  cli              Command-line optimization/JIT option tests.
  typing           Type annotations and bytecode metadata.
  typedop          Typed LIR operation generation.
  abce             Array-bounds-check elimination.
  cse              Common-subexpression elimination.
  simd             SIMD vectorization, fallback and bytecode tests.
  class            Class freezing and top-level declarations.
  scoping          Block scope, let and TDZ behavior.
  app              .nap packaging and require resolution.
  thread           Thread API tests.
  thread-stress    Repeat promotion/expansion races (default: 100 times).
  httpserver       HTTP server API tests.
  webapp           Web application framework tests.
  process          Process API tests.

Toolchain/integration suites:
  api [build-dir]  Embed/API backend test (default: build-static).
  ctrans [dir]     ANSI C translation tests (default: build-static).
  repl [dir]       REPL session tests (default: build-static).
  fma [dir]        FMA helper C test (default: build-debug).
  jit-slab [dir]   JIT slab allocator and retry tests.
  jit-branch ...   Long-branch test; arguments are [emulator ...] noct.
  beui [dir]       BeUI and PC-98 backend host tests.
  elisp            Emacs Lisp translation tests.

Cross-architecture suites:
  multiarch        Build and run available targets from multiarch.noct via QEMU.
  simd-qemu ...    Run SIMD tiers: ARCH NOCT_BINARY [SYSROOT].

Environment variables such as NOCT, CC, QEMU and QEMU_CPU are passed to
the concrete scripts in tests/testcases/.  Relative build directories are
resolved from the repository root.
EOF
}

run_script()
{
    script=$1
    shift
    cd "$case_dir"
    exec sh "./$script" "$@"
}

command=${1:-all}
if [ "$#" -gt 0 ]; then
    shift
fi

case "$command" in
help|-h|--help) show_help ;;
all)             run_script run-all.sh "$@" ;;
syntax)          run_script run-syntax.sh "$@" ;;
cli)             run_script run-cli-options.sh "$@" ;;
typing)          run_script run-typing.sh "$@" ;;
typedop)         run_script run-typedop.sh "$@" ;;
abce)            run_script run-abce.sh "$@" ;;
cse)             run_script run-cse.sh "$@" ;;
simd)            run_script run-simd.sh "$@" ;;
class)           run_script run-class.sh "$@" ;;
scoping)         run_script run-scoping.sh "$@" ;;
app)             run_script run-app.sh "$@" ;;
thread)          run_script run-thread.sh "$@" ;;
thread-stress)   run_script run-thread-stress.sh "$@" ;;
httpserver)      run_script run-httpserver.sh "$@" ;;
webapp)          run_script run-webapp.sh "$@" ;;
process)         run_script run-process.sh "$@" ;;
api)             run_script run-api-backend.sh "$@" ;;
ctrans)          run_script run-ctrans.sh "$@" ;;
repl)            run_script run-repl.sh "$@" ;;
fma)             run_script run-fma-helper.sh "$@" ;;
jit-slab)        run_script run-jit-slab.sh "$@" ;;
jit-branch)      run_script run-jit-long-branch.sh "$@" ;;
beui)            run_script run-beui.sh "$@" ;;
elisp)           run_script run-elisp.sh "$@" ;;
multiarch)       run_script run-multiarch.sh "$@" ;;
simd-qemu)       run_script run-simd-qemu.sh "$@" ;;
*)
    echo "Unknown test command: $command" >&2
    echo "Run '$0 help' for the command list." >&2
    exit 2
    ;;
esac
