#!/usr/bin/env bash
set -euo pipefail

BINARY=$1
SOURCE_DIR=$2

server_pid=""
tmpdir=$(mktemp -d)
mods_dir=""
next_port=$((28080 + RANDOM % 1000))

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$tmpdir"
}

fail() {
    echo "router smoke test failed: $*" >&2
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
        fail "$message (needle '$needle' not in '$haystack')"
    fi
}

wait_for_server() {
    local port=$1

    for _ in $(seq 1 50); do
        # Probe tolerates 404 (which isolated router returns for /) because -f would
        # fail on non-2xx. We only care that the server is up and processing HTTP.
        local code
        code=$(curl -sS --max-time 1 -o /dev/null -w "%{http_code}" "http://127.0.0.1:$port/" 2>/dev/null || echo 000)
        if [[ "$code" =~ ^(200|404|405|501)$ ]]; then
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
        "$BINARY" --port "$port" --modules-dir "$mods_dir" >"$tmpdir/server.log" 2>&1
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

setup_router_only_modules() {
    mods_dir="$tmpdir/mods"
    mkdir -p "$mods_dir"
    local so_path
    so_path="$(dirname "$BINARY")/modules/libgarcon_router_module.so"
    if [[ ! -f "$so_path" ]]; then
        fail "expected built router module at $so_path"
    fi
    cat >"$mods_dir/01-router.conf" <<EOF
path=$so_path
route=GET|/healthz|respond|200|text/plain; charset=utf-8|ok
route=GET|/readyz|respond|200|text/plain; charset=utf-8|ready
route=GET|/users/{id}|pass
route=GET|/api/{version}/items/{item_id}|pass
route=GET|/files/{path}/*|pass
route=POST|/echo|respond|200|text/plain; charset=utf-8|echoed
EOF
}

test_respond_healthz() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/healthz")
    assert_eq "$body" "ok" "healthz respond route did not return configured body"
}

test_respond_readyz() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/readyz")
    assert_eq "$body" "ready" "readyz respond route did not return configured body"
}

test_respond_method_mismatch_is_404() {
    local port=$1
    local status_line

    status_line=$(
        raw_http "$port" $'HEAD /healthz HTTP/1.1\r\nHost: localhost\r\n\r\n' |
            tr -d '\r' |
            head -n 1
    )

    assert_eq "$status_line" "HTTP/1.1 404 Not Found" \
        "HEAD /healthz with only GET route should fall through to pipeline 404 (independence)"
}

test_pass_single_param_injects_header_on_404() {
    local port=$1
    local headers

    headers=$(curl -sS -D - -o /dev/null "http://127.0.0.1:$port/users/123" 2>/dev/null | tr -d '\r')
    assert_contains "$headers" "X-Garcon-Route-Param-Id: 123" \
        "pass route with {id} must inject X-Garcon-Route-Param-Id even on downstream 404"
    # confirm final status from pipeline (no other modules)
    assert_contains "$headers" "HTTP/1.1 404 Not Found" \
        "unhandled pass should yield pipeline 404"
}

test_pass_multi_param_injects_headers() {
    local port=$1
    local headers

    headers=$(curl -sS -D - -o /dev/null "http://127.0.0.1:$port/api/v2/items/xyz-42" 2>/dev/null | tr -d '\r')
    assert_contains "$headers" "X-Garcon-Route-Param-Version: v2" \
        "multi-param route must inject Version"
    assert_contains "$headers" "X-Garcon-Route-Param-Item-Id: xyz-42" \
        "multi-param route must inject Item-Id (from {item_id})"
}

test_pass_prefix_param() {
    local port=$1
    local headers

    headers=$(curl -sS -D - -o /dev/null "http://127.0.0.1:$port/files/a/b/c.txt" 2>/dev/null | tr -d '\r')
    assert_contains "$headers" "X-Garcon-Route-Param-Path: a" \
        "prefix-with-param should capture first segment for {path} (design choice for simple segment matcher)"
    assert_contains "$headers" "HTTP/1.1 404 Not Found" \
        "prefix pass must still fall to 404"
}

test_direct_respond_no_param_header_needed() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/echo" -X POST)
    assert_eq "$body" "echoed" "direct respond body should be returned"
    # respond routes do not attach param headers (only pass does), which is fine
}

test_unmatched_target_yields_404_no_special_headers() {
    local port=$1
    local headers

    headers=$(curl -sS -D - -o /dev/null "http://127.0.0.1:$port/no/such/route" 2>/dev/null | tr -d '\r')
    assert_contains "$headers" "HTTP/1.1 404 Not Found" \
        "completely unmatched request must 404"
    if [[ "$headers" == *"X-Garcon-Route-Param"* ]]; then
        fail "unmatched request must not receive any route-param headers"
    fi
}

trap cleanup EXIT

setup_router_only_modules
start_server "$next_port"
test_respond_healthz "$next_port"
test_respond_readyz "$next_port"
test_respond_method_mismatch_is_404 "$next_port"
test_pass_single_param_injects_header_on_404 "$next_port"
test_pass_multi_param_injects_headers "$next_port"
test_pass_prefix_param "$next_port"
test_direct_respond_no_param_header_needed "$next_port"
test_unmatched_target_yields_404_no_special_headers "$next_port"
stop_server

echo "router smoke test passed"
