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
    echo "jwt-auth smoke test failed: $*" >&2
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
    mkdir -p "$tmpdir/www/jwt"
    cp -R "$SOURCE_DIR/www/." "$tmpdir/www/"
    printf 'jwt protected content\n' >"$tmpdir/www/jwt/ping.txt"
}

# Extract secret from the example config in source tree (used for signing test tokens)
get_secret() {
    sed -n 's/^secret=//p' "$SOURCE_DIR/modules.d/09-jwt-auth.conf" | tr -d '\r' | head -1
}

# Generate a HS256 JWT using only stdlib python (matches the module's alg/claims expectations)
make_jwt() {
    local secret=$1
    local sub=$2
    local exp_offset=$3   # relative seconds; negative for expired
    local extra_roles=${4:-"user"}

    python3 - "$secret" "$sub" "$exp_offset" "$extra_roles" <<'PY'
import sys, json, hmac, hashlib, base64, time

secret = sys.argv[1].encode("utf-8")
sub = sys.argv[2]
exp_offset = int(sys.argv[3])
roles = sys.argv[4]

now = int(time.time())
exp = now + exp_offset

header = {"alg": "HS256", "typ": "JWT"}
payload = {
    "sub": sub,
    "exp": exp,
    "iss": "garcon",
    "aud": "api",
    "roles": roles
}

def b64url(b):
    if isinstance(b, str):
        b = b.encode("utf-8")
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode("ascii")

h = b64url(json.dumps(header, separators=(",", ":")))
p = b64url(json.dumps(payload, separators=(",", ":")))
sig = hmac.new(secret, f"{h}.{p}".encode("utf-8"), hashlib.sha256).digest()
s = b64url(sig)
print(f"{h}.{p}.{s}")
PY
}

test_public_paths_not_affected() {
    local port=$1
    local body

    body=$(curl -fsS "http://127.0.0.1:$port/healthz")
    assert_eq "$body" "ok" "healthz (unscoped) must remain open without token"

    body=$(curl -fsS "http://127.0.0.1:$port/")
    assert_contains "$body" "Garçon" "root must still be served (unscoped)"
}

test_missing_token_is_rejected() {
    local port=$1
    local response status_line body

    response=$(raw_http "$port" $'GET /jwt/ping.txt HTTP/1.1\r\nHost: localhost\r\n\r\n' | tr -d '\r')
    status_line=$(printf '%s' "$response" | head -n 1)
    body=$(printf '%s' "$response" | awk 'BEGIN{blank=0} blank{print} /^$/{blank=1}' | paste -sd '\n' -)

    assert_eq "$status_line" "HTTP/1.1 401 Unauthorized" \
        "missing token on scoped path must be rejected"
    assert_eq "$body" "missing token" \
        "missing token must return the expected body"
}

test_bad_signature_is_rejected() {
    local port=$1
    local secret sub token bad_token status

    secret=$(get_secret)
    sub="bob"
    token=$(make_jwt "$secret" "$sub" 60 "tester")
    # Corrupt signature
    bad_token="${token%.*}.CORRUPTEDSIG"

    status=$(
        curl -sS -o /dev/null -w '%{http_code}' \
            -H "Authorization: Bearer $bad_token" \
            "http://127.0.0.1:$port/jwt/ping.txt"
    )

    assert_eq "$status" "401" "bad signature must be rejected with 401"
}

test_expired_token_is_rejected() {
    local port=$1
    local secret sub token status body response

    secret=$(get_secret)
    sub="carol"
    token=$(make_jwt "$secret" "$sub" -120 "expired-role")

    response=$(raw_http "$port" \
        $'GET /jwt/ping.txt HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer '"$token"$'\r\n\r\n' | tr -d '\r')
    status_line=$(printf '%s' "$response" | head -n 1)
    body=$(printf '%s' "$response" | awk 'BEGIN{blank=0} blank{print} /^$/{blank=1}' | paste -sd '\n' -)

    assert_eq "$status_line" "HTTP/1.1 401 Unauthorized" \
        "expired token must return 401"
    assert_eq "$body" "token expired" \
        "expired token must surface a clear non-leaky message"
}

test_valid_token_passes_and_injects_headers() {
    local port=$1
    local secret sub token body header_out

    secret=$(get_secret)
    sub="dave"
    token=$(make_jwt "$secret" "$sub" 300 "admin,viewer")

    # Body verification (fsS will fail the test on non-success)
    body=$(
        curl -fsS \
            -H "Authorization: Bearer $token" \
            -H "Host: localhost" \
            "http://127.0.0.1:$port/jwt/ping.txt"
    )
    assert_eq "$body" "jwt protected content" \
        "valid token must reach the protected static content"

    # Separate header injection check via HEAD (headers are attached on pass for the eventual response)
    header_out=$(
        curl -sS -I \
            -H "Authorization: Bearer $token" \
            -H "Host: localhost" \
            "http://127.0.0.1:$port/jwt/ping.txt"
    )
    # Header injection checks (case-insensitive header name match in practice)
    assert_contains "$header_out" "X-Garcon-User-Id: dave" \
        "valid token must inject X-Garcon-User-Id from sub claim"
    assert_contains "$header_out" "X-Garcon-Roles: admin,viewer" \
        "valid token must inject X-Garcon-Roles from roles claim"
    assert_contains "$header_out" "X-Garcon-Claim-iss: garcon" \
        "valid token must inject general X-Garcon-Claim-iss"
    assert_contains "$header_out" "X-Garcon-Claim-aud: api" \
        "valid token must inject general X-Garcon-Claim-aud"
}

test_scoped_rules_only() {
    local port=$1
    local status

    # A path not covered by the rule in 09-jwt-auth.conf must not require token
    # (even if it would 404 at static, we only care it didn't 401 at jwt)
    status=$(
        curl -sS -o /dev/null -w '%{http_code}' \
            "http://127.0.0.1:$port/jwt-missing-rule/anything.txt"
    )
    # 404 is fine (static), 401 would mean jwt wrongly enforced
    if [[ "$status" == "401" ]]; then
        fail "unscoped path must not be rejected by jwt (scoped rules not honored)"
    fi
}

trap cleanup EXIT

secret=$(get_secret)
if [[ -z "$secret" ]]; then
    fail "could not read secret from modules.d/09-jwt-auth.conf"
fi

make_fixture_tree
start_server "$next_port"
test_public_paths_not_affected "$next_port"
test_missing_token_is_rejected "$next_port"
test_bad_signature_is_rejected "$next_port"
test_expired_token_is_rejected "$next_port"
test_valid_token_passes_and_injects_headers "$next_port"
test_scoped_rules_only "$next_port"
stop_server

echo "jwt-auth smoke test passed"
