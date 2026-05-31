// Host-guard shared module implementation.
//
// This file contains the C++ module logic for a small ordered host guard.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "host_guard_module.h"

#include <cctype>
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

int parse_status_code(std::string_view value)
{
    if (value.empty())
        throw std::runtime_error("host_guard: empty status code");

    int status = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9')
            throw std::runtime_error("host_guard: invalid status code");
        status = (status * 10) + (ch - '0');
    }

    if (status < 100 || status > 999)
        throw std::runtime_error("host_guard: status code out of range");

    return status;
}

std::string reason_phrase_for_status(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 421: return "Misdirected Request";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default:  return "Misdirected Request";
    }
}

bool is_rule_line(std::string_view line)
{
    return line.substr(0, 5) == "rule=";
}

host_guard_module::pattern_kind classify_target_pattern(std::string& pattern)
{
    if (!pattern.empty() && pattern.back() == '*') {
        pattern.pop_back();
        return host_guard_module::pattern_kind::prefix;
    }

    return host_guard_module::pattern_kind::exact;
}

host_guard_module::host_pattern parse_host_pattern(std::string_view value)
{
    if (value.empty())
        throw std::runtime_error("host_guard: host pattern must not be empty");

    host_guard_module::host_pattern pattern;
    pattern.value = ascii_lowercase(value);

    if (pattern.value.size() > 2 && pattern.value[0] == '*' && pattern.value[1] == '.') {
        pattern.kind = host_guard_module::host_pattern_kind::suffix;
        pattern.value.erase(0, 1);
    }

    return pattern;
}

std::vector<host_guard_module::host_pattern> parse_host_patterns(std::string_view value)
{
    std::vector<host_guard_module::host_pattern> patterns;

    for (const auto& field : split_fields(value, ',')) {
        if (!field.empty())
            patterns.push_back(parse_host_pattern(field));
    }

    if (patterns.empty())
        throw std::runtime_error("host_guard: rule requires at least one host pattern");

    return patterns;
}

host_guard_module::guard_rule parse_rule_line(std::string_view line)
{
    const auto fields = split_fields(line.substr(5), '|');
    if (fields.size() != 5)
        throw std::runtime_error("host_guard: rule requires 5 fields");

    host_guard_module::guard_rule rule;
    rule.method_pattern = fields[0];
    rule.target_pattern = fields[1];
    rule.target_kind = classify_target_pattern(rule.target_pattern);
    rule.allowed_hosts = parse_host_patterns(fields[2]);

    if (rule.method_pattern.empty())
        throw std::runtime_error("host_guard: method pattern must not be empty");
    if (rule.target_pattern.empty())
        throw std::runtime_error("host_guard: target pattern must not be empty");

    rule.failure_status = parse_status_code(fields[3]);
    rule.failure_body = fields[4];
    return rule;
}

std::vector<host_guard_module::guard_rule> parse_rules(std::string_view config_text)
{
    std::vector<host_guard_module::guard_rule> rules;

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

bool target_matches(host_guard_module::pattern_kind kind,
                    std::string_view pattern,
                    std::string_view actual)
{
    switch (kind) {
    case host_guard_module::pattern_kind::exact:
        return pattern == actual;
    case host_guard_module::pattern_kind::prefix:
        return actual.substr(0, pattern.size()) == pattern;
    }

    return false;
}

std::string normalize_host_header(std::string_view value)
{
    auto host = trim(value);
    if (host.empty())
        return {};

    if (host.front() == '[') {
        const auto close = host.find(']');
        if (close == std::string::npos)
            return ascii_lowercase(host);
        return ascii_lowercase(std::string_view(host).substr(0, close + 1));
    }

    const auto colon = host.rfind(':');
    if (colon != std::string::npos && host.find(':') == colon) {
        bool numeric_port = (colon + 1) < host.size();
        for (std::size_t i = colon + 1; i < host.size(); ++i) {
            if (host[i] < '0' || host[i] > '9') {
                numeric_port = false;
                break;
            }
        }

        if (numeric_port)
            host.resize(colon);
    }

    return ascii_lowercase(host);
}

bool host_matches(const host_guard_module::host_pattern& pattern,
                  std::string_view actual_host)
{
    switch (pattern.kind) {
    case host_guard_module::host_pattern_kind::exact:
        return ascii_iequals(pattern.value, actual_host);
    case host_guard_module::host_pattern_kind::suffix:
        return actual_host.size() > pattern.value.size() &&
               actual_host.substr(actual_host.size() - pattern.value.size()) == pattern.value;
    }

    return false;
}

bool request_satisfies_rule(const host_guard_module::guard_rule& rule,
                            const http::request& request)
{
    const auto host = request.header_value("Host");
    if (!host)
        return false;

    const auto normalized_host = normalize_host_header(*host);
    if (normalized_host.empty())
        return false;

    for (const auto& pattern : rule.allowed_hosts) {
        if (host_matches(pattern, normalized_host))
            return true;
    }

    return false;
}

http::response failure_response(const host_guard_module::guard_rule& rule)
{
    return http::response::text(rule.failure_status,
                                reason_phrase_for_status(rule.failure_status),
                                rule.failure_body);
}

} // namespace

host_guard_module::host_guard_module(const garcon::module::host_context&,
                                     std::string_view config_text)
    : _rules(parse_rules(config_text))
{
}

garcon::module::result host_guard_module::handle(const http::request& request) const
{
    for (const auto& rule : _rules) {
        if (!method_matches(rule.method_pattern, request.method))
            continue;
        if (!target_matches(rule.target_kind, rule.target_pattern, request.target))
            continue;
        if (request_satisfies_rule(rule, request))
            continue;

        return garcon::module::result::respond(failure_response(rule));
    }

    return garcon::module::result::pass();
}

} // namespace garcon::modules
