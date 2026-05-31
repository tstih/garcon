// Route-table shared module implementation.
//
// This file contains the C++ module logic for a small ordered route table.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "route_table_module.h"

#include <algorithm>
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
        throw std::runtime_error("route_table: empty status code");

    int status = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9')
            throw std::runtime_error("route_table: invalid status code");
        status = (status * 10) + (ch - '0');
    }

    if (status < 100 || status > 999)
        throw std::runtime_error("route_table: status code out of range");

    return status;
}

std::string reason_phrase_for_status(int status)
{
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default:  return "OK";
    }
}

bool is_route_line(std::string_view line)
{
    return line.substr(0, 6) == "route=";
}

route_table_module::pattern_kind classify_target_pattern(std::string& pattern)
{
    if (!pattern.empty() && pattern.back() == '*') {
        pattern.pop_back();
        return route_table_module::pattern_kind::prefix;
    }

    return route_table_module::pattern_kind::exact;
}

route_table_module::route_entry parse_route_line(std::string_view line)
{
    const auto fields = split_fields(line.substr(6), '|');
    if (fields.size() < 3)
        throw std::runtime_error("route_table: route requires at least 3 fields");

    route_table_module::route_entry entry;
    entry.method_pattern = fields[0];
    entry.target_pattern = fields[1];
    entry.target_kind = classify_target_pattern(entry.target_pattern);

    if (entry.method_pattern.empty())
        throw std::runtime_error("route_table: route method pattern must not be empty");
    if (entry.target_pattern.empty())
        throw std::runtime_error("route_table: route target pattern must not be empty");

    const auto action = fields[2];
    if (action == "pass") {
        entry.outcome = garcon::module::outcome::pass;
        return entry;
    }

    if (action != "respond")
        throw std::runtime_error("route_table: unsupported route action");

    if (fields.size() != 6)
        throw std::runtime_error("route_table: respond route requires 6 fields");

    http::response response;
    response.status = parse_status_code(fields[3]);
    response.reason = reason_phrase_for_status(response.status);
    response.content_type = fields[4];
    response.body = fields[5];
    entry.outcome = garcon::module::outcome::respond;
    entry.response = std::move(response);
    return entry;
}

std::vector<route_table_module::route_entry> parse_routes(std::string_view config_text)
{
    std::vector<route_table_module::route_entry> routes;

    std::size_t line_begin = 0;
    while (line_begin <= config_text.size()) {
        auto line_end = config_text.find('\n', line_begin);
        if (line_end == std::string_view::npos)
            line_end = config_text.size();

        const auto raw_line = config_text.substr(line_begin, line_end - line_begin);
        const auto line = trim(raw_line);
        if (!line.empty() && line.front() != '#' && is_route_line(line))
            routes.push_back(parse_route_line(line));

        if (line_end == config_text.size())
            break;
        line_begin = line_end + 1;
    }

    return routes;
}

bool method_matches(std::string_view pattern, std::string_view actual)
{
    return pattern == "*" || pattern == actual;
}

bool target_matches(route_table_module::pattern_kind kind,
                    std::string_view pattern,
                    std::string_view actual)
{
    switch (kind) {
    case route_table_module::pattern_kind::exact:
        return pattern == actual;
    case route_table_module::pattern_kind::prefix:
        return actual.substr(0, pattern.size()) == pattern;
    }

    return false;
}

} // namespace

route_table_module::route_table_module(const garcon::module::host_context&,
                                       std::string_view config_text)
    : _routes(parse_routes(config_text))
{
}

garcon::module::result route_table_module::handle(const http::request& request) const
{
    for (const auto& route : _routes) {
        if (!method_matches(route.method_pattern, request.method))
            continue;
        if (!target_matches(route.target_kind, route.target_pattern, request.target))
            continue;

        if (route.outcome == garcon::module::outcome::pass)
            return garcon::module::result::pass();

        if (route.response)
            return garcon::module::result::respond(*route.response);

        return garcon::module::result::error();
    }

    return garcon::module::result::pass();
}

} // namespace garcon::modules
