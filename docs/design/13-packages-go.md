# 13 — Tier 5: tooling, templates, database, logging

Thirty-one packages, 2,878 exported declarations, 80,745 non-test lines. The
grab-bag tier: everything that is large, self-contained, depends on no
substrate beyond Tier 1, and is not on the critical path to `net/http`.

Which makes it the *easiest* tier to execute and the last one to matter. Its
one strategic question — whether to port `go/*` at all — is answered below with
yes, and for a reason that is not obvious.

| Group | Pkgs | Decls | LOC | Test LOC |
| --- | --- | --- | --- | --- |
| `go/*` | 15 | 1,664 | 52,963 | 22,976 |
| `debug/*` | 7 | 4,834 | 15,288 | 7,083 |
| `text/template`, `html/template`, `text/*` | 6 | 455 | 12,324 | 14,077 |
| `database/sql*` | 2 | 294 | 5,532 | 8,625 |
| `log*`, `runtime/pprof`, `runtime/trace` | 5 | 334 | 7,696 | 10,067 |

(`debug/elf`'s 3,296 declarations are almost all `const` — ELF machine types,
section flags, relocation kinds. Generated, not written.
→ [01](01-scope.md) §6)

## 1. Why port `go/*` into C at all

`go/parser`, `go/types`, `go/ast`, `go/printer` — 53,000 lines implementing a
complete front end for a language `burrow` does not compile. The obvious view
is that this is dead weight.

It is not, for three reasons:

**1. The coverage claim requires it.** "100% of the Go standard library, nothing
missing" is the project's defining promise ([08](08-naming-abi.md) §2), and
`go/*` is 7% of the exported API. Skipping it turns a mechanically checkable
claim into a marketing one. If the answer to "is `go/types` in?" is "no, we
decided that one wasn't useful", every other answer becomes negotiable too.

**2. It is the best possible test of the reflection and container layers.**
`go/types` is 25,000 lines of interface-heavy, map-heavy, recursive,
mutually-referential data structures with a real algorithm (Go's full type
checker, including generics inference) on top. If `burrow`'s `Map`,
interface dispatch, slices and `BURROW_STRUCT` embedding can carry `go/types`
unchanged, they can carry anything. It is a torture test that comes with
10,544 lines of its own tests and a large `testdata` corpus of expected-error
files.

**3. It has a real user: `burrow-gen --from-go`.** The tool that translates Go
declarations into `burrow` type descriptors needs to parse and type-check Go.
Today it does that by shelling out to Go. A `burrow` that contains `go/parser`
and `go/types` can bootstrap its own generator with no Go toolchain present —
which matters for the "no build step, no dependencies" story and is a genuinely
satisfying closure. → [07](07-reflect.md) §5

So: `go/*` is in, it is scheduled last, it is a validation vehicle as much as a
deliverable, and it is where a contributor who wants a large self-contained
mechanical project should be pointed.

### Notes on the `go/*` port

- **`go/token`, `go/ast`, `go/scanner`, `go/constant` first.** Mechanical.
  `go/constant` needs `math/big`.
- **`go/parser`** is a recursive-descent parser with error recovery; the
  recovery behaviour is tested against expected-output files and is fiddly but
  deterministic.
- **`go/printer`** is the hardest of the four to get exactly right, because
  `gofmt`'s output is byte-specified: the alignment logic, comment placement
  and `text/tabwriter` interaction have no tolerance. Its test is that
  `go/format.Source` applied to the entire `$GOROOT/src` tree produces
  byte-identical output to `gofmt`. That is a total, cheap, brutal test and it
  should be the acceptance gate.
- **`go/types`** is the big one. Its implementation is recursive and uses cyclic
  object graphs heavily (`Named` types referring to themselves through
  `Interface` method sets), which is the one place in the project where the
  arena model needs care: a type-checker's graph has no clean ownership tree.
  Answer: `go/types` allocates everything from a single arena owned by the
  `Config`/`Info` pair, freed wholesale when the caller is done, and nothing
  inside is individually freed. This is exactly the case arenas are for.
  → [05](05-memory.md) §6
- **`go/build`** cannot be fully ported — its `Context` queries a Go toolchain
  that may not exist. Build-constraint evaluation, `//go:build` parsing
  (`go/build/constraint`) and directory scanning all work; toolchain-dependent
  queries return an error. This is fidelity-ledger entry 12.
  → [01](01-scope.md) §6
- **`go/doc` + `go/doc/comment`** parse and render doc comments. Useful to us
  directly: they are how `burrow`'s generated documentation renders Go's
  original comments. → [08](08-naming-abi.md) §9

## 2. `debug/*`

Binary format readers: ELF, Mach-O, PE, Plan 9 a.out, DWARF, Go symbol tables,
and `buildinfo`.

Almost pure table-and-struct work, and mostly generated: the constant blocks
come from Go's source via a script, and the readers are straightforward
binary decoding over `io.ReaderAt`. The tests are the interesting part —
they parse real object files checked into `testdata`, which we reuse verbatim.

Two things make this tier worth doing *early* rather than last, despite its
Tier 5 label:

- **`burrow`'s own traceback and pprof support wants them.**
  [06](06-runtime.md) §11 needs to read symbol tables from the running binary;
  `debug/elf`, `debug/macho` and `debug/pe` are exactly that code. Writing it
  once and exposing it publicly is strictly better than writing a private
  version.
- **They are excellent onboarding tasks.** Self-contained, well-tested,
  no concurrency, no unsafe, real `testdata`.

`debug/dwarf` is the only substantial one (4,502 lines): full DWARF 4/5
parsing, line tables, type reconstruction. It has no dependencies beyond `io`
and is a clean, satisfying port.

## 3. `text/template` and `html/template`

`text/template` (39 decls, 2,898 lines + `text/template/parse` at 2,569) is a
complete interpreted template language: lexer, parser, and a tree-walking
evaluator that uses `reflect` for everything. Field access, method calls,
pipelines, ranging over arbitrary types, function maps with arbitrary
signatures, variable scoping, `{{if}}`/`{{with}}`/`{{range}}`/`{{block}}`/
`{{define}}`/`{{template}}`.

This is the **single most reflection-dependent package in the standard
library**, harder in that respect than `net/rpc` or `encoding/json`. A template
does `{{.User.Profile.Name}}` at runtime on a value the template has never
seen, and must handle maps, structs, pointers, interfaces and methods
uniformly, with Go's exact precedence between a method and a field of the same
name. And `FuncMap` accepts `any` — arbitrary function values called with
arbitrary argument counts and types, which requires the full
`reflect.Value.Call` path with variadic support.

Consequences for `burrow`:

- `text/template` is the acceptance test for [07](07-reflect.md) in the same
  way `fmt %v` is the Tier 0 gate. If it works, reflection is done.
- The thunk mechanism must support variadic calls and multi-return
  `(T, error)` functions, since `FuncMap` explicitly allows both.
- Types used in templates must be declared to the registry. This is the most
  visible place where [07](07-reflect.md) §7's boundary bites: a Go program can
  template over any struct; a `burrow` program can template over any *declared*
  struct. The error message when it fails must name the type and point at
  `BURROW_STRUCT`.

`html/template` (76 decls, 4,864 lines) wraps `text/template` with
contextual auto-escaping — a static analysis of the template that determines,
for each interpolation point, whether it lands in HTML text, an attribute, a
URL, CSS, or JavaScript, and inserts the correct escaper. This is a real
security mechanism with real CVEs behind it and 6,865 lines of tests. It is
ported exactly, including the `JSStr`/`URL`/`HTML`/`HTMLAttr`/`CSS` typed-string
escape hatches and the specific behaviour on ambiguous contexts. No
simplification, no "escape everything as HTML" shortcut.

`text/tabwriter` (601 lines) and `text/scanner` (792) are trivial and come
along for free. `text/tabwriter` is needed by `go/printer` anyway.

## 4. `database/sql` (166 + 128 decls, 5,532 lines)

A driver-agnostic database abstraction with connection pooling. Notable because
it is the package most likely to *attract users* in Tier 5 — a portable C
`database/sql` with Go's driver interface and Go's pool semantics is genuinely
useful — and because it is mostly a concurrency exercise rather than a
reflection one.

- `database/sql/driver` defines the interface a driver implements: `Driver`,
  `Connector`, `Conn`, `Stmt`, `Rows`, `Tx`, `Valuer`, plus the optional
  `QueryerContext`/`ExecerContext`/`SessionResetter`/`NamedValueChecker`
  interfaces that a real driver must support. 128 declarations, almost all
  interface definitions, and they must be exact because third-party drivers are
  written against them.
- `database/sql` is the pool: `DB` with `SetMaxOpenConns`/`SetMaxIdleConns`/
  `SetConnMaxLifetime`/`SetConnMaxIdleTime`, the connection-request queue,
  retry-on-bad-connection logic (`driver.ErrBadConn` and the exact retry
  count), `Tx` isolation, `Stmt` re-preparation across connections,
  `Rows.Scan`'s conversion matrix, `NullString` and friends, and context
  cancellation threaded throughout. Its 8,529 lines of tests include a fake
  driver and a large concurrency stress test — port both.
- `Rows.Scan` into `any` destinations is the reflection-dependent part, and it
  is a bounded conversion table rather than general reflection.
- **The payoff:** a `burrow` SQLite driver is ~500 lines over the amalgamation
  everyone already has, and it makes `burrow`'s deployment story
  (`burrow.c` + `sqlite3.c` + your code) concrete. Ship it in
  `tamnd/burrow-examples`. → [15](15-build-deploy.md) §6

## 5. Logging and profiling

`log` (51 decls) is trivial. `log/syslog` is small and Unix-only (it is a no-op
returning an error on Windows, as in Go).

`log/slog` (172 decls, 3,002 lines) is the modern structured logger and worth
real attention: it is new, it is what new Go code uses, and its design —
`Handler` interface, `Record` with inline attribute storage, `Attr`/`Value`
with a packed representation that avoids allocation, `Group`, `LogValuer`,
`With` pre-formatting — is exactly the kind of allocation-conscious API that
translates *well* to C. `slog.Value`'s tagged union becomes a natural C struct.
This is one of the few packages that may be nicer in `burrow` than in Go.

`runtime/pprof` (2,846 lines) and `runtime/trace` (989) are the public wrappers
over the runtime's profiling. Their contract is the *output format*, which is
`pprof`'s gzipped protobuf and Go's binary trace format — both already
committed to in [06](06-runtime.md) §11, because `go tool pprof` and
`go tool trace` working on `burrow` output is worth more than almost any other
single piece of tooling integration. Their implementation here is thin; the
work is in the runtime.

## 6. Tier 5 gate

1. `go/format.Source` over all of `$GOROOT/src` is byte-identical to `gofmt`.
2. `go/types` type-checks all of `$GOROOT/src` with the same results as Go's
   own type-checker (diff the `Info` maps), and passes its `testdata`
   expected-error corpus.
3. `text/template` and `html/template` pass their full test suites, including
   the auto-escaping context tests and every `FuncMap` signature shape.
4. `database/sql` passes its suite including the concurrency stress test, under
   TSan, and the SQLite example driver works on all three primary platforms.
5. `debug/*` parses every binary in Go's `testdata` plus, as a smoke test,
   every object file in the `burrow` build tree on each platform.
6. `runtime/pprof` output loads in `go tool pprof`; `runtime/trace` output loads
   in `go tool trace`.
7. `log/slog` passes `testing/slogtest`, Go's own handler conformance suite —
   another free external test, like `fstest.TestFS`.

Gates 1 and 2 are the ones to care about. They are single commands, they admit
no partial credit, and passing them means the container, interface and
reflection layers underneath are actually correct — which is most of what Tier 5
was for.
