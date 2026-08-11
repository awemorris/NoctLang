#!/bin/sh
set -eu
echo 'Process API tests'
for tc in process/*.noct; do
    echo "$tc"
    ../../build-mt-debug/noct -j0 "$tc" > out
    diff "$tc.out" out
done
rm -f out
echo 'All Process tests passed.'
