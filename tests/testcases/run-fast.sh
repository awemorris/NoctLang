#!/bin/sh

set -eu

NOCT=${NOCT:-../../build-stage-b-static-full/noct}
TMP=$(mktemp -d)
REQUIRED_APP=fast/.tmp-fast-required-$$.nap
MULTI_APP=fast/.tmp-fast-multi-$$.nap
trap 'rm -rf "$TMP"; rm -f "$REQUIRED_APP" "$MULTI_APP"' EXIT HUP INT TERM

run_ok()
{
    source=$1
    expected=$2
    for options in '-j0 -O0' '-j -O0' '-j -O2'; do
        # shellcheck disable=SC2086
        "$NOCT" $options "$source" > "$TMP/out"
        diff -u "$expected" "$TMP/out"
    done
    echo "PASS $source"
}

check_message()
{
    source=$1
    message=$2
    shift 2
    "$NOCT" "$@" "$source" > "$TMP/error" 2>&1 || true
    grep -F "$message" "$TMP/error" >/dev/null || {
        cat "$TMP/error"
        exit 1
    }
}

run_ok fast/exact-shape.noct fast/exact-shape.noct.out
run_ok fast/intrinsics.noct fast/intrinsics.noct.out
run_ok fast/guarded-safe.noct fast/guarded-safe.noct.out
run_ok fast/simd.noct fast/simd.noct.out

if [ "${FAST_EXPECT_SIMD:-0}" = 1 ]; then
    simd_info=$("$NOCT" --simd-info -j0 -O2 fast/simd.noct 2>&1)
    test "$(echo "$simd_info" | grep -c 'vectorized')" -ge 2 || {
        echo "$simd_info"
        echo '__fast contiguous loops were not vectorized' >&2
        exit 1
    }
fi

for options in '-j0 -O0' '-j -O0' '-j -O2'; do
    # Prototype scanning must not execute fast_leaf's initializer.  The single
    # leading I comes from the real dependency load.
    # shellcheck disable=SC2086
    "$NOCT" $options --path=fast/modules fast/required-call.noct > "$TMP/required.out"
    diff -u fast/required-call.noct.out "$TMP/required.out"
done
echo 'PASS fast/required-call.noct'

"$NOCT" --compile --app --path=fast/modules "$REQUIRED_APP" \
    fast/required-call.noct
"$NOCT" -j0 "$REQUIRED_APP" > "$TMP/required-app.out"
diff -u fast/required-call.noct.out "$TMP/required-app.out"

# The root is intentionally listed before its callee.  Prototype collection
# is a separate pass, so explicit Noct App input order cannot affect typing.
"$NOCT" --compile --app "$MULTI_APP" \
    fast/app-main.noct fast/app-helper.noct
"$NOCT" -j0 "$MULTI_APP" > "$TMP/multi-app.out"
diff -u fast/app.noct.out "$TMP/multi-app.out"

check_message fast/axis-oob.noct 'Index out of bounds.' -j0 -O0
check_message fast/versioning-oob.noct 'out-of-range' -j0 -O2
check_message fast/versioning-oob.noct 'out-of-range' -j -O2
check_message fast/shape-mismatch.noct 'does not match the exact shape' -j0 -O0
check_message fast/restrict-alias.noct 'same object to restricted parameters' -j0 -O0
check_message fast/wrong-primitive.noct 'wrong primitive type' -j0 -O0
check_message fast/wrong-element.noct 'wrong packed element type' -j0 -O0
check_message fast/dynamic-zero.noct 'shape extents must be positive' -j0 -O0

for level in 0 1 2 3; do
    check_message fast/bad-global.noct 'is not available inside __fast func' -j0 "-O$level"
    check_message fast/bad-local.noct 'requires a type annotation' -j0 "-O$level"
    check_message fast/static-oob.noct 'provably out of bounds' -j0 "-O$level"
    check_message fast/view-mismatch.noct 'shape rank does not match' -j0 "-O$level"
    check_message fast/mutual-recursion.noct 'mutually recursive __fast calls' -j0 "-O$level"
    check_message fast/fallthrough.noct 'must return a value' -j0 "-O$level"
done

cp fast/exact-shape.noct "$TMP/exact-shape.noct"
cp fast/intrinsics.noct "$TMP/intrinsics.noct"
(cd "$TMP" && "$NOCT" --compile exact-shape.noct intrinsics.noct)
"$NOCT" -j0 "$TMP/exact-shape.nb" > "$TMP/exact.out"
"$NOCT" -j0 "$TMP/intrinsics.nb" > "$TMP/intrinsics.out"
diff -u fast/exact-shape.noct.out "$TMP/exact.out"
diff -u fast/intrinsics.noct.out "$TMP/intrinsics.out"

echo 'All __fast func tests passed.'
