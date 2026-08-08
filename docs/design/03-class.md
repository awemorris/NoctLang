# 03 — Frozen Class Dictionaries and Top-Level Declarations

Status: **implemented** (2026-08-09). Decision Log: D11–D13.

## 1. Current state of the code (verified facts)

* `class { ... }` is **pure syntax sugar for a dict literal**:
  lexer.l:386 returns `TOKEN_CLASS`; parser.y:672–683 reduces
  `TOKEN_CLASS TOKEN_LBLK kv_list TOKEN_RBLK` straight to
  `ast_accept_dict_expr($3)` ("class is equal to dict"). There is no
  runtime notion of a class today.
* `extend` is lexed as the **same token as `new`** (lexer.l:379,
  "extend is equal to new") — the parser cannot tell them apart.
* `new Sym { ... }` (`AST_EXPR_NEW`; the class operand is restricted
  to a single `TOKEN_SYMBOL`) compiles to a call of the intrinsic
  **`Dict.merge(class_dict, overlay_dict)`** (`lir_visit_new_expr`,
  src/core/lir.c:1756: OP_LOADSYMBOL "Dict.merge" + OP_CALL).
  `rt_intrin_Dict_merge` → `rt_merge_dict` → `om_merge_dict`.
* **The merge is shallow**: `om_merge_dict` copies `rt_value` slots by
  assignment (objectmodel-st.c:1021–1024) — reference copies for heap
  values. (Per D12 this stays the official semantics.)
* `struct rt_dict` (runtime.h:101–138) has **no flags field**.
* All script-visible dict mutation funnels into exactly two functions
  per object-model variant: `om_write_dict` and `om_erase_dict_entry`
  (ST: objectmodel-st.c:571 / 808; MT: objectmodel-mt.c:2956 / ~3273).
  `om_write_dict_with_hash` delegates to `om_write_dict` in both.
* GC/objectmodel paths that create a *new* struct for the *same
  logical dict* — these copy an **explicit field list** (a new flag
  will NOT survive them unless added at each site):
  - `rt_gc_copy_dict_to_graduate` (gc.c:1906)
  - `rt_gc_promote_dict` (gc.c:1755)
  - `expand_dict` (objectmodel-st.c:702 and objectmodel-mt.c:1962)
  - (tenure compaction memmoves the whole struct — implicit carry)
  - dict alloc initializers set every field explicitly
    (gc.c ~655/~738/~818) — must initialize the new flag to 0.
* Top level of a file admits **only `func` definitions**
  (parser.y:225–246). No top-level statements exist.
* Compiled functions are registered as **globals of type FUNC** via
  `rt_set_global` (rt_register_lir, runtime.c:451–473). Duplicate
  names **silently overwrite** (runtime.c:2191–2201).
* Synthesized-name precedent: lambdas become functions named
  `"$anon.%s.%d"` (file, counter) — `hir_defer_anon_func`,
  hir.c:2067–2093. `$` cannot appear in user identifiers
  (lexer identifier class is `[a-zA-Z_0-9]+`), so `$`-names are
  collision-proof.
* Globals: `struct rt_bindglobal { char *name; uint32_t name_len,
  name_hash; struct rt_value val; bool is_removed; }` (runtime.h:193).
  `rt_expand_global` rehash-copies only name/len/hash/val — a new
  field must be added to that copy. The undefined-global error is
  `Symbol "%s" not found.` (runtime.c:2123).
* remacs loads editor files **one `noct_register_source` call per
  file** (apps/remacs/src/main.c:135–155), but the NB bundle
  (`apps/remacs/tools/build-nb.sh`) **concatenates all sources into one
  file**
  compiled as a single unit. Any per-file mechanism must behave
  sensibly in both modes.

## 2. Feature A — frozen class dicts (D11, D12)

### 2.1 Semantics

* The dict produced by a `class {...}` expression is **frozen**.
* The dict produced by `extend Base {...}` is **frozen** (a class
  template derived from another).
* The dict produced by `new Cls {...}` is **mutable** (instance).
* Writing to or removing from a frozen dict raises
  `N_TR("Dictionary is frozen.")`.
* Reads, iteration, `Dict.merge` (as source), `Dict.copy` are
  unaffected. **`Dict.copy` of a frozen dict yields a mutable copy**
  (the documented escape hatch).
* Shallow-sharing caveat is documented, not prevented (D12): a mutable
  container stored in a class template is shared by every instance.
  Recommended convention (goes into docs/syntax.md and README):
  *containers belong in the constructor* — assign fresh arrays/dicts
  in the `new` overlay or an init method, keep class templates to
  scalars and functions.

### 2.2 Implementation

**No new opcodes.** Freezing rides on the intrinsic call machinery the
`new` lowering already uses.

1. `struct rt_dict`: add `bool is_frozen;` (next to
   `native_finalizer`; both ST and MT builds).
2. New public intrinsic `Dict.freeze(d)` → sets `is_frozen = 1`,
   returns `d`. Register in the `intrin_items[]` table
   (src/core/intrinsics.c) like `Dict.merge`. (Public on purpose —
   scripts may freeze their own dicts; it also keeps the compiler
   lowering trivial.)
3. Lexer: give `extend` its own token `TOKEN_EXTEND` (edit the
   lexer.l:379 rule; regenerate — see 02-typing §3 for the exact
   flex/bison commands).
4. Parser/AST:
   - `class` literal: reduce to a new `AST_EXPR_CLASS` node wrapping
     the kv_list (or reuse the dict node plus an `is_class` flag —
     implementer's choice; flag is less code).
   - `extend Sym { ... }`: parse like `new` but produce
     `AST_EXPR_NEW` with a new `bool is_extend` flag in the `new_`
     payload.
5. HIR lowering:
   - class literal → `HIR_EXPR_CALL` of symbol `"Dict.freeze"` with
     the dict literal as the argument;
   - extend → `Dict.freeze(Dict.merge(Base, overlay))` (nest the
     existing new-lowering inside a freeze call);
   - `new` → unchanged.
   All expressible with existing HIR node kinds — elback/scmback keep
   working; they will translate the freeze as an ordinary call.
   **elback note:** with a flat symbol-term callee, elback emits the
   call verbatim as `(Dict.freeze arg)` — the ns-map CANNOT intercept
   it (`elback_ns_lookup` only matches dot-shaped callees). So the
   shim route is the one that works: add an identity
   `(defun Dict.freeze (d) d)` to the **generator** `gen_shim` in
   apps/remacs/tools/gen-napi.py (noct-shim.el is a generated file —
   never edit it directly). Dots are legal in elisp symbols.
6. Enforcement — add at the **top** of, in BOTH object models:
   - `om_write_dict` (objectmodel-st.c:571, objectmodel-mt.c:2956)
   - `om_erase_dict_entry` (objectmodel-st.c:808, objectmodel-mt.c:~3273)
   ```c
   if (dict->is_frozen) {
           rt_error(env, N_TR("Dictionary is frozen."));
           return false;
   }
   ```
   MT caveat: compiler-emitted freezes happen before a dict can be
   shared, so an unlocked read is fine for those. But `Dict.freeze`
   is public — a script may freeze an already-shared dict while
   another thread writes. Implement the freeze itself through a
   per-object-model `om_freeze_dict(env, dict)` (ST: plain store; MT:
   take the dict write lock around the store) so the mt-tsan build
   stays clean; the unlocked read at the write gate is then a benign
   race (a racing writer may complete before the freeze becomes
   visible — documented behavior).
7. Flag preservation — add `is_frozen` copying to **every** site in
   §1's explicit-copy list: `rt_gc_copy_dict_to_graduate`,
   `rt_gc_promote_dict`, both `expand_dict`s; add `is_frozen = false`
   to the three dict alloc initializers. `om_copy_dict` and
   `om_merge_dict` intentionally do **not** copy it (copies/instances
   are mutable). Grep check before finishing:
   `grep -n "native_finalizer" src/core/gc.c src/core/objectmodel-*.c`
   — every hit that *copies* the field is a site that must also copy
   `is_frozen` (same field-carry set), every hit that *initializes* it
   must initialize `is_frozen`.
8. Note: `expand_dict` runs on insertion into a full dict; a frozen
   dict rejects insertion at the gate, so expansion of a frozen dict
   is unreachable — carry the flag anyway (defense in depth).

## 3. Feature B — top-level declarations via load-time init (D13)

### 3.1 Surface syntax (new top-level forms)

```
class Person {            /* sugar for: let Person = class { ... };   */
    name: ""
}

var Counter = 0;          /* mutable global, created at load          */
let Version = "1.0";      /* immutable global binding                 */
let Config: dict = ...;   /* annotations compose (02-typing)          */
```

* Evaluation order: source order within a compilation unit; units in
  load order. A forward reference at load time hits the existing
  runtime error `Symbol "%s" not found.` — this is the intended
  behavior (no new machinery).
* Plain top-level *statements* (e.g. `print(1);`) remain **illegal** —
  only `class`, `var`, `let` declarations and `func` definitions are
  allowed at top level. (Keeps the grammar unambiguous and the file
  layout declarative.)

### 3.2 Mechanism — the synthesized init function

* The parser collects top-level declarations, **in order**, into the
  statement list of a synthesized function named
  `"$init.<file_name>"` appended to the unit's function list (exact
  precedent: `$anon.<file>.<n>`; the name derives from the path
  passed to the compiler, so a bundle compiled from
  `build-nb/remacs-all.noct` gets that path in its name — fine,
  nothing may hardcode it). If a file has no top-level declarations,
  no init function is synthesized.
* **Lowering rule (important):** inside `$init.<file>`, top-level
  `var X = E;` and `let X = E;` are synthesized as **plain
  assignments** (`is_var`/`is_let` stripped) so D18's
  undeclared-assignment-creates-global rule applies — keeping the
  flags would create dead block-scoped LOCALS of `$init` instead of
  globals once 04-scoping lands. The let-ness is expressed solely by
  the appended `Global.markConst("X")` call (§3.3). Top-level
  `class Person {...}` desugars to
  `Person = Dict.freeze({...}); Global.markConst("Person");`.
* Type annotations on top-level `var`/`let` parse and are
  **discarded** in v1 (grammar uniformity only; no storage, no
  checks).
* AOT/`--ansic` support for top-level declarations is **out of scope
  for v1** (the cfunc registration path has no `$init` hook; document
  the limitation in the C-backend docs).
* **Auto-execution hook:** at the end of `rt_register_source`
  (runtime.c, after the per-function registration loop succeeds and
  before cleanup) and at the end of `rt_register_bytecode`
  (runtime.c:482–543): if a function whose name starts with
  `"$init."` was just registered in this unit, call it via
  `rt_call_with_name` (0 args) and propagate failure. Track "just
  registered in this unit" locally (remember the name while looping)
  — do NOT scan the global table (older `$init.*` from other files
  must not re-run). Two bytecode-path specifics: (a) the loop at
  runtime.c:526–531 currently sets success even when a function
  fails to register mid-loop (unlike `rt_register_source`, which
  checks `i != func_count`) — guard the `$init` call on the loop
  having completed (and fix the success flag while there); (b)
  function names are parsed inside `rt_register_bytecode_function`,
  so add an out-parameter to surface each registered name to the
  outer loop.
* Both load modes behave correctly by construction:
  - per-file loading (remacs main.c): each file's `$init.<file>` runs
    at that file's registration → file order;
  - concatenated NB bundle (build-nb.sh cats everything into
    `remacs-all.noct`): the parser sees ONE unit and synthesizes ONE
    `$init.remacs-all.noct` holding all declarations in concatenation
    order — same relative order, one run.
  Name collisions cannot happen (unit file names differ; and even a
  duplicate would have already run before being overwritten).
* bcback: nothing special — the synthesized function is a function;
  it lands in the .nb like any other, and the bytecode loader's hook
  runs it.
* elback/scmback: they translate the synthesized function like any
  function. The Emacs oracle harness may call it explicitly if a test
  needs top-level state; not required for v1.

### 3.3 `let` globals — const bindings

* `struct rt_bindglobal` gains `bool is_const;`.
  - `rt_set_global_with_hash` (runtime.c:2151): on overwrite of an
    entry with `is_const` → `rt_error(env, N_TR("Cannot assign to
    constant \"%s\"."), name)`. On fresh insert, `is_const = false`.
  - `rt_expand_global` must carry `is_const` (it currently copies only
    name/len/hash/val — add the field).
* Marking mechanism: new intrinsic `Global.markConst(name)` (register
  next to the existing `Global.isSet`/`Global.get` in intrinsics.c).
  The compiler lowers a top-level `let X = E;` inside `$init.<file>`
  to:
  ```
  X = E;                     /* global store, existing semantics */
  Global.markConst("X");
  ```
  Semi-public by design (scripts could const their own globals);
  document it in docs/library.md.
* Function registration uses `rt_set_global` too — a `let X` followed
  by loading a `func X` errors at load ("Cannot assign to constant") —
  correct and desired (declaration collision).
* `let` **locals** are enforced at compile time by 04-scoping; no
  runtime flag involved there.

### 3.4 Grammar changes

parser.y start symbol area (225–246): generalize

```
unit_item : func
          | class_def          /* TOKEN_CLASS TOKEN_SYMBOL TOKEN_LBLK kv_list TOKEN_RBLK */
          | toplevel_var       /* TOKEN_VAR    symbol [: type] = expr ;                  */
          | toplevel_let       /* TOKEN_LET    symbol [: type] = expr ;                  */
unit_list : unit_item | unit_list unit_item ;
```

Note top-level `class Person { ... }` needs `TOKEN_CLASS
TOKEN_SYMBOL TOKEN_LBLK` while the *expression* form is `TOKEN_CLASS
TOKEN_LBLK` — one token of lookahead distinguishes them; if bison
reports a conflict, split with a fused-token lexer rule the way
`TOKEN_ELSE_LBLK` already does (`"class"[ \t\r\n]+"{"` → the
expression form). The AST accepts these into a per-unit init statement
list; `ast_build`/`hir_build` synthesize the `$init.<file>` function
from it (mirror `hir_defer_anon_func`'s queueing pattern).

## 4. What NOT to do

* No deep freeze, no freeze-on-read barriers, no copy-on-write.
* No method/`self` sugar changes; `->` this-call is untouched.
* No class identity/type-checking (`instanceof` etc.).
* No cross-unit dependency resolution — load order is the contract.
* Do not make `new` of a frozen template copy the frozen bit.

## 5. Testing plan

`tests/class/` (golden-diff pattern):

1. `freeze_basic.noct` — write to a class dict → error message
   `Dictionary is frozen.`; write to an instance → OK.
2. `freeze_extend.noct` — extend result frozen; base unaffected;
   instance of subclass mutable.
3. `freeze_gc.noct` — create class dicts, force GC churn (allocate in
   a loop), then verify frozenness still enforced (exercises the
   young-copy/promote flag carry). Also run under `--force-jit`.
4. `freeze_copy.noct` — `Dict.copy(frozenClass)` is mutable;
   `Dict.freeze` intrinsic works standalone.
5. `toplevel_basic.noct` — top-level `class`/`var`/`let`; `main` uses
   them; correct values.
6. `toplevel_order.noct` — later decl referencing an earlier one
   works; forward reference errors with `Symbol "..." not found.`
7. `toplevel_let.noct` — assignment to a `let` global from `main` →
   `Cannot assign to constant "X".`
8. `toplevel_nb.noct` — compile with `--compile`, run the `.nb`,
   verify init ran (exercises the bytecode-path hook).
9. MT build (`mt-debug` preset): run 1–8 plus `tests/run-thread.sh`
   (frozen check sits on the MT dict write path — must not deadlock or
   race; the flag is written pre-publication).
10. remacs full suite — unchanged behavior (remacs uses `class` in
    docs/examples only; grep first: if any remacs source mutates a
    class template, that is a migration item — fix the remacs source,
    per D19).

## 6. Acceptance criteria

* All new + existing suites green on ST and MT builds; three
  cross-target builds compile.
* `grep`-audit shows `is_frozen`/`is_const` handled at every site
  listed in §2.2(7) and §3.3.
* elback oracle suite (`apps/remacs/tests/run-oracle.sh` and
  `tests/run-elisp.sh`) still green (freeze lowers to a call the
  oracle shims as identity).
* No commits.
