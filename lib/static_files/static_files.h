// Static file serving service.
//
// This file defines the app::static_files class, which serves files from a
// fixed directory on disk. It maps request targets to filesystem paths in a
// safe manner and produces appropriate HTTP responses for supported requests.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "http/request.h"
#include "http/response.h"

#include <filesystem>

namespace app {

class static_files
{
public:
    // Creates a static file serving service rooted at the given filesystem path.
    explicit static_files(std::filesystem::path root);

    [[nodiscard]] http::response handle(const http::request& req) const;

private:
    std::filesystem::path _root;
};

} // namespace app
