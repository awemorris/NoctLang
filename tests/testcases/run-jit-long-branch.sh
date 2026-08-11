#!/bin/sh

set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_file="$repo_dir/tests/testcases/syntax/48-numeric-cond.noct"
expected_file="$test_file.out"
tmp_dir=$(mktemp -d)
trap 'rm -rf -- "$tmp_dir"' EXIT HUP INT TERM

if [ "$#" -eq 0 ]; then
	echo "usage: $0 [emulator ...] noct-binary" >&2
	exit 2
fi

NOCT_JIT_DEBUG=1 NOCT_JIT_FORCE_LONG_BRANCH=1 \
	"$@" -j "$test_file" >"$tmp_dir/actual" 2>"$tmp_dir/debug"

cmp "$expected_file" "$tmp_dir/actual"
grep -q '^noct-jit: main: compiled$' "$tmp_dir/debug"
