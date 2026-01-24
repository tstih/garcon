#pragma once

#include "http_request.h"
#include "http_response.h"

#include <filesystem>

namespace app {

class static_files
{
public:
    explicit static_files(std::filesystem::path root);

    http::response handle(const http::request& req) const;

private:
    std::filesystem::path _root;
};

} /* namespace app */
