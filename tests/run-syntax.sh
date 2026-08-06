#!/bin/sh

set -eu

NOCT=${NOCT:-../build-mt-debug/noct}

echo 'NoctLang Tests'
echo

echo 'Running bootstrap tests...'
echo "(Interpreter)";
for tc in syntax/*.noct; do
    echo "$tc";
    $NOCT --disable-jit $tc > out || true;
    diff $tc.out out;
done
echo "(JIT)";
for tc in syntax/*.noct; do
    echo "$tc";
    $NOCT --force-jit $tc > out || true;
    diff $tc.out out;
done
echo 'All tests passed.'
