# Source modules

`require name;` asks the embedding host to resolve `name`. The core VM does not
choose directories or know about an installation layout. An embedding host
enables the statement by setting `NoctConfig.require_resolver`; without a
resolver, using `require` is an error.

The callback returns a newly allocated path. The VM reads that source file and
frees the returned path. A resolved physical path is loaded at most once in a
VM, so a dependency shared by multiple modules is initialized once. Circular
loads and repeated attempts to load a failed module are errors.

## CLI resolution

The `noct` CLI supplies its own resolver. For `require name;`, it searches:

1. the current directory;
2. directories from each `--path=DIR1:DIR2` option, in command-line order.

Within each directory, `name.noct` is tried before `name.nct`. Module names are
ASCII-style identifiers containing letters, digits, or underscores, and may
not begin with a digit.

For example:

```sh
noct --path=lib:vendor main.noct
```

The CLI's path policy is intentionally outside `libnoct`. Another host may
resolve modules from application resources, an archive, or another source by
materializing the module as a source file and returning that file's path from
its callback.
