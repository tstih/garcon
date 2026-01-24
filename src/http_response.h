#pragma once

#include <string>
#include <string_view>

namespace http {

struct response
{
    int status = 200;
    std::string_view reason = "OK";
    std::string content_type;
    std::string body;

    std::string serialize() const;

    static response text(int status, std::string_view reason, std::string_view msg);
};

} /* namespace http */
