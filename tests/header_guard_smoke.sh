#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
next_port=$((29080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "header-guard smoke test failed: $*" >&2
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
    printf 'guarded api content\n' >"$tmpdir/www/api/ping.txt"
}

test_healthz_stays_open() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/healthz")
    assert_eq "$body" "ok" "healthz should remain open ahead of the header guard"
}

test_api_without_header_is_rejected() {
    local port=$1
    local response status_line body

    response=$(raw_http "$port" $'GET /api/ping.txt HTTP/1.1\r\nHost: localhost\r\n\r\n' | tr -d '\r')
    status_line=$(printf '%s' "$response" | head -n 1)
    body=$(printf '%s' "$response" | awk 'BEGIN{blank=0} blank{print} /^$/{blank=1}' | paste -sd '\n' -)

    assert_eq "$status_line" "HTTP/1.1 401 Unauthorized" \
        "missing API key should be rejected"
    assert_eq "$body" "missing or invalid api key" \
        "missing API key should return the configured body"
}

test_api_with_wrong_header_is_rejected() {
    local port=$1
    local status

    status=$(
        curl -sS -o /dev/null -w '%{http_code}' \
            -H 'X-Garcon-API-Key: wrong-key' \
            "http://127.0.0.1:$port/api/ping.txt"
    )

    assert_eq "$status" "401" "wrong API key should be rejected"
}

test_api_with_header_reaches_static_files() {
    local port=$1
    local body

    body=$(
        curl -fsS \
            -H 'x-garcon-api-key: dev-key' \
            "http://127.0.0.1:$port/api/ping.txt"
    )

    assert_eq "$body" "guarded api content" \
        "valid API key should allow the request to reach static files"
}

trap cleanup EXIT

make_fixture_tree
start_server "$next_port"
test_healthz_stays_open "$next_port"
test_api_without_header_is_rejected "$next_port"
test_api_with_wrong_header_is_rejected "$next_port"
test_api_with_header_reaches_static_files "$next_port"
stop_server
