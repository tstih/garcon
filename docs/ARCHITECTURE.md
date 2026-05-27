# Garçon Architecture

Garçon is structured as a small layered server. Each layer owns one job, which
keeps the code readable and allows new capabilities such as HTTPS,
concurrency, and alternate handlers to be added without rewriting the whole
stack.

## Layers

### 1. Process and configuration

`src/main.cpp` is the narrow entry point. In `v0.0.5` it delegates argument
handling to `src/app/command_line.*`, which parses CLI options, selects HTTP
or HTTPS mode, chooses the document root, applies concurrency defaults, and
builds `app::server_config`.

`app::server_config` is the runtime contract between the CLI layer and the
server runtime. It holds:

- bind address
- port
- document root
- worker count
- queue capacity
- optional TLS certificate and key

The packaging layout is intentionally aligned with this model: development runs
prefer a local `www/` directory, while installed packages can fall back to
`/usr/share/garcon/www`.

### 2. Server runtime

`app::server` coordinates the runtime. In `v0.0.5` it owns:

- the TCP listener
- the abstract request handler
- the abstract stream factory
- the runtime-events sink
- the work queue and worker-pool orchestration
- the per-connection request pipeline

Concurrency is introduced here, not in the HTTP layer. The server:

- accepts sockets
- hands accepted sockets to a bounded queue
- starts a fixed worker pool
- delegates each socket to the existing single-connection flow

This keeps the transport and HTTP code reusable.

In `v0.0.5`, `server` no longer constructs concrete transport streams or calls
the static-file handler directly. Those choices are delegated to abstractions.

### 3. Concurrency primitives

`src/app/work_queue.*` and `src/app/worker_pool.*` are the `v0.0.4` modules.

- `work_queue` is a bounded producer/consumer queue for accepted sockets
- `worker_pool` owns `std::jthread` lifetime and calls a supplied connection
  handler

This split keeps synchronization code out of `server.cpp` and keeps the worker
policy independent from HTTP details.

### 4. Request handling

`src/app/request_handler.h` defines the application seam used by the runtime.

- `request_handler` is the abstract strategy interface
- `static_files` is the current concrete implementation

This is the main extension point for future routing, APIs, or auth-aware
handlers.

### 5. Stream creation

`src/app/stream_factory.h` and `src/app/default_stream_factory.*` isolate
transport selection.

- `stream_factory` is the abstract factory interface
- `default_stream_factory` chooses plain TCP or TLS based on configuration

This keeps `server.cpp` transport-agnostic even though the project supports
both HTTP and HTTPS.

### 6. Networking

The `net/` layer owns raw socket concerns.

- `net::listener` binds and listens
- `net::accept_result` classifies accept outcomes explicitly
- `net::socket` owns the OS socket descriptor through RAII
- `net::stream` is the transport-neutral byte-stream interface
- `net::plain_stream` adapts a plain TCP connection to that interface

The rest of the server talks to `net::stream`, not directly to sockets.

### 7. TLS

The `tls/` layer adds HTTPS without leaking OpenSSL details into the HTTP
code.

- `tls::context` owns `SSL_CTX`
- `tls::stream` adapts a connected socket to `net::stream`

Because both plain and TLS connections implement the same stream interface,
the request pipeline does not need separate HTTP and HTTPS code paths.

### 8. HTTP

The `http/` layer works on bytes and protocol semantics only.

- `http::buffer` stores incoming bytes
- `http::framing` reads the header block safely
- `http::request` parses and validates the request line
- `http::response` builds the outbound response

This layer does not know whether the underlying bytes came from TCP or TLS.

### 9. Runtime diagnostics

`src/app/runtime_events.*` is the small observer-style seam used for runtime
visibility.

- accept failures are reported explicitly
- queue-full rejections are reported explicitly
- per-connection stage failures can be logged without crashing the process
- unexpected worker callback exceptions remain visible

The default implementation logs to stderr, but the runtime depends only on the
abstraction.

## Design patterns

The codebase uses a few deliberate patterns:

- Producer/consumer for accepted-connection dispatch
- Adapter for plain and TLS streams behind `net::stream`
- Abstract Factory for stream creation
- Strategy for request handling
- Observer-style runtime events for diagnostics
- RAII for sockets, TLS context, and worker lifetime
- Dependency inversion in `worker_pool`, which depends on a callable handler
  instead of concrete HTTP types

## Request flow

For a successful request, the runtime flow is:

1. `main.cpp` builds the runtime configuration.
2. `app::server` starts the listener.
3. The listener accepts a client socket.
4. `net::accept_result` classifies the accept outcome.
5. `work_queue` stores the socket if capacity is available.
6. A worker thread pops the socket.
7. `stream_factory` wraps that socket as a plain or TLS `net::stream`.
8. The stream applies timeouts and performs the TLS handshake when enabled.
9. `http::framing` reads the request header block.
10. `http::request` validates the request line.
11. `request_handler` resolves the target and builds a response.
12. `http::response` serializes the result back through the stream.

`v0.0.4` introduced steps 5 and 6. `v0.0.5` made steps 4, 7, and 11 explicit
abstraction points while preserving the same external behavior.

## Relationship between releases

- `v0.0.2` hardened the single-connection server
- `v0.0.3` added HTTPS through a transport abstraction
- `v0.0.4` added bounded concurrency around the same request pipeline
- `v0.0.5` refined the runtime with handler, factory, diagnostics, and
  explicit accept-result seams

See [CONCURRENCY-SCALABILITY.md](/home/tstih/data/wischner/garcon/docs/CONCURRENCY-SCALABILITY.md)
for the detailed concurrency record and
[HTTPS-v0.0.3.md](/home/tstih/data/wischner/garcon/docs/HTTPS-v0.0.3.md) for
the TLS transport design.
