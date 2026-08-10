# `require` source module loading

Status: implemented.

## Surface syntax

`require` is a top-level declaration and takes one bare module name.

```noct
require framework;
require json;
```

It is not an executable HIR statement. The parser records the dependency in
the compilation unit, and duplicate declarations of the same name in one file
are collapsed. As with `static` and `inline`, `require` remains usable as a
dictionary key or member name (`obj.require`).

## Search path

The CLI option is `--path=DIR1:DIR2`. It may be repeated and must precede the
first file operand. The current directory (`.`) is always the first search
directory. For `require foo;`, every directory is tried in this order:

1. `foo.noct`
2. `foo.nct`

The public runtime API `noct_add_require_path(vm, path_list)` appends the same
colon-separated representation. A `NoctVM` owns the split path array and its
module load-state table.

Module names are intentionally bare identifiers in the first implementation.
Absolute paths, parent traversal, and path separators cannot enter through a
`require` declaration.

## Runtime loading

Source loading is host-serialized because AST, HIR, and LIR construction use
process-global compiler state. One module is processed as follows:

1. mark its physical-path key `LOADING`;
2. parse it and retain its require list;
3. optimize/lower and register all of its LIR functions in the VM;
4. recursively resolve and load its dependencies;
5. execute its `$init.*` function;
6. mark it `LOADED`.

Thus unbound symbols remain legal while the current module is lowered, while
dependency initializers still run before the importing initializer. A module
already in `LOADED` state is not loaded or initialized twice. Reaching a
`LOADING` module is a deterministic circular-require error; `FAILED` modules
are not retried in the same VM.

Only recursively resolved dependencies remain in that table. A source passed
directly to `noct_register_source` is present while its require graph is being
walked (so a back-edge is detected), then its entry is removed. This preserves
the established REPL and `System.registerSource` behavior in which a host may
compile new text repeatedly under the same logical filename.

The duplicate key is an absolute, lexically normalized physical path. That
path exists only in loader state. Required sources are compiled with a safe
logical name such as `@require/framework.noct`, so static-name mangling and
`.nap` output do not disclose a home directory or other host absolute path.

## `.nap` compile-only linking

`noct --compile --app` uses the same resolver but does not create or execute a
runtime VM. Its compile-only link context owns search paths, module states,
all collected LIR functions, the public-symbol table, and the initializer
list.

For every command-line root, the collector registers that module's LIR before
walking its require edges. Initializer names are appended in DFS postorder.
After all roots and dependencies are collected, one aggregate initializer is
synthesized with calls in dependency-first order. The VM is never entered
during this process; execution happens only when the finished `.nap` is later
loaded.

Explicitly listing the same root twice remains an error. Encountering the same
module through more than one require edge is a successful no-op. Cycles are
rejected rather than assigned partially initialized semantics.

Standalone `.nb` compilation rejects a source containing `require`, because
the current bytecode format has no dependency metadata. Bundling with
`--compile --app` is the supported ahead-of-time route. The C, Emacs Lisp,
and Scheme transpilers also reject `require` explicitly until they gain an
equivalent multi-module linker; they never silently omit a dependency.

## Tests

`tests/run-app.sh` covers runtime source loading, transitive and duplicate
requires, `.nct` fallback, dependency-first initialization, interpreter and
JIT execution of the resulting `.nap`, absence of absolute paths, missing
modules, require-only cyclic modules, and standalone `.nb` rejection.
`tests/run-repl.sh` guards repeated root-source registration under one logical
filename, which must remain independent from dependency de-duplication.
