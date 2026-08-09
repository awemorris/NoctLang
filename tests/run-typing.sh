#!/bin/sh

#
# Type-annotation test suite (docs/design/02-typing.md).
# Annotations are inert below level 2; at level 2 annotated params get
# entry checks.  A case may provide NAME.noct.out2 as the level-2
# golden (used by the intentional violation case).
#

NOCT=${NOCT:-../build-static/noct}

echo 'Typing tests:'

FAILED=0
TMP_DIR=$(mktemp -d)
OUT="$TMP_DIR/out"
trap 'rm -rf -- "$TMP_DIR"' EXIT HUP INT TERM
for tc in typing/*.noct; do
    for lvl in "" "--optimize-level=2"; do
        golden="$tc.out"
        if [ -n "$lvl" ] && [ -f "$tc.out2" ]; then
            golden="$tc.out2"
        fi
        for jit in "--disable-jit" "--force-jit"; do
            $NOCT $jit $lvl "$tc" > "$OUT" 2>&1
            if ! diff -q "$golden" "$OUT" > /dev/null 2>&1; then
                echo "FAIL $tc ($jit $lvl)"
                diff "$golden" "$OUT" | head -5
                FAILED=1
            fi
        done
    done
    echo "PASS $tc"
done

# Exercise the strict bytecode metadata reader with the new packed/restrict
# parameter sections.  The compiler places the .nb beside its input.
cp typing/anno_packed_types.noct "$TMP_DIR/packed_roundtrip.noct"
$NOCT --compile "$TMP_DIR/packed_roundtrip.noct"
$NOCT --disable-jit "$TMP_DIR/packed_roundtrip.nb" > "$OUT" 2>&1
if ! diff -q typing/anno_packed_types.noct.out "$OUT" > /dev/null 2>&1; then
    echo "FAIL packed/restrict bytecode round trip"
    diff typing/anno_packed_types.noct.out "$OUT" | head -5
    FAILED=1
else
    echo "PASS packed/restrict bytecode round trip"
fi

if [ "$FAILED" -ne 0 ]; then
    echo 'Typing tests failed.'
    exit 1
fi
echo 'All typing tests passed.'
