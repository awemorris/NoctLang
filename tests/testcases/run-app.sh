#!/bin/sh

set -eu

NOCT=${NOCT:-../../build-mt-debug/noct}
tmp="app/.tmp-app-$$"
mkdir "$tmp"
trap 'rm -f "$tmp"/*; rmdir "$tmp"' EXIT HUP INT TERM

"$NOCT" --compile --app -O2 "$tmp/test.nap" \
    app/main.noct app/second.noct
head -n 1 "$tmp/test.nap" | grep -Fx '#!/usr/bin/noct'
test -x "$tmp/test.nap"
"$NOCT" -j0 "$tmp/test.nap" > "$tmp/interpreter.out"
diff app/app.out "$tmp/interpreter.out"
"$NOCT" -j "$tmp/test.nap" > "$tmp/jit.out"
diff app/app.out "$tmp/jit.out"

if "$NOCT" --compile --app "$tmp/duplicate.nap" \
    app/main.noct app/duplicate.noct > "$tmp/duplicate.log" 2>&1; then
    echo 'duplicate public symbol was accepted' >&2
    exit 1
fi
grep -F 'Duplicate public symbol "from_a"' "$tmp/duplicate.log"

if "$NOCT" --compile --app "$tmp/repeated.nap" \
    app/main.noct app/main.noct > "$tmp/repeated.log" 2>&1; then
    echo 'duplicate input was accepted' >&2
    exit 1
fi
grep -F 'Duplicate Noct App input' "$tmp/repeated.log"

if "$NOCT" --compile --app "$tmp/absolute.nap" \
    "$PWD/app/main.noct" > "$tmp/absolute.log" 2>&1; then
    echo 'absolute input path was accepted' >&2
    exit 1
fi

printf 'preserve-me\n' > "$tmp/preserved.nap"
if "$NOCT" --compile --app "$tmp/preserved.nap" \
    app/no-main.noct > "$tmp/no-main.log" 2>&1; then
    echo 'application without main was accepted' >&2
    exit 1
fi
grep -Fx 'preserve-me' "$tmp/preserved.nap"

# Runtime source loading and .nap linking share require resolution.  The leaf
# initializer must run once, followed by its importer and finally the root.
"$NOCT" -j0 --path=app/require/modules \
    app/require/main.noct > "$tmp/require-runtime.out"
diff app/require/require.out "$tmp/require-runtime.out"

"$NOCT" --compile --app --path=app/require/modules \
    "$tmp/require.nap" app/require/main.noct
"$NOCT" -j0 "$tmp/require.nap" > "$tmp/require-app.out"
diff app/require/require.out "$tmp/require-app.out"
"$NOCT" -j "$tmp/require.nap" > "$tmp/require-app-jit.out"
diff app/require/require.out "$tmp/require-app-jit.out"
if grep -F "$PWD" "$tmp/require.nap" >/dev/null; then
    echo 'Noct App leaked an absolute require path' >&2
    exit 1
fi

if "$NOCT" --compile --app --path=app/require/modules:app/require \
    "$tmp/cycle.nap" app/require/cycle-a.noct \
    > "$tmp/cycle.log" 2>&1; then
    echo 'circular require was accepted' >&2
    exit 1
fi
grep -F 'Circular require' "$tmp/cycle.log"

if "$NOCT" -j0 --path=app/require/modules \
    app/require/missing.noct > "$tmp/missing.log" 2>&1; then
    echo 'missing required module was accepted' >&2
    exit 1
fi
grep -F "Cannot resolve required module 'absent_module'" "$tmp/missing.log"

cp app/require/main.noct "$tmp/standalone.noct"
if "$NOCT" --compile "$tmp/standalone.noct" \
    > "$tmp/standalone.log" 2>&1; then
    echo 'standalone bytecode accepted require without bundling' >&2
    exit 1
fi
grep -F 'require is supported by --compile --app' "$tmp/standalone.log"

echo 'Noct App tests passed.'
