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
    echo "https smoke test failed: $*" >&2
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

generate_certificate() {
    openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
        -keyout "$tmpdir/key.pem" \
        -out "$tmpdir/cert.pem" \
        -days 1 \
        -subj "/CN=127.0.0.1" >/dev/null 2>&1
}

make_fixture_tree() {
    mkdir -p "$tmpdir/www" "$tmpdir/outside"
    cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"
    printf 'secret\n' >"$tmpdir/outside/secret.txt"
    ln -s ../outside/secret.txt "$tmpdir/www/escape.txt"
    truncate -s 7340032 "$tmpdir/www/served.bin"
    truncate -s 9437184 "$tmpdir/www/large.bin"
}

wait_for_https_server() {
    local port=$1

    for _ in $(seq 1 50); do
        if curl -k -fsS --max-time 1 "https://127.0.0.1:$port/" >/dev/null 2>&1; then
            return 0
        fi

        if ! kill -0 "$server_pid" 2>/dev/null; then
            cat "$tmpdir/server.log" >&2 || true
            fail "server exited during startup"
        fi

        sleep 0.1
    done

    cat "$tmpdir/server.log" >&2 || true
    fail "server did not become ready on HTTPS port $port"
}

start_https_server() {
    local port=$1

    : >"$tmpdir/server.log"

    (
        cd "$tmpdir"
        "$BINARY" --port "$port" --tls-cert cert.pem --tls-key key.pem \
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

test_https_startup_log() {
    local port=$1
    local log

    log=$(cat "$tmpdir/server.log")
    assert_contains "$log" "https://127.0.0.1:$port/" "startup log did not report the HTTPS listener"
}

test_https_content_matches_http_root() {
    local port=$1
    local actual expected

    actual=$(curl -k -fsS "https://127.0.0.1:$port/")
    expected=$(cat "$SOURCE_DIR/www/index.html")
    assert_eq "$actual" "$expected" "HTTPS response body did not match the static index file"
}

test_https_adds_hsts() {
    local port=$1
    local headers

    headers=$(curl -k -sS -D - -o /dev/null "https://127.0.0.1:$port/")
    assert_contains "$(tr -d '\r' <<<"$headers")" \
        "Strict-Transport-Security: max-age=31536000" \
        "HTTPS responses should advertise HSTS"
}

test_https_symlink_escape_is_rejected() {
    local port=$1
    local status

    status=$(curl -k -sS -o /dev/null -w '%{http_code}' "https://127.0.0.1:$port/escape.txt")
    assert_eq "$status" "404" "symlink escape was served over HTTPS"
}

test_https_oversized_file_is_rejected() {
    local port=$1
    local status

    status=$(curl -k -sS -o /dev/null -w '%{http_code}' "https://127.0.0.1:$port/large.bin")
    assert_eq "$status" "413" "oversized file was not rejected safely over HTTPS"
}

test_plain_http_is_rejected_on_https_port() {
    local port=$1
    local response

    response=$(python3 - "$port" <<'PY'
import socket
import sys

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
    sock.settimeout(2)
    sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")

    try:
        data = sock.recv(256)
    except OSError:
        data = b""

sys.stdout.buffer.write(data)
PY
)

    if [[ "$response" == HTTP/1.1* ]]; then
        fail "plain HTTP request received an HTTP response on the HTTPS port"
    fi
}

test_missing_tls_key_fails() {
    local port=$1
    local output status

    set +e
    output=$(
        cd "$tmpdir" &&
            "$BINARY" --port "$port" --tls-cert cert.pem 2>&1
    )
    status=$?
    set -e

    if [[ $status -eq 0 ]]; then
        fail "startup unexpectedly succeeded with only --tls-cert"
    fi

    assert_contains "$output" "--tls-cert and --tls-key must be provided together" \
        "startup failure message did not explain the missing TLS key"
}

test_broken_tls_client_does_not_crash_server() {
    local port=$1

    python3 - "$port" <<'PY'
import socket
import ssl
import struct
import sys

port = int(sys.argv[1])
raw = socket.create_connection(("127.0.0.1", port), timeout=3)
raw.setsockopt(socket.SOL_SOCKET,
               socket.SO_LINGER,
               struct.pack("ii", 1, 0))

context = ssl._create_unverified_context()
sock = context.wrap_socket(raw, server_hostname="127.0.0.1")
sock.sendall(b"GET /served.bin HTTP/1.1\r\nHost: localhost\r\n\r\n")
sock.close()
PY

    sleep 0.2
    curl -k -fsS --max-time 3 "https://127.0.0.1:$port/" >/dev/null
}

test_stalled_tls_handshake_times_out() {
    local port=$1
    local slow_pid

    python3 - "$port" <<'PY' &
import socket
import sys
import time

port = int(sys.argv[1])

with socket.create_connection(("127.0.0.1", port), timeout=3):
    time.sleep(7)
PY
    slow_pid=$!

    sleep 0.5
    timeout 10s curl -k -fsS --max-time 9 "https://127.0.0.1:$port/" >/dev/null
    wait "$slow_pid" || true
}

trap cleanup EXIT

generate_certificate
make_fixture_tree
test_missing_tls_key_fails "$next_port"
start_https_server "$next_port"
test_https_startup_log "$next_port"
test_https_content_matches_http_root "$next_port"
test_https_adds_hsts "$next_port"
test_https_symlink_escape_is_rejected "$next_port"
test_https_oversized_file_is_rejected "$next_port"
test_plain_http_is_rejected_on_https_port "$next_port"
test_broken_tls_client_does_not_crash_server "$next_port"
test_stalled_tls_handshake_times_out "$next_port"
stop_server
