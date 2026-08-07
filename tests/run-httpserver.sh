#!/bin/sh

set -eu

NOCT=${NOCT:-../build-mt-debug/noct}

echo 'NoctLang HttpServer Tests'
echo

echo "(Interpreter)"
for tc in httpserver/*.noct; do
    echo "$tc";
    $NOCT --disable-jit $tc > out || true;
    diff $tc.out out;
done
echo "(JIT)"
for tc in httpserver/*.noct; do
    echo "$tc";
    $NOCT --force-jit $tc > out || true;
    diff $tc.out out;
done
echo 'All HttpServer tests passed.'
