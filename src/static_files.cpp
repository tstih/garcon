#include "static_files.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace app {

static std::string read_file_bytes(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

static std::string_view guess_content_type(const fs::path& p)
{
    const auto ext = p.extension().string();
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js")  return "text/javascript; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

static std::optional<fs::path> map_to_path(std::string_view target)
{
    const auto q = target.find('?');
    if (q != std::string_view::npos)
        target = target.substr(0, q);

    if (target.empty() || target[0] != '/')
        return std::nullopt;

    if (target == "/")
        target = "/index.html";

    if (target.find("..") != std::string_view::npos)
        return std::nullopt;

    return fs::path(std::string(target.substr(1)));
}

static bool file_exists_regular(const fs::path& p)
{
    return fs::exists(p) && fs::is_regular_file(p);
}

static http::response serve_file(const fs::path& full, bool include_body)
{
    if (!file_exists_regular(full))
        return http::response::text(404, "Not Found", "not found\n");

    http::response resp;
    resp.status = 200;
    resp.reason = "OK";
    resp.content_type = std::string(guess_content_type(full));

    if (include_body) {
        resp.body = read_file_bytes(full);
        if (resp.body.empty())
            return http::response::text(404, "Not Found", "not found\n");
    }

    return resp;
}

static_files::static_files(fs::path root) : _root(std::move(root)) {}

http::response static_files::handle(const http::request& req) const
{
    const bool is_get  = (req.method == "GET");
    const bool is_head = (req.method == "HEAD");

    if (!is_get && !is_head)
        return http::response::text(405, "Method Not Allowed", "method not allowed\n");

    const auto rel = map_to_path(req.target);
    if (!rel)
        return http::response::text(404, "Not Found", "not found\n");

    return serve_file(_root / *rel, is_get);
}

} /* namespace app */
