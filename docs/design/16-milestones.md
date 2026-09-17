# 16 — Phasing, effort, and performance

The honest summary first: this is a multi-year project that cannot be
front-loaded, cannot be usefully parallelised until the substrate is finished,
and is worthless until a specific and fairly deep slice of it works end to end.
It is also, after that point, embarrassingly parallel across roughly a hundred
packages.

That shape — a long narrow neck followed by a wide fan-out — determines
everything below.

## 1. The measured work

From [01](01-scope.md), with the derivations restated so the effort model can
be checked rather than believed:

| | Packages | Decls | Non-test LOC | Test LOC |
| --- | --- | --- | --- | --- |
| Go 1.27.1 `std` | 180 | 23,730 | 628,165 | 440,736 |
| less `syscall` (95% generated tables) | −1 | −4,149 | −~180,000 | — |
| less `runtime` (replaced, not ported) | −1 | −~240 | −~100,000 | — |
| **Real portable library code to port** | **~171** | **~14,800** | **~347,000** | **~440,000** |
| plus vendored `x/net` (h2, h3, QUIC, IDNA, DNS) | — | — | +66,215 | — |
| plus the new substrate: runtime, PAL, reflect, allocators | — | — | +~35,000 | +~15,000 |

So: **roughly 450,000 lines of C to write, and roughly 455,000 lines of tests
to bring across.** The tests are not a rounding error on the estimate; they are
half of it, which is why [14](14-conformance.md) §2 mechanises their
translation rather than treating it as typing.

Two things make this less alarming than the raw number:

- A large share of the non-test lines are **generated**: `unicode` tables,
  `debug/elf` constants, `syscall`'s errno and struct definitions, the type
  descriptors, `idna` and `cookiejar` tables, the HPACK Huffman table. Writing
  a 300-line emitter that produces 15,000 lines of correct tables is not the
  same work as writing 15,000 lines.
- Around half the test lines are table-driven in a shape
  `burrow-gen tests` can recognise.

Net, the hand-written portion is closer to **~250,000 lines of implementation
and ~200,000 of test**, weighted heavily toward the mechanical end. That is a
large project and a legible one.

## 2. The phases

```
 P0  substrate            ████████                        blocking, serial
 P1  Tier 0 + Tier 1      ████████████████████            parallel, wide
 P2  Tier 2 (OS)          ██████████                      semi-parallel
 P3  Tier 4 (crypto)      ████████████                    specialist, serial-ish
 P4  Tier 3 (net/http)    ██████████████                  small team, deep
 P5  Tier 5 (go/*, etc.)  ████████████████                parallel, wide, late
 P6  1.0 hardening        ██████████                      review, fuzz, docs
 ──  post-1.0             QUIC/HTTP3, more platforms, perf
```

**P0 — substrate.** The narrow neck, and the part that cannot be parallelised
past about three people because every decision in it constrains every other.
Deliverables: the C dialect and CI skeleton ([03](03-c-dialect.md)), core types
([04](04-core-types.md)), the allocator interface and five backends
([05](05-memory.md)), the scheduler, channels, `select`, `defer`/`panic`/
`recover`, the netpoller, `sync` ([06](06-runtime.md)), the type-descriptor DSL
and registry ([07](07-reflect.md)), the PAL's 68 entry points
([10](10-packages-os.md) §2), the naming rules and coverage gate
([08](08-naming-abi.md)), `testing`, and — critically — the differential fuzzing
harness ([14](14-conformance.md) §3).

The gate is `06` §12's eleven-step construction order and the Tier 0 gate: a
goroutine sending on a channel while another `select`s on it, with a `defer`
running on panic, under TSan, on all three primary platforms; plus
`fmt.Printf("%v")` on a declared struct.

Three things belong in P0 that instinct puts later, and all three are
load-bearing: the **differential fuzzer** (because it makes every subsequent
line self-verifying), the **`cosmocc` hello-world demo** (because it makes the
project legible to strangers a year before it is finished, and recruiting is
the real constraint → [15](15-build-deploy.md) §6), and the **docs
toolchain** — `burrow-gen doc`, the example extractor, and the CI job that
compiles and runs every code block in `docs/`. The docs toolchain is in P0 for
exactly the reason the fuzzer is: it only works if nothing was ever written
without it, and retrofitting documentation across 180 packages at P6 is the
failure mode this plan is built to avoid. → [19](19-docs.md) §7

**P1 — Tier 0 and Tier 1.** The fan-out: ~110 packages of pure computation, no
OS, no concurrency. `strings`, `bytes`, `strconv`, `math`, `math/big`,
`unicode`, `sort`, `slices`, `maps`, `encoding/*`, `compress/*`, `archive/*`,
`image/*`, `hash/*`, `regexp`, `fmt`, `errors`, `io`, `bufio`, `path`,
`container/*`, `net/netip`, `net/url`, `net/textproto`.

This is where [09](09-packages-pure.md) §10's observation pays: each package is
an independent unit with an inherited test suite and a mechanical merge
criterion, so the phase absorbs as many contributors as show up. The
six-step porting recipe is the onboarding document.

**P2 — Tier 2.** `os`, `io/fs`, `time`, `path/filepath`, `os/exec`, `os/signal`,
`embed`, `syscall`'s generated surface. Depends on the PAL being right, and the
Windows path and filesystem semantics are the long pole.
→ [10](10-packages-os.md) §7

**P3 — Tier 4.** Crypto. Runs mostly in parallel with P2 because it depends on
Tier 1 (`math/big`, `encoding/asn1`) and not on the OS beyond `crypto/rand`.
Needs different people: this phase wants someone who has written constant-time
code before, and the Wycheproof/ACVP vectors make their work verifiable from
day one. → [12](12-packages-crypto.md)

**P4 — Tier 3.** `net`, then `net/http` with h2. Small team, deep work, the
most concurrency-intensive code in the library, and the phase where the
scheduler gets its real test. Gated on P3 for TLS.
→ [11](11-packages-net.md) §5

**P5 — Tier 5.** `go/*`, `debug/*`, templates, `database/sql`, `log/slog`. Wide
and parallel again, and the `gofmt`-over-`$GOROOT` and
`go/types`-over-`$GOROOT` gates make it self-verifying.
→ [13](13-packages-go.md) §6

**P6 — hardening.** The coverage gate at 100%-minus-ledger, the full sanitiser
matrix green, the external security review (§6), the benchmark table published,
the size table measured, all Tier A and B platforms green. No new packages.

Note what is *not* in P6: writing the documentation. Reference pages, examples
and package guides are item 8 of the per-package gate
([14](14-conformance.md) §8), so a package that reaches P6 already has them.
What P6 adds is the hand-written layer that cannot be produced per-package —
the tour, the cookbook, the porting guide and the three migration essays
([19](19-docs.md) §3) — plus one editorial pass over the whole corpus.

## 3. What ships before 1.0

A library that is only useful when complete will never be complete, so there
are three usable releases before 1.0:

| Release | Contents | The pitch |
| --- | --- | --- |
| **0.1 "substrate"** | P0 + `strings`, `strconv`, `fmt`, `errors`, `io`, `bufio`, `time`, `sort`, `slices`, `maps` | goroutines, channels and Go's string/format library in one C file — already better than what most C projects hand-roll |
| **0.5 "batteries"** | + all of Tier 1, Tier 2 | `os`, `io/fs`, `encoding/json`, `log/slog`, `compress/*`, `regexp` — enough for real CLI tools and file processing |
| **0.9 "networked"** | + Tier 3 (h1+h2), Tier 4 | HTTP/2 + TLS 1.3 client and server; the headline, behind a not-yet-reviewed banner |
| **1.0** | + Tier 5, P6 | the coverage gate passes, the review is done, the ledger is the only qualification |

0.1 is the important one. It should ship early and be genuinely useful, because
it is the only way to find out whether the ergonomics of
[04](04-core-types.md) and [05](05-memory.md) survive contact with users — and
if they do not, the time to learn that is before 300,000 lines depend on them.
Every API decision in the substrate is revisable up to 0.5 and frozen after.

## 4. Post-1.0

Deliberately out of 1.0, tracked, and not blockers:

1. **QUIC and HTTP/3.** `x/net/quic` plus `x/net/http3`: packet crypto,
   congestion control, loss recovery, and a state machine of roughly TLS's
   complexity. Behind `BURROW_ENABLE_HTTP3`, conformance via
   `quic-interop-runner` against the fifteen-plus implementations that suite
   tracks. In scope for fidelity, last in order.
   → [11](11-packages-net.md) §3
2. **Hand-written assembly** beyond intrinsics, for crypto and `memmove`-class
   routines, behind `BURROW_ASM=1`. → [12](12-packages-crypto.md) §6
3. **Tier 3 and 4 platforms**: solaris, aix, plan9, openbsd/netbsd/dragonfly,
   loong64, mips variants, ppc64. Community-maintained, CI best-effort.
   → [10](10-packages-os.md) §3
4. **Optional copying stacks**, if a credible approach appears. Today the answer
   is fixed 256 KB guard-paged stacks, which is the second of three
   acknowledged compromises versus Go. → [06](06-runtime.md) §4
5. **Go-source-to-`burrow` transpilation.** `burrow-gen --from-go` currently
   translates declarations; translating bodies is a different and much larger
   project. Interesting, not promised. → [17](17-open-questions.md) §5
6. **FIPS 140-3 CMVP submission**, if funded. → [12](12-packages-crypto.md) §4

## 5. Performance

The published targets, referenced from [05](05-memory.md) §9,
[03](03-c-dialect.md) §4 and [14](14-conformance.md) §6. All ratios are
`burrow / Go`, same hardware, same workload, geometric mean over the inherited
`Benchmark` functions in the relevant packages.

| Area | Target | Why it should hold, or not |
| --- | --- | --- |
| Pure computation (`strconv`, `math`, `regexp`, `compress`, hashes) | **0.9–1.1×** | same algorithms, C compilers at least as good as Go's; some wins from cross-package inlining in the amalgamation |
| Slice and string operations | **0.95–1.15×** | bounds checks cost the same as Go's; Go's compiler elides more of them, C's inliner recovers some |
| `Map` | **1.0–1.3×** | same Swiss-table design; Go's compiler specialises hot map types, we dispatch through descriptors |
| Allocation-heavy paths | **0.5–2.0×** | arenas *beat* Go's GC on request-scoped work; `heap` is worse than Go's size-class allocator on churn |
| Channel send/recv, uncontended | **0.9–1.2×** | same algorithm, our own park/ready |
| Goroutine creation | **worse in memory, similar in time** | 256 KB reserved (lazily committed) vs Go's 8 KB growable; the cost is address space and page faults, not cycles |
| Context switch | **1.0–1.5×** | hand-written assembly is comparable; `ucontext` fallback is 5–10× worse and is not a shipping configuration |
| Preemption latency under CPU-bound loops | **worse** | cooperative at safe points; a tight loop with no calls is not preempted |
| `net/http` throughput and p99 | **≤ 1.5×** | release gate; this is the number people will quote |
| TLS handshake | **1.0–1.3× with intrinsics, 5–20× without** | why §6 of [12](12-packages-crypto.md) is not optional |
| Startup time | **much better** | no GC init, no runtime, no type metadata registration |
| Binary size | **much better** | → [15](15-build-deploy.md) §7 |

Three rows are worth dwelling on because they are the honest bad news:
goroutine memory footprint, preemption latency, and `heap` under churn. All
three trace to the same root cause — C cannot move a stack and cannot rewrite
a pointer it does not know about — and all three are stated in
[06](06-runtime.md) and [05](05-memory.md) rather than hidden here.

**Procedure.** Benchmarks are translated in the same commit as the package.
Results are published per release as a table, not a summary number. A >5%
regression on any individual benchmark blocks a merge; the baseline is stored
in the repository and updated deliberately. `bench` runs on dedicated hardware,
pinned cores, performance governor — a benchmark suite that is noisy is a
benchmark suite that gets ignored.

## 6. Security review

Two reviews, both by external firms, both scheduled and budgeted from the
start rather than discovered at the end.

**Review A — crypto**, before `crypto` loses its banner. Scope:
`crypto/tls`'s four handshake state machines, `crypto/x509`'s chain building
and DER parsing, the constant-time discipline in
[12](12-packages-crypto.md) §3, the FIPS module boundary and self-tests, and
the accelerated-vs-portable equivalence. Must be a firm that does
cryptography specifically. Gate 9 of [12](12-packages-crypto.md) §8.

**Review B — memory safety and parsers**, before 1.0. Scope: the runtime's
assembly and stack handling, the allocator and ownership annotations, every
Tier 3 parser (HTTP/1 and /2 request parsing, chunked decoding, HPACK, URL,
MIME header), and the `archive/*` and `image/*` decoders. These are the
untrusted-input surfaces, and memory safety is the one axis where `burrow` is
strictly worse than Go. → [03](03-c-dialect.md) §4

Between and around them, continuously: the fuzzing corpora from
[14](14-conformance.md) §3, a public security policy with a disclosure address,
CVE regression tests for every historical Go vulnerability written *before* the
corresponding package, and a `BURROW_HARDENED=1` build mode documented as the
recommended default for internet-facing deployment.
→ [11](11-packages-net.md) §7

Until Review A is complete, `crypto` and `net/http` carry a banner in the
README, in the headers, and in the release notes, in the same words each time:
*not yet externally reviewed; do not use against untrusted adversaries.* The
banner comes off when the review is done and its findings are addressed, not
when the tests go green.

## 7. The real constraint

It is not lines of code. Mechanical porting with an inherited test suite and a
differential oracle is the most tractable form of large-scale software work
there is — the specification is executable, the reference implementation is
readable, and correctness is decidable.

The constraint is **sustained attention over years, by people who can hold both
Go's semantics and C's hazards in mind at once**, on a project whose value is
zero until a fairly deep slice of it works and then large. That is why 0.1
ships early, why the `cosmocc` demo is in P0, why the porting recipe is written
as onboarding documentation, and why the gates are mechanical: a contributor
should be able to pick a package, run one command to see what is missing, and
know without asking anyone whether their work is done.
