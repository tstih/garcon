# Garçon Architecture

Garçon is structured as a small layered server. Each layer owns one job, which
keeps the code readable and allows new capabilities such as HTTPS and
concurrency to be added without rewriting the whole stack.

## Layers

### 1. Process and configuration

`src/main.cpp` is the narrow entry point. It parses CLI options, selects HTTP
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

`app::server` coordinates the runtime. In `v0.0.4` it owns:

- the TCP listener
- the static-file handler
- the optional TLS context
- the work queue and worker-pool orchestration
- the per-connection request pipeline

Concurrency is introduced here, not in the HTTP layer. The server:

- accepts sockets
- hands accepted sockets to a bounded queue
- starts a fixed worker pool
- delegates each socket to the existing single-connection flow

This keeps the transport and HTTP code reusable.

### 3. Concurrency primitives

`src/app/work_queue.*` and `src/app/worker_pool.*` are the `v0.0.4` modules.

- `work_queue` is a bounded producer/consumer queue for accepted sockets
- `worker_pool` owns `std::jthread` lifetime and calls a supplied connection
  handler

This split keeps synchronization code out of `server.cpp` and keeps the worker
policy independent from HTTP details.

### 4. Networking

The `net/` layer owns raw socket concerns.

- `net::listener` binds and listens
- `net::socket` owns the OS socket descriptor through RAII
- `net::stream` is the transport-neutral byte-stream interface
- `net::plain_stream` adapts a plain TCP connection to that interface

The rest of the server talks to `net::stream`, not directly to sockets.

### 5. TLS

The `tls/` layer adds HTTPS without leaking OpenSSL details into the HTTP
code.

- `tls::context` owns `SSL_CTX`
- `tls::stream` adapts a connected socket to `net::stream`

Because both plain and TLS connections implement the same stream interface,
the request pipeline does not need separate HTTP and HTTPS code paths.

### 6. HTTP

The `http/` layer works on bytes and protocol semantics only.

- `http::buffer` stores incoming bytes
- `http::framing` reads the header block safely
- `http::request` parses and validates the request line
- `http::response` builds the outbound response

This layer does not know whether the underlying bytes came from TCP or TLS.

### 7. Application handler

`app::static_files` is the current request handler.

It maps a validated request target into a filesystem path under the configured
document root, rejects traversal and symlink escapes, enforces the served-file
size limit, and builds the HTTP response.

Today Garçon is a static file server. Later versions can introduce routing and
application handlers without changing the transport and framing layers.

## Design patterns

The codebase uses a few deliberate patterns:

- Producer/consumer for accepted-connection dispatch
- Adapter for plain and TLS streams behind `net::stream`
- RAII for sockets, TLS context, and worker lifetime
- Dependency inversion in `worker_pool`, which depends on a callable handler
  instead of concrete HTTP types

## Request flow

For a successful request, the runtime flow is:

1. `main.cpp` builds the runtime configuration.
2. `app::server` starts the listener.
3. The listener accepts a client socket.
4. `work_queue` stores the socket if capacity is available.
5. A worker thread pops the socket.
6. The server wraps that socket as a plain or TLS `net::stream`.
7. The stream applies timeouts and performs the TLS handshake when enabled.
8. `http::framing` reads the request header block.
9. `http::request` validates the request line.
10. `app::static_files` resolves the target and builds a response.
11. `http::response` serializes the result back through the stream.

Only steps 4 and 5 are new in `v0.0.4`. The inner request pipeline is shared
by HTTP and HTTPS and remains per-connection and worker-local.

## Relationship between releases

- `v0.0.2` hardened the single-connection server
- `v0.0.3` added HTTPS through a transport abstraction
- `v0.0.4` added bounded concurrency around the same request pipeline

See [CONCURRENCY-SCALABILITY.md](/home/tstih/data/wischner/garcon/docs/CONCURRENCY-SCALABILITY.md)
for the detailed concurrency record and
[HTTPS-v0.0.3.md](/home/tstih/data/wischner/garcon/docs/HTTPS-v0.0.3.md) for
the TLS transport design.
