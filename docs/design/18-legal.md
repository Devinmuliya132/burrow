# 18 — Licensing, attribution, and naming

**This is not legal advice.** It is the analysis a careful engineer can do by
reading the licences, and it is written so a lawyer can check it in an hour
rather than starting from nothing. [17](17-open-questions.md) §14 says to
actually get that hour, before 0.1, and means it.

The short version: Go is BSD-3-Clause plus an additional patent grant, the
licence permits exactly what this project does, the obligations are attribution
and notice retention, and the one genuine ambiguity is a clause in the patent
grant that a reimplementation should think about rather than wave at.

## 1. Go's licence

Two files in the Go distribution, and both matter:

- `LICENSE` — "Copyright 2009 The Go Authors", a standard three-clause BSD
  licence. Permits use, modification and redistribution in source and binary
  form, commercially, with three conditions: retain the copyright notice and
  disclaimer in source redistributions, reproduce them in binary
  redistributions' documentation, and do not use the copyright holder's name or
  contributors' names to endorse or promote derived products without
  permission.
- `PATENTS` — "Additional IP Rights Grant (Patents)". A perpetual, worldwide,
  royalty-free patent licence from Google covering claims necessarily infringed
  by "this implementation of Go", with a defensive-termination clause.

**Is `burrow` a derivative work of Go?** Yes, and the spec should say so
plainly rather than hedge. This is not a clean-room reimplementation from the
specification: the plan is to read Go's source, translate it, and carry across
its tests, its `testdata`, its generated tables and its doc comments verbatim.
That is textbook derivative work, and BSD-3-Clause permits it.

**So `burrow` is BSD-3-Clause.** Not by preference but by the cleanest route:
matching the upstream licence means no compatibility question ever arises, and
BSD-3 happens to be the right licence for something meant to be vendored into
other people's products. → [00](00-overview.md)

`tamnd/burrow`, `burrow-conformance`, `burrow-testdata` and `burrow-examples`
are all BSD-3-Clause.

### The patent clause worth reading twice

> This grant does not include claims that would be infringed only as a
> consequence of further modification of this implementation.

Read strictly, the grant covers Go as Google distributes it, and a
reimplementation is a modification. Two observations, neither of which is a
legal conclusion:

- The carve-out is about *new* infringement introduced by modification, not a
  revocation of the grant for modified copies. A faithful port that implements
  the same algorithms does not plausibly infringe claims that Go's own code
  does not.
- The practical risk is low and structural: the algorithms in Go's standard
  library are overwhelmingly published standards (TLS, DEFLATE, SHA-2, ML-KEM,
  DWARF, HTTP) whose patent positions are well understood and mostly expired or
  royalty-free by design.

Still, this is the entry a lawyer should look at. It is the one place where
"faithful port" versus "further modification" is a distinction with possible
consequences, and it is cheap to ask about now.

## 2. Attribution and notice

The obligations are mechanical, so they are automated and CI-enforced rather
than left to good intentions.

**Top-level files:**

- `LICENSE` — `burrow`'s BSD-3-Clause, with the Go Authors' copyright retained
  alongside `burrow`'s own.
- `PATENTS` — Go's patent grant, reproduced verbatim and unmodified.
- `NOTICE` — the attribution statement, in these words:

  > `burrow` is a C port of the Go standard library. Substantial portions of
  > this software are derived from the Go programming language's standard
  > library, Copyright 2009 The Go Authors, licensed under the BSD 3-Clause
  > licence reproduced in LICENSE, with the additional patent grant in PATENTS.
  > `burrow` is not affiliated with, endorsed by, or sponsored by Google or the
  > Go project.

**Per-file headers.** Every source file derived from Go carries, at the top:

```c
/* Derived from Go's src/strings/strings.go.
 * Copyright 2009 The Go Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style licence
 * that can be found in the LICENSE file.
 *
 * C port copyright 2026 The burrow Authors, same licence.
 * Go source: go1.27.1 5f0b5b1b  */
```

The Go source version and commit are recorded because they make the port
*auditable*: a reviewer can diff `burrow`'s `strings.c` against the exact Go
file it came from. That is worth more than the licence compliance it
incidentally provides. A CI check fails any file in the derived set without a
header naming a real upstream file.

**Doc comments.** `burrow-gen doc` reproduces Go's doc comments verbatim,
because a paraphrase would be both worse documentation and a fresh chance to
introduce an error. They carry a per-symbol attribution header:

> Documentation for this symbol is reproduced from Go's `strings` package,
> Copyright 2009 The Go Authors, BSD-3-Clause. Lifetime and allocator notes are
> `burrow`'s own.

Copied doc comments are part of the same BSD-licensed distribution and are
covered by the same grant; retaining the notice is the obligation, and we
retain it per symbol rather than once at the top, which is stricter than
required and cheaper than arguing. → [08](08-naming-abi.md) §9

**Binary distributions.** Clause 2 requires the notice in "documentation and/or
other materials provided with the distribution". The amalgamation zip contains
`LICENSE`, `PATENTS` and `NOTICE`; `burrow.h` carries the full notice as a
header comment so that even a user who takes only the two files has it; and
`burrow_license()` returns the notice text at runtime, so a shipped binary can
satisfy the requirement with one call. That last one is a small thing that
downstream users will thank us for.

**`testdata`.** `tamnd/burrow-testdata` mirrors Go's `testdata` files with
their upstream paths preserved and a manifest recording the Go version. Some
of those files carry their own third-party licences — test certificates, image
files, compressed archives from other projects — so the mirror includes Go's
own `LICENSE` files from every subdirectory that has one, and the sync script
refuses to drop a licence file. → [14](14-conformance.md) §2

## 3. The name, and the trademark position

"Go" and the Go gopher are Google trademarks. The Go trademark policy permits
nominative use — saying truthfully what your software is — and prohibits use
that suggests endorsement or origin.

The decisions, all of which exist to stay clearly on the right side of that
line:

- **The project is called `burrow`**, not `cgo`, `go-c`, `libgo`, `goc`, or
  anything containing "Go". `libgo` was rejected for the additional reason that
  it collides with gccgo's runtime library. → [00](00-overview.md)
- **Descriptions use nominative form**: "a C port of the Go standard library",
  "implements the API of Go's `net/http`". Never "Go for C", never "C Go",
  never the gopher, never Go's colour scheme or logo, never a logo derived from
  Go's.
- **Every artefact carries the non-affiliation statement** — README, NOTICE,
  generated documentation, and release notes — because the cheapest trademark
  defence is being unambiguous about it everywhere.
- **The symbol names are functional, not source-identifying.** There is no
  library prefix at all: `strings_contains` is the mechanically obvious
  spelling of `strings.Contains`, and the leading segment is Go's *package*
  name, which is descriptive of what the function does
  ([08](08-naming-abi.md) §1). Package names like `strings`, `http` and `json`
  are generic descriptive terms, and trademark law protects source
  identification, not descriptive identifiers inside a library that loudly
  states its non-affiliation. The one prefix that does exist, `BURROW_` on
  macros, names *this* project rather than Go's.
- **`BURROW_PREFIX` exists partly for this reason.** A downstream user — or we
  ourselves, if asked — can rename every public symbol at generation time with
  a single flag. → [08](08-naming-abi.md) §6, [15](15-build-deploy.md) §2

If Google ever objects to any of this, the answer is to change it immediately
and without argument — which is another reason the prefix is configurable and
the brand is not "Go"-derived. The project is designed so that a trademark
complaint is a configuration change rather than a crisis.

## 4. Third-party code and licences

`burrow`'s dependency count is zero by design ([02](02-landscape.md) §4), which
makes this section short — but not empty, because several inputs are derived
from elsewhere and each needs its licence carried.

| Input | Source | Licence | How it arrives |
| --- | --- | --- | --- |
| `math` | FDLIBM via Go | Sun Microsystems permissive notice, retained by Go | ported from Go, notice retained |
| `nistec`, `edwards25519` field arithmetic | `fiat-crypto` | MIT / Apache-2.0 / BSD-1 (tri-licensed) | regenerated from fiat-crypto's C backend |
| Unicode tables | Unicode Character Database | Unicode licence (permissive) | regenerated from UCD by our own emitter |
| tzdata | IANA | public domain | embedded zip, unmodified |
| Public-suffix list (`cookiejar`) | Mozilla PSL | MPL-2.0 | generated table; **MPL applies to the list, check the generated form** |
| `x/net`, `x/crypto`, `x/text` vendored code | Go subrepos | BSD-3-Clause + PATENTS | ported as `burrow` internals |
| `debug/elf` etc. constants | Go, ultimately from OS headers | BSD-3 / various permissive | generated from Go's source |
| SQLite (in `burrow-examples` only) | SQLite | public domain | not part of `burrow` |
| Wycheproof, ACVP, `h2spec`, BoringSSL `runner` | various | Apache-2.0 / BSD | CI only, never distributed |

Two entries want attention. The **public-suffix list is MPL-2.0**, which is a
weak copyleft: Go handles this by generating a table from it and shipping the
result under Go's licence with the PSL's notice retained, and `burrow` does the
same, but the generated artefact's status is worth confirming rather than
assuming. And the CI-only row matters: BoringSSL's test runner and the
conformance suites are never redistributed, so their licences impose nothing on
`burrow`'s users — but the distinction between "we build against it" and "we
ship it" must stay true, which means the amalgamation generator must never pull
from `burrow-conformance`.

A `licences/` directory holds every third-party notice, `burrow-gen licences`
regenerates an SPDX and a CycloneDX SBOM from it, and CI fails if a source file
references an input with no corresponding notice.
→ [15](15-build-deploy.md) §8

## 5. Contributions and provenance

Because `burrow` is a derivative work and its value depends on being auditably
derived from Go, provenance discipline is stricter than a typical project's:

- **DCO sign-off** (`Signed-off-by:`) on every commit, not a CLA. BSD-3 needs no
  copyright assignment, and a CLA would deter exactly the contributors this
  project needs.
- **Every ported file names its upstream.** §2's header check enforces it. A PR
  that adds a derived file without naming the Go source it came from does not
  merge.
- **Contributors must state, in the PR template, whether the code is a port of
  Go's or original.** Original contributions to the substrate (the runtime, the
  PAL, the allocators) are `burrow`'s own; ports are Go's with `burrow`'s
  changes. Mixing them silently in one file is what makes a codebase's
  provenance unauditable, so the header format distinguishes them.
- **No code from GPL or LGPL sources, ever**, and no code from other Go
  reimplementations (TinyGo, gccgo, GopherJS) whose licences differ. The
  upstream is Go's `std` tree and nothing else.
- **AI-assisted contributions follow the same rule as any other**: the
  contributor is responsible for provenance and must be able to name the
  upstream file. A port that cannot be diffed against a real Go file is not a
  port.

## 6. Export control

`burrow` contains cryptography, which puts it in scope for export regulation,
and this is the item most open-source projects discover late.

Under the US EAR, publicly available open-source encryption software qualifies
for the TSU exception (§740.13(e)), which requires a one-time notification
email to BIS and the NSA giving the URL where the source is available, at or
before the time of first publication. It is an email, it takes ten minutes, and
it is the difference between compliant and not.

So: **send the §740.13(e) notification when `crypto` first ships publicly**,
record it in the repository, and note in the README that `burrow` contains
cryptographic software and that recipients are responsible for compliance with
their own jurisdiction's import and use rules — the standard notice, in the
standard place. France, China, Russia and others have import or use
restrictions that are the user's problem but should be flagged rather than left
to be discovered.

Go itself carries this notice and its wording is a reasonable model.

## 7. Checklist

Things to do, in order, with the first two before any code ships:

1. **Get a lawyer to review §1 and §3, once, before 0.1.** Specifically: the
   derivative-work conclusion, the PATENTS "further modification" clause, the
   verbatim doc-comment position, and the trademark posture. A few hours of
   someone's time de-risks the whole project.
   → [17](17-open-questions.md) §14
2. `LICENSE`, `PATENTS`, `NOTICE` in every repository from the first commit.
3. Per-file header check in CI, with the upstream Go path and version, from the
   first ported file. Retrofitting provenance is far harder than recording it.
4. `licences/` directory, SBOM generation, and the reference-without-notice CI
   check, wired up during P0.
5. DCO enforcement and the PR template's provenance question, from the first
   external contribution.
6. `burrow_license()` and the notice in `burrow.h`, before 0.1 ships an
   amalgamation.
7. EAR §740.13(e) notification, before `crypto` is published.
8. Confirm the MPL-2.0 status of the generated public-suffix table before
   `net/http/cookiejar` ships.
9. A `SECURITY.md` with a disclosure address, before 0.9 exposes `net/http` to
   the internet. → [16](16-milestones.md) §6

None of this is interesting work and all of it is cheap at the start and
expensive later. The per-file provenance headers in particular are the thing
that makes `burrow` defensible — not just legally, but technically: a port you
can diff against its original is a port someone can audit, and that is the same
property the licence compliance happens to require.
