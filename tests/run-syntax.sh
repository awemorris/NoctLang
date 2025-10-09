#!/bin/sh

set -eu

echo 'NoctLang Tests'
echo

echo 'Running bootstrap tests...'
echo "(Interpreter)";
for tc in syntax/*.noct; do
    echo "$tc";
    ../build/noctcli --disable-jit $tc > out || true;
    diff $tc.out out;
done
echo "(JIT)";
for tc in syntax/*.noct; do
    echo "$tc";
    ../build/noctcli --force-jit $tc > out || true;
    diff $tc.out out;
done
echo 'All tests passed.'
