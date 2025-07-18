NoctLang
========

`Noct` is a lightweight, modern, embeddable programming language
designed for portability, simplicity, and freedom — and for being
*learnable*.

Noct is implemented in standard C89, with no external dependencies,
and can be compiled with almost any ANSI C compiler.  It is free
software, respecting your freedom to run, study, modify, and share it.

Try writing your first Noct program. It might take less time than you
think.

**Status**: Noct is currently in active development. The language
syntax and bytecode design may change before the first stable release.

---

## Why Noct?

_"If a programming language were learnable in a single classroom
session, how would the world change?"_

Noct was born from this question — a desire to create a language that
is both minimal and meaningful.

It is a scripting language designed not just for portability and
performance, but for clarity — so that even a beginner can understand
its core in a single session, and an expert can trust it in a system
where correctness matters.

Noct is not just a tool. It is a statement: that programming languages
can be simple, teachable, and still powerful.

---

## Key Features

- **C-like Syntax** — Familiar to most programmers.
- **Arrays and Dictionaries** — Native, expressive data structures.
- **Lambda Functions** — First-class functions with explicit environments, without closures.
- **Efficient Execution** — A built-in JIT compiler for improved performance.
- **Explicit Environments** — Avoids closures by passing environment explicitly via `with`

---

## Try it!

### Your First Program

Noct is simple enough to try immediately.

Save the following code as `script.noct`:

```
func main() {
    for (i in 0..10) {
        print("Hello, World!");
    }
}
```

### Installation

```
git clone https://github.com/awemorris/NoctLang.git noct
cd noct
mkdir build
cd build
cmake ..
make
```

### Usage

To run a script:

```
./noct script.noct
```

---

## Technical Motivations

To design a scripting language that balances clarity and performance,
Noct sets out with three concrete goals:

1. **Understandability** — Easy to learn for C programmers
2. **Analyzability** — Suitable for AOT/JIT with predictable semantics
3. **Portability** — No runtime dependencies, works in restricted systems

Most existing scripting languages fail at one or more of these goals,
due to design choices that favor dynamic power over static clarity:

- Their syntax and semantics often depart significantly from C,  
  making them unintuitive to systems programmers.
- Many rely on implicit features like closures or dynamic scoping,  
  which complicate reasoning and static analysis.
- Some depend on heavyweight runtimes or require JIT compilation,  
  making them unsuitable for embedded or security-restricted platforms.

Noct addresses these issues with a design that is:

- **Familiar** — C-like syntax, minimal and readable
- **Predictable** — No implicit closures; all environments are passed explicitly
- **Embeddable** — Written in C89, with no dependencies
- **Portable** — Supports both just-in-time (JIT) and ahead-of-time (AOT) compilation

While Noct is primarily designed for JIT execution, it also supports
interpretation and AOT compilation via C translation. This makes it
suitable for platforms where JIT is restricted or disallowed — such
as embedded systems or smartphone app marketplaces.

---

## Philosophy and Intent

The goal of Noct is not novelty, but **clarity and control** —
clarity in what code does, and control over how it is executed.

The language was designed with a practical mindset: to be simple,
analyzable, and embeddable—not as an experiment in language theory,
but as a tool that is easy to reason about, implement, and extend.

Rather than pursuing cutting-edge type systems or advanced language
abstractions, Noct aims to:

- Be **simple enough** to support analysis and optimization of JIT compilation
- Avoid implicit behavior (such as closures)
- Make **data dependencies explicit** for future auto-parallelization
- Enable **ahead-of-time compilation** (AOT) to plain C

This philosophy reflects the author's background as a systems
programmer, not a language theorist. It is a language for system-level
developers who prioritize structure, analyzability, and practical
performance — and for educators who value clarity and teachability.

In addition, Noct is being experimentally used in elementary programming
education to validate whether the language specification is intuitive
and understandable for young learners. Feedback from this experience is
being incorporated into the language design and implementation.

By eliminating implicit state and enforcing explicit data flow, Noct
invites the programmer to think clearly — not just about what code
does, but how and why it does so.

---

## Educational Vision

Noct is not just a scripting language — it is also a teaching tool.
Its syntax was shaped with the intention that it could be introduced,
understood, and used in practical programming within a single
classroom session.

By eliminating implicit behaviors and enforcing explicit structure,
Noct helps learners build correct mental models from the start. It
aims to serve both as a practical tool for systems programming and a
bridge for beginners into the world of structured code.

Noct is not just a language for writing programs —  
it is a language for teaching *what programming is*.

---

## Common Ground

In a world of increasingly complex programming languages, Noct offers
a small, shared space — a language small enough to be learned
quickly, and rich enough to express real-world structure.

It is a language of clarity, not complexity.  A tool of thought, not
just of code.

---

## Examples

Noct scripts consist of functions, expressions, and control structures
similar to C. The `main` function is the entry point.

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

Noct internally uses two distinct intermediate representations (IRs):
**HIR** and **LIR**.

- **HIR (High-level Intermediate Representation)**  
  Represents code structure and control flow.
  It is designed to support future advanced optimizations, including
  SSA-based transformations and flow-sensitive analysis.

- **LIR (Low-level Intermediate Representation)**  
  Represents execution-level instructions.  
  This form corresponds closely to the VM's bytecode, and is used
  directly for interpretation and code generation.

By separating these two layers, Noct enables a lightweight,
template-based JIT code generation pipeline, while keeping the
language architecture analyzable and portable.

---

## Roadmap

The following features are planned or currently under development.
These focus on improving performance, portability, and expressiveness
of the language.

**Language Internals**
- Adding data-structure-related intrinsics.
- Adding more file I/O FFI functions.

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

## Contributing

Noct is currently in active development. We welcome contributions of
all kinds — whether it's fixing bugs, adding examples, improving
documentation, or proposing new features.

Please feel free to open issues or submit pull requests on GitHub.  
Together, we can make Noct better for everyone.

## Join Us

The `NoctVM` family is still in their early stages.  We're building a
game engine with care, hoping it will inspire and empower creators
around the world.

If you're interested in contributing — whether it's code,
documentation, testing, or ideas — we'd be happy to have you with us.

Every small step helps shape what `NoctVM` can become.  You're welcome
to join us on this journey.

[Join us on Discord](https://discord.gg/ybHWSqDVEX)
