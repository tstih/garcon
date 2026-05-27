// Implementation of the static file request handler.
//
// This file implements the app::static_files class and supporting helpers for
// serving files from a fixed directory on disk. It includes logic for safely
// mapping request targets to filesystem paths, determining content types, and
// constructing appropriate HTTP responses.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "static_files.h"

#include <fstream>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace app {

namespace {

constexpr std::uintmax_t max_served_file_size = 8U * 1024U * 1024U;

std::optional<std::string> read_file_bytes(const fs::path& p,
                                           std::uintmax_t size)
{
    if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
        return std::nullopt;

    std::ifstream f(p, std::ios::binary);
    if (!f)
        return std::nullopt;

    std::string body(static_cast<std::size_t>(size), '\0');
    if (!body.empty()) {
        f.read(body.data(), static_cast<std::streamsize>(body.size()));
        if (f.gcount() != static_cast<std::streamsize>(body.size()))
            return std::nullopt;
    }

    return body;
}

std::string_view guess_content_type(const fs::path& p)
{
    const auto ext = p.extension().string();
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")                  return "text/css; charset=utf-8";
    if (ext == ".js")                   return "text/javascript; charset=utf-8";
    if (ext == ".txt")                  return "text/plain; charset=utf-8";
    if (ext == ".png")                  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")                  return "image/gif";
    if (ext == ".svg")                  return "image/svg+xml";
    return "application/octet-stream";
}

std::optional<fs::path> map_to_relative_path(std::string_view target)
{
    const auto q = target.find('?');
    if (q != std::string_view::npos)
        target = target.substr(0, q);

    if (target.empty() || target[0] != '/')
        return std::nullopt;

    if (target == "/")
        target = "/index.html";

    fs::path relative;
    bool has_component = false;

    std::size_t pos = 1;
    while (pos <= target.size()) {
        auto next = target.find('/', pos);
        if (next == std::string_view::npos)
            next = target.size();

        const auto segment = target.substr(pos, next - pos);
        if (!segment.empty()) {
            if (segment == "." || segment == ".." ||
                segment.find('\0') != std::string_view::npos) {
                return std::nullopt;
            }

            relative /= std::string(segment);
            has_component = true;
        }

        pos = next + 1;
    }

    if (!has_component)
        return std::nullopt;

    return relative;
}

fs::path canonicalize_path(const fs::path& p)
{
    std::error_code ec;
    const auto absolute = fs::absolute(p, ec);
    if (ec)
        return {};

    const auto canonical = fs::weakly_canonical(absolute, ec);
    if (ec)
        return {};

    return canonical;
}

bool is_within_root(const fs::path& root, const fs::path& candidate)
{
    auto root_it = root.begin();
    auto candidate_it = candidate.begin();

    for (; root_it != root.end() && candidate_it != candidate.end();
         ++root_it, ++candidate_it) {
        if (*root_it != *candidate_it)
            return false;
    }

    return root_it == root.end();
}

std::optional<fs::path> resolve_under_root(const fs::path& root,
                                           const fs::path& relative)
{
    if (root.empty())
        return std::nullopt;

    std::error_code ec;
    const auto resolved = fs::weakly_canonical(root / relative, ec);
    if (ec || !is_within_root(root, resolved))
        return std::nullopt;

    if (!fs::is_regular_file(resolved, ec) || ec)
        return std::nullopt;

    return resolved;
}

http::response serve_file(const fs::path& root,
                          std::string_view target,
                          bool include_body)
{
    const auto relative = map_to_relative_path(target);
    if (!relative)
        return http::response::text(404, "Not Found", "not found\n");

    const auto resolved = resolve_under_root(root, *relative);
    if (!resolved)
        return http::response::text(404, "Not Found", "not found\n");

    std::error_code ec;
    const auto file_size = fs::file_size(*resolved, ec);
    if (ec)
        return http::response::text(404, "Not Found", "not found\n");

    if (file_size > max_served_file_size)
        return http::response::text(413,
                                    "Payload Too Large",
                                    "file too large\n");

    http::response resp;
    resp.status = 200;
    resp.reason = "OK";
    resp.content_type = std::string(guess_content_type(*resolved));
    resp.content_length = static_cast<std::size_t>(file_size);

    if (include_body) {
        const auto body = read_file_bytes(*resolved, file_size);
        if (!body)
            return http::response::text(404, "Not Found", "not found\n");

        resp.body = *body;
    }

    return resp;
}

} // namespace

static_files::static_files(fs::path root)
    : _root(canonicalize_path(std::move(root)))
{
}

http::response static_files::handle(const http::request& req) const
{
    const bool is_get  = (req.method == "GET");
    const bool is_head = (req.method == "HEAD");

    if (!is_get && !is_head)
        return http::response::text(405,
                                    "Method Not Allowed",
                                    "method not allowed\n");

    return serve_file(_root, req.target, is_get);
}

} // namespace app
