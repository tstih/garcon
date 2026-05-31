// Static-files shared module implementation.
//
// This file defines the C++ module implementation used by the shared
// static-files plugin. The ABI export is kept separate in module.cpp.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "garcon/module_cpp.h"
#include "static_files.h"

#include <string_view>

namespace garcon::modules {

class static_files_module final : public garcon::module::http_terminal_module
{
public:
    static_files_module(const garcon::module::host_context& host,
                        std::string_view config_text);

private:
    http::response handle_http(const http::request& request) const override;

    app::static_files _files;
};

} // namespace garcon::modules
