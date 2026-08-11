#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$root"

show_help()
{
    cat <<'EOF'
Usage: ./build.sh command [arguments]

Build commands:
  build              List build targets available on this host.
  build TARGET       Configure and build TARGET.
                     Examples: static, windows-mingw-x86_64, pc98.
  configure PRESET   Configure without building.

Test and benchmark commands:
  test [command ...]  Run tests/test.sh (for example: test help, test simd).
  bench [command ...] Run bench/bench.sh (for example: bench help, bench simd).

Other:
  presets            List all CMake configure and build presets.
  help               Show this command list.
EOF
}

show_build_targets()
{
    echo 'Usage: ./build.sh build TARGET'
    echo
    cmake --list-presets=configure
}

preset_exists()
{
    kind=$1
    preset=$2
    cmake --list-presets="$kind" 2>/dev/null |
        grep -F "  \"$preset\"" >/dev/null
}

build_target()
{
    target=$1

    if ! preset_exists configure "$target"; then
        echo "Unknown or unavailable build target: $target" >&2
        echo >&2
        show_build_targets >&2
        exit 2
    fi

    cmake --preset "$target"
    if preset_exists build "$target"; then
        cmake --build --preset "$target" --parallel
    else
        # Configure presets without a matching build preset use the standard
        # ${sourceDir}/build-${presetName} layout in CMakePresets.json.
        cmake --build "$root/build-$target" --parallel
    fi
}

command=${1:-help}
if [ "$#" -gt 0 ]; then
    shift
fi

case "$command" in
help|-h|--help) show_help ;;
build)
    if [ "$#" -eq 0 ]; then
        show_build_targets
    elif [ "$#" -eq 1 ]; then
        build_target "$1"
    else
        echo "usage: $0 build [TARGET]" >&2
        exit 2
    fi
    ;;
configure)
    if [ "$#" -ne 1 ]; then
        echo "usage: $0 configure PRESET" >&2
        exit 2
    fi
    cmake --preset "$1"
    ;;
test)    exec sh "$root/tests/test.sh" "$@" ;;
bench)   exec sh "$root/bench/bench.sh" "$@" ;;
presets) exec cmake --list-presets=all ;;
*)
    echo "Unknown build command: $command" >&2
    echo "Run '$0 help' for the command list." >&2
    exit 2
    ;;
esac
