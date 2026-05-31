// Implementation of module configuration loading.
//
// This file reads configured modules from a Unix-style modules.d directory.
// Each .conf file contributes one module entry, and files are processed in
// lexical order.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/module_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

namespace app {

namespace {

std::string trim(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string read_text_file(const fs::path& file)
{
    std::ifstream input(file);
    if (!input)
        throw std::runtime_error("failed to open module config file: " + file.string());

    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

fs::path parse_library_path(const fs::path& file, std::string_view config_text)
{
    std::size_t line_begin = 0;
    while (line_begin <= config_text.size()) {
        auto line_end = config_text.find('\n', line_begin);
        if (line_end == std::string_view::npos)
            line_end = config_text.size();

        const auto raw_line = config_text.substr(line_begin, line_end - line_begin);
        const auto line = trim(raw_line);

        if (!line.empty() && line.front() != '#') {
            const auto equals = line.find('=');
            if (equals != std::string::npos) {
                const auto key = trim(std::string_view(line).substr(0, equals));
                if (key == "path") {
                    const auto value =
                        trim(std::string_view(line).substr(equals + 1));
                    if (value.empty()) {
                        throw std::runtime_error(
                            "empty path= entry in module config file: " +
                            file.string());
                    }

                    fs::path path(value);
                    if (path.is_relative())
                        path = file.parent_path() / path;

                    std::error_code ec;
                    const auto absolute = fs::absolute(path, ec);
                    if (ec)
                        return path;

                    return absolute;
                }
            }
        }

        if (line_end == config_text.size())
            break;
        line_begin = line_end + 1;
    }

    throw std::runtime_error("missing path= entry in module config file: " +
                             file.string());
}

} // namespace

std::expected<std::vector<configured_module>, std::string> load_configured_modules(
    const fs::path& directory)
{
    std::error_code ec;
    if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec)) {
        return std::unexpected("module config directory does not exist: " +
                               directory.string());
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".conf")
            continue;
        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());

    std::vector<configured_module> modules;
    modules.reserve(files.size());

    for (const auto& file : files) {
        try {
            const auto config_text = read_text_file(file);
            modules.push_back(configured_module{
                .library_file = parse_library_path(file, config_text),
                .config_directory = file.parent_path(),
                .config_text = config_text,
            });
        } catch (const std::exception& e) {
            return std::unexpected(e.what());
        }
    }

    if (modules.empty()) {
        return std::unexpected("no module config files found in: " +
                               directory.string());
    }

    return modules;
}

} // namespace app
