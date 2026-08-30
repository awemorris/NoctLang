#!/bin/sh

set -eu

NOCT=${NOCT:-../../build-mt-debug/noct}

echo 'NoctLang Tests'
echo

echo 'Running bootstrap tests...'
# Keep the removed __fast fixture out of the maintained syntax matrix.
echo "(Interpreter)";
for tc in syntax/*.noct; do
    case "$tc" in
    syntax/68-fast-prefix.noct) continue ;;
    esac
    echo "$tc";
    $NOCT -j0 $tc > out || true;
    diff $tc.out out;
done
echo "(JIT)";
for tc in syntax/*.noct; do
    case "$tc" in
    syntax/68-fast-prefix.noct) continue ;;
    esac
    echo "$tc";
    $NOCT -j $tc > out || true;
    diff $tc.out out;
done
echo "(JIT + -O2)";
for tc in syntax/*.noct; do
    case "$tc" in
    syntax/68-fast-prefix.noct) continue ;;
    esac
    echo "$tc";
    $NOCT -j -O2 $tc > out || true;
    diff $tc.out out;
done
echo 'All tests passed.'
