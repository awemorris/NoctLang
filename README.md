NoctLang
========

Noct is a tiny yet mighty programming language for scripting.
Small enough to learn today, powerful enough to ship tomorrow!

Just ~160 KB — featuring a fast JIT compiler, robust generational GC,
and clean C/JS-like syntax with a novel Dictionary-based OOP.

Written in portable ANSI C with no external dependencies, it runs
everywhere — from desktop PCs down to single-chip environments.

Try it now — launch REPL or write your first program.  It might take
less time than you think.

## Status

Actively developed and constantly evolving.

### JIT Targets:

- Mainstream
    - x86, x86_64, ARMv7, Arm64
- Next Generation
    - RISC-V 32-bit, RISC-V 64-bit
- Misc
    - PowerPC 32-bit, PowerPC 64-bit, MIPS 32-bit, MIPS 64-bit

## OSes:

- Desktop
    - Windows, macOS, Linux
- Mobile
    - iOS, Android
- Server
    - FreeBSD, NetBSD, OpenBSD
- Game Consoles

Note: On major smartphones and consoles, runtime code generation (JIT)
is generally prohibited or tightly restricted by platform
policies. Noct runs there with interpreter or AOT compilation.

---

## Core Design & Features

Noct combines simplicity, speed, and portability — traits rarely
found together in scripting languages:

- **Familiar Syntax** — C/JS-like and easy to learn.
- **Lightweight JIT** — Fast execution in a tiny runtime.
- **Generational GC** — Young semi-space copying + old mark-sweep-compact.
- **Portable ANSI C** — No dependencies; runs everywhere.
- **Tiny Footprint** — Runtime fits in ~160 KB.
- **AOT Compilation** — Translate to C for JIT-restricted platforms. (e.g. iOS, Android)

While most languages compromise on at least one of these,  
Noct delivers all without sacrificing clarity or speed.

In addition, Noct introduces **Dictionary-based OOP (D-OOP)** — a
novel paradigm for object-oriented programming, achieved through
dictionary merging instead of prototype chains or heavyweight class
hierarchies. Thanks to this design, Noct programs can even be
translated into Emacs Lisp code, reflecting the language's
Lisp-inspired roots.

---

## Why Noct?

_"What if a programming language could be learned in a single
afternoon — and used the next day to create real games?"_

Noct was born from this question: a desire to create a language
that’s minimal yet meaningful — **simple enough for beginners, fast
enough for production**.  It bridges the gap between **play** and
**production**, letting you focus on making games, not fighting tools.

At the same time, Noct brings **commercial-grade VM technology** —
once limited to large industrial runtimes — into a form small enough
for game projects, built on proven techniques from engines like Java
and .NET.

---

## Try it!

### Your First Program

Noct is simple enough to try right now — no setup, no hassle.

Save the following as `first.noct`, and just run `noct first.noct`.

```
func main() {
    Person = class {
        name: ""
    };

    var jessie = new Person { name: "Jessie" };
    var tom = new Person { name: "Tom" };

    var people = [jessie, tom];
    for (person in people) {
        print("Hello, " + person.name + "!");
    }
}
```

Output:
```
Hello, Jessie!
Hello, Tom!
```

That's it. You've written your first Noct program.

## Installation

### Download Prebuild Binaries

Visit
[the release page](https://github.com/awemorris/NoctLang/releases)
to obtain the latest prebuilt binaries.

### Manually Build from Source

Clone the repository, build it with CMake, and you’re ready to go:

```
git clone https://github.com/awemorris/NoctLang.git noct
cd noct
cmake -B build .
cmake --build build
./build/noct
```

### Run

To run a script:

```
noct script.noct
```

---

## Examples

Noct programs consist of functions, expressions, and control structures
similar to C and JavaScript. The `main` function is the entry point.

### Arrays

```
func main() {
    var array = [1, 2, 3];
    for (v in array) {
        print(v);
    }
}
```

### Dictionaries

```
func main() {
    var dict = {name: "Apple", price: 100};
    for (key, value in dict) {
        print(key + "=" + value);
    }
}
```

### Lambda Functions

```
func main() {
    // Lambda notation.
    var f = (x) => { return x + 1; }
    print(f(1));

    // No closures. Use the 'with' argument explicitly.
    var g = (x, with) => {
        return x + with.y;
    };
    var y = 2;
    var z = g(1, {y: y});
}
```

### Objective Notation

The object-oriented model in Noct is a lightweight variation of prototype-based OOP.

- Classes are simply dictionary templates
- Inheritance and instantiation are realized by dictionary merging
- There is no prototype chain, and modifying a class does not affect existing instances

This design treats dictionaries as first-class objects, and the author refers to it as Dictionary-based OOP (D-OOP).

```
func main() {
    // The base class definition. (A class is just a dictionary.)
    Animal = class {
        name: "Animal",
        cry: (this) => {
        }
    };

    // The subclass definition. (Just a dictionary merging.)
    Cat = extend Animal {
        name: "Cat",
        voice: "meow",
        cry: (this) => {
            print(this.name + " cries like " + this.voice);
        }
    };

    // Instantiation. (Just a dictionary merging.)
    var myCat = new Cat {
        voice: "neee"
    };

    // This-call uses -> () syntax. (Equal to myCat.cry(myCat))
    myCat->cry();
}
```

---

## FFI API

The Noct runtime can be embedded in C applications. This allows you to
load, compile, and execute scripts directly within your software.

```
void call_noct(const char *file_name, const char *file_text)
{
    // Create a VM.
    NoctVM *vm;
    NoctEnv *env
    noct_create_vm(&vm, &env);

    // Compile source.
    noct_register_source(env, file_name, file_text);

    // Call the main() function.
    NoctValue ret = NOCT_ZERO;
    noct_enter_vm(env, "main", 0, NULL, &ret);

    // Destroy the runtime.
    noct_destroy_vm(rt);
}
```

This API requires linking against the Noct runtime and including the
appropriate header (`noct/noct.h`).

Error handling and result introspection are left to the host
application, giving full control over integration.

---

## Intermediate Representations

Noct uses two intermediate representations:

- **HIR (High-level Intermediate Representation)**  
  Structured control flow graph (CFG) for flow-sensitive optimizations.

- **LIR (Low-level Intermediate Representation)**  
  VM bytecode, used for execution or code generation.

Their separation enables a lightweight JIT pipeline with a clear,
analyzable architecture.

---

## Test and CI

Noct is tested on Windows, macOS, and Linux.  It also supports
FreeBSD, NetBSD, OpenBSD, and Haiku.

Continuous integration is powered by GitHub Actions.  Each push to the
main branch triggers builds and binary releases, ensuring stability
across supported platforms.

---

## License

Noct is open source, released under the MIT license.

This means you can use it freely — for personal, educational, or
commercial purposes.  You're also free to modify, redistribute, and
build upon it, with minimal restrictions.

---

## Contributing

Noct is under active development, and we welcome all kinds of
contributions — bug fixes, examples, documentation, ideas, or new
features.

We're also building the broader `NoctVM` family, including a game
engine designed to empower creators.

Whether you're here to code, teach, test, or explore — we'd love to
have you with us.

[Join the community on Discord](https://discord.gg/ybHWSqDVEX)
