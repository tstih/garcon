// Implementation of the dynamic request-module adapter.
//
// This file loads Garcon module shared libraries through the public ABI and
// maps them onto the in-process request_module interface used by the request
// pipeline.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/dynamic_module.h"

#include "garcon/module_abi.h"

#include <dlfcn.h>

#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace app {

namespace {

garcon_string_view make_string_view(std::string_view value)
{
    return garcon_string_view{
        .data = value.data(),
        .size = value.size(),
    };
}

std::string copy_string(garcon_string_view value)
{
    if (!value.data || value.size == 0)
        return {};

    return std::string(value.data, value.size);
}

std::vector<http::header_field> copy_headers(const garcon_response_view& response_view)
{
    std::vector<http::header_field> headers;
    headers.reserve(response_view.header_count);

    for (std::size_t i = 0; i < response_view.header_count; ++i) {
        const auto& header_view = response_view.headers[i];
        headers.push_back(http::header_field{
            .name = copy_string(header_view.name),
            .value = copy_string(header_view.value),
        });
    }

    return headers;
}

std::string require_dlerror(std::string_view operation)
{
    const char* detail = ::dlerror();
    if (!detail)
        return std::string(operation);

    return std::string(operation) + ": " + detail;
}

http::response to_http_response(const garcon_response_view& response_view)
{
    http::response response;
    response.status = (response_view.status == 0) ? 200 : response_view.status;
    response.reason = copy_string(response_view.reason);
    if (response.reason.empty())
        response.reason = "OK";

    response.content_type = copy_string(response_view.content_type);
    response.headers = copy_headers(response_view);
    response.body = copy_string(response_view.body);
    if (response_view.has_content_length)
        response.content_length = response_view.content_length;

    return response;
}

module_result with_response_headers(module_result result,
                                    const garcon_response_view& response_view)
{
    for (const auto& header : copy_headers(response_view))
        result.add_response_header(header.name, header.value);

    return result;
}

} // namespace

dynamic_module::dynamic_module(std::filesystem::path library_file,
                               std::filesystem::path config_directory,
                               std::string config_text,
                               const server_config& config)
    : _config_text(std::move(config_text)),
      _default_document_root(config.www_root.string()),
      _config_directory(config_directory.string())
{
    try {
        _library_handle = ::dlopen(library_file.string().c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!_library_handle)
            throw std::runtime_error(require_dlerror("dlopen() failed"));

        ::dlerror();
        const auto entrypoint =
            reinterpret_cast<garcon_get_module_descriptor_fn>(
                ::dlsym(_library_handle, GARCON_MODULE_ENTRYPOINT));
        if (!entrypoint)
            throw std::runtime_error(require_dlerror("dlsym() failed"));

        _descriptor = entrypoint();
        if (!_descriptor)
            throw std::runtime_error("module entrypoint returned a null descriptor");

        if (_descriptor->abi_version != GARCON_MODULE_ABI_VERSION) {
            throw std::runtime_error("module ABI mismatch for: " +
                                     library_file.string());
        }

        _name = (_descriptor->name && _descriptor->name[0] != '\0')
            ? _descriptor->name
            : library_file.filename().string();

        _host_api = garcon_host_api{
            .abi_version = GARCON_MODULE_ABI_VERSION,
            .default_document_root = make_string_view(_default_document_root),
            .config_directory = make_string_view(_config_directory),
        };

        const char* error_message = nullptr;
        _instance = _descriptor->create(&_host_api,
                                        _config_text.c_str(),
                                        &error_message);
        if (!_instance) {
            const auto detail = error_message ? error_message
                                              : "module create() failed";
            throw std::runtime_error("failed to create module '" + _name +
                                     "': " + detail);
        }
    } catch (...) {
        if (_library_handle) {
            ::dlclose(_library_handle);
            _library_handle = nullptr;
        }
        throw;
    }
}

dynamic_module::~dynamic_module()
{
    if (_descriptor && _descriptor->destroy && _instance)
        _descriptor->destroy(_instance);

    if (_library_handle)
        ::dlclose(_library_handle);
}

std::string_view dynamic_module::name() const
{
    return _name;
}

module_result dynamic_module::handle(const http::request& request) const
{
    if (!_descriptor || !_descriptor->handle || !_instance)
        return module_result::error();

    std::vector<garcon_header_view> header_views;
    header_views.reserve(request.headers.size());
    for (const auto& header : request.headers) {
        header_views.push_back(garcon_header_view{
            .name = make_string_view(header.name),
            .value = make_string_view(header.value),
        });
    }

    garcon_exchange exchange{
        .request =
            {
                .method = make_string_view(request.method),
                .target = make_string_view(request.target),
                .headers = header_views.data(),
                .header_count = header_views.size(),
            },
        .response = {},
    };

    const char* error_message = nullptr;
    garcon_module_result result{.outcome = GARCON_MODULE_ERROR};

    try {
        result = _descriptor->handle(_instance, &exchange, &error_message);
    } catch (...) {
        return module_result::error();
    }

    switch (result.outcome) {
    case GARCON_MODULE_PASS:
        return with_response_headers(module_result::pass(), exchange.response);
    case GARCON_MODULE_RESPOND:
        return module_result::respond(to_http_response(exchange.response));
    case GARCON_MODULE_UPGRADE:
        return with_response_headers(module_result::upgrade(), exchange.response);
    case GARCON_MODULE_ERROR:
        if (exchange.response.status != 0)
            return module_result::respond(to_http_response(exchange.response));
        return with_response_headers(module_result::error(), exchange.response);
    }

    return module_result::error();
}

} // namespace app
