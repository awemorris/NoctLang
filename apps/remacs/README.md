# remacs — Re-implemented Editing Macros

A clean-room re-implementation of GNU Emacs's basic functionality on
the Noct VM. The editor core (buffers, keymaps, windows, an Emacs Lisp
interpreter, SKK input, a shell, a gud-style debugger, compilation
mode) is written entirely in Noct; every OS primitive it needs is a
standard or non-standard Noct API (Term, Process, Regex, File, System).

## Running

The whole editor ships as a single bytecode file that an **unmodified**
`noct` runs:

```
make
noct build-nb/REMACS.NB [file]
```

## Install

```
sudo make install     # -> /usr/bin/remacs + /usr/share/remacs/noct/REMACS.NB
remacs [file]
```

`make install` detects the OS family (Debian, RedHat, FreeBSD, macOS)
to choose the resource prefix; `make showconfig` prints the chosen
paths. `/usr/bin/remacs` is a small shell script that runs
`noct /usr/share/remacs/noct/REMACS.NB`.

## Startup files

- `~/.remacs` — Noct source; must define `remacsInit(ed)`.
- `~/.emacs`  — Emacs Lisp, evaluated by the built-in interpreter.

## Development

A C launcher builds against the parent NoctLang tree for the test
suite:

```
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug .
cmake --build build-debug -j
make test        # or: cd tests && sh run-all.sh
```

## Layout

- `editor/*.noct`  — the editor, in Noct
- `src/napi.def`   — the Editor.* API table (single source of truth)
- `tools/`         — gen-napi.py (bridge/table generator), build-nb.sh
- `tests/`         — unit, pty, GNU Emacs oracle, and Lisp oracle tests
- `oracle/`, `shim/` — GNU Emacs oracle support
