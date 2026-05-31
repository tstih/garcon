#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
next_port=$((27080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "route-table smoke test failed: $*" >&2
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
    printf 'api pass through\n' >"$tmpdir/www/api/ping.txt"
}

test_healthz_route() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/healthz")
    assert_eq "$body" "ok" "healthz route did not return the configured body"
}

test_readyz_route() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/readyz")
    assert_eq "$body" "ready" "readyz route did not return the configured body"
}

test_method_specific_route() {
    local port=$1
    local status_line

    status_line=$(
        raw_http "$port" $'HEAD /healthz HTTP/1.1\r\nHost: localhost\r\n\r\n' |
            tr -d '\r' |
            head -n 1
    )

    assert_eq "$status_line" "HTTP/1.1 404 Not Found" \
        "HEAD /healthz should fall through when only GET is configured"
}

test_api_prefix_passes_to_static_files() {
    local port=$1
    local body

    body=$(curl -fsS -H 'X-Garcon-API-Key: dev-key' "http://127.0.0.1:$port/api/ping.txt")
    assert_eq "$body" "api pass through" \
        "api prefix route did not pass through to the static-files module"
}

trap cleanup EXIT

make_fixture_tree
start_server "$next_port"
test_healthz_route "$next_port"
test_readyz_route "$next_port"
test_method_specific_route "$next_port"
test_api_prefix_passes_to_static_files "$next_port"
stop_server
