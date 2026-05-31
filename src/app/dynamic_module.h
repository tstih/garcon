// Dynamic request-module adapter.
//
// This file defines the adapter that loads a Garcon module shared library and
// exposes it through the in-process request_module interface.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/request_module.h"
#include "garcon/module_abi.h"
#include "server_config.h"

#include <filesystem>
#include <string>

struct garcon_module_descriptor;
struct garcon_module_instance;

namespace app {

class dynamic_module final : public request_module
{
public:
    dynamic_module(std::filesystem::path library_file,
                   std::filesystem::path config_directory,
                   std::string config_text,
                   const server_config& config);

    ~dynamic_module() override;

    std::string_view name() const override;
    module_result handle(const http::request& request) const override;

private:
    void* _library_handle = nullptr;
    const garcon_module_descriptor* _descriptor = nullptr;
    garcon_module_instance* _instance = nullptr;
    std::string _name;
    std::string _config_text;
    std::string _default_document_root;
    std::string _config_directory;
    garcon_host_api _host_api{};
};

} // namespace app
