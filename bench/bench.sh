#!/bin/sh

set -eu

bench_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

show_help()
{
    cat <<'EOF'
Usage: bench/bench.sh command [arguments]

Commands:
  abce          Compare ABCE O0/O2 in interpreter and JIT modes.
  simd          Run generic RGBA, f32 and u32 SIMD benchmarks as CSV.
  simd-report   Convert SIMD CSV to a Markdown speedup table.
                Optional argument: path to CSV; otherwise reads stdin.
  drawimage-alpha
                Measure the canonical DRAW_IMAGE_ALPHA function at O2/O3.
                SAMPLES, PIXELS, LEVELS, CPU and NOCT_BUILD are configurable.
  multi-doall   Compare one large five-stage CPU/JIT calculation with managed
                OpenGL ES and Vulkan __accel calls. SAMPLES is configurable.
  help          Show this command list.

The benchmark scripts honor NOCT.  The SIMD runner also honors RUNS,
WARMUPS, CPU and MODES; see bench/run-simd-bench.sh for details.
EOF
}

command=${1:-help}
if [ "$#" -gt 0 ]; then
    shift
fi
cd "$bench_dir"

case "$command" in
help|-h|--help) show_help ;;
abce)           exec sh ./run-bench.sh "$@" ;;
simd)           exec sh ./run-simd-bench.sh "$@" ;;
simd-report)    exec sh ./report-simd-bench.sh "$@" ;;
drawimage-alpha) exec sh ./drawimage/run-alpha.sh "$@" ;;
multi-doall)     exec sh ./run-multi-doall-bench.sh "$@" ;;
*)
    echo "Unknown benchmark command: $command" >&2
    echo "Run '$0 help' for the command list." >&2
    exit 2
    ;;
esac
