#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
next_port=$((31080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "cors smoke test failed: $*" >&2
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

    : >"$tmpdir/server.log"

    (
        cd "$tmpdir"
        "$BINARY" --port "$port" >"$tmpdir/server.log" 2>&1
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
    mkdir -p "$tmpdir/www/api"
    cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"
    printf 'cors content\n' >"$tmpdir/www/api/ping.txt"
}

test_preflight_allowed() {
    local port=$1
    local response

    response=$(
        raw_http "$port" $'OPTIONS /api/ping.txt HTTP/1.1\r\nHost: localhost\r\nOrigin: http://localhost:3000\r\nAccess-Control-Request-Method: GET\r\nAccess-Control-Request-Headers: X-Garcon-API-Key\r\n\r\n' |
            tr -d '\r'
    )

    assert_contains "$response" "HTTP/1.1 204 No Content" \
        "allowed preflight should return 204"
    assert_contains "$response" "Access-Control-Allow-Origin: http://localhost:3000" \
        "allowed preflight should echo the configured origin"
    assert_contains "$response" "Access-Control-Allow-Methods: GET,HEAD,OPTIONS" \
        "allowed preflight should advertise configured methods"
    assert_contains "$response" "Access-Control-Allow-Headers: X-Garcon-API-Key,Content-Type" \
        "allowed preflight should advertise configured headers"
}

test_preflight_disallowed_origin_is_rejected() {
    local port=$1
    local status_line

    status_line=$(
        raw_http "$port" $'OPTIONS /api/ping.txt HTTP/1.1\r\nHost: localhost\r\nOrigin: http://evil.example\r\nAccess-Control-Request-Method: GET\r\n\r\n' |
            tr -d '\r' |
            head -n 1
    )

    assert_eq "$status_line" "HTTP/1.1 403 Forbidden" \
        "disallowed preflight origin should be rejected"
}

test_get_with_origin_and_api_key_gets_cors_headers() {
    local port=$1
    local headers body

    headers=$(mktemp)
    body=$(mktemp)
    trap 'rm -f "$headers" "$body"' RETURN

    curl -fsS \
        -D "$headers" \
        -o "$body" \
        -H 'Origin: http://localhost:3000' \
        -H 'X-Garcon-API-Key: dev-key' \
        "http://127.0.0.1:$port/api/ping.txt"

    assert_eq "$(cat "$body")" "cors content" \
        "allowed CORS GET should reach static files"
    assert_contains "$(tr -d '\r' <"$headers")" "Access-Control-Allow-Origin: http://localhost:3000" \
        "allowed CORS GET should include Access-Control-Allow-Origin"
}

test_cors_headers_survive_downstream_401() {
    local port=$1
    local response

    response=$(
        raw_http "$port" $'GET /api/ping.txt HTTP/1.1\r\nHost: localhost\r\nOrigin: http://localhost:3000\r\n\r\n' |
            tr -d '\r'
    )

    assert_contains "$response" "HTTP/1.1 401 Unauthorized" \
        "missing API key should still be rejected downstream"
    assert_contains "$response" "Access-Control-Allow-Origin: http://localhost:3000" \
        "CORS headers should survive downstream 401 responses"
}

trap cleanup EXIT

make_fixture_tree
start_server "$next_port"
test_preflight_allowed "$next_port"
test_preflight_disallowed_origin_is_rejected "$next_port"
test_get_with_origin_and_api_key_gets_cors_headers "$next_port"
test_cors_headers_survive_downstream_401 "$next_port"
stop_server
