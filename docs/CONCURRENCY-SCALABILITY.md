# Concurrency And Scalability Architecture For v0.0.4

This document records the concurrency model shipped in `v0.0.4` and retained
through the `v0.0.5` internal refactor.

Garçon now keeps the `v0.0.3` transport-neutral HTTP/HTTPS pipeline, but no
longer serves only one client at a time. The runtime uses standard C++ threads
only and keeps concurrency centered around connection dispatch.

## Goals

`v0.0.4` improves parallel throughput and resilience to slow clients while
preserving the small, layered design established in earlier releases.

The release deliberately avoids:

- one thread per connection
- an event-loop rewrite
- separate HTTP and HTTPS serving pipelines

## Core runtime model

Garçon uses a producer/consumer design:

1. `app::server` owns one listening socket.
2. The accept loop receives connected `net::socket` instances.
3. Accepted sockets are handed to `app::work_queue`, which has a fixed
   capacity.
4. `app::worker_pool` owns a fixed number of `std::jthread` workers.
5. Each worker pops one socket and runs the existing single-connection flow:
   - build a plain or TLS `net::stream`
   - apply read and write timeouts
   - perform the TLS handshake when enabled
   - read and validate the request
   - delegate to `static_files`
   - serialize and send the response

Only the dispatch boundary changed in `v0.0.4`. The transport, HTTP, and
handler layers remain reusable.

## Main modules

### `src/app/work_queue.*`

The work queue is the single shared hand-off point between the accept loop and
the worker pool.

Responsibilities:

- bounded socket storage
- blocking worker-side `pop()`
- non-blocking accept-side `try_push()`
- cooperative close and wake-up behavior

The queue exposes `queue_push_result` so the caller can distinguish between
successful enqueue, overload, and shutdown.

### `src/app/worker_pool.*`

The worker pool owns thread lifetime, nothing more.

Responsibilities:

- construct a fixed number of `std::jthread` workers
- request cooperative stop on shutdown
- keep per-connection exceptions local to the affected client

The pool depends only on a narrow callable handler. That keeps worker
orchestration independent of HTTP internals.

### `src/server.cpp`

`app::server` remains the integration point:

- own listener, static-file handler, and optional TLS context
- build the queue and worker pool from `server_config`
- accept sockets and dispatch them to workers
- preserve the existing per-connection serving pipeline

This is where the producer/consumer pattern meets the transport abstraction.

## Runtime tuning

`v0.0.4` adds two explicit concurrency settings:

- `--workers N`
- `--queue-capacity N`

These map to `server_config::worker_threads` and
`server_config::connection_queue_capacity`.

Defaults:

- worker threads: `std::thread::hardware_concurrency()`
- fallback worker count: `4`
- queue capacity: `worker_threads * 64`

The CLI rejects zero or malformed values so the server always starts from a
valid configuration.

## Overload and failure handling

The queue is intentionally bounded. When it is full, newly accepted sockets
are rejected by dropping the connection instead of growing memory without
limit.

Per-connection failures remain local:

- malformed requests still return `400 Bad Request`
- timeout behavior still returns `408 Request Timeout` where applicable
- broken pipes and TLS handshake failures do not terminate the process
- worker threads continue serving later connections after a local failure

This preserves the security behavior introduced in `v0.0.2` and `v0.0.3`.

## Patterns and design choices

`v0.0.4` keeps the code object-oriented without becoming framework-like:

- Producer/consumer pattern for accepted-connection dispatch
- Adapter pattern through `net::stream`, `net::plain_stream`, and `tls::stream`
- RAII ownership for sockets, TLS context, and worker lifetime
- Dependency inversion through the worker-pool handler callback

The resulting split aligns well with SOLID:

- `work_queue` manages buffering and synchronization
- `worker_pool` manages worker lifetime
- `server` coordinates modules rather than implementing queueing logic inline

## Verification

The release is covered by three smoke suites:

- `tests/security_smoke.sh` for the `v0.0.2` hardening behavior
- `tests/https_smoke.sh` for the `v0.0.3` HTTPS transport
- `tests/concurrency_smoke.sh` for the `v0.0.4` concurrency model

The concurrency suite verifies:

- progress with a stalled HTTP client
- progress with a stalled TLS handshake
- bounded overload recovery with a tiny worker-and-queue configuration
- startup logging of the configured worker count and queue capacity

In `v0.0.5`, the same model remains in place, but the runtime now reports
explicit accept failures and queue-full rejections through a small diagnostics
seam.

Together these checks show that HTTP and HTTPS both run through the same
bounded worker-pool architecture.
