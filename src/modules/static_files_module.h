// Static-files request module.
//
// This module adapts the reusable static-files service to the ordered request
// pipeline. It is the first concrete example of a Garcon pipeline module.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/request_module.h"
#include "static_files.h"

#include <filesystem>

namespace app {

class static_files_module final : public request_module
{
public:
    explicit static_files_module(std::filesystem::path root);

    std::string_view name() const override;
    module_result handle(const http::request& request) const override;

private:
    static_files _files;
};

} // namespace app
