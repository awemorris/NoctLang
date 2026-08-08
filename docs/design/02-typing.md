# 02 — Type Annotations as Optimization Hints

Status: **implemented** (2026-08-09). Decision Log: D8–D10.

## 1. Semantics in one paragraph

Annotations never change what a correct program computes. They are
inert below `--optimize-level=2`. At level ≥ 2, every **annotated
function parameter** gets a one-instruction type check at function
entry; a violation is a runtime error. That entry check is what makes
it sound for the optimizer to *believe* the annotation inside the
body. Annotated or inferred locals additionally enable typed-arithmetic
lowering in the provable cases (§6, deferred to v2 but specified here).

## 2. Surface syntax

```
func foo(name: string, n: int) { ... }     /* annotated params        */
var s: string = "abc";                     /* annotated local         */
var s: string;                             /* annotated, no init      */
var s = "abc";                             /* unannotated (inferred)  */
let k: int = 3;                            /* works with let (04-)    */
```

* Return-type annotations are **out of scope** (no `func f(): int`).
* `var s;` (bare, no annotation, no initializer) remains a syntax
  error, as today. The annotated-uninitialized form is newly legal and
  initializes the local to integer 0 (the natural zero-cleared value;
  note this is *not* checked against the annotation — the annotation
  is a hint about future assignments, not a constructor).
* Annotations are allowed on `var`/`let` locals and on parameters
  only. Nowhere else (no dict keys, no for-counters).

### Type name vocabulary (D10)

| Name | Runtime tag checked | Notes |
|---|---|---|
| `int` | `NOCT_VALUE_INT` | ≡ `i32` |
| `long` | `NOCT_VALUE_LONG` | ≡ `i64` |
| `float` | `NOCT_VALUE_FLOAT` | |
| `double` | `NOCT_VALUE_DOUBLE` | |
| `string` | `NOCT_VALUE_STRING` | |
| `array` | `NOCT_VALUE_ARRAY` | |
| `dict` | `NOCT_VALUE_DICT` | |
| `packed` | `NOCT_VALUE_PACKED` | element type NOT checked in v1 |
| `func` | `NOCT_VALUE_FUNC` | |
| `i8 i16 i32 u8 u16 u32` | `NOCT_VALUE_INT` | width/sign are metadata only |
| `i64 u64` | `NOCT_VALUE_LONG` | width/sign are metadata only |

Sized names exist so future Packed accessors and typed arithmetic can
consume the width; **their runtime check degrades to the storage-class
tag** (int or long). Document this in docs/syntax.md when
implementing.

An **unknown type name is a compile-time error** ("Unknown type name
'%s'.") — typo protection is the one place annotations are strict.

## 3. Grammar and AST changes

Facts: the lexer/parser are flex/bison with **generated files checked
in** (`src/core/lexer.yy.c`, `src/core/parser.tab.c`); the CMake
FLEX/BISON blocks are commented out, so regeneration is manual:

```
cd src/core
flex --prefix=ast_yy -o lexer.yy.c lexer.l
bison -p ast_yy -d -o parser.tab.c parser.y
```

(Confirm the exact prefixes against the commented block in
CMakeLists.txt ~lines 100–122 before running; commit-ready generated
files are part of the change.)

* `TOKEN_COLON` already exists (used in dict literals). Type names lex
  as ordinary `TOKEN_SYMBOL`s **except `func`, which is the keyword
  token `TOKEN_FUNC`** (lexer.l:367). Introduce a tiny nonterminal so
  the `func` type name parses:
  ```
  type_name : TOKEN_SYMBOL            /* yields the symbol string  */
            | TOKEN_FUNC              /* yields the string "func"  */
  ```
* Parameters (parser.y:247–257): extend
  ```
  param_list : TOKEN_SYMBOL
             | TOKEN_SYMBOL TOKEN_COLON type_name
             | param_list TOKEN_COMMA TOKEN_SYMBOL
             | param_list TOKEN_COMMA TOKEN_SYMBOL TOKEN_COLON type_name
  ```
  `struct ast_param` (ast.h:114) gains `char *type_name;` (NULL when
  absent). `ast_accept_param_list` gains the extra argument.
* var statement (parser.y:360–370): add productions
  ```
  | TOKEN_VAR expr TOKEN_COLON type_name TOKEN_ASSIGN expr TOKEN_SEMICOLON
  | TOKEN_VAR expr TOKEN_COLON type_name TOKEN_SEMICOLON
  ```
  The assign payload (`ast.h` `assign` struct with `bool is_var`)
  gains `char *type_name;` and the second production synthesizes
  RHS = integer constant 0. (When 04-scoping adds `let`, mirror only
  the initializer-bearing productions — `let x: T;` stays a syntax
  error per D14.)
* No dict-literal ambiguity: the `:` productions above appear only
  after `TOKEN_VAR`/`TOKEN_LET`/inside parameter lists, never in
  expression position.

## 4. Compile-time plumbing

* HIR (as implemented): the FUNC block gains
  `int param_type[HIR_PARAM_SIZE]` (tag or -1), resolved in
  `hir_visit_param_list`. `var`/`let` annotations are **validated and
  then discarded** in v1 (`hir_check_type_annotation` in
  `hir_visit_assign_stmt`) — per-local storage (`hir_local
  value_type/size_hint`) is deferred to v2 alongside the typed
  arithmetic that would consume it.
* The type-name → (tag, width) resolver is one static table + lookup
  function in hir.c; unknown name → compile error with the exact
  message in §2. **Caution:** `hir_fatal(int line, const char *msg)`
  is NOT printf-style (unlike the variadic `lir_fatal`) — snprintf the
  type name into a local buffer first, then pass the formatted string.
* `struct rt_func` (runtime.h) gains
  `int param_type[NOCT_ARG_MAX];` (tag or -1), populated through
  `lir_func` (lir.h — add `int param_type[LIR_PARAM_SIZE];`) and
  `rt_register_lir`.
  **-1 is NOT the zero value.** Construction sites memset(0) or build
  field-by-field (`rt_register_lir` runtime.c:420,
  `rt_register_bytecode_function`'s stack lfunc runtime.c:559,
  `lir_build` lir.c:209–254), and `0 == NOCT_VALUE_INT` — relying on
  zero-init would silently mark every unannotated param as
  "annotated int" and fire spurious entry checks at level 2. Every
  construction site must explicitly fill `param_type[i] = -1` first.
* Bytecode file format: `rt_register_bytecode_function`
  (runtime.c:547–655) is a **strict line-sequence parser** (strcmp on
  each header line; no skip-unknown tolerance). Implement the optional
  `Parameter Types` line as an explicit peek: after the param names,
  read one line; if it equals `Parameter Types`, parse the tags and
  then read the `Temporary Size` header; otherwise treat the line just
  read as the `Temporary Size` header. Absent line = all -1.
  (New-format .nb files will not load on older runtimes; that
  direction is accepted.)

## 5. Entry checks (the only v1 runtime behavior)

At `--optimize-level >= 2`, `lir_build` emits, before the function
body's first instruction, for each param `k` with `param_type[k] != -1`:

```
OP_CHECKTYPE  param_slot:u16, tag:u8
```

New opcode (append after the ABCE block in `enum bytecode`):

```c
OP_CHECKTYPE,  /* 0x33: error unless typeof(tmpvar) == imm8 */
```

Helper `noct_ex_checktype_helper(env, slot, tag)`:
* match → true;
* **widening leniency**: an `int`-tagged value passes a `long`
  annotation, and a `float`-tagged value passes a `double` annotation
  (numeric literals are int/float-tagged; rejecting them would make
  `long`/`double` params unusable in practice);
* mismatch → `rt_error(env, N_TR("%s(): argument type mismatch (expected %s)."), func_name, type_name)` → false (normal error path).
* `type_name` is the **canonical storage-class name derived from the
  tag** ("int", "long", "float", "double", "string", "array", "dict",
  "packed", "func") — never the sized alias (only the tag survives
  into the imm8, and `u8`→int etc. is many-to-one). `func_name` comes
  from the currently executing function: `env->frame->func->name`
  (verify the field path in `struct rt_frame` before use). The golden
  test asserts this exact string.

Params are in tmpvar slots `0..param_count-1` (facts: args are copied
positionally in `rt_call`; slot 0 doubles as the return slot — checks
run before any body code touches it, so that is fine).

Backend work: interpreter case + helper + `lir_dump` + cback case +
10 JIT helper-call cases — identical mechanics to the ABCE opcodes
(01-abce §5). elback/scmback consume HIR and never see it; do **not**
surface annotations there (elback output must stay oracle-compatible).

Level < 2: `lir_build` emits nothing — annotations parse, store, and
have zero runtime footprint.

## 6. v2 (specified now, implement later — separate task)

Do NOT start this until v1 is merged and green.

* **Provable-int locals:** a local is int-proven when (a) annotated
  `int`/`i8..u32` on a parameter (entry check makes it sound), or (b)
  its every assignment in the function is an int-pure expression
  (int constants, int-proven locals, arithmetic thereon — same
  int-purity walk as ABCE E6, function-wide).
* **Typed arithmetic opcodes:** `OP_IADD, OP_ISUB, OP_IMUL, OP_IDIV,
  OP_IMOD, OP_ILT, OP_ILTE, OP_IGT, OP_IGTE` — "assume operands are
  ints" (precedent: `OP_INC`, `OP_EQI` already assume ints). Emitted
  by `lir_visit_binary_expr` when both operands are int-proven and
  level ≥ 2. Interpreter/JIT implement them without the type dispatch
  of `ex_add_helper` (x86_64: inline add of the `.val.i` fields).
* Overflow semantics must match the generic ops exactly — before
  implementing, read `ex_add_helper` and replicate its int overflow
  behavior (whatever it is: wraparound or promotion). Behavioral
  identity between levels is the acceptance bar, enforced by running
  the whole `tests/syntax` suite at level 2.
* Type feedback / hot-path re-specialization (JIT profiling) is
  explicitly future work beyond v2 (owner: "future work").

## 7. Testing plan

`tests/typing/` in the golden-diff style, each case run at level 0 and
level 2 (+ jit/no-jit):

1. `anno_ok.noct` — annotated params called correctly; identical
   output at both levels.
2. `anno_violate.noct` — `func f(s: string)` called with an int:
   level 0 prints the program's normal (unchecked) result; level 2
   errors with the exact §5 message. **Two golden files** — this is
   the one intentional behavioral difference; encode it as two
   expected outputs in the runner (`anno_violate.noct.out0`,
   `anno_violate.noct.out2`).
3. `anno_var.noct` — `var x: string = ...`, `var y: int;` (zero
   init), sized names `u8`/`i64` accepted.
4. `anno_unknown.noct` — `var x: strnig = 1;` → compile error message.
5. `anno_let.noct` — only after 04-scoping lands.
6. Regression: full `tests/syntax` at level 2 (annotations absent —
   proves the gate itself changes nothing).
7. remacs suite green (remacs sources are unannotated; nothing may
   change).

## 8. Acceptance criteria

* Annotations are zero-cost and invisible at levels 0/1 (byte-identical
  bytecode except for the stored param_type metadata).
* Exactly one behavioral delta at level 2: the documented entry-check
  error. Everything else byte-identical across levels.
* Cross-target builds green; generated lexer/parser files regenerated
  and consistent; no commits.
