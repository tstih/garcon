# Garçon

Garçon is a lean and minimal HTTP web server written in modern C++.

The project starts as a deliberately simple, single-threaded static file server
and is incrementally evolved into a production-grade web server supporting
basic web APIs, authentication, and authorization mechanisms such as OAuth 2.0
and OpenID Connect (OIDC), all while keeping the codebase small, readable, and
architecturally clean.

## Philosophy

Garçon is built with the following principles in mind:

- **Minimalism** - no frameworks, no magic, only what is necessary
- **Modern C++** - C++23, RAII, value semantics, explicit ownership
- **Clear layering** - transport, HTTP, and application logic are kept separate
- **Incremental evolution** - each feature is added in small, reviewable steps
- **Security-aware** - correctness and security are considered from the start

The goal is not to compete with existing web servers, but to serve as a
well-structured, comprehensible foundation for modern C++ backend development.

## Current status

**v0.0.8**

- Concurrent HTTP/1.1 server with direct HTTPS support
- HTTP keep-alive for reusable HTTP/1.0 and HTTP/1.1 connections
- Serves static files from a configurable `www/` directory
- Strict request-line validation and safer response handling
- Fixed-size `std::jthread` worker pool with a bounded accepted-connection queue
- Ordered request pipeline with `pass`, `respond`, `upgrade`, and `error` outcomes
- Request headers parsed into `http::request` and exposed to shared modules
- Arbitrary response headers supported in `http::response` and exposed to shared modules
- Dedicated `connection_handler` runtime for per-connection lifecycle ownership
- Coarse per-IP concurrent connection admission limiting in the host runtime
- Public module ABI under `include/garcon/`
- Simple C++ module authoring layer in `include/garcon/module_cpp.h`
- Shared-module loading from a configured `modules.d/` directory
- Low-overhead `host-guard` gateway module under `lib/host_guard/`
- Low-overhead `route-table` gateway module under `lib/route_table/`
- Low-overhead `cors` gateway module under `lib/cors/`
- Low-overhead `header-guard` gateway module under `lib/header_guard/`
- Static files served by the shared `static-files` module under `lib/static_files/`
- Reusable static-file service moved under `lib/static_files/`
- TLS mode uses explicit modern cipher policy and adds HSTS automatically
- Per-request access logs written to stdout and source-located diagnostics to stderr
- Request-handler and stream-factory seams for cleaner extensibility
- Explicit accept-error classification and runtime diagnostics
- Clean modular structure (`net`, `http`, `tls`, `app`)
- Transport-neutral connection layer shared by HTTP and HTTPS
- Local-only bind by default, with explicit opt-in for wider exposure
- Document-root containment checks and bounded file serving
- Explicit runtime tuning through `--workers` and `--queue-capacity`
- Designed to be extended with routing, cookies, and authentication

`v0.0.8` builds on `v0.0.7` by making the shared-module runtime more
gateway-friendly:

- requests now flow through an ordered root pipeline loaded lexically from `modules.d/`
- each module can `pass`, `respond`, request a future `upgrade`, or signal `error`
- parsed request headers are now exposed to shared modules
- arbitrary response headers can now be produced and propagated across the pipeline
- HTTP keep-alive, HSTS, and access logging now live in the host runtime
- the host server now focuses on transport, TLS, request parsing, connection reuse, and orchestration
- module authors can implement normal C++ classes behind the ABI with `module_cpp.h`
- the default development chain is `host-guard`, then `route-table`, then `cors`, then `header-guard`, then `static-files`
- the `v0.0.2` through `v0.0.7` security and concurrency guarantees stay intact

This version is intended for development, experimentation, and learning.
It is not yet suitable for exposure to untrusted networks.

The consolidated transport, TLS, concurrency, and module architecture for this
release is documented in
[docs/ARCHITECTURE.md](/home/tstih/data/wischner/garcon/docs/ARCHITECTURE.md).

## Planned features

- Richer request routing, upstream proxying, and handler plugins
- Cookie parsing and session management
- OAuth 2.0 and OpenID Connect (OIDC) support
- Basic JSON-based web APIs

## Documentation

- [docs/ARCHITECTURE.md](/home/tstih/data/wischner/garcon/docs/ARCHITECTURE.md) gives the detailed design overview for Garçon's transport, TLS, concurrency, pipeline, module, and static-file layers
- [docs/TUTORIAL.md](/home/tstih/data/wischner/garcon/docs/TUTORIAL.md) walks through build, configuration, HTTP, HTTPS, concurrency tuning, and packaging
- [docs/MODULE-DEVELOPMENT-TUTORIAL.md](/home/tstih/data/wischner/garcon/docs/MODULE-DEVELOPMENT-TUTORIAL.md) shows how to build a shared Garçon module in modern C++

## Build

Garçon uses CMake and requires a C++23-capable compiler.

## Dependencies

Build dependencies:

- CMake 3.25 or newer
- A C++23-capable compiler
- OpenSSL development headers and libraries

Test dependencies:

- `bash`
- `curl`
- `openssl`
- `python3`
- `ss` from `iproute2`

Debian package build dependencies:

- `dpkg-dev`

~~~sh
cmake -S . -B build
cmake --build build
~~~

The resulting binary is placed in `bin/`. Shared modules are written to
`bin/modules/`, and the default module configuration is copied to
`bin/modules.d/`.

To print the build version sourced from the repository `VERSION` file:

~~~sh
./bin/garcon --version
~~~

## Run

By default Garçon binds only to `127.0.0.1`.

~~~sh
./bin/garcon
~~~

`v0.0.4` and later choose conservative concurrency defaults automatically:

- worker threads: `std::thread::hardware_concurrency()`, with a fallback of `4`
- queue capacity: `worker_threads * 64`

You can tune both at runtime:

~~~sh
./bin/garcon --workers 4 --queue-capacity 256
~~~

Garçon also enforces a coarse per-IP cap on accepted concurrent connections so
one client cannot monopolize the worker pool and accepted-connection queue.

To run HTTPS locally with a certificate and key, provide both files
explicitly. HTTPS defaults to port `8443` unless `--port` is set:

~~~sh
./bin/garcon --tls-cert cert.pem --tls-key key.pem --workers 8 --queue-capacity 512
~~~

By default Garçon looks for `modules.d/` next to the executable, then in a
local `modules.d/`, then under `/etc/garcon/modules.d`. You can point it at a
different configuration directory with:

~~~sh
./bin/garcon --modules-dir /path/to/modules.d
~~~

In the default development configuration, the host-guard module first allows:

- `Host: localhost`
- `Host: 127.0.0.1`

The route-table module then responds to:

- `GET /healthz`
- `GET /readyz`

Requests under `/api/*` then pass through the `cors` module, which allows:

- `http://localhost:3000`
- `http://127.0.0.1:3000`

The `header-guard` module then requires:

- `X-Garcon-API-Key: dev-key`

Preflight `OPTIONS` requests for those origins are handled by the `cors`
module before static files act as the fallback handler.

HTTP/1.1 connections stay alive by default unless the client asks to close
them. In HTTPS mode, Garçon also adds
`Strict-Transport-Security: max-age=31536000` automatically.


To listen on a different interface, make the choice explicit for either HTTP
or HTTPS:

~~~sh
./bin/garcon --bind 0.0.0.0 --port 8080
./bin/garcon --bind 0.0.0.0 --tls-cert cert.pem --tls-key key.pem --port 8443
~~~

For a fuller operator walkthrough, including packaging and concurrency tuning,
see
[docs/TUTORIAL.md](/home/tstih/data/wischner/garcon/docs/TUTORIAL.md).

## Packaging

Garçon can produce a Debian package from a configured build tree:

~~~sh
cmake -S . -B build
cmake --build build
cd build
make dist
~~~

The generated `.deb` file is written to the repository `bin/` directory.
Temporary CPack working directories stay under the build tree, not in `bin/`.

Development builds also stage shared modules in `bin/modules/` and default
module configuration in `bin/modules.d/`. The installed Debian package does
not yet populate an installed module directory or `/etc/garcon/modules.d`, so
the current package should still be treated as a base server package rather
than the final pluggable-runtime layout.

Every handled request also emits one access-log line to stdout. Runtime
diagnostics, including connection-stage failures and queue-full rejections,
continue to go to stderr.

## License

MIT License.
