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
- **Modern C++** - C++20, RAII, value semantics, explicit ownership
- **Clear layering** - transport, HTTP, and application logic are kept separate
- **Incremental evolution** - each feature is added in small, reviewable steps
- **Security-aware** - correctness and security are considered from the start

The goal is not to compete with existing web servers, but to serve as a
well-structured, comprehensible foundation for modern C++ backend development.

## Current status

**v0.0.3**

- Single-threaded HTTP/1.1 server with direct HTTPS support
- Serves static files from a configurable `www/` directory
- Strict request-line validation and safer response handling
- Clean modular structure (`net`, `http`, `app`)
- Transport-neutral connection layer shared by HTTP and HTTPS
- Local-only bind by default, with explicit opt-in for wider exposure
- Document-root containment checks and bounded file serving
- Designed to be extended with routing, cookies, and authentication

`v0.0.3` adds direct HTTPS support to the hardened `v0.0.2` baseline while
keeping the codebase intentionally small and layered:

- HTTPS mode through OpenSSL with explicit certificate and private-key configuration
- Local-only exposure by default unless wider network access is explicitly enabled
- Strict request-line validation with early rejection of malformed input
- Connection time limits so stalled clients cannot block the process indefinitely
- Document-root containment checks that reject traversal and symlink escapes
- Bounded file serving so files larger than 8 MiB fail safely

This version is intended for development, experimentation, and learning.
It is not yet suitable for exposure to untrusted networks.

The transport and TLS architecture for this release is documented in
[docs/HTTPS-v0.0.3.md](/home/tstih/data/wischner/garcon/docs/HTTPS-v0.0.3.md).

## Planned features

- Request routing and handler plugins
- Cookie parsing and session management
- OAuth 2.0 and OpenID Connect (OIDC) support
- Basic JSON-based web APIs
- Concurrency and scalability improvements

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

To run HTTPS locally with a certificate and key, provide both files
explicitly. HTTPS defaults to port `8443` unless `--port` is set:

~~~sh
./bin/garcon --tls-cert cert.pem --tls-key key.pem
~~~

To listen on a different interface, make the choice explicit for either HTTP
or HTTPS:

~~~sh
./bin/garcon --bind 0.0.0.0 --port 8080
./bin/garcon --bind 0.0.0.0 --tls-cert cert.pem --tls-key key.pem --port 8443
~~~

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
