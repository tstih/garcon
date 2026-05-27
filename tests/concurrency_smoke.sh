#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
next_port=$((30080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "concurrency smoke test failed: $*" >&2
    exit 1
}

assert_contains() {
    local haystack=$1
    local needle=$2
    local message=$3

    if [[ "$haystack" != *"$needle"* ]]; then
        fail "$message"
    fi
}

generate_certificate() {
    openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
        -keyout "$tmpdir/key.pem" \
        -out "$tmpdir/cert.pem" \
        -days 1 \
        -subj "/CN=127.0.0.1" >/dev/null 2>&1
}

make_fixture_tree() {
    mkdir -p "$tmpdir/www"
    cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"
}

wait_for_http_server() {
    local port=$1

    for _ in $(seq 1 50); do
        if curl -fsS --max-time 1 "http://127.0.0.1:$port/" >/dev/null 2>&1; then
            return 0
        fi

        if ! kill -0 "$server_pid" 2>/dev/null; then
            cat "$tmpdir/server.log" >&2 || true
            fail "HTTP server exited during startup"
        fi

        sleep 0.1
    done

    fail "HTTP server did not become ready"
}

wait_for_https_server() {
    local port=$1

    for _ in $(seq 1 50); do
        if curl -k -fsS --max-time 1 "https://127.0.0.1:$port/" >/dev/null 2>&1; then
            return 0
        fi

        if ! kill -0 "$server_pid" 2>/dev/null; then
            cat "$tmpdir/server.log" >&2 || true
            fail "HTTPS server exited during startup"
        fi

        sleep 0.1
    done

    fail "HTTPS server did not become ready"
}

start_http_server() {
    local port=$1
    shift

    : >"$tmpdir/server.log"

    (
        cd "$tmpdir"
        "$BINARY" --port "$port" "$@" >"$tmpdir/server.log" 2>&1
    ) &
    server_pid=$!

    wait_for_http_server "$port"
}

start_https_server() {
    local port=$1
    shift

    : >"$tmpdir/server.log"

    (
        cd "$tmpdir"
        "$BINARY" --port "$port" --tls-cert cert.pem --tls-key key.pem "$@" \
            >"$tmpdir/server.log" 2>&1
    ) &
    server_pid=$!

    wait_for_https_server "$port"
}

stop_server() {
    if [[ -z "$server_pid" ]]; then
        return
    fi

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    server_pid=""
}

start_slow_http_client() {
    local port=$1

    python3 - "$port" <<'PY' &
import socket
import sys
import time

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(b"GET /")
    time.sleep(7)
PY
    echo $!
}

start_stalled_tls_client() {
    local port=$1

    python3 - "$port" <<'PY' &
import socket
import sys
import time

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3):
    time.sleep(7)
PY
    echo $!
}

pressure_http_server() {
    local port=$1

    python3 - "$port" <<'PY'
import socket
import sys

port = int(sys.argv[1])

for _ in range(32):
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=0.5) as sock:
            sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    except OSError:
        pass
PY
}

test_http_progress_with_stalled_client() {
    local port=$1
    local slow_pid log

    slow_pid=$(start_slow_http_client "$port")
    sleep 0.5
    timeout 4s curl -fsS --max-time 3 "http://127.0.0.1:$port/" >/dev/null
    wait "$slow_pid" || true

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "with 2 workers and queue capacity 4" \
        "startup log did not report the configured HTTP concurrency settings"
}

test_https_progress_with_stalled_handshake() {
    local port=$1
    local slow_pid log

    slow_pid=$(start_stalled_tls_client "$port")
    sleep 0.5
    timeout 5s curl -k -fsS --max-time 4 "https://127.0.0.1:$port/" >/dev/null
    wait "$slow_pid" || true

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "with 2 workers and queue capacity 4" \
        "startup log did not report the configured HTTPS concurrency settings"
}

test_bounded_overload_recovery() {
    local port=$1
    local slow_pid log

    slow_pid=$(start_slow_http_client "$port")
    sleep 0.5
    pressure_http_server "$port"
    wait "$slow_pid" || true
    timeout 8s curl -fsS --max-time 3 "http://127.0.0.1:$port/" >/dev/null

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "connection rejected: accepted-connection queue full" \
        "bounded overload did not report an explicit queue-full rejection"
}

trap cleanup EXIT

generate_certificate
make_fixture_tree

start_http_server "$next_port" --workers 2 --queue-capacity 4
test_http_progress_with_stalled_client "$next_port"
stop_server

next_port=$((next_port + 1))
start_https_server "$next_port" --workers 2 --queue-capacity 4
test_https_progress_with_stalled_handshake "$next_port"
stop_server

next_port=$((next_port + 1))
start_http_server "$next_port" --workers 1 --queue-capacity 1
test_bounded_overload_recovery "$next_port"
stop_server
