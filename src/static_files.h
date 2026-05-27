// Static file request handler.
//
// This file defines the app::static_files class, which implements a simple
// HTTP request handler for serving files from a fixed directory on disk.
// It maps request targets to filesystem paths in a safe manner and produces
// appropriate HTTP responses for supported requests.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/request_handler.h"

#include <filesystem>

namespace app {

class static_files final : public request_handler
{
public:
    // Creates a static file handler rooted at the given filesystem path.
    explicit static_files(std::filesystem::path root);

    // Handles an HTTP request and produces a corresponding HTTP response.
    http::response handle(const http::request& req) const override;

private:
    std::filesystem::path _root;
};

} // namespace app
