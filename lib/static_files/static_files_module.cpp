// Static-files shared module implementation.
//
// This file contains the actual C++ module logic for the shared static-files
// plugin. The C ABI export stays in module.cpp.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "static_files_module.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace garcon::modules {

namespace fs = std::filesystem;

static_files_module::static_files_module(const garcon::module::host_context& host,
                                         std::string_view config_text)
    : _files(garcon::module::key_value_config(config_text).resolve_path(
          "root",
          host,
          host.default_document_root().empty()
              ? fs::path("www")
              : fs::path(std::string(host.default_document_root()))))
{
}

http::response static_files_module::handle_http(const http::request& request) const
{
    return _files.handle(request);
}

} // namespace garcon::modules
