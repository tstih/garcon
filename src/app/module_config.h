// Module configuration loading for Garcon.
//
// This file defines the small loader that reads configured module entries from
// a Unix-style modules.d directory.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace app {

struct configured_module
{
    std::filesystem::path library_file;
    std::filesystem::path config_directory;
    std::string config_text;
};

std::vector<configured_module> load_configured_modules(
    const std::filesystem::path& directory);

} // namespace app
