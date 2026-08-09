# Design Documents: ABCE, Type Hints, Class Freezing, Block Scoping

This directory holds the implementation-ready design documents for four
coupled language/VM features.  They were designed in discussion with the
project owner; every open question has been resolved.  The implementing
model should treat this directory as the single source of truth and
should NOT re-litigate decisions recorded in the Decision Log below.

## Documents

| Doc | Feature | Depends on |
|-----|---------|------------|
| [01-abce.md](01-abce.md) | Array Boundary Check Elimination for Packed via ranged-for loop versioning | nothing (start here) |
| [02-typing.md](02-typing.md) | Type annotations as optimization hints | 01 (shares the guard/versioning mindset; independent code-wise) |
| [03-class.md](03-class.md) | Read-only class dicts, top-level `class` / top-level `var`/`let` via load-time init | 04 (uses `let` semantics) |
| [04-scoping.md](04-scoping.md) | `let`, block scoping, redeclaration errors, static use-before-declaration errors | nothing |
| [05-cse.md](05-cse.md) | HIR optimizer build split (NOCT_ENABLE_OPTIMIZER) + common subexpression elimination | 01 (splits ABCE out of hir.c; CSE runs after ABCE) |

Recommended implementation order: **01 → 04 → 03 → 02**.
01 is self-contained and highest value (remacs Editor/Buffer speedup).
04 is frontend-only.  03 builds on 04's `let`.  02 is most valuable
after 01 exists (it reuses the "guarded specialization" idea).

## Decision Log (authoritative)

These decisions were made explicitly by the project owner.  Do not
change them without asking.

### ABCE
- D1. Optimization target: **ranged-for loops over Packed only**.  No
  Array support.  No general loops.  Goal is remacs `Editor.*` /
  `Buffer.*` hot loops; do not over-generalize.
- D2. **No SSA.**  Ranged-for survives structurally into HIR, so the
  induction variable, bounds, and step are available without IV
  discovery.  Loop invariance = "no assignment to the variable inside
  the loop body subtree" (a def-set walk).  The SSA placeholder fields
  in HIR stay unused.
- D3. The fast (versioned) loop body **omits the back-edge safepoint**.
  Rationale: safepoints are the only GC entry points; a safepoint-free
  body with no allocating operations cannot race with GC, so a base
  pointer computed before the loop stays valid.  STW response delay is
  bounded by the loop's runtime (ranged-for is finite, body is
  straight-line), which is acceptable.  Strip-mining is a known future
  escape hatch; do not implement it now.
- D4. Eligibility is intentionally narrow ("aware scripts win"): body
  must be allocation-free and call-free.  Affine index forms with
  coefficient 1 only (`i`, `i+v`, `v+i`, `i-v`).
- D5. New IR ops: `PBASE` (materialize payload address as 64-bit int),
  and width-parameterized base-relative `PLOAD`/`PSTORE`.  Addresses
  are 64-bit fixed; 32-bit JIT backends truncate.  Ops exist at both
  HIR and LIR levels, implemented in the interpreter, the JIT
  backends, and the C backend (so ABCE-optimized C source can be
  emitted).
- D6. The pass is independent and runs when the optimization level is
  at or above a threshold (see 01-abce.md for the concrete flag).
- D7. Bounds guard is `0 <= index && index < length` (upper bound
  strict; the owner's original sketch had a typo).

### Type hints
- D8. Annotations are **optimization hints only**.  No advanced type
  inference or type checking in this round.
- D9. Hints must never change program semantics on their own.  They
  are consumed in two sound ways: (a) provable cases (literal init +
  no reassignment) compile directly to typed ops; (b) hinted-only
  cases (function parameters) get **entry-point type checks, inserted
  only when the optimizer actually specializes that function**.
  Unoptimized code has zero checks and annotations are fully inert.
  An entry-check violation is a runtime error.
- D10. Type name vocabulary: `string, int, long, float, double, dict,
  array, packed, func` plus sized integers `i8, i16, i32, i64, u8,
  u16, u32, u64`.  (`func` is a lexer keyword — the grammar needs a
  `type_name : TOKEN_SYMBOL | TOKEN_FUNC` nonterminal; see
  02-typing.md.)  `int` ≡ `i32`, `long` ≡ `i64`.  Sized types carry
  width/sign information for future Packed accessors and typed
  arithmetic; their runtime checks degrade to the storage-class tag
  (int or long).

### Class (D-OOP)
- D11. `class {...}` and `extend B {...}` produce **frozen (read-only)
  dictionaries**.  `new C {...}` produces a normal **mutable** dict
  (the frozen bit is not inherited).  Writing to a frozen dict is a
  runtime error.
- D12. Shallow merge stays the **official language semantics**: values
  are reference-copied, so a mutable object stored in a class template
  is shared by all instances.  This is documented, not "fixed".  The
  recommended convention: **constructors assign fresh objects** (the
  `new` overlay or an init method assigns per-instance containers).
- D13. Top-level `class Name {...}` and top-level `var`/`let`
  declarations are compiled into a **reserved-name init function**
  which the VM auto-executes at registration/load time (like an
  `.init` section).  File load order defines evaluation order;
  forward references fail at load time with the existing
  "unassigned global" error.

### Scoping
- D14. `let` freezes the **binding only** (not the value).  `let`
  requires an initializer.
- D15. Redeclaration of the same name **in the same scope** is a
  compile error (`var` and `let` alike).  **Shadowing in an inner
  block is allowed** (classic lexical scoping).
- D16. `var`/`let` become **block-scoped**.  The ranged-for counter is
  scoped to the loop body.
- D17. Use of a name **before its `var`/`let` declaration in the same
  block** is a **compile-time error** (static TDZ; zero runtime cost).
- D18. The existing rule "assignment to an undeclared name creates a
  global; reference to an undefined global is a runtime error"
  is **kept unchanged** (remacs depends on it, e.g. `SkkDict = 0;`
  inside init functions).
- D19. These are **breaking changes and that is accepted**.  There is
  no language-level/compat flag.  remacs will be migrated by editing
  its sources.

## Global invariants for the implementing model

These are hard rules learned from past incidents in this codebase.
Violating them causes heap corruption or target breakage.

1. **C89 only in `src/core/`** — the msdos/OpenWatcom 1.9 target
   compiles the core.  Concretely: declarations at the top of a block,
   `/* */` comments only, no `<stdbool.h>` (use `noct/c89compat.h`,
   where `bool` is `typedef int`), no C99 loops (`for (int i…`), no
   designated initializers, no VLAs.
2. **GC discipline** — every `NoctValue`/`rt_value` slot that the GC
   can scan must always hold a valid value.  Slots are zero-cleared
   (`type = 0` = int 0 by design).  `rt_gc_pin_local` zero-clears the
   slot at pin time; never break that.  If you add a header flag to a
   GC-managed object (e.g. the dict frozen bit), it must be preserved
   across **every** GC copy/promotion path (young, graduate, tenure —
   three separate copy functions per type in `src/core/gc.c`) — and
   in MT builds, in the MT object model too.
3. **All three targets must keep building**: Linux (gcc), mingw-w64
   (`cmake --preset windows-mingw-x86_64`), msdos (OpenWatcom in
   `~/opt/openwatcom-1.9`, preset `msdos`).  Run all three builds
   before declaring a task done.
4. **The remacs suite is the acceptance gate**:
   `cd apps/remacs && cmake --build build-debug && cd tests && sh run-all.sh`
   plus `sh run-lisp.sh`.  All existing tests must stay green.
5. **Do not commit.**  The project owner reviews and commits all
   changes.  Leave work uncommitted.
6. New opcodes must be handled (or explicitly rejected/gated) in
   **every backend**: interpreter, each JIT arch, C backend
   (`cback.c`), Emacs Lisp backend (`elback.c`).  The per-feature docs
   state the gating strategy; do not leave a backend silently broken.
7. When editing generator/tool scripts, re-run the affected build and
   grep-verify the edit landed (a past incident: heredoc edits ran in
   the wrong cwd and silently did nothing).

## How to build and test (quick reference)

```
# Core (Linux debug used by remacs):
cd /home/awe/NoctLang/apps/remacs && cmake --build build-debug

# remacs test suites:
cd /home/awe/NoctLang/apps/remacs/tests && sh run-all.sh && sh run-lisp.sh

# Cross targets:
cd /home/awe/NoctLang && cmake --build build-mingw-x86_64
export WATCOM=~/opt/openwatcom-1.9 PATH=$WATCOM/binl64:$WATCOM/binl:$PATH INCLUDE=$WATCOM/h
cmake --build build-msdos
```
