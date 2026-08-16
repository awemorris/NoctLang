# Packages and native libraries

## Source packages

`require name;` first searches the current directory and every `--path=`
entry for `name.noct` and `name.nct`.  It then searches installed packages in
this order:

1. `$HOME/.noct/packages/name/name.noct` (or `%USERPROFILE%` on Windows)
2. `/usr/local/share/noct/packages/name/name.noct`
3. `/usr/share/noct/packages/name/name.noct`

The `.nct` suffix is the fallback at each location.  Package names contain
ASCII letters, digits and underscores and may not start with a digit.  A
physical source file is registered only once per VM, including diamond-shaped
`require` graphs.

A package entry may define a private initializer:

```noct
func package_init(): void {
    // Runs once for each VM that loads this package.
}
```

It has no parameters, must explicitly return `void`, and is mangled from the
logical `@package/name/name.noct` path.  Dependencies initialize first.  Within
one package, top-level `var`/`let` initialization runs before `package_init()`.
The mangled name contains neither the user's home directory nor another host
absolute path.

Run a package entry point with:

```sh
noct -m name arg1 arg2
```

This loads the installed package and invokes public `name_main()`.  Like normal
`main()`, it accepts either no parameters or one Array parameter.

`noct --compile --app` follows package dependencies and embeds their code and
initializer calls in the resulting `.nap`; it does not execute package code at
compile time.

### Package bytecode cache

Installed source packages are compiled into a content-addressed cache below
`$HOME/.noct/cache/packages/name/` (or `%USERPROFILE%` on Windows).  The key
includes the compiler/cache schema, optimization settings, logical source
names, and the contents of every transitive source dependency.  Physical home
directory paths are never written into cached bytecode.

Each `.nbp` entry has a SHA-256 checksum sidecar and is published by atomic
rename.  A missing, stale, truncated, or corrupt entry is ignored and rebuilt
from source.  Cache creation is best effort: an unwritable cache directory does
not prevent package execution.  Loading cached code still runs each package's
top-level initializer and `package_init()` once for the current VM; the cache
stores compiled code, not initialized VM state.

## Native libraries

Hosted Windows, Linux and macOS builds provide:

```noct
System.loadDLL("codec");     // returns 1 or raises an error
System.tryLoadDLL("codec");  // returns 0 only when it is absent
```

Names are logical, not paths.  They map to `codec.dll`, `libcodec.so`, or
`libcodec.dylib`.  Path separators, suffixes and traversal are rejected.  For
a call made by package code, Noct checks the package's
`native/<architecture-os>/` directory and package root before the normal
current/user/system library directories.  Merely installing a native library
does not load it; package source must request it explicitly.

Every library exports exactly this entry point:

```c
NOCT_LIBRARY_EXPORT bool CDECL
noct_library_init(const NoctAPI *api, NoctEnv *env);
```

The library must verify `api->abi_version`, `api->struct_size`, and any feature
bits it uses.  Function-table fields are append-only.  Registration performed
by a failing initializer is rolled back atomically.  The OS handle stays loaded
for the process lifetime, while `noct_library_init()` runs once for every VM.
VM-specific data belongs in `noct_register_cfunc_with_data()` and must be
released through `noct_register_vm_finalizer()`.

Libraries must not retain `NoctEnv`, `NoctVM`, or `NoctValue` pointers after a
call.  They may retain the immutable `NoctAPI` pointer.  DLL unloading and a
library shutdown entry point are intentionally unsupported.  Native code is
trusted code and is outside the VM sandbox.

GPU backends are unrelated to this ABI: Vulkan, OpenGL ES and DirectX 12 remain
compiled into Noct and use a private, VM-local backend registry.
