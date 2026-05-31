# Debian packaging

This folder contains the Debian packaging configuration used by CPack.

## Build a `.deb`

From the repository root:

~~~sh
cmake -S . -B build
cmake --build build
cd build
make dist
~~~

The generated Debian package is written to the repository `bin/` directory.
CPack uses the build tree for temporary packaging files, so `bin/` only
receives the final `.deb` artifact.

## Installed layout

The package installs:

- `garcon` to `/usr/bin/garcon`
- static web files to `/usr/share/garcon/www`
- project docs to `/usr/share/doc/garcon`

At runtime Garçon prefers a local `www/` directory for development, and falls
back to `/usr/share/garcon/www` when running from an installed package.

The runtime can also discover module configuration from `modules.d/` next to
the executable, a local `modules.d/`, or `/etc/garcon/modules.d`. The current
Debian package does not yet install shared modules or populate
`/etc/garcon/modules.d`, so the packaged layout is presently a base server
package rather than the final pluggable-runtime layout.

The installed binary accepts the same runtime flags as the development build,
including `--bind`, `--port`, `--tls-cert`, `--tls-key`, `--modules-dir`,
`--workers`, and `--queue-capacity`.
