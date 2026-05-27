# HTTPS Architecture For v0.0.3

## Summary

`v0.0.3` adds direct HTTPS support to Garçon.

This document describes the implemented architecture for that release.

The only new capability in this version is HTTPS. Request parsing, static file
handling, and the current single-threaded execution model stay intact. TLS is
introduced as a transport concern, not as an HTTP or application concern.

## Goals

- Add direct HTTPS support to the existing server.
- Keep the `http/` layer unaware of whether bytes come from plain TCP or TLS.
- Reuse the current timeout and connection-closing behavior as much as possible.
- Keep the code small, explicit, and easy to reason about.
- Make HTTPS opt-in through explicit certificate and key configuration.

## Non-goals

- No HTTP/2 or HTTP/3.
- No automatic certificate provisioning such as ACME or Let's Encrypt.
- No client-certificate authentication.
- No multi-domain certificate routing or advanced SNI features.
- No simultaneous HTTP and HTTPS listeners in `v0.0.3`.
- No redirect layer from HTTP to HTTPS in `v0.0.3`.

## Design decision

TLS should sit between the listening socket and the HTTP framing code.

In `v0.0.3`, the `http/` layer reads through a small stream abstraction so
both plain TCP and TLS can present the same API without coupling HTTP parsing
to OpenSSL details.

## Module layout

Existing modules that stay:

- `src/net/socket.*`: owns the raw file descriptor.
- `src/net/listener.*`: accepts raw TCP connections.
- `src/http/*`: request framing, parsing, buffering, and response building.
- `src/static_files.*`: static file handler.

Modules added in `v0.0.3`:

- `src/net/stream.h`
  - Small interface for byte-oriented connections.
  - Exposes `recv()`, `send()`, `close()`, and timeout configuration.
- `src/net/plain_stream.*`
  - Adapts the existing `net::socket` to `net::stream`.
- `src/tls/context.*`
  - Owns `SSL_CTX`.
  - Loads certificate and private key.
  - Applies TLS policy and validates startup configuration.
- `src/tls/stream.*`
  - Wraps `net::socket` plus `SSL*`.
  - Implements the same stream API as `net::plain_stream`.
- `src/server_config.h`
  - Holds bind address, port, document root, and optional TLS settings.

## Core architecture

### 1. Stream abstraction

`http::read_header_block()` and `app::server::serve_client()` now depend on a
transport-neutral byte stream instead of `net::socket`.

Interface:

```cpp
namespace net {

class stream
{
public:
    virtual ~stream() = default;
    virtual bool valid() const = 0;
    virtual void close() = 0;
    virtual bool set_receive_timeout(std::chrono::milliseconds timeout) = 0;
    virtual bool set_send_timeout(std::chrono::milliseconds timeout) = 0;
    virtual io_status handshake() = 0;
    virtual read_result recv_some(std::span<std::byte> out) = 0;
    virtual io_status send_all(std::span<const std::byte> data) = 0;
};

} // namespace net
```

This keeps the HTTP layer simple:

- plain HTTP uses `net::plain_stream`
- HTTPS uses `tls::stream`

The `http/` layer does not need to know which one it received.

### 2. Keep `net::socket` as the raw fd owner

`net::socket` should remain the small RAII type that owns the file descriptor.
It should not learn TLS behavior.

That separation is useful because:

- socket ownership stays simple
- TLS state stays out of the low-level networking code
- plain and TLS modes can share the same accepted socket type

### 3. Add a TLS context object

`tls::context` should own all OpenSSL process-local configuration needed for
server TLS:

- create `SSL_CTX`
- load PEM certificate
- load PEM private key
- verify that the key matches the certificate
- configure allowed protocol versions
- disable insecure legacy features

This object should be constructed once at server startup, not per connection.

### 4. Add a TLS stream wrapper

`tls::stream` should:

- take ownership of an accepted `net::socket`
- create `SSL*` from `tls::context`
- bind the socket fd to the SSL object
- perform the server-side handshake
- expose `recv()` and `send()` through `SSL_read()` and `SSL_write()`
- close both TLS state and the underlying socket safely

Handshake failure should terminate only the current connection.
It must never terminate the process.

### 5. Server chooses transport at connection setup time

`app::server` accepts a `server_config` object and decides which stream type to
create:

- if TLS config is absent: build `net::plain_stream`
- if TLS config is present: build `tls::stream` and complete the handshake

After that point the request pipeline stays the same:

1. read header block
2. parse request
3. build response
4. send response

## Runtime model

`v0.0.3` should keep one listener and one protocol mode per process.

That means one Garçon process runs in exactly one of these modes:

- HTTP mode
- HTTPS mode

This keeps startup, logging, testing, and error handling simple. Dual-listener
HTTP plus HTTPS support can be added later if needed, but it should not be part
of the first HTTPS release.

## Configuration

CLI additions in `v0.0.3`:

- `--tls-cert PATH`
- `--tls-key PATH`

Behavior:

- if neither flag is provided, the server runs in HTTP mode exactly as today
- if both flags are provided, the server runs in HTTPS mode
- if only one flag is provided, startup fails with a clear error

Startup log examples:

- `Garçon listening on http://127.0.0.1:8080/`
- `Garçon listening on https://127.0.0.1:8443/`

Port policy:

- keep `8080` as the default port in HTTP mode
- use `8443` as the default port in HTTPS mode if the user did not pass `--port`

## TLS library choice

Use OpenSSL for `v0.0.3`.

Reasons:

- mature and widely deployed
- available on typical Linux systems
- supports the required server-side TLS features
- avoids inventing any cryptography internally

Garçon should treat OpenSSL as an implementation detail behind `tls::context`
and `tls::stream`.

## Security policy for the first HTTPS release

`v0.0.3` should ship with conservative defaults:

- server-side TLS only
- minimum protocol version TLS 1.2
- prefer TLS 1.3 when available
- no SSLv2, SSLv3, TLS 1.0, or TLS 1.1
- no TLS compression
- no custom crypto primitives
- certificate and key must both load successfully before the server starts

This version should not try to expose every OpenSSL knob. It should pick a safe
default profile and keep configuration small.

## Error handling

Error handling should stay aligned with the current server style:

- startup configuration errors are fatal and reported once
- per-connection handshake failures close only that connection
- read and write failures during TLS traffic are treated like normal I/O errors
- timeout behavior should remain in force for both handshake and application I/O

Plain HTTP sent to an HTTPS port should fail closed. The server should not try
to auto-detect or downgrade the connection.

## Impact on existing code

Code changes in `v0.0.3`:

- `src/http/framing.*`
  - change transport parameter type from `net::socket&` to `net::stream&`
- `src/server.*`
  - construct either a plain or TLS stream
  - log `http://` or `https://` correctly
- `src/main.cpp`
  - parse TLS CLI options
  - choose default port by protocol mode
- `src/CMakeLists.txt`
  - link against OpenSSL
- new `src/net/plain_stream.*`
- new `src/net/stream.h`
- new `src/tls/context.*`
- new `src/tls/stream.*`
- optional new `src/server_config.h`

Code that should stay unchanged or nearly unchanged:

- `src/http/request.*`
- `src/http/response.*`
- `src/http/buffer.*`
- `src/static_files.*`

## Proposed implementation order

### Phase 1

Introduce `net::stream` and `net::plain_stream`, then switch the HTTP framing
and server pipeline to use the abstraction without changing behavior.

### Phase 2

Add `tls::context` and `tls::stream`, but keep them isolated until the build
and startup path are stable.

### Phase 3

Wire HTTPS mode into `main.cpp` and `app::server`, including protocol-aware
logging and default port selection.

### Phase 4

Add tests for HTTPS success and failure paths.

## Test plan for v0.0.3

At minimum, add checks for:

1. HTTPS startup succeeds with a valid self-signed certificate and key.
2. HTTPS startup fails cleanly when the certificate or key is missing.
3. `curl -k https://127.0.0.1:8443/` returns the same static content as HTTP.
4. A broken TLS client during response write does not crash the process.
5. A stalled TLS handshake times out and does not block later connections.
6. Plain HTTP bytes sent to the HTTPS port are rejected cleanly.
7. Existing static-file security behavior still holds over HTTPS.

## Out of scope for later versions

These are reasonable follow-ups, but not part of `v0.0.3`:

- simultaneous HTTP and HTTPS listeners
- HTTP-to-HTTPS redirect behavior
- HSTS
- SNI-based multi-certificate hosting
- OCSP stapling
- automatic certificate renewal
- HTTP/2 over TLS

## Recommendation

The cleanest `v0.0.3` architecture is:

- keep `net::socket` as the raw fd owner
- add a narrow `net::stream` abstraction
- implement plain and TLS stream adapters
- keep HTTP parsing and response generation transport-agnostic
- run one protocol mode per process

That gives Garçon direct HTTPS support without collapsing the current clean
layering.
