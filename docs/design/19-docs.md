# 19 — Documentation

A C library is judged by its header and its website, in that order, and usually
within ninety seconds. `burrow` is asking a C programmer to adopt 23,730
functions they have never seen, whose names come from a language they may not
write, with an allocator argument in front of everything. That is a large
learning surface, and it is entirely the documentation's job.

There is a second, harsher reason this document exists. Go's standard library
is good *partly because its documentation is good* — `pkg.go.dev` is not an
accessory to the library, it is half the reason the library is pleasant. A port
that reproduces the code and loses the prose has ported the cheaper half.

So: **docs are a deliverable of every package, written with the code, gated in
CI, and never deferred.** Item 8 of the per-package gate.
→ [14](14-conformance.md) §8

## 1. The bar, named explicitly

Copying what works, from the five C-adjacent documentation sets that are
actually good:

| Source | What we take | What we leave |
| --- | --- | --- |
| **SQLite** | Every API page is self-contained; the site is a single downloadable tree that works offline; "how to use this correctly" pages sit next to the reference | The hand-maintained HTML — ours is generated |
| **Go / pkg.go.dev** | Doc comment adjacent to the declaration; runnable examples with verified output; package-level overview prose that explains the *model*, not the functions | The server; ours must work as files |
| **curl** | One man page per entry point, with a complete compiling example in every single one, plus `RETURN VALUE` and `AVAILABILITY` sections that are never omitted | Nothing, really. curl's man pages are the standard |
| **raylib** | The one-page cheatsheet: every function, one line each, scannable in a scroll. It is why people try raylib | The lack of depth behind it |
| **libgit2** | Ownership documented per-function, in a fixed phrasing, machine-checkable | Hand-written ownership prose — ours comes from annotations |

The synthesis is: **generated reference with hand-written narrative on top, both
offline-first, and every code block in both is compiled by CI.**

## 2. Layout

`docs/` is in the repository, not a separate site repo, because a docs tree in
a separate repo is a docs tree that drifts.

```
docs/
  index.md                     the 90-second pitch and the 4-line install
  tour.md                      one page: strings, errors, goroutines, http
  cheatsheet.md                every package, every function, one line each
  guides/
    allocators.md              the single most important concept
    strings-and-slices.md      Str, Slice, and why they are not char*
    errors.md                  Error, wrapping, sentinels, panic/recover
    concurrency.md             go, channels, select, context, sync
    interfaces.md              IoReader/IoReaderVT, vtables, satisfying one
    types-and-reflect.md       STRUCT(), the descriptor registry, json/fmt
    building.md                amalgamation, BURROW_OMIT_*, cross-compiling
    from-go.md                 for Go programmers: the mapping, in 3 pages
    from-c.md                  for C programmers: what Go idioms buy you
    porting.md                 for contributors: the six-step package recipe
  cookbook/
    *.md                       ~60 task-shaped recipes, each a full program
  pkg/
    strings/index.md           package overview (Go's package doc, verbatim)
    strings/reference.md       every exported symbol
    strings/examples/*.c       compiled and run by CI
    ... × 180
  ledger.md                    the fidelity ledger, rendered
  CHANGELOG.md
```

Rendered output is a **single self-contained HTML tree plus a man page set**,
both produced by `burrow-gen doc`, both attached to every release as a tarball.
The website is that tarball, unpacked. No JavaScript is required to read a
page; search is a prebuilt index and degrades to browser find-in-page.

## 3. Four layers, four different jobs

Written by different people at different times, and it matters that they are
not confused with each other.

**The tour** (`tour.md`, one page, ~1,200 words). Four programs: read a file
and count words; parse and re-emit JSON; run three goroutines over a channel;
serve HTTP. Each complete, each compiling, each under 30 lines. Anyone who
finishes the tour knows whether they want the library. This page is written
first, at 0.1, and rewritten whenever it starts lying.

**The cheatsheet** (`cheatsheet.md`, generated). Every exported symbol, one
line: the C signature and a half-sentence. 23,730 lines, grouped by package,
collapsed by default per package in HTML. Generated wholly from the reference
data, and it exists because raylib proved a scannable single page converts
browsers into users better than any amount of good prose.

**The guides** (`guides/`, hand-written, ~10 pages). Concept documents, not API
documents. `allocators.md` is the load-bearing one: if a reader does not
understand why every allocating function takes `Alloc *a` first, nothing else
in the library makes sense, and that is a page of prose, not a generated
signature. → [05](05-memory.md)

`from-go.md` and `from-c.md` are the two onboarding ramps, and they are
genuinely different documents. A Go programmer needs the naming mapping
([08](08-naming-abi.md) §1) and the list of things that are not automatic
(allocation, `defer` scope, generics). A C programmer needs to be told what
`Str` is, why `Error` is not `errno`, and that goroutines are real threads
multiplexed, not callbacks.

**The reference** (`pkg/`, generated + per-symbol prose). §4.

The cookbook sits between guides and reference: task-shaped, complete programs
— "download a URL to a file with a timeout", "read a CSV and aggregate",
"serve static files with a graceful shutdown", "make a TLS client with a
pinned certificate", "parse JSON into a declared struct". Roughly 60 of them
at 1.0. Each is one `.c` file in the repo, compiled and run by CI, and the
markdown includes it by reference rather than copying it.

## 4. The reference page contract

This is the part that is mechanically enforced, because it is the part that
180 packages' worth of contributors will otherwise do inconsistently.

Every exported symbol gets an entry with these fields, and `burrow-gen doc`
fails the build if a required one is empty:

| Field | Source | Required |
| --- | --- | --- |
| C signature | the header, exactly | yes, generated |
| Go original | `pkg.Symbol`, linked to `pkg.go.dev` | yes, generated |
| Description | **Go's doc comment, verbatim**, with attribution header | yes, generated → [18](18-legal.md) §2 |
| Lifetime | derived from `BURROW_OWNS`/`BURROW_BORROWS`/`BURROW_RETAINS` | yes, generated → [05](05-memory.md) §4 |
| Allocator | which parameter, and what is allocated from it | yes, generated |
| Errors | the sentinel values this can return, each linked | yes, generated |
| Example | a compiling C snippet | yes, hand-written or from Go's `Example*` |
| Differs from Go | a ledger-linked note | only when it does |
| Since | first release containing it | yes, generated |
| Thread safety | one of four fixed phrasings | yes, annotated |

Two of these deserve their rationale stated.

**The lifetime sentence is generated, not written.** libgit2 documents
ownership by hand and it is right most of the time; "most of the time" is not
a property you want in memory documentation. Ours comes from the same
annotations the `track` allocator validates at runtime
([05](05-memory.md) §4), so the docs and the checker cannot disagree — if the
prose is wrong, a test fails.

**Thread safety is one of four fixed phrasings**, never free prose:
*safe for concurrent use*; *safe for concurrent use after the first call*;
*not safe for concurrent use*; *safe only with the value's own mutex held*.
Go documents this inconsistently (`sync.Map` is explicit, many types say
nothing and the answer is folklore), and this is one of a small number of
places where the port is allowed to be *better* than the original, because
adding a fact is not changing a behaviour.

## 5. Examples are code, not prose

The rule that makes the whole thing work: **no code block in `docs/` is
markdown.** Every one is an `#include`-by-reference of a real file under
`docs/pkg/<pkg>/examples/` or `docs/cookbook/`, and CI compiles and runs all of
them on every platform tier.

```
burrow-gen doc --check
  ├─ extract every referenced example
  ├─ compile each against the amalgamation, -Wall -Werror
  ├─ run each under ASan+UBSan
  ├─ compare stdout against the recorded expected output
  └─ fail on: missing example, compile error, output mismatch
```

Three consequences worth stating:

1. **Documentation cannot rot.** An API change that breaks an example breaks
   the build in the same commit. This is the single highest-value property in
   this document and it is why the toolchain is in P0 rather than P6.
   → [16](16-milestones.md) §2
2. **The corpus is seeded, not started.** Go's 1,017 `Example*` functions
   translate to C as doctests with their `// Output:` comments preserved as the
   expected output, so 1,017 verified examples exist before anyone writes one.
   They are also part of the conformance suite, which means they are paid for
   twice. → [14](14-conformance.md) §8 item 3
3. **Examples use `BURROW_SHORT`.** Every example begins with
   `#define BURROW_SHORT 1`, shown, never hidden, because the code people
   actually write uses `S(...)` and `DEFER(...)` and documentation that hides
   the ergonomics undersells the library. Library-header examples are the
   exception and say why in a comment. → [08](08-naming-abi.md) §4

The gap to fill by hand is the difference: ~23,730 symbols against 1,017
inherited examples. The rule is one example per *exported type or function
group*, not per symbol — `strings_trim`, `strings_trim_left`,
`strings_trim_right`, `strings_trim_prefix` share one — which brings the
hand-written obligation to roughly **3,000 examples**, distributed across the
same people porting the packages. Budgeted in [16](16-milestones.md) §1 as part
of the per-package cost, not as a separate project.

## 6. Doc comments live in the header

`pkg.go.dev` works because the documentation is next to the declaration and
nobody has to go anywhere else. So the generated headers carry the full doc
comment above each declaration — not a link, not a one-liner:

```c
/* strings_contains reports whether substr is within s.
 *
 * Go: strings.Contains — go/src/strings/strings.go (verbatim doc comment,
 *     BSD-3-Clause, © The Go Authors; see NOTICE)
 * Lifetime: borrows nothing, allocates nothing.
 * Thread safety: safe for concurrent use.
 *
 *   #define BURROW_SHORT 1
 *   if (strings_contains(line, S("error"))) { ... }
 */
bool strings_contains(Str s, Str substr);
```

This is what an editor's hover shows with no language server, no plugin and no
index, on every platform, which is the C tooling reality and worth designing
for. It costs header size — an estimated 4.2 MB across the full split-form
header set — so the amalgamated `burrow.h` ships in two variants,
`burrow.h` (comments stripped, ~900 KB) and `burrow-documented.h`, selected by
a release artefact rather than a `#define`. → [15](15-build-deploy.md) §1

## 7. The toolchain

`burrow-gen doc` is one subcommand of the existing generator
([15](15-build-deploy.md) §2), not a separate program, and has no dependency
beyond a C compiler and the Go toolchain used at generation time — end users
never run it.

| Input | Used for |
| --- | --- |
| The generated headers | signatures, `BURROW_OWNS`/`BORROWS` lifetimes, `since` |
| `$GOROOT/src/**/*.go` | doc comments, verbatim, with file+SHA recorded |
| `$GOROOT/api/go1.*.txt` | the coverage denominator, rendered as per-package completeness |
| `docs/**/*.md` | narrative, and the example references it must resolve |
| `docs/**/examples/*.c` | compiled, run, output-compared |
| `ledger.toml` | the "differs from Go" notes, linked from both sides |

| Output | Form |
| --- | --- |
| `burrow-docs-<version>.tar.gz` | self-contained HTML, prebuilt search index, no JS required to read |
| `man/man3/*.3` | one page per exported function, curl-style, `RETURN VALUE` and `THREAD SAFETY` never omitted |
| `burrow-doc.json` | machine-readable, for editor tooling and third-party binding generators |
| `burrow-documented.h` | headers with full comments (§6) |
| `cheatsheet.html` | the one-page scan (§3) |

A `docs/` change is reviewed like a code change, in the same PR as the code it
documents. There is no "docs sprint", no docs backlog label, and no separate
docs team — those three things are how the failure mode arrives.

## 8. House style, enforced by a linter

Short, because rules nobody remembers are not rules. `burrow-gen doc --lint`
checks all of these:

- **Second person, present tense, active voice.** "You must free the result
  with", not "the result should be freed by the caller".
- **Every allocating function's prose names its allocator parameter.** Checked
  mechanically: if the signature has `Alloc *a` the description must mention
  what comes out of it.
- **No "simply", "just", "obviously", "easy".** A hard lint failure, with the
  list in the linter, because these words only ever tell a struggling reader
  that the problem is them.
- **Sentence-per-line in markdown source**, so review diffs are readable.
- **Every link resolves**, including the `pkg.go.dev` ones, checked offline
  against a cached index and online nightly.
- **No forward references to unfinished work** in shipped docs; a planned
  feature is either in `ROADMAP.md` or absent.
- **The ledger is linked from both ends.** A symbol with a ledger entry must
  say so on its page, and the ledger entry must link back.

## 9. Documenting the places we are not Go

Fourteen entries in the fidelity ledger ([01](01-scope.md) §6), an explicit
allocator on every allocating function, no compiler-emitted RTTI, and a
`reflect` that only sees registered types ([07](07-reflect.md) §9). These are
the things that will bite a Go programmer who assumes, and the documentation's
posture toward them is to be conspicuous rather than tactful:

- `ledger.md` is linked from `index.md`, not buried, and each entry states what
  Go does, what `burrow` does, why, and whether it is permanent.
- Every affected symbol carries the **Differs from Go** field (§4), so a reader
  encounters the caveat at the point of use and not afterwards.
- `from-go.md` opens with the five differences that matter most, before any
  of the pleasant news, on the theory that a library which front-loads its
  limitations is one whose other claims you can believe.

## 10. The gate

Per package, as item 8 of [14](14-conformance.md) §8:

1. `docs/pkg/<pkg>/index.md` exists and carries Go's package doc verbatim plus
   a hand-written *model* paragraph — what this package is for, in our words.
2. Every exported symbol has a reference entry with all required fields (§4)
   non-empty.
3. Every function group has at least one example; every example compiles, runs
   and matches its recorded output on all Tier A platforms.
4. All inherited `Example*` functions for the package are present and passing.
5. `burrow-gen doc --lint` is clean for the package.
6. Every ledger entry touching the package is linked from the affected symbols.

And once, project-wide, before 1.0: the tour, the cheatsheet, all ten guides,
~60 cookbook recipes, and a single editorial pass by someone who did not write
the packages. → [16](16-milestones.md) §2, P6.
