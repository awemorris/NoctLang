#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$script_dir"

echo 'NoctLang Test Suites'
echo

for suite in \
    syntax cli-options typing typedop abce cse parallel-analysis accel-analysis \
    accel accel-program simd class scoping app \
    thread httpserver webapp process fileutil-mmap
do
    sh "run-$suite.sh"
done

echo
echo 'All suites passed.'
