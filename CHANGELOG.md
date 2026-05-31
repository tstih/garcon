# Changelog

## [v0.0.8] - Unreleased
### Added
- Added HTTP keep-alive support with explicit HTTP/1.0 and HTTP/1.1
  connection policy handling.
- Added parsed request headers to `http::request` and exposed them to shared
  modules through the ABI and C++ wrapper layer.
- Added arbitrary response headers to `http::response` and exposed them to
  shared modules through the ABI and C++ wrapper layer.
- Added a low-overhead `host-guard` shared module to protect requests with
  `Host` allowlists.
- Added a low-overhead `cors` shared module to answer preflight requests and
  decorate downstream API responses.
- Added a low-overhead `header-guard` shared module to protect `/api/*`
  routes with header checks.
- Added per-request access logging to stdout through the new `access_log`
  seam.
- Added coarse per-IP accepted-connection limiting ahead of the bounded work
  queue.
- Added automatic HSTS injection for HTTPS responses.
- Added explicit TLS cipher-suite selection instead of relying on OpenSSL
  defaults.
- Added a repository `VERSION` file and `--version` CLI output.
- Added isolated `unit_http` coverage for request parsing, framing, buffers,
  and response-header validation.
- Added focused keep-alive smoke coverage.

### Refactored
- Extracted per-connection request servicing into `app::connection_handler`.
- Moved the reusable `static_files` service fully under `lib/static_files/`
  and linked the shared module against that library target.
- Switched runtime diagnostics and access-log string assembly to `std::format`
  where the current toolchain supports it cleanly.
- Replaced several status-code-plus-payload APIs with `std::expected`,
  including request parsing, header framing, listener accept, and module-config
  loading.
- Replaced the in-process pipeline `module_result` enum/optional pairing with a
  variant-backed representation.
- Replaced source globs in `src/CMakeLists.txt` with explicit source lists.

### Security
- Validated response headers before serialization so CR/LF injection attempts
  are rejected or dropped.
- Added graceful write-side socket shutdown before close.

### Build
- Added opt-in AddressSanitizer, UBSan, and ThreadSanitizer build options.

### Verified
- Added focused smoke coverage for guarded host allowlisting through the new
  host-guard module.
- Added focused smoke coverage for CORS preflight handling and downstream
  response-header propagation through the new `cors` module.
- Added focused smoke coverage for guarded API access through the new
  header-guard module.
- Added focused smoke coverage for connection reuse and explicit close
  behavior.
- Added focused smoke coverage for per-IP connection admission limiting.
- Extended HTTPS smoke coverage to verify HSTS.

### Documented
- Refreshed the main README, architecture guide, tutorial, and
  module-development tutorial for the keep-alive-capable gateway runtime.

## [v0.0.7] - Unreleased
### Added
- Added the public shared-module ABI in `include/garcon/module_abi.h`.
- Added dynamic request-module loading from a configured `modules.d/` directory.
- Added the first shared module under `lib/static_files/`.
- Added a low-overhead `route-table` shared module with default `healthz`,
  `readyz`, and `/api/*` pass-through rules.
- Added `--modules-dir` to override the default module-configuration directory.

### Refactored
- Removed the built-in `src/modules/static_files_module.*` request module from the main server.
- Kept the server focused on transport, TLS, parsing, and pipeline orchestration while moving static-file request behavior behind the shared-module boundary.

### Verified
- Preserved the existing HTTP security, HTTPS, and concurrency smoke-test coverage after switching the default pipeline to shared modules.
- Added focused smoke coverage for the default route-table module behavior and lexical module ordering.

### Documented
- Refreshed the main README, architecture guide, tutorial, and Debian packaging
  notes for the first shared-module runtime.

## [v0.0.6] - Unreleased
### Added
- Introduced a small ordered request pipeline with four module outcomes: `pass`, `respond`, `upgrade`, and `error`.
- Added `app::request_module` and `app::request_pipeline` as the new request-handling core.
- Extracted `static_files_module` as the first concrete pipeline module.

### Refactored
- Returned `static_files` to a reusable file-serving service instead of letting it double as the root request abstraction.
- Wired the server runtime to build its root handler from the ordered pipeline rather than binding directly to the static-files path.

### Verified
- Preserved the existing HTTP security, HTTPS, and concurrency smoke-test coverage after moduleizing the static-files path.

## [v0.0.5] - Unreleased
### Refactored
- Introduced an explicit accept-result model so listener failures are classified as retryable or fatal instead of collapsing into invalid sockets.
- Added a request-handler abstraction and moved static file serving behind that strategy seam.
- Added a stream-factory abstraction so the server runtime no longer depends directly on concrete plain-TCP or TLS stream types.
- Moved command-line parsing into a dedicated `app::command_line_parser`.
- Made `http::response` fully owning by storing the reason phrase as `std::string`.

### Operational
- Added stderr-backed runtime diagnostics for accept failures, queue-full rejections, connection-stage failures, and unexpected worker callback exceptions.
- Added retry backoff for retryable accept failures to avoid tight busy-spin behavior.

### Verified
- Extended concurrency smoke coverage to assert that bounded overload now produces an explicit queue-full runtime diagnostic.

## [v0.0.4] - Unreleased
### Added
- Bounded concurrency using a fixed-size pool of standard C++ worker threads and a bounded accepted-connection queue.
- New `app::work_queue` and `app::worker_pool` modules to keep queueing, worker lifetime, and per-connection handling separated cleanly.
- Explicit runtime tuning for worker count and queue capacity through `--workers` and `--queue-capacity`.
- Shared HTTP and HTTPS request handling through the existing transport-neutral stream abstraction.

### Verified
- Added end-to-end concurrency smoke coverage for slow HTTP clients, stalled TLS handshakes, and bounded overload recovery.
- Preserved the existing HTTP security and HTTPS smoke-test coverage under the new worker-pool runtime.

### Documented
- Added the `v0.0.4` concurrency architecture record in `docs/CONCURRENCY-SCALABILITY.md`.
- Added `docs/ARCHITECTURE.md` to describe how the main runtime modules fit together.
- Added `docs/TUTORIAL.md` with build, configuration, HTTP, HTTPS, packaging, and concurrency guidance.

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
