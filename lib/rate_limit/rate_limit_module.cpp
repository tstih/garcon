// Rate limit shared module implementation.
//
// This file contains the C++ module logic for a token-bucket rate limiter
// usable as a gateway-style module. It supports per-header-key (typical for
// API keys), global, and simple prefix/exact route matching.
//
// The implementation is intentionally self-contained and thread-safe because
// handle() may be called concurrently from multiple worker threads on the
// same module instance.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "rate_limit_module.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

bool is_rule_line(std::string_view line)
{
    return line.substr(0, 5) == "rule=";
}

rate_limit_module::pattern_kind classify_target_pattern(std::string& pattern)
{
    if (!pattern.empty() && pattern.back() == '*') {
        pattern.pop_back();
        return rate_limit_module::pattern_kind::prefix;
    }
    return rate_limit_module::pattern_kind::exact;
}

bool method_matches(std::string_view pattern, std::string_view actual)
{
    return pattern == "*" || pattern == actual;
}

bool target_matches(rate_limit_module::pattern_kind kind,
                    std::string_view pattern,
                    std::string_view actual)
{
    switch (kind) {
    case rate_limit_module::pattern_kind::exact:
        return pattern == actual;
    case rate_limit_module::pattern_kind::prefix:
        return actual.size() >= pattern.size() &&
               actual.substr(0, pattern.size()) == pattern;
    }
    return false;
}

int parse_positive_int(std::string_view value, std::string_view what)
{
    if (value.empty())
        throw std::runtime_error(std::string("rate_limit: ") + std::string(what) + " must not be empty");

    int v = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9')
            throw std::runtime_error(std::string("rate_limit: invalid ") + std::string(what));
        v = (v * 10) + (ch - '0');
    }
    if (v <= 0)
        throw std::runtime_error(std::string("rate_limit: ") + std::string(what) + " must be positive");
    return v;
}

int parse_window_seconds(std::string_view value)
{
    if (value.empty())
        throw std::runtime_error("rate_limit: window must not be empty");

    // Accept forms: 60s, 5m, 1h, or bare seconds number
    std::string_view num_part = value;
    int multiplier = 1;

    if (!value.empty()) {
        char last = value.back();
        if (last == 's' || last == 'S') {
            num_part = value.substr(0, value.size() - 1);
            multiplier = 1;
        } else if (last == 'm' || last == 'M') {
            num_part = value.substr(0, value.size() - 1);
            multiplier = 60;
        } else if (last == 'h' || last == 'H') {
            num_part = value.substr(0, value.size() - 1);
            multiplier = 3600;
        }
    }

    int seconds = parse_positive_int(num_part, "window");
    long long total = static_cast<long long>(seconds) * multiplier;
    if (total > 1000000000LL) // sanity
        throw std::runtime_error("rate_limit: window too large");
    return static_cast<int>(total);
}

rate_limit_module::key_kind parse_key_spec(std::string_view spec, std::string& out_name)
{
    out_name.clear();
    if (spec == "global") {
        return rate_limit_module::key_kind::global;
    }
    if (spec.size() > 7 && spec.substr(0, 7) == "header:") {
        out_name = std::string(spec.substr(7));
        if (out_name.empty())
            throw std::runtime_error("rate_limit: header key name must not be empty");
        return rate_limit_module::key_kind::header;
    }
    // Convenience: bare header name is treated as header:NAME
    if (!spec.empty() && spec.find(':') == std::string_view::npos) {
        out_name = std::string(spec);
        return rate_limit_module::key_kind::header;
    }
    throw std::runtime_error("rate_limit: key must be 'global' or 'header:NAME' (or bare NAME)");
}

std::string ascii_lowercase(std::string_view v)
{
    std::string out;
    out.reserve(v.size());
    for (char ch : v) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

std::optional<std::string_view> find_header_icase(const http::request& request,
                                                  std::string_view name)
{
    const std::string lower = ascii_lowercase(name);
    for (const auto& h : request.headers) {
        if (ascii_lowercase(h.name) == lower)
            return h.value;
    }
    return std::nullopt;
}

std::string extract_subject(const http::request& request, const rate_limit_module::rule& r)
{
    if (r.key_type == rate_limit_module::key_kind::global) {
        return "global";
    }
    // header key
    auto val = find_header_icase(request, r.key_name);
    if (!val || val->empty()) {
        return "<anonymous>";
    }
    return std::string(*val);
}

http::response make_rate_limit_response(int limit,
                                        int window_seconds,
                                        int retry_after_seconds)
{
    http::response resp = http::response::text(
        429,
        "Too Many Requests",
        "rate limit exceeded\n");

    resp.set_header("X-RateLimit-Limit", std::to_string(limit));
    resp.set_header("Retry-After", std::to_string(retry_after_seconds > 0 ? retry_after_seconds : 1));

    // Best-effort reset time (window from now)
    resp.set_header("X-RateLimit-Reset", std::to_string(window_seconds));

    return resp;
}

} // namespace

rate_limit_module::rate_limit_module(const garcon::module::host_context&,
                                     std::string_view config_text)
{
    std::vector<rule> parsed;

    std::size_t line_begin = 0;
    while (line_begin <= config_text.size()) {
        auto line_end = config_text.find('\n', line_begin);
        if (line_end == std::string_view::npos)
            line_end = config_text.size();

        const auto raw = config_text.substr(line_begin, line_end - line_begin);
        const auto line = trim(raw);

        if (!line.empty() && line.front() != '#' && is_rule_line(line)) {
            const auto fields = split_fields(line.substr(5), '|');
            if (fields.size() < 5)
                throw std::runtime_error("rate_limit: rule requires at least 5 fields (method|target|key|limit|window)");

            rule r;
            r.method_pattern = fields[0];
            r.target_pattern = fields[1];
            r.target_kind = classify_target_pattern(r.target_pattern);

            if (r.method_pattern.empty())
                throw std::runtime_error("rate_limit: method pattern must not be empty");
            if (r.target_pattern.empty())
                throw std::runtime_error("rate_limit: target pattern must not be empty");

            std::string key_name;
            r.key_type = parse_key_spec(fields[2], key_name);
            r.key_name = std::move(key_name);

            r.limit = parse_positive_int(fields[3], "limit");
            r.window_seconds = parse_window_seconds(fields[4]);

            // Optional 6th field = burst override
            if (fields.size() >= 6 && !fields[5].empty()) {
                r.burst = parse_positive_int(fields[5], "burst");
            } else {
                r.burst = r.limit;
            }

            if (r.burst < r.limit)
                r.burst = r.limit; // at least allow the sustained rate as burst

            parsed.push_back(std::move(r));
        }

        if (line_end == config_text.size())
            break;
        line_begin = line_end + 1;
    }

    if (parsed.empty())
        throw std::runtime_error("rate_limit: at least one rule= line is required");

    _rules = std::move(parsed);
}

std::string rate_limit_module::make_state_key(std::size_t rule_index,
                                              std::string_view subject) const
{
    return std::to_string(rule_index) + ":" + std::string(subject);
}

std::optional<http::response>
rate_limit_module::check_and_consume(const rule& r,
                                     std::size_t rule_index,
                                     std::string_view subject) const
{
    const auto now = std::chrono::steady_clock::now();
    const std::string skey = make_state_key(rule_index, subject);

    const double rate_per_sec = static_cast<double>(r.limit) / static_cast<double>(r.window_seconds > 0 ? r.window_seconds : 1);
    const double max_tokens = static_cast<double>(r.burst);

    std::lock_guard<std::mutex> lock(_mutex);

    auto& bucket = _buckets[skey];
    if (bucket.last_refill.time_since_epoch().count() == 0) {
        // first time seeing this subject for this rule
        bucket.tokens = max_tokens;
        bucket.last_refill = now;
    }

    // Refill
    const auto elapsed = std::chrono::duration<double>(now - bucket.last_refill).count();
    if (elapsed > 0.0) {
        bucket.tokens = std::min(bucket.tokens + (elapsed * rate_per_sec), max_tokens);
        bucket.last_refill = now;
    }

    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;

        // caller will attach headers on the pass result
        return std::nullopt; // allowed
    }

    // Compute a reasonable retry-after: time until at least 1 token
    double needed = 1.0 - bucket.tokens;
    int retry_after = 1;
    if (rate_per_sec > 0.0) {
        double secs = needed / rate_per_sec;
        retry_after = static_cast<int>(secs + 0.999);
        if (retry_after < 1) retry_after = 1;
        if (retry_after > r.window_seconds) retry_after = r.window_seconds;
    }

    return make_rate_limit_response(r.limit, r.window_seconds, retry_after);
}

garcon::module::result rate_limit_module::handle(const http::request& request) const
{
    for (std::size_t i = 0; i < _rules.size(); ++i) {
        const auto& r = _rules[i];

        if (!method_matches(r.method_pattern, request.method))
            continue;
        if (!target_matches(r.target_kind, r.target_pattern, request.target))
            continue;

        const std::string subject = extract_subject(request, r);
        auto maybe_response = check_and_consume(r, i, subject);

        if (maybe_response) {
            // rate limited
            http::response resp = std::move(*maybe_response);
            // Ensure the standard headers are present even if make_... missed one
            if (!resp.find_header("X-RateLimit-Limit"))
                resp.set_header("X-RateLimit-Limit", std::to_string(r.limit));
            return garcon::module::result::respond(std::move(resp));
        }

        // Allowed: pass downstream but attach rate limit info headers
        // We report the configured limit and a best-effort remaining.
        // To get accurate "remaining" we need the current token count after decrement.
        // We re-query under lock briefly (cheap) to report remaining.
        std::string remaining_str = std::to_string(r.burst); // fallback
        {
            std::lock_guard<std::mutex> lock(_mutex);
            const std::string skey = make_state_key(i, subject);
            auto it = _buckets.find(skey);
            if (it != _buckets.end()) {
                int rem = static_cast<int>(it->second.tokens);
                if (rem < 0) rem = 0;
                remaining_str = std::to_string(rem);
            }
        }

        auto result = garcon::module::result::pass();
        result.add_response_header("X-RateLimit-Limit", std::to_string(r.limit));
        result.add_response_header("X-RateLimit-Remaining", remaining_str);
        // X-RateLimit-Reset is approximate; we don't expose exact time easily here.
        result.add_response_header("X-RateLimit-Reset", std::to_string(r.window_seconds));
        return result;
    }

    // No rule matched this request
    return garcon::module::result::pass();
}

} // namespace garcon::modules
