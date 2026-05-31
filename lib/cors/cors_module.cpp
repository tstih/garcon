// CORS shared module implementation.
//
// This file contains the C++ module logic for a small ordered CORS policy
// module.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "cors_module.h"

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

bool is_rule_line(std::string_view line)
{
    return line.substr(0, 5) == "rule=";
}

cors_module::pattern_kind classify_target_pattern(std::string& pattern)
{
    if (!pattern.empty() && pattern.back() == '*') {
        pattern.pop_back();
        return cors_module::pattern_kind::prefix;
    }

    return cors_module::pattern_kind::exact;
}

cors_module::origin_pattern parse_origin_pattern(std::string_view value)
{
    if (value.empty())
        throw std::runtime_error("cors: origin pattern must not be empty");

    cors_module::origin_pattern pattern;
    pattern.allow_any = value == "*";
    pattern.value = std::string(value);
    return pattern;
}

std::vector<cors_module::origin_pattern> parse_origin_patterns(std::string_view value)
{
    std::vector<cors_module::origin_pattern> patterns;

    for (const auto& field : split_fields(value, ',')) {
        if (!field.empty())
            patterns.push_back(parse_origin_pattern(field));
    }

    if (patterns.empty())
        throw std::runtime_error("cors: rule requires at least one origin");

    return patterns;
}

std::vector<std::string> parse_allowed_methods(std::string_view value)
{
    std::vector<std::string> methods;

    for (const auto& field : split_fields(value, ',')) {
        if (!field.empty())
            methods.push_back(ascii_lowercase(field));
    }

    if (methods.empty())
        throw std::runtime_error("cors: rule requires at least one method");

    return methods;
}

cors_module::rule parse_rule_line(std::string_view line)
{
    const auto fields = split_fields(line.substr(5), '|');
    if (fields.size() != 4)
        throw std::runtime_error("cors: rule requires 4 fields");

    cors_module::rule rule;
    rule.target_pattern = fields[0];
    rule.target_kind = classify_target_pattern(rule.target_pattern);
    rule.allowed_origins = parse_origin_patterns(fields[1]);
    rule.allowed_methods = parse_allowed_methods(fields[2]);
    rule.allow_methods_header = fields[2];
    rule.allow_headers_header = fields[3];

    if (rule.target_pattern.empty())
        throw std::runtime_error("cors: target pattern must not be empty");

    return rule;
}

std::vector<cors_module::rule> parse_rules(std::string_view config_text)
{
    std::vector<cors_module::rule> rules;

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

bool target_matches(cors_module::pattern_kind kind,
                    std::string_view pattern,
                    std::string_view actual)
{
    switch (kind) {
    case cors_module::pattern_kind::exact:
        return pattern == actual;
    case cors_module::pattern_kind::prefix:
        return actual.substr(0, pattern.size()) == pattern;
    }

    return false;
}

bool is_preflight_request(const http::request& request)
{
    return request.method == "OPTIONS" &&
           request.header_value("Origin").has_value() &&
           request.header_value("Access-Control-Request-Method").has_value();
}

bool origin_matches(const cors_module::origin_pattern& pattern, std::string_view origin)
{
    return pattern.allow_any || pattern.value == origin;
}

bool origin_allowed(const cors_module::rule& rule, std::string_view origin)
{
    for (const auto& pattern : rule.allowed_origins) {
        if (origin_matches(pattern, origin))
            return true;
    }

    return false;
}

bool any_origin_allowed(const cors_module::rule& rule)
{
    for (const auto& pattern : rule.allowed_origins) {
        if (pattern.allow_any)
            return true;
    }

    return false;
}

bool method_allowed(const cors_module::rule& rule, std::string_view method)
{
    const auto normalized = ascii_lowercase(method);
    for (const auto& allowed_method : rule.allowed_methods) {
        if (allowed_method == normalized)
            return true;
    }

    return false;
}

std::string allowed_origin_value(const cors_module::rule& rule, std::string_view request_origin)
{
    return any_origin_allowed(rule) ? "*" : std::string(request_origin);
}

void add_cors_headers(http::response& response,
                      const cors_module::rule& rule,
                      std::string_view request_origin)
{
    response.set_header("Access-Control-Allow-Origin",
                        allowed_origin_value(rule, request_origin));
    if (!any_origin_allowed(rule))
        response.add_header("Vary", "Origin");
}

http::response make_preflight_response(const cors_module::rule& rule,
                                       std::string_view request_origin)
{
    http::response response;
    response.status = 204;
    response.reason = "No Content";
    response.content_length = 0;
    add_cors_headers(response, rule, request_origin);
    response.set_header("Access-Control-Allow-Methods", rule.allow_methods_header);
    if (!rule.allow_headers_header.empty())
        response.set_header("Access-Control-Allow-Headers", rule.allow_headers_header);
    return response;
}

http::response make_cors_error(int status,
                               std::string_view reason,
                               std::string_view body)
{
    return http::response::text(status, reason, body);
}

} // namespace

cors_module::cors_module(const garcon::module::host_context&,
                         std::string_view config_text)
    : _rules(parse_rules(config_text))
{
}

garcon::module::result cors_module::handle(const http::request& request) const
{
    const auto request_origin = request.header_value("Origin");
    if (!request_origin)
        return garcon::module::result::pass();

    for (const auto& rule : _rules) {
        if (!target_matches(rule.target_kind, rule.target_pattern, request.target))
            continue;

        if (!origin_allowed(rule, *request_origin)) {
            if (is_preflight_request(request)) {
                return garcon::module::result::respond(
                    make_cors_error(403, "Forbidden", "cors origin not allowed\n"));
            }

            return garcon::module::result::pass();
        }

        if (is_preflight_request(request)) {
            const auto requested_method =
                request.header_value("Access-Control-Request-Method").value_or("");

            if (!method_allowed(rule, requested_method)) {
                return garcon::module::result::respond(
                    make_cors_error(405,
                                    "Method Not Allowed",
                                    "cors method not allowed\n"));
            }

            return garcon::module::result::respond(
                make_preflight_response(rule, *request_origin));
        }

        if (!method_allowed(rule, request.method)) {
            return garcon::module::result::respond(
                make_cors_error(405,
                                "Method Not Allowed",
                                "cors method not allowed\n"));
        }

        auto result = garcon::module::result::pass();
        result.add_response_header("Access-Control-Allow-Origin",
                                   allowed_origin_value(rule, *request_origin));
        if (!any_origin_allowed(rule))
            result.add_response_header("Vary", "Origin");
        return result;
    }

    return garcon::module::result::pass();
}

} // namespace garcon::modules
