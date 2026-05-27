// Command-line parsing for Garcon.
//
// This parser turns process arguments into a validated app::server_config
// while keeping option handling separate from the process entry point.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "server_config.h"

#include <optional>
#include <string>
#include <string_view>

namespace app {

struct command_line_result
{
    bool show_help = false;
    std::optional<server_config> config;
};

class command_line_parser
{
public:
    command_line_parser(int argc, char** argv);

    command_line_result parse() const;

    std::string usage() const;

private:
    int _argc;
    char** _argv;
};

} // namespace app
