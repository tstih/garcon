#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
next_port=$((32080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "keep-alive smoke test failed: $*" >&2
    exit 1
}

wait_for_server() {
    local port=$1

    for _ in $(seq 1 50); do
        if curl -fsS --max-time 1 "http://127.0.0.1:$port/healthz" >/dev/null 2>&1; then
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

    : >"$tmpdir/server.log"

    (
        cd "$tmpdir"
        "$BINARY" --port "$port" >"$tmpdir/server.log" 2>&1
    ) &
    server_pid=$!

    wait_for_server "$port"
}

test_keep_alive_and_close_behavior() {
    local port=$1

    python3 - "$port" <<'PY'
import socket
import sys

port = int(sys.argv[1])

def read_response(sock):
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise SystemExit("connection closed before response headers completed")
        data.extend(chunk)

    header_bytes, body = data.split(b"\r\n\r\n", 1)
    headers = header_bytes.decode("latin1").split("\r\n")
    status_line = headers[0]
    content_length = 0
    connection = None

    for line in headers[1:]:
        name, value = line.split(":", 1)
        if name.lower() == "content-length":
            content_length = int(value.strip())
        if name.lower() == "connection":
            connection = value.strip().lower()

    while len(body) < content_length:
        chunk = sock.recv(4096)
        if not chunk:
            raise SystemExit("connection closed before response body completed")
        body.extend(chunk)

    return status_line, connection, body[:content_length]

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(b"GET /healthz HTTP/1.1\r\nHost: localhost\r\n\r\n")
    status_1, connection_1, body_1 = read_response(sock)
    if status_1 != "HTTP/1.1 200 OK":
        raise SystemExit(f"unexpected first status line: {status_1}")
    if connection_1 != "keep-alive":
        raise SystemExit(f"first response did not keep connection alive: {connection_1}")
    if body_1 != b"ok":
        raise SystemExit(f"unexpected first response body: {body_1!r}")

    sock.sendall(b"GET /readyz HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    status_2, connection_2, body_2 = read_response(sock)
    if status_2 != "HTTP/1.1 200 OK":
        raise SystemExit(f"unexpected second status line: {status_2}")
    if connection_2 != "close":
        raise SystemExit(f"second response did not close connection: {connection_2}")
    if body_2 != b"ready":
        raise SystemExit(f"unexpected second response body: {body_2!r}")

    if sock.recv(1) != b"":
        raise SystemExit("connection stayed open after Connection: close")

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(b"GET /healthz HTTP/1.0\r\nHost: localhost\r\n\r\n")
    status, connection, body = read_response(sock)
    if status != "HTTP/1.1 200 OK":
        raise SystemExit(f"unexpected HTTP/1.0 status line: {status}")
    if connection != "close":
        raise SystemExit(f"HTTP/1.0 response should close by default: {connection}")
    if body != b"ok":
        raise SystemExit(f"unexpected HTTP/1.0 response body: {body!r}")
    if sock.recv(1) != b"":
        raise SystemExit("HTTP/1.0 connection stayed open without keep-alive")
PY
}

trap cleanup EXIT

mkdir -p "$tmpdir/www"
cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"

start_server "$next_port"
test_keep_alive_and_close_behavior "$next_port"
