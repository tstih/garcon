#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace http {

struct request
{
    std::string method;
    std::string target;

    static std::optional<request> parse(std::string_view header_block);
};

} /* namespace http */
