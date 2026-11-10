# Security Policy

## Reporting a vulnerability

Please report security problems privately, not in a public issue:

1. Open the [Security tab](https://github.com/open-nt/Nt6/security) of the repository.
2. Choose **Report a vulnerability** and describe the problem, with a small program or input that shows it if you can.

You will get a reply on the report itself. Once a fix is released, the advisory is published and credits you, unless
you prefer otherwise.

## What counts

Nt parses data from untrusted peers, so these are in scope in particular: memory safety errors, crashes and unbounded
resource use reachable from the network or from a file in Nt Network (including TLS handling), Nt HTTP Server,
Nt WebSockets, Nt D-Bus, Nt SVG, and the parsers of Nt Core (JSON, `QDataStream`, `QSettings`, `QUrl`, resources).

A missing feature, or a difference from Qt that the documentation lists, is a normal
[issue](https://github.com/open-nt/Nt6/issues/new/choose).

## Supported versions

Nt is in its release candidates. Fixes go to `main` and into the next release; older release candidates are not
patched.

## Checking a download

Release packages carry [SLSA](https://slsa.dev) level 3 provenance. The README shows how to
[verify a package](README.md#release-provenance) with `slsa-verifier`.
