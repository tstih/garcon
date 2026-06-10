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
    echo "rate-limit smoke test failed: $*" >&2
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
        fail "$message (needle '$needle' not found)"
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

make_fixture_tree() {
    mkdir -p "$tmpdir/www/api"
    cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"
    printf 'rate-limit test content\n' >"$tmpdir/www/api/ping.txt"
}

get_header() {
    # usage: get_header "$response" "Header-Name"
    local response=$1
    local name=$2
    # crude but sufficient for smoke: look for the header line
    printf '%s' "$response" | tr -d '\r' | awk -v h="$name:" 'tolower($0) ~ tolower("^" h) {print substr($0, length(h)+2); exit}'
}

test_healthz_not_limited() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/healthz")
    assert_eq "$body" "ok" "healthz should never be rate limited"
}

test_non_api_path_not_limited() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/")
    if [[ -z "$body" ]]; then
        fail "root path should not be rate limited"
    fi
}

test_success_includes_rate_headers() {
    local port=$1
    local response headers

    response=$(curl -sS -i \
        -H 'X-Garcon-API-Key: rate-smoke-key' \
        "http://127.0.0.1:$port/api/ping.txt")

    headers=$(printf '%s' "$response" | tr -d '\r' | head -n 30)

    assert_contains "$headers" "X-RateLimit-Limit:" "successful request should include X-RateLimit-Limit"
    assert_contains "$headers" "X-RateLimit-Remaining:" "successful request should include X-RateLimit-Remaining"
}

hammer_key_and_expect_429() {
    local port=$1
    local key=$2
    local attempts=140
    local got_429=0
    local got_success_with_headers=0
    local last_status=""

    for i in $(seq 1 "$attempts"); do
        local status
        status=$(curl -sS -o /dev/null -w '%{http_code}' \
            -H "X-Garcon-API-Key: $key" \
            --max-time 2 \
            "http://127.0.0.1:$port/api/ping.txt" || echo "000")

        last_status=$status

        if [[ "$status" == "429" ]]; then
            got_429=$((got_429 + 1))
            # fetch one 429 response with headers for inspection
            if [[ $got_429 -eq 1 ]]; then
                local resp
                resp=$(curl -sS -i \
                    -H "X-Garcon-API-Key: $key" \
                    --max-time 2 \
                    "http://127.0.0.1:$port/api/ping.txt" || true)
                assert_contains "$resp" "429" "429 response should contain status"
                assert_contains "$resp" "X-RateLimit-Limit:" "429 should include X-RateLimit-Limit"
                assert_contains "$resp" "Retry-After:" "429 should include Retry-After"
            fi
        elif [[ "$status" == "200" ]]; then
            got_success_with_headers=$((got_success_with_headers + 1))
        fi

        # If we've seen a decent number of 429s we can stop early
        if [[ $got_429 -ge 5 ]]; then
            break
        fi
    done

    if [[ $got_429 -lt 1 ]]; then
        fail "expected at least one 429 when hammering key '$key' (last status was $last_status, successes seen: $got_success_with_headers)"
    fi
}

test_independent_buckets() {
    local port=$1

    # Hammer a "bad" key (will be rejected by header-guard later) until rate limiter
    # starts returning 429 for that specific subject. This proves rate limiting
    # happens before header-guard.
    hammer_key_and_expect_429 "$port" "rate-smoke-bad-A"

    # A different bad key should *not* be rate limited yet (separate bucket),
    # so it should reach header-guard and get 401 (instead of 429).
    local status
    status=$(curl -sS -o /dev/null -w '%{http_code}' \
        -H 'X-Garcon-API-Key: rate-smoke-bad-B' \
        --max-time 2 \
        "http://127.0.0.1:$port/api/ping.txt")

    if [[ "$status" != "401" ]]; then
        fail "different rate limit key should have its own bucket and reach header-guard (got $status instead of 401)"
    fi
}

trap cleanup EXIT

make_fixture_tree
start_server "$next_port"

test_healthz_not_limited "$next_port"
test_non_api_path_not_limited "$next_port"
test_success_includes_rate_headers "$next_port"
hammer_key_and_expect_429 "$next_port" "rate-smoke-key"
test_independent_buckets "$next_port"

stop_server
echo "rate-limit smoke test passed"
