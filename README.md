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

**v0.0.6**

- Concurrent HTTP/1.1 server with direct HTTPS support
- Serves static files from a configurable `www/` directory
- Strict request-line validation and safer response handling
- Fixed-size `std::jthread` worker pool with a bounded accepted-connection queue
- Ordered request pipeline with `pass`, `respond`, `upgrade`, and `error` outcomes
- `static_files_module` extracted as the first concrete pipeline module
- Request-handler and stream-factory seams for cleaner extensibility
- Explicit accept-error classification and runtime diagnostics
- Clean modular structure (`net`, `http`, `tls`, `app`)
- Transport-neutral connection layer shared by HTTP and HTTPS
- Local-only bind by default, with explicit opt-in for wider exposure
- Document-root containment checks and bounded file serving
- Explicit runtime tuning through `--workers` and `--queue-capacity`
- Designed to be extended with routing, cookies, and authentication

`v0.0.6` builds on `v0.0.5` by introducing the simplified module pipeline we
discussed:

- requests now flow through an ordered root pipeline
- each module can `pass`, `respond`, request a future `upgrade`, or signal `error`
- static-file serving is no longer wired directly into the server runtime
- the first extracted module is `static_files_module`, backed by a reusable file-serving service
- the `v0.0.2` through `v0.0.5` security and concurrency guarantees stay intact

This version is intended for development, experimentation, and learning.
It is not yet suitable for exposure to untrusted networks.

The consolidated transport, TLS, concurrency, and module architecture for this
release is documented in
[docs/ARCHITECTURE.md](/home/tstih/data/wischner/garcon/docs/ARCHITECTURE.md).

## Planned features

- Request routing and handler plugins
- Cookie parsing and session management
- OAuth 2.0 and OpenID Connect (OIDC) support
- Basic JSON-based web APIs

## Documentation

- [docs/ARCHITECTURE.md](/home/tstih/data/wischner/garcon/docs/ARCHITECTURE.md) gives the detailed design overview for Garçon's transport, TLS, concurrency, pipeline, and static-file layers
- [docs/TUTORIAL.md](/home/tstih/data/wischner/garcon/docs/TUTORIAL.md) walks through build, configuration, HTTP, HTTPS, concurrency tuning, and packaging

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

The resulting binary is placed in the bin/ directory.

## Run

By default Garçon binds only to `127.0.0.1`.

~~~sh
./bin/garcon
~~~

`v0.0.4` through `v0.0.6` choose conservative concurrency defaults automatically:

- worker threads: `std::thread::hardware_concurrency()`, with a fallback of `4`
- queue capacity: `worker_threads * 64`

You can tune both at runtime:

~~~sh
./bin/garcon --workers 4 --queue-capacity 256
~~~

To run HTTPS locally with a certificate and key, provide both files
explicitly. HTTPS defaults to port `8443` unless `--port` is set:

~~~sh
./bin/garcon --tls-cert cert.pem --tls-key key.pem --workers 8 --queue-capacity 512
~~~

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

## License

MIT License.
