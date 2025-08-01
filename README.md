NoctLang
========

NoctLang is a fast, powerful, yet tiny scripting language runtime that
combines:

- Baseline JIT
- Generational GC (Semi-Space Copy + Mark-Sweep-Compact)
- Easy-to-use FFI
- Only 118 KB on x86_64

Written entirely in ANSI C, NoctLang builds and runs reliably on
virtually any platform — making it easy to integrate into your own
games and embedded projects.

Try it now — launch the REPL or write your first script.  It might
take less time than you think.

**Status**: Actively developed and constantly evolving.

---

## Why Noct?

_"If a programming language could be learned in a single classroom
session, how would the world change?"_

_"What if that very first language could also power fun game
programming — how many more children would smile?"_

_"And if it could also become a tool to earn a living, what kind of
future would that create?"_

Noct was born from these questions: a desire to create a language
that’s minimal yet meaningful, accessible yet powerful — a tool
simple enough for learning, but strong enough to build real games and
professional applications.  It bridges the gap between **play** and
**production**, between **education** and **industry**.

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

Noct is designed to strike a balance between **simplicity** and
**performance**, with five core goals:

1. **Learnability** — Easy to pick up and reason about, even for beginners
2. **High Speed** —  JIT execution, competitive with Java VMs
3. **Robustness** — Generational GC (copy + mark-sweep-compact)
4. **Portability** — Pure ANSI C implementation, with no dependencies
5. **Small Footprint** — Complete runtime with JIT and GC in under 256 KB

While most scripting languages fall short on at least one of these,
Noct stays lean — delivering **modern GC and JIT techniques** in a
runtime small enough for games, embedded systems, and teaching tools.

---

## Philosophy and Intent

Noct brings commercial-grade VM technology — once reserved for
large-scale industrial runtimes — into the open-source world.  Its
design doesn’t rely on experimental tricks, but on battle-tested
techniques proven in modern language engines like Java, .NET, and
LuaJIT.

With Noct, the strengths of commercial runtimes are now available to
every developer — without cost, without heavy dependencies, and
without compromise.

---

## Educational Vision

Noct proves that industrial-grade technology can also be easy to
learn.  Its syntax and semantics were intentionally designed so that
even complete beginners can grasp the core concepts in a single
classroom session — yet the runtime itself is powerful enough for
professional applications.

By removing hidden behaviors and enforcing explicit structure, Noct
helps learners build accurate mental models from day one. The same
qualities that make it clear for education — predictable control flow
— also make it reliable for games and embedded systems.

This dual nature means Noct is more than just a teaching tool: it
provides an approachable first step into programming, while remaining
a practical language for real-world software development.

Noct doesn’t just teach how to write code — it teaches how code works.

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
      name: "Apple",
      price: 100,
      getName: (this) => { return this.name; },
      getPrice: (this) => { return this.price; }
   };
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

## Roadmap

Ongoing work focuses on performance, portability, and expressiveness.

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
