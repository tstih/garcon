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

**v0.0.1**

- Single-threaded HTTP/1.1 server
- Serves static files from a configurable `www/` directory
- Minimal request parsing and response serialization
- Clean modular structure (`net`, `http`, `app`)
- Designed to be extended with routing, cookies, and authentication

This version is intended for development, experimentation, and learning.
It is not yet suitable for exposure to untrusted networks.

## Planned features

- Improved security hardening
- Request routing and handler plugins
- Cookie parsing and session management
- OAuth 2.0 and OpenID Connect (OIDC) support
- Basic JSON-based web APIs
- Concurrency and scalability improvements
- Optional TLS support (direct or via reverse proxy)

## Build

Garçon uses CMake and requires a C++20-capable compiler.

~~~sh
cmake -S . -B build
cmake --build build
~~~

The resulting binary is placed in the bin/ directory.

## License

MIT License.