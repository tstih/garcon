// Implementation of the static-files request module.
//
// This file contains the thin pipeline adapter that turns the reusable static
// file serving service into a pipeline module.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "modules/static_files_module.h"

#include <utility>

namespace app {

static_files_module::static_files_module(std::filesystem::path root)
    : _files(std::move(root))
{
}

std::string_view static_files_module::name() const
{
    return "static-files";
}

module_result static_files_module::handle(const http::request& request) const
{
    return module_result::respond(_files.handle(request));
}

} // namespace app
