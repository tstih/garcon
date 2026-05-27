# Changelog

## [v0.0.3] - Unreleased
### Added
- Direct HTTPS mode using OpenSSL while keeping the existing HTTP and static-file pipeline intact.
- A transport-neutral stream abstraction with plain TCP and TLS adapters so the HTTP layer stays protocol-agnostic.
- CLI support for `--tls-cert` and `--tls-key`, with HTTPS defaulting to port `8443`.
- Debian packaging support through `make dist`, with `.deb` artifacts written to `bin/`.

### Verified
- Added end-to-end HTTPS smoke coverage for startup, static content delivery, TLS handshake timeout handling, TLS client disconnect handling, and rejection of plain HTTP on the HTTPS port.

## [v0.0.2] - Unreleased
### Security
- Security-hardening upgrade release for the `v0.0.1` baseline.
- Tightened request parsing so malformed request lines are rejected early with `400 Bad Request`.
- Added connection time limits and safer socket write handling so stalled or disconnected clients fail closed instead of hanging or terminating the process.
- Restricted static file resolution to the configured document root, including rejection of traversal attempts and symlinks that escape the root.
- Bounded file serving with an 8 MiB limit so oversized requests fail safely instead of requiring unbounded memory.
- Made the development server local-only by default and aligned startup logging with the actual bind address.

## [v0.0.1] - 2026-01-25
### Added
- Minimal single-threaded HTTP/1.1 server that serves static files from `www/`.
- Basic request-line parsing (method + target).
- HTTP response serialization with content length and content type.
- Simple project layout with `net/` and `http/` modules and CMake build.
