# Garçon Tutorial

This tutorial shows how to build, configure, and run Garçon in development. It
covers the current shared-module layout, HTTP and HTTPS, the present packaging
story, and the bounded-concurrency controls introduced in `v0.0.4` and
retained in later releases.

## 1. Install build dependencies

Minimum build requirements:

- CMake 3.25 or newer
- a C++23-capable compiler
- OpenSSL development headers and libraries

Useful test tools:

- `bash`
- `curl`
- `openssl`
- `python3`
- `ss` from `iproute2`

For Debian package generation, also install:

- `dpkg-dev`

## 2. Build the project

From the repository root:

~~~sh
cmake -S . -B build
cmake --build build
~~~

The main executable is written to `bin/garcon`.
The default shared modules are written to `bin/modules/`, and the default
module configuration is copied to `bin/modules.d/`.

You can also check the configured build version directly:

~~~sh
./bin/garcon --version
~~~

## 3. Prepare content and understand the default modules

Garçon serves files from a `www/` directory.

- in a development checkout, it uses the local `www/`
- in an installed package, it can fall back to `/usr/share/garcon/www`

At minimum, place an `index.html` file under `www/`.

Garçon also loads request modules from a `modules.d/` directory. In the
default development build:

- `bin/modules.d/03-host-guard.conf` points at the shared `host-guard`
  module in `bin/modules/`
- `bin/modules.d/05-route-table.conf` points at the shared `route-table`
  module in `bin/modules/`
- `bin/modules.d/07-cors.conf` points at the shared `cors`
  module in `bin/modules/`
- `bin/modules.d/08-header-guard.conf` points at the shared `header-guard`
  module in `bin/modules/`
- `bin/modules.d/10-static-files.conf` points at the shared `static-files`
  module in `bin/modules/`

Those numeric prefixes are just ordering hints. Garçon loads `.conf` files in
lexical order, so the default development chain is:

1. `host-guard`
2. `route-table`
3. `cors`
4. `header-guard`
5. `static-files`

By default that means:

- only `Host: localhost` and `Host: 127.0.0.1` are accepted in development
- `GET /healthz` returns `200 ok`
- `GET /readyz` returns `200 ready`
- CORS allows `http://localhost:3000` and `http://127.0.0.1:3000` for `/api/*`
- `/api/*` requires `X-Garcon-API-Key: dev-key`
- everything else eventually falls back to static files
- HTTP/1.1 connections stay alive unless the client asks to close them

## 4. Run HTTP locally

By default Garçon binds only to `127.0.0.1` and uses port `8080`.

~~~sh
./bin/garcon
~~~

Then open:

~~~text
http://127.0.0.1:8080/
~~~

The default route-table module also gives you a quick liveness check:

~~~text
http://127.0.0.1:8080/healthz
~~~

To pick a different port:

~~~sh
./bin/garcon --port 9090
~~~

There is also a positional port form:

~~~sh
./bin/garcon 9090
~~~

To use a different module configuration directory:

~~~sh
./bin/garcon --modules-dir ./my-modules.d
~~~

Without `--modules-dir`, Garçon first looks for `modules.d/` next to the
executable, then for a local `modules.d/`, and finally for
`/etc/garcon/modules.d`.

## 5. Tune concurrency

`v0.0.4` adds bounded concurrency settings:

- `--workers N`
- `--queue-capacity N`

Defaults:

- worker count: `std::thread::hardware_concurrency()`
- fallback worker count: `4`
- queue capacity: `worker_threads * 64`
- per-IP accepted concurrent connections: `8`

Example:

~~~sh
./bin/garcon --workers 4 --queue-capacity 256
~~~

The startup log reports the effective worker count and queue capacity.
Garçon also applies a coarse per-IP admission cap before sockets reach the
worker pool. `v0.0.5` and later report queue-full rejection, per-IP
rejection, and connection-stage diagnostics to stderr when those events occur.
Handled requests are written to stdout as access-log lines.

## 6. Expose HTTP on another interface

Garçon is local-only by default. To listen on another address, make it
explicit:

~~~sh
./bin/garcon --bind 0.0.0.0 --port 8080
~~~

Use that only when you understand the network exposure you are creating.

## 7. Generate a local HTTPS certificate

For local testing, a self-signed certificate is enough:

~~~sh
openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
  -keyout key.pem \
  -out cert.pem \
  -days 1 \
  -subj "/CN=127.0.0.1"
~~~

## 8. Run HTTPS locally

Garçon enables HTTPS when both a certificate and a private key are provided.
HTTPS defaults to port `8443`.

~~~sh
./bin/garcon --tls-cert cert.pem --tls-key key.pem
~~~

Then open:

~~~text
https://127.0.0.1:8443/
~~~

For a different port, bind address, or concurrency profile:

~~~sh
./bin/garcon --bind 0.0.0.0 --port 9443 --tls-cert cert.pem --tls-key key.pem
./bin/garcon --tls-cert cert.pem --tls-key key.pem --workers 8 --queue-capacity 512
~~~

If only one TLS file is provided, startup fails intentionally.
HTTPS responses also include `Strict-Transport-Security: max-age=31536000`
automatically.

## 9. Build a Debian package

From the repository root:

~~~sh
cmake -S . -B build
cmake --build build
cd build
make dist
~~~

The final `.deb` is written to `bin/`. Temporary CPack files stay in the build
tree.

Today the Debian package installs the main binary, docs, and packaged `www/`
content. Shared modules and a populated installed `modules.d/` layout are not
yet packaged, so development builds remain the most complete way to exercise
the pluggable architecture.

## 10. Verify the server quickly

Simple HTTP check:

~~~sh
curl -fsS http://127.0.0.1:8080/
~~~

Route-table health check:

~~~sh
curl -fsS http://127.0.0.1:8080/healthz
~~~

Guarded API check:

~~~sh
curl -fsS -H 'X-Garcon-API-Key: dev-key' http://127.0.0.1:8080/api/ping.txt
~~~

Preflight CORS check:

~~~sh
curl -i -X OPTIONS \
  -H 'Origin: http://localhost:3000' \
  -H 'Access-Control-Request-Method: GET' \
  -H 'Access-Control-Request-Headers: X-Garcon-API-Key' \
  http://127.0.0.1:8080/api/ping.txt
~~~

Simple HTTPS check with a self-signed certificate:

~~~sh
curl -k -fsS https://127.0.0.1:8443/
~~~

To run the automated smoke tests from a configured build tree:

~~~sh
ctest --test-dir build --output-on-failure
~~~

This runs the CORS, host-guard, header-guard, security, HTTPS, concurrency,
keep-alive, and route-table smoke suites plus the small `unit_http` test
binary.

If you want to write your own shared module next, continue with
[docs/MODULE-DEVELOPMENT-TUTORIAL.md](/home/tstih/data/wischner/garcon/docs/MODULE-DEVELOPMENT-TUTORIAL.md).
