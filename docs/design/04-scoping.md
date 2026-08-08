# 04 — `let`, Block Scoping, Redeclaration Errors, Static TDZ

Status: **implemented** (2026-08-09; remacs required zero migration
edits). Decision Log: D14–D19. **These are accepted breaking
changes; there is no compatibility flag (D19).** remacs is migrated by
editing its sources.

## 1. Current state of the code (verified facts)

* There is no scope structure anywhere. `var x = e;` is an assignment
  statement with `bool is_var` (no dedicated AST node); `var x;`
  without initializer is a **syntax error** today.
* Locals live in a **flat per-function list** `struct hir_local
  { char *symbol; int index; struct hir_local *next; }` hanging off
  the FUNC block (`val.func.local`). `hir_add_local` (hir.c:736–777)
  walks `parent` up to the FUNC block and appends; **a duplicate name
  silently returns the existing slot** (hir.c:750–759) — that is the
  whole reason `var x; ... var x;` is accepted today.
* Slot indices are assigned at declaration in HIR (`local->index` =
  pre-insertion list length — the list itself is **prepended**, so it
  runs newest-first while slot numbers follow declaration order);
  parameters are registered first so param i = slot i; slot 0 doubles
  as the return slot (`"$return"`). LIR resolves names by `strcmp`
  over the same flat list at **three sites**: `lir_visit_symbol_term`
  (lir.c:1871; miss → `OP_LOADSYMBOL` global fallback),
  `lir_check_lhs_local` (lir.c:1086; miss → `OP_STORESYMBOL` at
  lir.c:1017), and `lir_get_local_index` (lir.c:897; loop symbols
  only — **asserts on a miss**, no fallback).
* For-loop counter/key/value symbols are registered as locals
  implicitly at loop visit (hir.c:1220/1229/1238), no `var` needed.
* Lambdas cannot capture enclosing locals: they are deferred and
  compiled as independent functions named `$anon.<file>.<n>`
  (hir.c:1870–1911, 2067–2093); names that miss the lambda's own
  locals become global lookups.
* `$` cannot appear in user identifiers (`[a-zA-Z_0-9]+`), so
  `$`-prefixed internal names are collision-proof.
* Assignment to an undeclared name creates a **global** at runtime;
  reading an undefined global raises `Symbol "%s" not found.`
  (This rule is load-bearing: remacs initializes globals like
  `SkkDict = 0;` inside init functions. **It stays unchanged**, D18.)
* Generated lexer/parser files are checked in; regeneration is manual
  (see 02-typing §3 for commands).
* Budget: `TMPVAR_MAX` = 128 tmpvar slots per function (locals +
  scratch). Block scoping with unique slots per declaration inflates
  local counts — see §5 migration step M3.

## 2. Specification

### 2.1 `let` (D14)

* `let x = e;` declares a block-scoped local whose **binding** is
  immutable (the value is not frozen).
* `let` **requires an initializer** — `let x;` is a syntax error.
  Annotated form `let x: T = e;` is allowed (02-typing).
* Any later assignment to `x` in its scope is a **compile error**:
  `"Cannot assign to 'let' variable '%s'."` Enforced in HIR when an
  assignment's LHS resolves to a let-local. (`x[0] = 1` and `x.f = 1`
  on a let-bound container are allowed — binding-only immutability.)
* Top-level `let` is a const **global** — that variant is specified in
  03-class §3.3 and enforced at runtime, not here.

### 2.2 Block scoping (D16)

* Every `{ ... }` statement block (function body, if/else arms, loop
  bodies) is a scope. `var`/`let` declare into the **innermost**
  scope. A declaration is visible from its declaration statement to
  the end of its block, including nested blocks.
* Ranged-for counters and for-in key/value symbols are scoped to the
  **loop body** (they are declarations in the loop's scope). Two
  sequential loops using `i` are two distinct locals.
* Lambdas are unaffected (they are separate functions with their own
  scope stack rooted at their own FUNC block).

### 2.3 Redeclaration and shadowing (D15)

* Declaring a name twice **in the same scope** (any combination of
  `var`/`let`) is a compile error:
  `"Variable '%s' is already declared in this scope."`
* Declaring a name in an inner scope that exists in an outer scope is
  **legal shadowing** (classic lexical scoping). The inner declaration
  wins within its block.
* A local may shadow a global implicitly (it already can today —
  locals are checked first).

### 2.4 Static TDZ (D17)

* If a scope contains a declaration of `x`, then any use of `x`
  textually **before** that declaration, in that same scope (or in a
  nested block positioned before it), is a compile error:
  `"Variable '%s' is used before its declaration."`
* This is fully static (zero runtime cost) and eliminates the
  otherwise-possible horror of one name meaning "global" in the first
  half of a block and "local" in the second half.

### 2.5 Unchanged (D18)

* Assignment to a name with no visible local declaration still
  creates/assigns a **global**. Reading an unknown name still
  compiles to a global load that errors at runtime if unset.

## 3. Implementation

### 3.1 Strategy: scope stack + alpha-renaming

The key constraint: `lir_get_local_index` and elback resolve names by
`strcmp` over the flat per-function list, and `hir_term` carries only
a `char *symbol`. Rather than teaching every consumer about scopes,
**resolve scoping entirely inside hir.c by alpha-renaming**:

* **Every inner-block declaration** (anything not in the function's
  root scope — parameters and function-body-level vars keep their
  source names) gets a unique internal name `x$N` (function-wide
  counter). `$` is collision-proof. Renaming unconditionally — not
  only when shadowing — is what makes a binding invisible AFTER its
  block: LIR resolves locals by name against the flat list, so an
  out-of-scope use of the bare name must fall through to the global
  path instead of matching the dead binding's slot. (Implemented in
  `hir_scope_intern`; verified by tests/scoping/for_scope.noct.)
* Every symbol **use** is rewritten (in the HIR term being built) to
  the internal name of the declaration it resolves to. Uses that
  resolve to no local keep their original name (→ global path).
* After HIR build, the flat `hir_local` list contains only unique
  names; LIR, elback, scmback, and cback need **zero changes**.
  (elback output will show `x$1` — a valid Emacs Lisp symbol — only
  for genuinely shadowed names.)

### 3.2 Scope machinery in hir.c

Add to hir.c (file-local):

```c
struct hir_scope_entry {
        char *src_name;        /* name as written            */
        char *int_name;        /* internal (possibly x$N)    */
        bool  is_let;
        bool  declared;        /* false until decl stmt seen */
        struct hir_scope_entry *next;
};
struct hir_scope {
        struct hir_scope_entry *entries;
        struct hir_scope *up;
};
```

* Push a scope when entering a statement-list block (function body,
  if arm, else arm, loop body); pop on exit. The natural hook is
  `hir_visit_stmt_list` plus the loop/if visitors — audit every
  `hir_visit_*` that owns a `{ ... }`.
* **Pre-scan (TDZ):** on scope push, walk that block's own statement
  list (one level, not nested blocks) and create an entry
  (`declared = false`) for every `var`/`let` LHS and, for loop-owning
  scopes, the counter/key/value symbols (`declared = true`
  immediately — loop variables are born initialized).
* **Declaration visit** (`hir_visit_assign_stmt` where
  `is_var`/`is_let`):
  1. same-scope entry already `declared` → fatal redeclaration error
     (§2.3);
  2. compute the internal name (`x` if no live entry for `x` in any
     enclosing scope or function-wide used-name set; else `x$N`);
  3. `hir_add_local(int_name)` (slot allocation unchanged);
  4. mark `declared = true`.
  **Ordering trap:** `hir_visit_assign_stmt` builds the LHS term
  FIRST (hir.c:684) and only then runs the `is_var` branch
  (hir.c:705–715). A generic use-resolution hook would therefore see
  the declaration's own LHS while its entry is still
  `declared = false` and raise a spurious TDZ error on every
  declaration. Exempt the declaration's own LHS term from use
  resolution, and rewrite that already-built term's symbol
  (`hstmt->lhs->val.term.term->val.symbol`) to `int_name` in the
  declaration step (the same string then flows into `hir_add_local`
  at hir.c:708).
  **Initializer rule:** the RHS of a `var`/`let` IS inside the TDZ —
  `var x = x + 1;` is a use-before-declaration error even when an
  outer or global `x` exists (JS `let` behavior; add a test).
* **For-loop symbols (critical):** ranged-for counters and for-in
  key/value names are NOT terms — they are `char *` fields on the FOR
  block (`for_.counter_symbol`/`key_symbol`/`value_symbol`) that LIR
  resolves by name via `lir_get_local_index` (lir.c:595, 708–709,
  832; **asserts on a miss**) and elback/scmback print verbatim. In
  `hir_visit_for_stmt`: compute the internal names through the scope
  machinery, **write them back into the block fields**, and pass the
  internal names to the `hir_add_local` calls at hir.c:1220/1229/1238.
  Without this, two sequential `for (i in …)` loops either trip the
  dedup assert or silently share one slot while body uses resolve to
  a nonexistent `i$1` (runtime "Symbol not found").
* **Use resolution** (`hir_visit_term`/symbol-term construction and
  assignment-LHS handling): search the scope stack innermost-out for
  `src_name`:
  - hit with `declared == false` → fatal TDZ error (§2.4);
  - hit with `is_let` **and this use is an assignment LHS** → fatal
    let-assignment error (§2.1);
  - hit → rewrite the term's symbol to `int_name`;
  - miss → leave the name (global semantics).
* `hir_add_local`'s silent-dedup branch (hir.c:750–759) becomes
  `assert`-only: with renaming, a duplicate internal name indicates a
  compiler bug.
* Parameters populate the function's root scope (`declared = true`,
  `is_let = false`).
* Error reporting: use the existing `hir_fatal`-style path so messages
  carry file/line like current compile errors. All three new
  message strings are listed in §2 — use them verbatim (tests match
  them).

### 3.3 Grammar

* New keyword `let`: lexer.l rule `"let"` → `TOKEN_LET` (before the
  identifier rule, next to `"var"` at lexer.l:506); parser.y:
  `%token TOKEN_LET` + mirror **only the initializer-bearing** `var`
  productions: today that is the single `TOKEN_VAR expr TOKEN_ASSIGN
  expr TOKEN_SEMICOLON` (parser.y:365); once 02-typing adds its two
  annotated forms, additionally mirror the annotated-with-initializer
  one. **Do not** mirror the annotated-no-initializer form —
  `let x: T;` stays a syntax error per §2.1. AST `assign` payload
  gains `bool is_let;`.
* Regenerate checked-in lexer.yy.c / parser.tab.c (02-typing §3).

### 3.4 GC note (unchanged but load-bearing)

Unique-slot-per-declaration means dead scopes leave stale values in
their slots until function exit. That is exactly today's situation for
any since-last-use local, and safe: frame slots are zero-initialized
at frame entry and always hold *valid* values afterwards. **Do not**
add slot reuse/compaction in this change (it would require liveness
zeroing to keep the GC scan sound — the class of bug behind the
pin-before-init incident). If TMPVAR_MAX pressure ever demands reuse,
it is a separate, carefully-reviewed change.

## 4. What NOT to do

* No runtime TDZ (no "uninitialized" sentinel values).
* No slot reuse across sibling scopes (see §3.4).
* No change to the global-assignment rule (D18).
* No `const`-value freezing on `let` (binding only).
* No closure/capture changes.

## 5. remacs migration (part of this task)

The compiler now rejects code it used to accept. Migrate
`apps/remacs/editor/*.noct` (and `tests/`, `generated/` templates in
`tools/gen-napi.py` if they emit declarations):

* **M1 — duplicate `var` in the same scope**: previously silently
  merged. Find by compiling; fix by removing the second `var` (plain
  assignment) or renaming.
* **M2 — use-before-declaration within a block**: move the
  declaration up or split the block.
* **M3 — TMPVAR_MAX audit**: sequential same-name loop counters now
  each take a slot. After migration compiles, verify no function
  nears 128 slots: instrument once by printing
  `tmpvar_count` per function in `lir_build` under an env var, run
  the remacs build, eyeball the max. If anything exceeds ~100, refactor
  that function in remacs source (split it) — do not implement slot
  reuse.
* **M4 — loop counter used after the loop**: now out of scope →
  becomes a global read that fails at runtime or a TDZ error. Grep
  candidates: `grep -n "for (" -A6` reviewing uses after the closing
  brace, or just compile + run the full remacs suite and fix fallout.
* The full remacs suite (`run-all.sh` + `run-lisp.sh`) green is the
  migration exit criterion.

## 6. Testing plan

`tests/scoping/` golden-diff cases (compile-error cases assert on the
compiler's stderr message + nonzero exit; follow how existing suites
capture output, or add a tiny `run-scoping.sh` that greps the message):

1. `let_ok.noct` — let with reads, let-bound dict/array element
   mutation allowed.
2. `let_reassign.noct` — assignment to let → exact §2.1 message.
3. `redecl.noct` — same-scope var/var, var/let, let/var, let/let →
   §2.3 message.
4. `shadow.noct` — inner-block shadowing: inner sees inner, outer
   unchanged after the block; also loop-counter shadowing an outer
   var; and a lambda whose param shadows nothing weird.
5. `tdz.noct` — use before declaration in same block → §2.4 message;
   variant where an outer variable of the same name exists (must
   still error — the inner declaration wins the resolution).
6. `blockscope.noct` — sibling blocks reusing a name legally (two
   loops with `var i`-free counters; two ifs each declaring `var t`).
7. `global_rule.noct` — undeclared assignment still creates a global
   visible from another function; undeclared read still errors at
   runtime (message `Symbol "..." not found.`).
8. `for_scope.noct` — counter invisible after the loop (reading it
   → runtime global error or, with a later declaration, TDZ).
9. elback check: `tests/run-elisp.sh` still green; add one shadowing
   case to `tests/elisp/` to pin the `x$1` renaming through the
   Emacs oracle.
10. Full remacs suite + 3 cross-target builds.

## 7. Acceptance criteria

* All scoping tests pass; every error message matches §2 verbatim.
* remacs fully migrated: suite green; a summary list of remacs source
  changes (file + reason M1–M4) is included in the work report for
  owner review.
* elback/scmback/cback/LIR required **zero code changes** (proof that
  alpha-renaming carried the whole feature).
* No commits.
