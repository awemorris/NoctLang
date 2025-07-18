NoctLang
========

`Noct` is a lightweight, embeddable scripting language — designed for
clarity, control, and above all, *learnability*.

It offers a shared space in a world of increasingly complex languages:
a language small enough to learn in a single session, yet rich enough
to express real-world structure.

Noct is written in standard ANSI C with no external dependencies, and
compiles on virtually any ANSI C compiler.  As open-source software,
it respects your freedom to run, study, modify, and share.

Try it now — launch the REPL or write your first script.  It might
take less time than you think.

**Status**: Actively developed and constantly evolving.

---

## Why Noct?

_"If a programming language could be learned in a single classroom
session, how would the world change?"_

Noct was born from this question — a desire to create a language
that’s minimal, meaningful, and truly accessible.

If learners can grasp a language in under an hour, they can start
building right away.  And when more people start building, more ideas
come to life.  We believe this kind of accessibility can spark a
global wave of creativity — accelerating the evolution of computing,
and opening up its possibilities to everyone.

---

## Key Features


- **C-like Syntax** — Clean and familiar to most programmers.
- **Arrays & Dictionaries** — Built-in, expressive data structures.
- **Lambda Functions** — First-class, with explicit environments.
- **Efficient Execution** — Powered by a lightweight JIT compiler.
- **Translation Backend** — Translates to both C and Emacs Lisp.

---

## Try it!

### Your First Program

Noct is simple enough to try right now — no setup, no hassle.

Just run the `noct` command and type:

```
for (i in 0..10) {
    print("Hello, World!");
}
```

That’s it. You’ve written your first Noct program.

### Installation

## Download Prebuild Binaries

Visit [the release
page](https://github.com/awemorris/NoctLang/releases) to obtain the
latest prebuilt binaries.

## Or Manually Build from Source

Clone the repository, build it with CMake, and you’re ready to go:

```
git clone https://github.com/awemorris/NoctLang.git noct
cd noct
cmake -B build .
cmake --build build
./build/noct
```

## Run

To run a script:

```
./noct script.noct
```

---

## Technical Motivations


Noct was designed to strike a balance between clarity and performance,
with three core goals:

1. **Understandability** — Easy to learn and reason about
2. **Analyzability** — Friendly to AOT/JIT with predictable semantics
3. **Portability** — No dependencies, runs anywhere C can

Most scripting languages fall short on at least one of these.  They
drift from C-style semantics, rely on hidden state, or demand heavy
runtimes.

Noct stays lean and explicit:

- **C-like and Minimal** — Syntax that feels familiar and readable
- **No Hidden State** — Closures are explicit, not magical
- **Pure ANSI C** — Embeddable and dependency-free
- **Flexible Execution** — JIT by default, AOT when needed

Whether you’re targeting embedded systems or locked-down
environments, Noct’s design keeps you in control — without giving up
power.

---

## Philosophy and Intent

Noct isn’t a showcase of novelty — it’s a reinvention through
rethinking.  A language distilled from decades of experience, reshaped
for clarity and control.

It reexamines familiar ideas across languages, and rebuilds them with
a fresh, structural lens — favoring simplicity, explicitness, and
analyzability.

This isn’t minimalism as limitation, but as intent: a belief that
precision beats complexity.

Noct stands for the idea that rethinking is invention — and that real
progress begins by rediscovering what truly matters.  This reflects
the author’s systems programming background.

---

## Educational Vision

Even young learners have used Noct successfully in classroom trials.

Its syntax was designed to be introduced, understood, and applied
within a single classroom session — even by complete beginners.

By removing implicit behaviors and enforcing explicit structure, Noct
helps learners form accurate mental models from day one.

It serves both as a practical system language and as a clear,
approachable path into the world of structured programming.

Noct doesn't just teach you how to write code — it teaches what code
*is*.

---

## Examples

Noct scripts consist of functions, expressions, and control structures
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
    var f = lambda (x) => { return x + 1; }
    print(f(1));

    // No closures. Use the 'with' argument explicitly.
    var g = lambda (x, with) => {
        return x + with.y;
    };
    var y = 2;
    var z = g(1, {y: y});
}
```

### Objective Notation

This example demonstrates how Noct can express object-like structures
using method-style lambdas with explicit `this`, without relying on
implicit closures.

Note: In this case, `this` refers to the object literal returned by
`new_apple()`.

```
func main() {
    var apple = new_apple();
    print(apple->getName());
    print(apple->getPrice());
}

func new_apple() {
   return {
      getName: lambda (this) => {
          return "Apple";
      },
      getPrice: lambda (this) => {
          return 100;
      }
   };
}
```

---

## Embedding API

The Noct runtime can be embedded in C applications. This allows you to
load, compile, and execute scripts directly within your software.

```
void call_noct(const char *file_name, const char *file_text)
{
    /* Create a runtime. */
    struct rt_env rt;
    rt_create(&rt);

    /* Compile source. */
    rt_register_source(rt, file_name, file_text);

    /* Call the main() function. */
    struct rt_value ret;
    rt_call_with_name(rt, "main", NULL, 0, NULL, &ret);

    /* Destroy the runtime. */
    rt_destroy(rt);
}
```

This API requires linking against the Noct runtime and including the
appropriate header (`noct/runtime.h`).

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

## Roadmap

Ongoing work focuses on performance, portability, and expressiveness.

**Language Internals**
- Binary operations
- File I/O FFI extensions

**JIT Architecture Support**

**Status of JIT Support for Architectures**

|Architecture    |Status     |
|----------------|-----------|
|x86 32bit       |OK         |
|x86 64bit       |OK         |
|Arm 32bit       |OK         |
|Arm 64bit       |OK         |
|MIPS 32bit      |OK         |
|MIPS 64bit      |OK         |
|PowerPC 32bit   |OK         |
|PowerPC 64bit   |OK         |
|RISC-V 32bit    |Soon       |
|RISC-V 64bit    |Soon       |
|SPARC 32bit     |Soon       |
|SPARC 64bit     |Soon       |
|s390x           |Planned    |

---

## IDE

[NoctScript IDE](https://github.com/awemorris/NoctScriptIDE) is a
lightweight editor for writing and running Noct scripts. It supports
syntax highlighting, REPL integration, and runs on Windows, macOS, and
Linux.

Designed for classroom use, it works out of the box — no installation
needed, just unzip and start coding.

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
commercial purposes.  You’re also free to modify, redistribute, and
build upon it, with minimal restrictions.

---

## Contributing

Noct is under active development, and we welcome all kinds of
contributions — bug fixes, examples, documentation, ideas, or new
features.

We're also building the broader `NoctVM` family, including a game
engine designed to empower creators.

Whether you're here to code, teach, test, or explore — we’d love to
have you with us.

[Join the community on Discord](https://discord.gg/ybHWSqDVEX)
