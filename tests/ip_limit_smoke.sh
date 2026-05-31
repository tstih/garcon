#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
slow_client_pid=""
tmpdir=$(mktemp -d)
next_port=$((33080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "ip-limit smoke test failed: $*" >&2
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
        "$BINARY" --port "$port" --workers 8 --queue-capacity 4 >"$tmpdir/server.log" 2>&1
    ) &
    server_pid=$!

    wait_for_server "$port"
}

start_slow_client() {
    local port=$1

    python3 - "$port" <<'PY' &
import socket
import sys
import time

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.sendall(b"GET /")
    time.sleep(6)
PY
    slow_client_pid=$!
}

test_per_ip_limit() {
    local port=$1
    local pids=()
    local log

    for _ in $(seq 1 12); do
        start_slow_client "$port"
        pids+=("$slow_client_pid")
    done

    sleep 0.5

    for pid in "${pids[@]}"; do
        wait "$pid" || true
    done

    curl -fsS --max-time 3 "http://127.0.0.1:$port/healthz" >/dev/null

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "per-IP concurrent connection limit exceeded: 127.0.0.1" \
        "server did not report per-IP connection limiting"
}

trap cleanup EXIT

mkdir -p "$tmpdir/www"
cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"

start_server "$next_port"
test_per_ip_limit "$next_port"
