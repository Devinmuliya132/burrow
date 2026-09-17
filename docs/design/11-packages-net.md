# 11 — Tier 3: networking

Sixteen packages, 1,352 exported declarations, 51,061 lines, and **77,362 lines
of tests** — the highest test-to-code ratio in the library, which tells you
where the bugs are. Plus 66,215 lines of vendored `golang.org/x/net`
(`http2`, `http3`, `quic`, `idna`, `dns`) that `net` and `net/http` require and
that Go does not expose publicly.

`net/http` is the package most people would come to `burrow` for. A production
HTTP/1.1+2+3 client and server, with TLS 1.3, in portable C, with no
dependencies, in a single file, is not otherwise available. It is also the
package with the deepest dependency stack — 186 transitive `std` dependencies —
so it lands late by necessity.

## 1. `net` (381 decls, 19,082 lines, 24,255 test lines)

The foundation, and the package where the netpoller design
([06](06-runtime.md) §8) either works or does not.

**Structure.** `net.Conn`, `net.Listener`, `net.Addr`, `net.PacketConn` are
interfaces; `TCPConn`, `UDPConn`, `UnixConn`, `IPConn` implement them over an
internal `netFD` that pairs a PAL socket with a `pollDesc`. Go's
`internal/poll.FD` handles the reference counting that prevents a `Read`
racing a `Close` from touching a reused descriptor; we port that mechanism,
because the alternative is a use-after-close bug class that only shows up under
load.

**Deadlines.** `SetDeadline`/`SetReadDeadline`/`SetWriteDeadline` are
per-connection timers that interrupt a parked goroutine with
`os.ErrDeadlineExceeded`. They must be settable from another goroutine while a
read is in flight, and resettable to zero to disarm. This interacts with the
netpoller and the timer heap and it is the most common source of subtle bugs in
any network runtime. It gets its own `synctest`-based deterministic test suite.
→ [06](06-runtime.md) §10

**Name resolution** is the genuinely awkward part, and Go's own solution is
instructive: Go ships *two* resolvers, a cgo one calling `getaddrinfo` and a
pure-Go one that parses `/etc/resolv.conf`, `/etc/hosts` and
`/etc/nsswitch.conf` and speaks DNS itself, choosing between them at runtime
based on configuration complexity. `burrow` has no cgo distinction but the same
dilemma:

- **Default: the PAL's `getaddrinfo`.** Correct for split-horizon DNS, mDNS,
  NSS modules, VPN resolvers and macOS/Windows system resolvers — all the cases
  a hand-rolled resolver gets wrong. Its problem is that it blocks a thread,
  which we handle by running it on a dedicated resolver thread pool rather than
  an M (as Go does with its cgo resolver).
- **Available: the pure resolver**, ported from Go's, including the vendored
  `x/net/dns/dnsmessage` package, `Resolver.PreferGo`, `GODEBUG=netdns=` control
  and the same `resolv.conf` parsing. Needed for wasm, for static binaries on
  systems where NSS is unavailable, and for `net.Resolver`'s custom `Dial`
  hook, which is public API and must work.

Both, with Go's selection logic and Go's `GODEBUG` knobs, because that
selection logic encodes years of learned platform behaviour.

**`net/netip`** (80 decls, 1,684 lines) is the modern value-type address API —
`Addr`, `AddrPort`, `Prefix` — comparable, allocation-free, with a compact
16-byte representation and an interned zone. Port it early: it has 41
dependencies, it is self-contained, it has 3,536 lines of tests, and it is
strictly nicer than `net.IP`. `net.IP`'s legacy 4-vs-16-byte duality and its
`String()` formatting rules must also be reproduced, warts included.

**Unix sockets, raw sockets, multicast, `SO_REUSEPORT`, `TCP_NODELAY`,
keepalive config (`KeepAliveConfig`, Go 1.23+), `Dialer.Control`,
`ListenConfig.Control`, `FileConn`/`FileListener`, `SyscallConn`** — all
public API, all required. `net`'s 381 declarations are wide rather than deep.

## 2. `net/url` (62 decls) and IDNA

`net/url` is mechanical and full of specified edge cases: `Parse` vs
`ParseRequestURI`, `Opaque` URLs, the `RawPath`/`Path` duality that preserves
encoding, `Userinfo`, `Values` encoding order, and `JoinPath`/`ResolveReference`
semantics. 2,982 lines of tests, all table-driven, all mechanically
translatable. Do it early — it has only 64 dependencies and `net/http` needs it.

`vendor/golang.org/x/net/idna` implements IDNA2008 with UTS-46 mapping and is
needed by `net/http` (host header handling) and `net`. It comes with generated
tables, which we regenerate with a C emitter exactly as with `unicode`.
→ [09](09-packages-pure.md) §3

## 3. `net/http` (505 decls, 18,605 lines, 34,394 test lines)

The largest and most valuable target in the project. Notes on the parts where
fidelity is hard-won.

**Server.**
- `ServeMux`'s Go 1.22+ pattern syntax — method prefixes (`GET /path`),
  wildcards (`{name}`, `{path...}`), precedence rules between overlapping
  patterns, and the conflict detection that panics on ambiguous registration.
  This is newer, more specified and more tested than the old mux and is a
  common source of surprise.
- `http.Server`'s lifecycle: `Serve`, `ListenAndServe`, `Shutdown` (graceful,
  with connection draining), `Close`, `RegisterOnShutdown`, `BaseContext`,
  `ConnContext`, and the read/write/idle/header timeouts.
- `ResponseWriter`'s contract: header mutation only before `WriteHeader`,
  implicit `WriteHeader(200)` on first `Write`, content-type sniffing via
  `DetectContentType` (which has its own precise algorithm), automatic
  `Content-Length` vs chunked selection, and the `Flusher`/`Hijacker`/
  `CloseNotifier`/`ReaderFrom`/`ResponseController` extension interfaces.
- Request body semantics: `Body` is always non-nil on the server, must be
  drained or closed, `MaxBytesReader`, `ParseMultipartForm`'s spill-to-disk
  behaviour, and `Request.Clone`/`WithContext`.
- Protocol-level correctness that has had CVEs and therefore has pinned tests:
  header count and size limits, `Transfer-Encoding`/`Content-Length` conflict
  rejection (request smuggling), chunked-encoding edge cases, header
  continuation-line rejection, and `Expect: 100-continue` handling.

**Client.**
- `http.Transport`'s connection pool: per-host idle connection lists,
  `MaxIdleConns`/`MaxIdleConnsPerHost`/`MaxConnsPerHost`, `IdleConnTimeout`,
  the `wantConn` queue, connection reuse eligibility rules, and the
  `readLoop`/`writeLoop` goroutine pair per connection. This is the most
  concurrency-intensive code in the standard library and the best stress test
  `burrow`'s scheduler will get.
- Redirect handling with `CheckRedirect`, header and cookie forwarding rules
  across redirects (which are security-sensitive and specified),
  `Request.GetBody` for replayable bodies.
- `http.Client` timeouts vs `Transport` timeouts vs context cancellation, and
  the interaction between all three.
- `httptrace`'s hook points — 32 declarations of callback that must fire at
  exactly the right moments, which is a de facto specification of the
  transport's internal sequence.

**HTTP/2.** Go bundles `x/net/http2` (11,802 lines in
`net/http/internal/http2`). Required, not optional: `net/http` enables h2 by
default over TLS via ALPN, and a `burrow` that only speaks HTTP/1.1 is not a
faithful `net/http`. The work is HPACK (with its own generated Huffman table),
stream multiplexing, flow control at connection and stream level, priority
handling, `SETTINGS` negotiation, GOAWAY, and the server push API (deprecated
but present). It also has a well-defined external conformance suite, `h2spec`,
which we run in CI — an unusually good position to be in.

**HTTP/3 and QUIC.** Go 1.27 vendors `x/net/http3` and `x/net/quic`.
Together with `x/net/internal/http3` this is the newest and least settled part
of the stack. Position: **in scope, last, behind a build flag, not a 1.0
blocker.** QUIC needs packet-level crypto (`crypto/tls`'s QUIC hooks),
congestion control, loss recovery, and its own state machine — roughly the
complexity of TLS again. It is tracked as an explicit post-1.0 milestone with
`quic-interop-runner` as the conformance mechanism.
→ [16](16-milestones.md) §4

**The rest of the constellation** is small and mechanical once `net/http`
exists: `httptest` (33 decls — and essential, because most of `net/http`'s own
tests use it, so it must come *with* `net/http`, not after), `httputil`
(`ReverseProxy` is the substantive part, and it is widely used),
`cookiejar` (public-suffix list handling, with generated tables), `cgi`,
`fcgi`, `pprof` (an HTTP wrapper over `runtime/pprof`).

## 4. Mail, SMTP, textproto, RPC

`net/textproto` (64 decls) is the shared MIME-header reader that `net/http`,
`net/mail` and `net/smtp` all use; its canonical-header-key caching and
continuation-line handling are exact. Port before `net/http`.

`net/mail` (20 decls) parses RFC 5322 addresses and headers, including the
RFC 2047 encoded-word handling and the deliberately lenient behaviour on
real-world malformed input that its tests encode.

`net/smtp` (27 decls) is small, frozen upstream, and includes the AUTH
mechanisms and STARTTLS flow.

`net/rpc` + `net/rpc/jsonrpc` (62 decls) are reflection-heavy: `Register`
enumerates a type's methods, checks their signatures, and dispatches
dynamically. This is the one stdlib package that genuinely requires
`reflect.Value.Call`, and it is why the thunk mechanism in
[07](07-reflect.md) §6 exists. Both are frozen upstream and low-priority, but
they are in the API and therefore in scope.

## 5. Ordering within Tier 3

```
netip ──┐
url ────┤
textproto┤
        ├──▶ net ──▶ [crypto/tls] ──▶ net/http (h1) ──▶ httptest ──▶ httputil
idna ───┘                                    │                       cgi, fcgi
                                             ├──▶ http2 (h2spec)     cookiejar
mail, smtp ◀─ textproto                      └──▶ http3 + quic       pprof
                                                  (post-1.0)
rpc, jsonrpc ◀─ net + reflect.Call
```

`netip`, `url`, `textproto` and `idna` are Tier-1-shaped and can be done during
Tier 1 with no `net` at all. `net` needs the netpoller. `net/http` needs
`crypto/tls` for anything real, which is why Tier 4 is sequenced before the
HTTP work despite `net/http` being the headline deliverable.

## 6. Conformance, specifically

Networking is the area where `burrow` can lean hardest on *external*
conformance suites, which is a significant advantage over the
translate-Go's-tests approach used elsewhere:

| Suite | Covers | Gate |
| --- | --- | --- |
| Go's own `net/http` tests (34,394 lines) | everything | must pass |
| [`h2spec`](https://github.com/summerwind/h2spec) | HTTP/2 RFC 7540/9113 conformance | 100%, both client and server |
| `quic-interop-runner` | QUIC/HTTP3 against 15+ implementations | post-1.0 |
| Differential: `burrow` client ↔ Go server, and Go client ↔ `burrow` server | wire-level compatibility in both directions | every PR |
| Differential: `burrow` client ↔ nginx, Caddy, Apache, Envoy | real-world interop | nightly |
| `wrk`/`h2load` benchmarks against Go's server | throughput and latency parity | release gate |
| Fuzzing: request parser, chunked decoder, HPACK, URL parser, header reader | untrusted input | continuous |

The cross-product differential test is the important one and it is cheap to
build: run Go's `net/http` server and `burrow`'s against the same handler
logic, drive both with both clients, and compare responses byte for byte
including header order. Header order is not semantically meaningful in HTTP but
it is an excellent canary for having taken a different code path.

## 7. The security posture

`net/http` in C, handling untrusted input from the internet, is the highest-risk
component in the project by a wide margin. Memory safety is the one place where
this port is strictly worse than the original ([03](03-c-dialect.md) §4), and
pretending otherwise would be irresponsible. The commitments:

- Every parser in Tier 3 is fuzzed continuously, seeded from Go's `testdata`
  and from public HTTP request-smuggling corpora.
- The full Tier 3 test suite runs under ASan, UBSan and TSan on every PR.
- Every historical `net/http`, `net/url` and `path/filepath` CVE in Go gets a
  regression test in `burrow`, added before the corresponding code is written.
  Go's release notes and the Go vulnerability database enumerate them; this is
  a finite, known list and it is free security work.
- `net/http` ships behind a documented "not yet externally reviewed" banner
  until an external security review is funded and completed.
  → [16](16-milestones.md) §6
- A `BURROW_HARDENED=1` build mode enables bounds checking that would otherwise
  be elided, stricter header limits, and `-fsanitize=bounds,object-size` in
  production — slower, and the right default for an internet-facing deployment.
