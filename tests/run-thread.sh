#!/bin/sh

set -eu

NOCT=${NOCT:-../build-mt-debug/noct}

echo 'NoctLang Thread Tests'
echo

echo "(Interpreter)"
for tc in thread/*.noct; do
    echo "$tc";
    $NOCT --disable-jit $tc > out || true;
    diff $tc.out out;
done
echo "(JIT)"
for tc in thread/*.noct; do
    echo "$tc";
    $NOCT --force-jit $tc > out || true;
    diff $tc.out out;
done
echo 'All thread tests passed.'
