// Header-guard shared module implementation.
//
// This file contains the C++ module logic for a small ordered header guard.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "header_guard_module.h"

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

int parse_status_code(std::string_view value)
{
    if (value.empty())
        throw std::runtime_error("header_guard: empty status code");

    int status = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9')
            throw std::runtime_error("header_guard: invalid status code");
        status = (status * 10) + (ch - '0');
    }

    if (status < 100 || status > 999)
        throw std::runtime_error("header_guard: status code out of range");

    return status;
}

std::string reason_phrase_for_status(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default:  return "Unauthorized";
    }
}

bool is_rule_line(std::string_view line)
{
    return line.substr(0, 5) == "rule=";
}

header_guard_module::pattern_kind classify_target_pattern(std::string& pattern)
{
    if (!pattern.empty() && pattern.back() == '*') {
        pattern.pop_back();
        return header_guard_module::pattern_kind::prefix;
    }

    return header_guard_module::pattern_kind::exact;
}

header_guard_module::guard_rule parse_rule_line(std::string_view line)
{
    const auto fields = split_fields(line.substr(5), '|');
    if (fields.size() != 5 && fields.size() != 6)
        throw std::runtime_error("header_guard: rule requires 5 or 6 fields");

    header_guard_module::guard_rule rule;
    rule.method_pattern = fields[0];
    rule.target_pattern = fields[1];
    rule.target_kind = classify_target_pattern(rule.target_pattern);
    rule.header_name = fields[2];

    if (rule.method_pattern.empty())
        throw std::runtime_error("header_guard: method pattern must not be empty");
    if (rule.target_pattern.empty())
        throw std::runtime_error("header_guard: target pattern must not be empty");
    if (rule.header_name.empty())
        throw std::runtime_error("header_guard: header name must not be empty");

    std::size_t status_index = 3;
    if (fields.size() == 6) {
        rule.expected_value = fields[3];
        status_index = 4;
    }

    rule.failure_status = parse_status_code(fields[status_index]);
    rule.failure_body = fields[status_index + 1];
    return rule;
}

std::vector<header_guard_module::guard_rule> parse_rules(std::string_view config_text)
{
    std::vector<header_guard_module::guard_rule> rules;

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

bool target_matches(header_guard_module::pattern_kind kind,
                    std::string_view pattern,
                    std::string_view actual)
{
    switch (kind) {
    case header_guard_module::pattern_kind::exact:
        return pattern == actual;
    case header_guard_module::pattern_kind::prefix:
        return actual.substr(0, pattern.size()) == pattern;
    }

    return false;
}

bool request_satisfies_rule(const header_guard_module::guard_rule& rule,
                            const http::request& request)
{
    const auto actual = request.header_value(rule.header_name);
    if (!actual)
        return false;

    if (!rule.expected_value)
        return true;

    return *actual == *rule.expected_value;
}

http::response failure_response(const header_guard_module::guard_rule& rule)
{
    return http::response::text(rule.failure_status,
                                reason_phrase_for_status(rule.failure_status),
                                rule.failure_body);
}

} // namespace

header_guard_module::header_guard_module(const garcon::module::host_context&,
                                         std::string_view config_text)
    : _rules(parse_rules(config_text))
{
}

garcon::module::result header_guard_module::handle(const http::request& request) const
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
