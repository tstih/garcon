#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
next_port=$((28080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "security smoke test failed: $*" >&2
    exit 1
}

assert_eq() {
    local actual=$1
    local expected=$2
    local message=$3

    if [[ "$actual" != "$expected" ]]; then
        fail "$message (expected '$expected', got '$actual')"
    fi
}

assert_contains() {
    local haystack=$1
    local needle=$2
    local message=$3

    if [[ "$haystack" != *"$needle"* ]]; then
        fail "$message"
    fi
}

wait_for_server() {
    local port=$1

    for _ in $(seq 1 50); do
        if curl -fsS --max-time 1 "http://127.0.0.1:$port/" >/dev/null 2>&1; then
            return 0
        fi

        if ! kill -0 "$server_pid" 2>/dev/null; then
            cat "$tmpdir/server.log" >&2 || true
            fail "server exited during startup"
        fi

        sleep 0.1
    done

    cat "$tmpdir/server.log" >&2 || true
    fail "server did not become ready on port $port"
}

start_server() {
    local port=$1
    shift

    : >"$tmpdir/server.log"

    (
        cd "$tmpdir"
        "$BINARY" --port "$port" "$@" >"$tmpdir/server.log" 2>&1
    ) &
    server_pid=$!

    wait_for_server "$port"
}

stop_server() {
    if [[ -z "$server_pid" ]]; then
        return
    fi

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    server_pid=""
}

raw_http() {
    local port=$1
    local request=$2

    REQUEST="$request" python3 - "$port" <<'PY'
import os
import socket
import sys

port = int(sys.argv[1])
request = os.environ["REQUEST"].encode("latin1")

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(request)
    sock.shutdown(socket.SHUT_WR)

    response = bytearray()
    while True:
        chunk = sock.recv(65536)
        if not chunk:
            break
        response.extend(chunk)

sys.stdout.buffer.write(response)
PY
}

make_fixture_tree() {
    mkdir -p "$tmpdir/www" "$tmpdir/outside"
    cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"
    printf 'secret\n' >"$tmpdir/outside/secret.txt"
    ln -s ../outside/secret.txt "$tmpdir/www/escape.txt"
    truncate -s 1048576 "$tmpdir/www/medium.bin"
    truncate -s 9437184 "$tmpdir/www/large.bin"
}

test_default_bind() {
    local port=$1
    local listeners log

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "http://127.0.0.1:$port/" "startup log did not report the loopback bind"

    listeners=$(ss -ltnH)
    assert_contains "$listeners" "127.0.0.1:$port" "server was not bound to loopback"
    if [[ "$listeners" == *"0.0.0.0:$port"* ]]; then
        fail "server was unexpectedly bound to all interfaces by default"
    fi
}

test_explicit_wide_bind() {
    local port=$1
    local listeners log

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "http://0.0.0.0:$port/" "startup log did not report the explicit bind"

    listeners=$(ss -ltnH)
    assert_contains "$listeners" "0.0.0.0:$port" "explicit wide bind did not take effect"
}

test_bad_request_returns_400() {
    local port=$1
    local status_line

    status_line=$(
        raw_http "$port" $'GET  / HTTP/1.1\r\nHost: localhost\r\n\r\n' |
            tr -d '\r' |
            head -n 1
    )

    assert_eq "$status_line" "HTTP/1.1 400 Bad Request" "malformed request line did not produce 400"
}

test_symlink_escape_is_rejected() {
    local port=$1
    local status

    status=$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/escape.txt")
    assert_eq "$status" "404" "symlink escape was served"
}

test_oversized_file_is_rejected() {
    local port=$1
    local status

    status=$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/large.bin")
    assert_eq "$status" "413" "oversized file was not rejected safely"
}

test_slow_header_client_times_out() {
    local port=$1
    local slow_pid

    python3 - "$port" <<'PY' &
import socket
import sys
import time

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(b"GET /")
    time.sleep(7)
PY
    slow_pid=$!

    sleep 0.5
    timeout 8s curl -fsS --max-time 7 "http://127.0.0.1:$port/" >/dev/null
    wait "$slow_pid" || true
}

test_broken_pipe_does_not_crash_server() {
    local port=$1

    python3 - "$port" <<'PY'
import socket
import struct
import sys

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    linger = struct.pack("ii", 1, 0)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
    sock.sendall(b"GET /medium.bin HTTP/1.1\r\nHost: localhost\r\n\r\n")
PY

    sleep 0.2
    curl -fsS --max-time 2 "http://127.0.0.1:$port/" >/dev/null
}

test_slow_reader_write_timeout() {
    local port=$1
    local slow_pid

    python3 - "$port" <<'PY' &
import socket
import sys
import time

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(b"GET /medium.bin HTTP/1.1\r\nHost: localhost\r\n\r\n")
    time.sleep(7)
PY
    slow_pid=$!

    sleep 0.5
    timeout 10s curl -fsS --max-time 9 "http://127.0.0.1:$port/" >/dev/null
    wait "$slow_pid" || true
}

trap cleanup EXIT

make_fixture_tree

start_server "$next_port"
test_default_bind "$next_port"
test_bad_request_returns_400 "$next_port"
test_symlink_escape_is_rejected "$next_port"
test_oversized_file_is_rejected "$next_port"
test_slow_header_client_times_out "$next_port"
test_broken_pipe_does_not_crash_server "$next_port"
test_slow_reader_write_timeout "$next_port"
stop_server

next_port=$((next_port + 1))
start_server "$next_port" --bind 0.0.0.0
test_explicit_wide_bind "$next_port"
stop_server
