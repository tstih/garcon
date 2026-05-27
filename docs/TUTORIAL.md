# Garçon Tutorial

This tutorial shows how to build, configure, and run Garçon in development. It
covers both HTTP and HTTPS, the packaged install layout, and the concurrency
controls introduced in `v0.0.4` and retained in `v0.0.5`.

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

## 3. Prepare static content

Garçon serves files from a `www/` directory.

- in a development checkout, it uses the local `www/`
- in an installed package, it can fall back to `/usr/share/garcon/www`

At minimum, place an `index.html` file under `www/`.

## 4. Run HTTP locally

By default Garçon binds only to `127.0.0.1` and uses port `8080`.

~~~sh
./bin/garcon
~~~

Then open:

~~~text
http://127.0.0.1:8080/
~~~

To pick a different port:

~~~sh
./bin/garcon --port 9090
~~~

There is also a positional port form:

~~~sh
./bin/garcon 9090
~~~

## 5. Tune concurrency

`v0.0.4` adds bounded concurrency settings:

- `--workers N`
- `--queue-capacity N`

Defaults:

- worker count: `std::thread::hardware_concurrency()`
- fallback worker count: `4`
- queue capacity: `worker_threads * 64`

Example:

~~~sh
./bin/garcon --workers 4 --queue-capacity 256
~~~

The startup log reports the effective worker count and queue capacity.
`v0.0.5` also reports queue-full rejection and connection-stage diagnostics to
stderr when those events occur.

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

## 10. Verify the server quickly

Simple HTTP check:

~~~sh
curl -fsS http://127.0.0.1:8080/
~~~

Simple HTTPS check with a self-signed certificate:

~~~sh
curl -k -fsS https://127.0.0.1:8443/
~~~

To run the automated smoke tests from a configured build tree:

~~~sh
ctest --test-dir build --output-on-failure
~~~

This runs the security, HTTPS, and concurrency smoke suites.
