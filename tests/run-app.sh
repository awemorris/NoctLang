#!/bin/sh

set -eu

NOCT=${NOCT:-../build-mt-debug/noct}
tmp="app/.tmp-app-$$"
mkdir "$tmp"
trap 'rm -f "$tmp"/*; rmdir "$tmp"' EXIT HUP INT TERM

"$NOCT" --compile --app --optimize-level=2 "$tmp/test.nap" \
    app/main.noct app/second.noct
head -n 1 "$tmp/test.nap" | grep -Fx '#!/usr/bin/noct'
test -x "$tmp/test.nap"
"$NOCT" --disable-jit "$tmp/test.nap" > "$tmp/interpreter.out"
diff app/app.out "$tmp/interpreter.out"
"$NOCT" --force-jit "$tmp/test.nap" > "$tmp/jit.out"
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

echo 'Noct App tests passed.'
