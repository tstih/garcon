// JWT-auth shared module implementation.
//
// This file contains the C++ module logic for a small ordered JWT / Bearer
// token authentication policy module using HS256 only.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "jwt_auth_module.h"

#include <cctype>
#include <cstring>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace garcon::modules {

namespace {

std::string trim(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::vector<std::string> split_fields(std::string_view value, char delimiter)
{
    std::vector<std::string> fields;

    std::size_t begin = 0;
    while (begin <= value.size()) {
        auto end = value.find(delimiter, begin);
        if (end == std::string_view::npos)
            end = value.size();

        fields.push_back(trim(value.substr(begin, end - begin)));

        if (end == value.size())
            break;
        begin = end + 1;
    }

    return fields;
}

char ascii_lower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<char>(ch - 'A' + 'a');
    return ch;
}

std::string ascii_lowercase(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (const char ch : value)
        lowered.push_back(ascii_lower(ch));
    return lowered;
}

bool ascii_iequals(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (ascii_lower(left[i]) != ascii_lower(right[i]))
            return false;
    }

    return true;
}

bool is_rule_line(std::string_view line)
{
    return line.substr(0, 5) == "rule=";
}

jwt_auth_module::pattern_kind classify_target_pattern(std::string& pattern)
{
    if (!pattern.empty() && pattern.back() == '*') {
        pattern.pop_back();
        return jwt_auth_module::pattern_kind::prefix;
    }

    return jwt_auth_module::pattern_kind::exact;
}

jwt_auth_module::auth_rule parse_rule_line(std::string_view line)
{
    const auto fields = split_fields(line.substr(5), '|');
    if (fields.size() != 2)
        throw std::runtime_error("jwt_auth: rule requires 2 fields (method|target)");

    jwt_auth_module::auth_rule rule;
    rule.method_pattern = fields[0];
    rule.target_pattern = fields[1];
    rule.target_kind = classify_target_pattern(rule.target_pattern);

    if (rule.method_pattern.empty())
        throw std::runtime_error("jwt_auth: method pattern must not be empty");
    if (rule.target_pattern.empty())
        throw std::runtime_error("jwt_auth: target pattern must not be empty");

    return rule;
}

std::vector<jwt_auth_module::auth_rule> parse_rules(std::string_view config_text)
{
    std::vector<jwt_auth_module::auth_rule> rules;

    std::size_t line_begin = 0;
    while (line_begin <= config_text.size()) {
        auto line_end = config_text.find('\n', line_begin);
        if (line_end == std::string_view::npos)
            line_end = config_text.size();

        const auto raw_line = config_text.substr(line_begin, line_end - line_begin);
        const auto line = trim(raw_line);
        if (!line.empty() && line.front() != '#' && is_rule_line(line))
            rules.push_back(parse_rule_line(line));

        if (line_end == config_text.size())
            break;
        line_begin = line_end + 1;
    }

    return rules;
}

bool method_matches(std::string_view pattern, std::string_view actual)
{
    return pattern == "*" || pattern == actual;
}

bool target_matches(jwt_auth_module::pattern_kind kind,
                    std::string_view pattern,
                    std::string_view actual)
{
    switch (kind) {
    case jwt_auth_module::pattern_kind::exact:
        return pattern == actual;
    case jwt_auth_module::pattern_kind::prefix:
        return actual.size() >= pattern.size() &&
               actual.substr(0, pattern.size()) == pattern;
    }

    return false;
}

std::optional<std::string> extract_bearer_token(std::string_view auth_value)
{
    auto val = trim(auth_value);
    if (val.empty())
        return std::nullopt;

    auto sp = val.find(' ');
    if (sp == std::string_view::npos)
        return std::nullopt;

    auto scheme = ascii_lowercase(trim(val.substr(0, sp)));
    if (scheme != "bearer")
        return std::nullopt;

    auto token = trim(val.substr(sp + 1));
    if (token.empty())
        return std::nullopt;

    return std::string(token);
}

std::string base64url_decode(std::string_view input)
{
    // Minimal base64url decoder (no padding required). Maps -_ to +/.
    //
    // The entire JWT verification surface (this decoder + the tiny flat-JSON claim
    // extractors below + the HS256-only validate_and_extract) is deliberately
    // self-contained and small. This keeps the module a pure, standalone policy
    // module (no new external dependencies, no core/ABI changes, no framework
    // machinery) so that the implementation remains reviewable in one sitting and
    // follows the project's explicit preference for small focused modules over
    // anticipatory generality. See Guardian standards checklist and rate_limit/
    // header_guard as the reference voice.
    static const signed char table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::string out;
    out.reserve((input.size() * 3) / 4 + 1);

    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (c == '-' || c == '_' || c == '+' || c == '/' || (c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            signed char b = (c == '-' ? table[(unsigned char)'+'] :
                             c == '_' ? table[(unsigned char)'/'] :
                             table[c]);
            if (b < 0)
                continue; // invalid ignored for robustness, but we will fail later on sig
            val = (val << 6) + b;
            valb += 6;
            if (valb >= 0) {
                out.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
    }
    return out;
}

std::string extract_json_string(std::string_view payload, std::string_view claim)
{
    std::string search = "\"" + std::string(claim) + "\":\"";
    auto pos = payload.find(search);
    if (pos == std::string_view::npos) {
        // try with space after colon for some serializers
        search = "\"" + std::string(claim) + "\": \"";
        pos = payload.find(search);
        if (pos == std::string_view::npos)
            return {};
    }
    pos += search.size();

    std::string val;
    bool escape = false;
    for (std::size_t i = pos; i < payload.size(); ++i) {
        char c = payload[i];
        if (escape) {
            val += c;
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"')
            break;
        val += c;
    }
    return val;
}

long long extract_json_int(std::string_view payload, std::string_view claim, long long def = 0)
{
    std::string search = "\"" + std::string(claim) + "\":";
    auto pos = payload.find(search);
    if (pos == std::string_view::npos)
        return def;
    pos += search.size();

    while (pos < payload.size() &&
           std::isspace(static_cast<unsigned char>(payload[pos])))
        ++pos;

    if (pos >= payload.size())
        return def;

    bool neg = false;
    if (payload[pos] == '-') {
        neg = true;
        ++pos;
    }

    long long v = 0;
    bool seen = false;
    for (std::size_t i = pos; i < payload.size(); ++i) {
        char c = payload[i];
        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
            seen = true;
        } else {
            break;
        }
    }
    if (!seen)
        return def;
    return neg ? -v : v;
}

struct jwt_claims
{
    std::string sub;
    std::string roles;
    std::string iss;
    std::string aud;
    long long exp = 0;
};

bool validate_and_extract(std::string_view token,
                          std::string_view secret,
                          std::optional<std::string_view> required_issuer,
                          std::optional<std::string_view> required_audience,
                          jwt_claims& out_claims,
                          std::string& out_failure)
{
    out_failure.clear();
    out_claims = {};

    auto p1 = token.find('.');
    if (p1 == std::string_view::npos) {
        out_failure = "malformed";
        return false;
    }
    auto p2 = token.find('.', p1 + 1);
    if (p2 == std::string_view::npos) {
        out_failure = "malformed";
        return false;
    }
    if (token.find('.', p2 + 1) != std::string_view::npos) {
        out_failure = "malformed";
        return false;
    }

    std::string_view h64 = token.substr(0, p1);
    std::string_view p64 = token.substr(p1 + 1, p2 - p1 - 1);
    std::string_view s64 = token.substr(p2 + 1);
    if (h64.empty() || p64.empty() || s64.empty()) {
        out_failure = "malformed";
        return false;
    }

    std::string header = base64url_decode(h64);
    std::string payload = base64url_decode(p64);
    std::string sig = base64url_decode(s64);

    // Require HS256 explicitly (defense in depth)
    if (header.find("\"alg\":\"HS256\"") == std::string::npos &&
        header.find("\"alg\": \"HS256\"") == std::string::npos) {
        out_failure = "malformed";
        return false;
    }

    // Verify signature with HMAC-SHA256
    std::string to_sign = std::string(h64) + "." + std::string(p64);
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int mac_len = 0;
    if (HMAC(EVP_sha256(),
             reinterpret_cast<const unsigned char*>(secret.data()),
             static_cast<int>(secret.size()),
             reinterpret_cast<const unsigned char*>(to_sign.data()),
             static_cast<int>(to_sign.size()),
             mac, &mac_len) == nullptr) {
        out_failure = "bad signature";
        return false;
    }

    if (mac_len != sig.size() ||
        std::memcmp(mac, sig.data(), mac_len) != 0) {
        out_failure = "bad signature";
        return false;
    }

    // Extract claims (minimal, no full JSON parser)
    out_claims.sub = extract_json_string(payload, "sub");
    out_claims.iss = extract_json_string(payload, "iss");
    out_claims.aud = extract_json_string(payload, "aud");
    if (out_claims.aud.empty()) {
        // simplistic array support for aud:["..."]
        auto apos = payload.find("\"aud\":[");
        if (apos != std::string_view::npos) {
            auto q1 = payload.find('"', apos + 7);
            if (q1 != std::string_view::npos) {
                auto q2 = payload.find('"', q1 + 1);
                if (q2 != std::string_view::npos) {
                    out_claims.aud = std::string(payload.substr(q1 + 1, q2 - q1 - 1));
                }
            }
        }
    }
    out_claims.roles = extract_json_string(payload, "roles");
    if (out_claims.roles.empty())
        out_claims.roles = extract_json_string(payload, "role");
    out_claims.exp = extract_json_int(payload, "exp");

    // Optional issuer/audience checks (do not leak details in failure body)
    if (required_issuer && !required_issuer->empty() &&
        out_claims.iss != *required_issuer) {
        out_failure = "issuer mismatch";
        return false;
    }
    if (required_audience && !required_audience->empty() &&
        out_claims.aud != *required_audience) {
        out_failure = "audience mismatch";
        return false;
    }

    // Expiry (standard numeric date)
    if (out_claims.exp != 0) {
        long long now = static_cast<long long>(std::time(nullptr));
        if (out_claims.exp <= now) {
            out_failure = "expired";
            return false;
        }
    }

    return true;
}

http::response make_unauthorized(std::string_view body)
{
    return http::response::text(401, "Unauthorized", std::string(body));
}

} // namespace

jwt_auth_module::jwt_auth_module(const garcon::module::host_context&,
                                 std::string_view config_text)
    : _rules(parse_rules(config_text))
{
    // Top-level keys via the provided helper (duplicates style of simple modules)
    garcon::module::key_value_config kv(config_text);

    auto sec = kv.get("secret");
    if (!sec || sec->empty())
        throw std::runtime_error("jwt_auth: secret must not be empty");
    _secret = std::string(*sec);

    auto algv = kv.get("alg");
    _alg = algv ? std::string(*algv) : std::string("HS256");
    if (_alg != "HS256")
        throw std::runtime_error("jwt_auth: only alg=HS256 is supported");

    if (auto iss = kv.get("issuer"); iss && !iss->empty())
        _issuer = std::string(*iss);

    if (auto aud = kv.get("audience"); aud && !aud->empty())
        _audience = std::string(*aud);
}

garcon::module::result jwt_auth_module::handle(const http::request& request) const
{
    // Determine if this request is in scope for JWT enforcement
    bool enforce = _rules.empty();
    if (!enforce) {
        for (const auto& rule : _rules) {
            if (method_matches(rule.method_pattern, request.method) &&
                target_matches(rule.target_kind, rule.target_pattern, request.target)) {
                enforce = true;
                break;
            }
        }
    }

    if (!enforce)
        return garcon::module::result::pass();

    auto auth_val = request.header_value("Authorization");
    std::optional<std::string> token;
    if (auth_val)
        token = extract_bearer_token(*auth_val);

    if (!token) {
        return garcon::module::result::respond(
            make_unauthorized("missing token\n"));
    }

    jwt_claims claims;
    std::string failure;
    if (!validate_and_extract(*token, _secret,
                              _issuer ? std::optional<std::string_view>(*_issuer) : std::nullopt,
                              _audience ? std::optional<std::string_view>(*_audience) : std::nullopt,
                              claims, failure)) {
        std::string body = "invalid token\n";
        if (failure == "expired")
            body = "token expired\n";
        return garcon::module::result::respond(make_unauthorized(body));
    }

    // Success: pass and inject selected claim headers (never the raw token)
    auto result = garcon::module::result::pass();
    if (!claims.sub.empty())
        result.add_response_header("X-Garcon-User-Id", claims.sub);
    if (!claims.roles.empty())
        result.add_response_header("X-Garcon-Roles", claims.roles);
    if (!claims.iss.empty())
        result.add_response_header("X-Garcon-Claim-iss", claims.iss);
    if (!claims.aud.empty())
        result.add_response_header("X-Garcon-Claim-aud", claims.aud);

    return result;
}

} // namespace garcon::modules
