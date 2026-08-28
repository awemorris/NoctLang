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
  packed-loop      Width-1 Packed loop and JIT register-cache tests.
  cse              Common-subexpression elimination.
  simd             SIMD vectorization, fallback and bytecode tests.
  fast             __fast functions, exact shapes and row-major indexing.
  class            Class freezing and top-level declarations.
  scoping          Block scope, let and TDZ behavior.
  app              .nap packaging and require resolution.
  thread           Thread API tests.
  thread-stress    Repeat promotion/expansion races (default: 100 times).
  httpserver       HTTP server API tests.
  webapp           Web application framework tests.
  process          Process API tests.
  dynlib [build-dir] Dynamic-library ABI, loading and rollback tests.
  mmap [build-dir] FileUtil mmap and Packed native-finalizer tests.
  parallel-analysis Target-neutral loop fact/classification tests.
  accel            Accelerator syntax, contracts and serialization tests.
  accel-analysis   Target-neutral accelerator loop analysis tests.
  accel-program    Accelerator program descriptor unit tests.

Accelerator hardware suites:
  accel-opengl     OpenGL ES execution tests (NOCT selects the binary).
  accel-vulkan     Vulkan execution tests (NOCT selects the binary).
  accel-dx12       DirectX 12 execution tests on Windows (NOCT is required).
  accel-serialization  Relocated OpenGL .nb/.nap hardening tests.

Model and ONNX suites:
  model-weights    Binary, hash and NWT1 model-weight tests.
  onnx2noct        ONNX reader, normalization and code-generation tests.
  onnx-gpu         Generated base GPU model OpenGL tests.
  onnx-conv        Generated convolution OpenGL tests.
  onnx-contraction Generated Gemm/MatMul OpenGL tests.
  onnx-pool        Generated pooling OpenGL tests.
  onnx-concat      Generated concatenation OpenGL tests.
  onnx-reduce      Generated reduction OpenGL tests.
  onnx-batchnorm   Generated BatchNormalization OpenGL tests.
  onnx-package     Complete generated-package OpenGL tests.
  onnx-mnist       Locked MNIST model OpenGL tests.
  onnx-cifar       Project CIFAR model OpenGL tests.
  onnx-squeezenet  Locked SqueezeNet model OpenGL tests.
  onnx-tinyyolo    Tiny YOLOv2 static-shape blocker test.

Toolchain/integration suites:
  api [build-dir]  Public File/Term registration test (default: build-static).
  ctrans [dir]     ANSI C translation tests (default: build-static).
  repl [dir]       REPL session tests (default: build-static).
  fma [dir]        FMA helper C test (default: build-debug).
  jit-slab [dir]   JIT slab allocator and retry tests.
  jit-branch ...   Long-branch test; arguments are [emulator ...] noct.
  beui [dir]       Independent BeUI platform/core host tests.
  elisp            Emacs Lisp translation tests.

Cross-architecture suites:
  multiarch        Build and run available targets from multiarch.noct via QEMU.
  simd-qemu ...    Run SIMD tiers: ARCH NOCT_BINARY [SYSROOT].

Environment variables such as NOCT, CC, QEMU and QEMU_CPU are passed to
the concrete scripts in tests/testcases/.  Relative build directories are
resolved from the repository root.  Hardware/model suites may additionally
use CONVERTER_NOCT, NOCT_OPENGL_RENDERER_PATTERN and ONNX_ORACLE_PYTHON.
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
packed-loop)     run_script run-packed-loop.sh "$@" ;;
cse)             run_script run-cse.sh "$@" ;;
simd)            run_script run-simd.sh "$@" ;;
fast)            run_script run-fast.sh "$@" ;;
class)           run_script run-class.sh "$@" ;;
scoping)         run_script run-scoping.sh "$@" ;;
app)             run_script run-app.sh "$@" ;;
thread)          run_script run-thread.sh "$@" ;;
thread-stress)   run_script run-thread-stress.sh "$@" ;;
httpserver)      run_script run-httpserver.sh "$@" ;;
webapp)          run_script run-webapp.sh "$@" ;;
process)         run_script run-process.sh "$@" ;;
dynlib)          run_script run-dynlib.sh "$@" ;;
mmap)            run_script run-fileutil-mmap.sh "$@" ;;
parallel-analysis) run_script run-parallel-analysis.sh "$@" ;;
accel)           run_script run-accel.sh "$@" ;;
accel-analysis)  run_script run-accel-analysis.sh "$@" ;;
accel-program)   run_script run-accel-program.sh "$@" ;;
accel-opengl)    run_script run-accel-opengl.sh "$@" ;;
accel-vulkan)    run_script run-accel-vulkan.sh "$@" ;;
accel-dx12)      run_script run-accel-dx12.sh "$@" ;;
accel-serialization) run_script run-accel-serialization.sh "$@" ;;
model-weights)   run_script run-model-weights.sh "$@" ;;
onnx2noct)       run_script run-onnx2noct.sh "$@" ;;
onnx-gpu)        run_script run-onnx-gpu-opengl.sh "$@" ;;
onnx-conv)       run_script run-onnx-gpu-conv-opengl.sh "$@" ;;
onnx-contraction) run_script run-onnx-gpu-contraction-opengl.sh "$@" ;;
onnx-pool)       run_script run-onnx-gpu-pool-opengl.sh "$@" ;;
onnx-concat)     run_script run-onnx-gpu-concat-opengl.sh "$@" ;;
onnx-reduce)     run_script run-onnx-gpu-reduce-opengl.sh "$@" ;;
onnx-batchnorm)  run_script run-onnx-gpu-batchnorm-opengl.sh "$@" ;;
onnx-package)    run_script run-onnx-package-opengl.sh "$@" ;;
onnx-mnist)      run_script run-onnx-mnist-opengl.sh "$@" ;;
onnx-cifar)      run_script run-onnx-cifar-opengl.sh "$@" ;;
onnx-squeezenet) run_script run-onnx-squeezenet-opengl.sh "$@" ;;
onnx-tinyyolo)   run_script run-onnx-tinyyolo-blocker.sh "$@" ;;
api)             run_script run-api.sh "$@" ;;
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
