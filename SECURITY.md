# Security

## Reporting

Use GitHub's private vulnerability reporting on this repository, under Security then Report a vulnerability. That opens a private thread with the maintainers and nothing is visible until it is fixed.

If that is not available to you, mail security@burrow.dev. Include what you found, how to reproduce it, and what you think the impact is. You will get an acknowledgement within three days and a real answer within a week.

Please do not open a public issue for a vulnerability, and please do not post it anywhere else while it is unfixed.

## What counts

burrow is a port of Go's standard library, which means most of its attack surface is somebody else's design and a good fraction of its bugs are ours.

These are in scope:

- Memory safety in any burrow code. Out of bounds access, use after free, double free, uninitialised reads, integer overflow leading to any of those.
- Any divergence from Go in a security relevant path. If Go rejects an input and burrow accepts it, that is a bug even when neither one crashes, and it is the class of bug this project is most worried about.
- Anything in `crypto/*`. Timing variability on secret dependent paths, wrong constant time reasoning, a validation check that Go performs and we skip.
- Request smuggling, header injection, path traversal and the rest of the `net/http` family.
- Anything that lets an attacker controlled input reach an allocator in an unbounded way.

These are not, at least not yet:

- Findings against unreleased code on a branch.
- Denial of service through resource exhaustion where Go has the same behaviour. Report it anyway, but it goes in the normal issue tracker unless we are worse than Go.
- Anything requiring a compromised build environment.

## Current state

burrow has not been through an external security review and is not ready for production use. Two reviews are planned and budgeted before 1.0, one for the runtime and memory layer and one for crypto and TLS, and neither has happened. Until they do, treat every claim in this repository as unverified by anyone other than us.

`crypto/*` in particular is unreviewed. It is a from scratch port of code whose correctness matters more than most, and the fact that it passes Wycheproof and the ACVP vectors is necessary rather than sufficient. Do not use it to protect anything you care about yet. This notice will be removed when the review is done and its report is published here.

## Supported versions

Pre 1.0, only the latest release gets fixes. After 1.0 this section will say something more useful.
