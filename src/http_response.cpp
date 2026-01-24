#include "http_response.h"

#include <sstream>

namespace http {

std::string response::serialize() const
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
    oss << "Connection: close\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    if (!content_type.empty())
        oss << "Content-Type: " << content_type << "\r\n";
    oss << "\r\n";
    oss.write(body.data(), static_cast<std::streamsize>(body.size()));
    return oss.str();
}

response response::text(int s, std::string_view r, std::string_view msg)
{
    response resp;
    resp.status = s;
    resp.reason = r;
    resp.content_type = "text/plain; charset=utf-8";
    resp.body = std::string(msg);
    return resp;
}

} /* namespace http */
