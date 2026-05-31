# Garçon Architecture

Garçon is a small layered HTTP/HTTPS server built to stay understandable as it
gains features. The architecture favors explicit ownership, narrow interfaces,
and conservative concurrency over framework-like abstraction or hidden runtime
machinery.

This document is the consolidated design overview for the current `v0.0.7`
codebase. It covers process startup, transport and TLS handling, bounded
concurrency, the HTTP parsing path, shared-module loading, the ordered request
pipeline, static-file serving, diagnostics, and the main extension seams.

## Architectural goals

The current structure is shaped by a few deliberate constraints:

- keep HTTP and HTTPS on the same serving path after stream creation
- add concurrency around connection dispatch, not by rewriting the protocol
  layer as an event loop
- keep each module focused on one job so future features can be added in small
  steps
- surface operational failures explicitly instead of hiding them behind broad
  catch-all behavior
- use RAII and value-oriented ownership to make lifetime boundaries obvious

Several things are intentionally not implemented yet:

- HTTP keep-alive or request pipelining
- request bodies, richer header parsing, and upstream proxying
- WebSocket or other connection-upgrade support beyond the placeholder
  `upgrade` outcome
- one thread per connection
- an `epoll` or reactor-style runtime

## System overview

At a high level, Garçon runs one accept loop, hands accepted sockets to a
bounded queue, and lets a fixed worker pool drive a single-request serving
pipeline.

The major layers are:

1. process startup and runtime configuration
2. server runtime and connection dispatch
3. concurrency primitives
4. networking and transport abstraction
5. TLS integration
6. HTTP framing and message types
7. application pipeline and modules
8. reusable static-file service
9. runtime diagnostics

The key design choice is that concurrency stops at the socket-dispatch
boundary. Once a worker receives a client socket, the rest of the flow is the
same sequential request-serving path regardless of whether the transport is
plain TCP or TLS.

## 1. Process startup and configuration

`src/main.cpp` is intentionally thin. Its job is to:

- build a `command_line_parser`
- parse CLI arguments into `app::server_config`
- construct `app::server`
- call `server::run()`

The real startup policy lives in `src/app/command_line.*`. That layer decides:

- bind address
- port
- whether HTTPS is enabled
- document root
- worker count
- queue capacity
- TLS certificate and key paths

Normal startup always goes through this parser, which means the effective
defaults come from code rather than from external shell scripts or packaging.

Important runtime defaults:

- HTTP defaults to port `8080`
- HTTPS defaults to port `8443`
- the default bind is `127.0.0.1`
- worker threads default to `std::thread::hardware_concurrency()`
- if the platform reports `0`, worker threads fall back to `4`
- queue capacity defaults to `worker_threads * 64`

The CLI rejects malformed or zero worker and queue values, and it requires
`--tls-cert` and `--tls-key` to be provided together. This keeps the runtime
configuration valid before networking starts.

Module configuration discovery follows a similarly small policy:

- prefer `modules.d/` next to the executable
- then try a local `modules.d/`
- then try `/etc/garcon/modules.d`
- allow `--modules-dir` to override all of the above

Document-root discovery also follows a small policy:

- prefer a local `www/` directory during development
- fall back to `/usr/share/garcon/www` for installed packages

That alignment between the runtime model and the packaging layout keeps local
development and installed operation close to each other.

## 2. Server runtime

`app::server` is the integration point for the whole runtime. It owns or
coordinates:

- the TCP listener
- the root request handler
- the transport-selection policy
- the runtime diagnostics sink
- the bounded work queue
- the fixed worker pool

The current root request handler is not a monolithic function. It is an
ordered `request_pipeline`, populated at startup from a configured
`modules.d/` directory. That means `server` coordinates the serving path
without embedding application behavior directly in `server.cpp`.

The runtime loop is split in two parts:

- the accept side accepts sockets and pushes them into the queue
- the worker side pops sockets and runs the per-connection flow

This keeps the concurrency policy isolated from HTTP parsing and file-serving
logic.

## 3. Concurrency model

Garçon uses a classic producer/consumer design.

### Accept side

The accept loop owns one listening socket and repeatedly calls
`net::listener::accept()`. Each result is classified explicitly:

- accepted connection
- retryable accept error
- fatal accept error

Retryable accept failures are logged and followed by a short backoff delay.
Fatal accept failures are surfaced as exceptions and terminate the loop.

### Queue

`src/app/work_queue.*` is the single shared hand-off point between the accept
loop and the worker pool. Its responsibilities are deliberately narrow:

- bounded storage of accepted `net::socket` values
- non-blocking `try_push()` for the accept loop
- blocking `pop()` for worker threads
- cooperative close and wake-up behavior for shutdown

The queue reports accept-side outcomes through `queue_push_result`:

- `queued`
- `full`
- `closed`

That makes overload behavior explicit instead of silently growing buffers or
blocking the accept loop indefinitely.

### Worker pool

`src/app/worker_pool.*` owns a fixed number of `std::jthread` workers. Each
worker:

1. waits on `work_queue::pop()`
2. receives one client socket
3. invokes the supplied per-connection handler
4. catches local exceptions
5. loops back for more work

The pool depends only on a narrow callable of type
`std::function<void(net::socket)>`. This is an intentional dependency
inversion boundary: the concurrency machinery does not need to know about HTTP,
TLS, or file serving.

### Overload behavior

The queue is intentionally bounded. When it is full, Garçon drops newly
accepted connections rather than letting memory usage grow without limit.
Current runtime diagnostics make those rejections visible.

This is a conservative policy with three benefits:

- resource use remains predictable
- slow clients cannot create unbounded backlog growth
- the HTTP and TLS layers stay simple because they do not manage admission
  control themselves

## 4. Networking and transport abstraction

The `net/` layer owns raw socket concerns and hides OS-level descriptor
handling from the rest of the program.

### `net::socket`

`net::socket` is the RAII wrapper around a file descriptor. It is move-only and
responsible for:

- owning and closing the descriptor exactly once
- applying send and receive timeouts
- sending and receiving raw bytes
- translating low-level failures into transport-neutral status values

Because sockets move by value through the queue and worker pool, ownership
stays explicit all the way from `accept()` to connection handling.

### `net::listener`

`net::listener` encapsulates:

- socket creation
- `SO_REUSEADDR`
- IPv4 bind
- `listen()`
- `accept()`

Instead of returning raw sentinel values, `accept()` returns `accept_result`,
which keeps the accept loop explicit about successful connections, retryable
errors, and fatal errors.

### `net::stream`

`net::stream` is the transport-neutral byte-stream interface used by higher
layers. It exposes:

- validation
- close
- receive timeout configuration
- send timeout configuration
- handshake
- receive
- send

The important design point is that the HTTP layer talks to `net::stream`, not
to sockets or OpenSSL types. That keeps request parsing and response
serialization independent from the transport details.

### `net::plain_stream`

`net::plain_stream` is the non-TLS adapter. Its handshake is effectively a
no-op, so HTTP and HTTPS can still share the same per-connection flow.

## 5. TLS integration

The `tls/` layer adds HTTPS without forking the rest of the server into a
separate code path.

### `tls::context`

`tls::context` owns the OpenSSL `SSL_CTX` object and is responsible for:

- constructing the TLS server context
- enforcing the minimum TLS configuration policy
- loading the certificate
- loading the private key
- verifying that the key matches the certificate

This concentrates OpenSSL setup in one place rather than spreading
configuration across the runtime.

### `tls::stream`

`tls::stream` adapts a connected socket to the `net::stream` interface. It:

- creates and owns the connection-specific `SSL*`
- binds it to the accepted socket descriptor
- performs the TLS server handshake
- maps OpenSSL read/write/handshake conditions back to `net::io_status`

That status mapping is what lets the rest of the server reason about:

- orderly close
- timeout
- fatal I/O error

without knowing whether the underlying transport is plain TCP or TLS.

### Stream creation

`src/app/stream_factory.h` and `src/app/default_stream_factory.*` keep
transport selection out of `server.cpp`.

`default_stream_factory` owns the optional TLS context. For each accepted
socket it chooses one of two adapters:

- `net::plain_stream` when TLS is disabled
- `tls::stream` when TLS is enabled

After that choice, the rest of the serving path is shared.

## 6. HTTP framing and message model

The `http/` layer works on protocol bytes and message semantics only.

### Buffer and framing

`http::buffer` stores incoming bytes. `http::framing` reads until a complete
header block ending in `"\r\n\r\n"` is available, or until an explicit limit or
I/O condition is reached.

Framing returns a typed result:

- `ok`
- `closed`
- `timeout`
- `too_large`
- `error`

This keeps header-read outcomes explicit and lets the server choose the
appropriate behavior, such as returning `408 Request Timeout` or `431 Request
Header Fields Too Large`.

The current maximum request-header size is configured in `src/config.h`.

### Request parsing

`http::request` currently models the minimal parsed request required by the
server:

- method
- target
- header fields

The parser accepts only origin-form targets and HTTP/1.0 or HTTP/1.1. It also
parses header fields into a small owning collection with case-insensitive
lookup helpers. Request bodies are still left for future work.

### Response building

`http::response` owns the outbound message representation:

- status code
- reason phrase
- optional content type
- arbitrary response headers
- optional explicit content length
- response body

It serializes itself into a wire-format HTTP/1.1 response so the server runtime
does not have to build raw response strings by hand.

## 7. Application layer and request pipeline

The application seam lives under `src/app/`.

### Request handler

`request_handler` is the root interface used by `server`. The runtime depends
on this abstraction rather than on a specific static-file or routing class.

### Ordered pipeline

`request_pipeline` is the current root handler implementation. It stores an
ordered list of modules and calls them in sequence. Each module returns a
`module_result` with one of four outcomes:

- `pass`
- `respond`
- `upgrade`
- `error`

The current meaning of each outcome is:

- `pass`: continue to the next module
- `respond`: stop and return the supplied `http::response`
- `upgrade`: currently mapped to `501 Not Implemented`
- `error`: currently mapped to `500 Internal Server Error`

This model is intentionally small but already gives the runtime a stable seam
for future routing, authentication, or API modules. The host loads concrete
modules through the public ABI in `include/garcon/module_abi.h`.

Modules may also attach response headers to a `pass` result. The pipeline
accumulates those headers and merges them into whichever later response becomes
final. That is the mechanism used by the current CORS module to decorate
downstream `200`, `401`, `404`, and similar responses without owning the whole
request.

### Shared-module loading

`src/app/module_config.*` treats `modules.d/` like a small Unix-style config
directory:

- each `.conf` file contributes one configured module
- files are sorted lexically before loading
- each file must contain a `path=` entry naming the shared library
- relative `path=` values are resolved relative to the config file
- the full config text is passed to the module constructor

The C ABI stays intentionally small for loader stability. Module authors are
expected to implement normal C++ classes through the helper layer in
`include/garcon/module_cpp.h`, which hides the ABI glue and exposes
`http::request`, `http::response`, request headers, and small configuration
helpers.

### Current module set

Today the default development pipeline contains five configured shared modules:

- `host-guard`, loaded first through `modules.d/03-host-guard.conf`
- `route-table`, loaded second through `modules.d/05-route-table.conf`
- `cors`, loaded third through `modules.d/07-cors.conf`
- `header-guard`, loaded fourth through `modules.d/08-header-guard.conf`
- `static-files`, loaded fifth through `modules.d/10-static-files.conf`

`host-guard` is the earliest allowlist module and checks the `Host` header
before later processing begins. In the default development config it allows
local `localhost` and `127.0.0.1` requests only. `route-table` then responds
to `/healthz` and `/readyz`, and passes `/api/*` onward. `cors` then answers
matching preflight `OPTIONS` requests and decorates later responses for allowed
origins. `header-guard` then protects `/api/*` by requiring the development
API key header before the request may reach later modules. `static-files` is
the terminal fallback module and is a thin adapter around the reusable
`static_files` service. The server runtime therefore depends on the pipeline
abstraction and loader, while the actual request behavior lives outside
`server.cpp`.

## 8. Static-file service

`app::static_files` is a reusable file-serving service rather than a hard-coded
branch in the server runtime.

Its responsibilities include:

- mapping URL targets to filesystem paths
- enforcing root containment
- rejecting unsafe path segments
- fully resolving symlinks before final containment checks
- serving only regular files
- guessing a small set of content types
- limiting file size
- supporting `GET` and `HEAD`

Important current behavior:

- `/` maps to `/index.html`
- query strings are ignored for path resolution
- `.` and `..` path segments are rejected
- NUL bytes in path segments are rejected
- the configured root is canonicalized at construction time
- non-existent or invalid roots fail fast at startup
- files larger than the configured limit return `413 Payload Too Large`
- unsupported methods return `405 Method Not Allowed`

This logic is what carries most of the server's current security posture for
static content.

## 9. Runtime diagnostics and failure policy

`src/app/runtime_events.*` is the small diagnostics seam used by the runtime.
The default implementation logs to stderr, but the server depends only on the
interface.

The seam currently reports:

- accept failures
- queue rejections
- per-connection stage failures
- unexpected worker callback failures

This keeps failures visible without forcing the concurrency or transport layers
to depend directly on a concrete logger.

The failure policy is intentionally local where possible:

- malformed requests return `400`
- header timeouts return `408`
- oversized request headers return `431`
- broken connections do not terminate the process
- TLS handshake failures remain local to the affected connection
- worker exceptions are caught so later connections can still be served

## 10. End-to-end request flow

For a successful HTTP or HTTPS request, the runtime flow is:

1. `main.cpp` parses CLI arguments and builds `server_config`.
2. `app::server` constructs the listener, pipeline, stream factory, and
   diagnostics sink.
3. The accept loop receives a client socket from `net::listener`.
4. The socket is pushed into `work_queue` if capacity is available.
5. A worker thread pops that socket.
6. `default_stream_factory` wraps it as `plain_stream` or `tls::stream`.
7. The stream receives read and write timeouts.
8. The TLS handshake runs when HTTPS is enabled.
9. `http::framing` reads the header block.
10. `http::request::parse()` validates the request line and header fields.
11. `request_pipeline` calls modules in order.
12. The configured `host-guard` module may reject or pass based on `Host`.
13. The configured `route-table` module may respond directly or pass onward.
14. The configured `cors` module may answer preflight requests or attach
    response headers for later responses.
15. The configured `header-guard` module may reject or pass `/api/*` requests.
16. The configured `static-files` module delegates to `static_files`.
17. `http::response` serializes the final result through the stream.

The crucial point is that steps 9 through 17 are the same regardless of
transport.

## 11. Tunable limits and runtime constants

`src/config.h` centralizes several runtime constants used across layers:

- maximum file size: `8 MiB`
- maximum request-header bytes: `64 KiB`
- retry delay after transient `accept()` failure: `50 ms`
- per-connection send/receive timeout: `5 s`

These values are intentionally small and explicit. They keep safety-related
limits visible instead of scattering raw literals across the codebase.

## 12. Extension points

The current architecture is designed to grow in a few predictable directions.

Likely future extension seams are:

- more pipeline modules before, between, or after the current host-guard,
  route-table, cors, header-guard, and static-files chain
- routing and path dispatch on top of `request_pipeline`
- richer request-header normalization and mutation
- richer response-header policy and transformation
- request bodies and JSON handling in the `http/` and `app/` layers
- cookies, sessions, and authentication modules
- richer host services and exchange data behind the module ABI
- alternative diagnostics sinks behind `runtime_events`

Because the concurrency boundary, stream abstraction, and request pipeline are
already explicit, those features can be added without rewriting the accept
loop, TLS integration, or file-serving service.

## 13. Verification and operational confidence

The current architecture is backed by seven smoke suites:

- `tests/cors_smoke.sh`
- `tests/host_guard_smoke.sh`
- `tests/header_guard_smoke.sh`
- `tests/security_smoke.sh`
- `tests/https_smoke.sh`
- `tests/concurrency_smoke.sh`
- `tests/route_table_smoke.sh`

Together they exercise:

- safer bind defaults
- malformed-request handling
- static-file containment checks
- bounded file serving
- guarded host allowlisting
- request-header parsing and guarded API access
- response-header propagation and CORS behavior
- lexical module ordering and gateway-style route handling
- TLS startup and handshake behavior
- progress with stalled HTTP or TLS clients
- queue-full overload behavior
- continued service after local connection failures

These tests are intentionally black-box oriented. They verify that the layers
work together as a runtime, not just as isolated units.

## 14. Release evolution

The current structure was not introduced all at once:

- `v0.0.2` hardened the original single-connection server
- `v0.0.3` introduced HTTPS through the `net::stream` transport abstraction
- `v0.0.4` added bounded concurrency with a work queue and worker pool
- `v0.0.5` introduced explicit accept-result and runtime-diagnostics seams
- `v0.0.6` turned the root handler into an ordered request pipeline and
  extracted the first in-process static-files module
- `v0.0.7` introduced a public module ABI, `modules.d/` configuration, a
  small C++ module SDK, parsed request headers, arbitrary response headers,
  and the default shared `host-guard`, `route-table`, `cors`,
  `header-guard`, and `static-files` module chain

That incremental history explains why the codebase remains small: each release
added one architectural seam at a time instead of replacing the whole runtime.
