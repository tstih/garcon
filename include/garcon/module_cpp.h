#pragma once

#include "garcon/module_abi.h"
#include "http/request.h"
#include "http/response.h"

#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace garcon::module {

enum class outcome
{
    pass = GARCON_MODULE_PASS,
    respond = GARCON_MODULE_RESPOND,
    upgrade = GARCON_MODULE_UPGRADE,
    error = GARCON_MODULE_ERROR,
};

struct result
{
    outcome next = outcome::pass;
    std::optional<http::response> response_value;
    std::vector<http::header_field> response_headers;

    static result pass()
    {
        return result{.next = outcome::pass};
    }

    static result respond(http::response response_value)
    {
        return result{
            .next = outcome::respond,
            .response_value = std::move(response_value),
        };
    }

    static result upgrade()
    {
        return result{.next = outcome::upgrade};
    }

    static result error()
    {
        return result{.next = outcome::error};
    }

    result& add_response_header(std::string_view name, std::string_view value)
    {
        response_headers.push_back(http::header_field{
            .name = std::string(name),
            .value = std::string(value),
        });
        return *this;
    }
};

class host_context
{
public:
    explicit host_context(const garcon_host_api& host) : _host(host) {}

    std::string_view default_document_root() const
    {
        return to_string_view(_host.default_document_root);
    }

    std::string_view config_directory() const
    {
        return to_string_view(_host.config_directory);
    }

private:
    const garcon_host_api& _host;

    static std::string_view to_string_view(garcon_string_view value)
    {
        if (!value.data || value.size == 0)
            return {};

        return std::string_view(value.data, value.size);
    }
};

class key_value_config
{
public:
    explicit key_value_config(std::string_view text) : _text(text) {}

    std::optional<std::string_view> get(std::string_view key) const
    {
        std::size_t line_begin = 0;
        while (line_begin <= _text.size()) {
            auto line_end = _text.find('\n', line_begin);
            if (line_end == std::string_view::npos)
                line_end = _text.size();

            const auto line =
                trim(std::string_view(_text).substr(line_begin, line_end - line_begin));

            if (!line.empty() && line.front() != '#') {
                const auto equals = line.find('=');
                if (equals != std::string_view::npos) {
                    const auto found_key = trim(line.substr(0, equals));
                    if (found_key == key)
                        return trim(line.substr(equals + 1));
                }
            }

            if (line_end == _text.size())
                break;
            line_begin = line_end + 1;
        }

        return std::nullopt;
    }

    std::filesystem::path resolve_path(std::string_view key,
                                       const host_context& host,
                                       std::filesystem::path fallback) const
    {
        const auto value = get(key);
        if (!value || value->empty())
            return fallback;

        std::filesystem::path path{std::string(*value)};
        if (path.is_absolute())
            return path;

        const auto config_directory = host.config_directory();
        if (!config_directory.empty())
            return std::filesystem::path(std::string(config_directory)) / path;

        return path;
    }

private:
    std::string _text;

    static std::string_view trim(std::string_view value)
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

        return value.substr(begin, end - begin);
    }
};

class module
{
public:
    virtual ~module() = default;

    // The host may invoke handle() concurrently on the same module instance.
    virtual result handle(const http::request& request) const = 0;
};

class http_terminal_module : public module
{
public:
    result handle(const http::request& request) const final
    {
        return result::respond(handle_http(request));
    }

protected:
    virtual http::response handle_http(const http::request& request) const = 0;
};

namespace detail {

inline garcon_string_view to_abi_view(std::string_view value)
{
    return garcon_string_view{
        .data = value.data(),
        .size = value.size(),
    };
}

inline http::request to_http_request(const garcon_request_view& request_view)
{
    http::request request;
    request.method.assign(request_view.method.data ? request_view.method.data : "",
                          request_view.method.size);
    request.target.assign(request_view.target.data ? request_view.target.data : "",
                          request_view.target.size);

    request.headers.reserve(request_view.header_count);
    for (std::size_t i = 0; i < request_view.header_count; ++i) {
        const auto& header_view = request_view.headers[i];

        http::header_field header;
        header.name.assign(header_view.name.data ? header_view.name.data : "",
                           header_view.name.size);
        header.value.assign(header_view.value.data ? header_view.value.data : "",
                            header_view.value.size);
        request.headers.push_back(std::move(header));
    }

    return request;
}

inline void set_error_message(const char** error_message, std::string message)
{
    thread_local std::string stored_message;
    stored_message = std::move(message);

    if (error_message)
        *error_message = stored_message.c_str();
}

inline void publish_response_headers(const std::vector<http::header_field>& headers,
                                     garcon_response_view& response_view)
{
    struct header_storage
    {
        std::vector<std::string> names;
        std::vector<std::string> values;
        std::vector<garcon_header_view> views;
    };

    thread_local header_storage storage;
    storage.names.clear();
    storage.values.clear();
    storage.views.clear();

    storage.names.reserve(headers.size());
    storage.values.reserve(headers.size());
    storage.views.reserve(headers.size());

    for (const auto& header : headers) {
        storage.names.push_back(header.name);
        storage.values.push_back(header.value);
    }

    for (std::size_t i = 0; i < storage.names.size(); ++i) {
        storage.views.push_back(garcon_header_view{
            .name = to_abi_view(storage.names[i]),
            .value = to_abi_view(storage.values[i]),
        });
    }

    response_view.headers = storage.views.data();
    response_view.header_count = storage.views.size();
}

inline void publish_response(const http::response& response_value,
                             garcon_exchange& exchange_value)
{
    struct response_storage
    {
        std::string reason;
        std::string content_type;
        std::string body;
    };

    thread_local response_storage storage;
    storage.reason = response_value.reason;
    storage.content_type = response_value.content_type;
    storage.body = response_value.body;

    exchange_value.response.status = response_value.status;
    exchange_value.response.reason = to_abi_view(storage.reason);
    exchange_value.response.content_type = to_abi_view(storage.content_type);
    publish_response_headers(response_value.headers, exchange_value.response);
    exchange_value.response.body = to_abi_view(storage.body);
    exchange_value.response.has_content_length =
        response_value.content_length.has_value() ? 1 : 0;
    exchange_value.response.content_length =
        response_value.content_length.value_or(storage.body.size());
}

template <typename Module>
struct exported_module_instance
{
    explicit exported_module_instance(std::unique_ptr<Module> implementation)
        : implementation(std::move(implementation))
    {
    }

    std::unique_ptr<Module> implementation;
};

template <typename Module>
garcon_module_instance* create(const garcon_host_api* host,
                               const char* config,
                               const char** error_message)
{
    static_assert(std::is_base_of_v<module, Module>,
                  "exported module type must derive from garcon::module::module");

    try {
        if (!host)
            throw std::runtime_error("module host context must not be null");

        auto instance =
            std::make_unique<exported_module_instance<Module>>(std::make_unique<Module>(
                host_context(*host),
                config ? std::string_view(config) : std::string_view{}));

        return reinterpret_cast<garcon_module_instance*>(instance.release());
    } catch (const std::exception& e) {
        set_error_message(error_message, e.what());
        return nullptr;
    } catch (...) {
        set_error_message(error_message, "module create() threw an unknown exception");
        return nullptr;
    }
}

template <typename Module>
void destroy(garcon_module_instance* instance)
{
    delete reinterpret_cast<exported_module_instance<Module>*>(instance);
}

template <typename Module>
garcon_module_result handle(garcon_module_instance* instance,
                            garcon_exchange* exchange_value,
                            const char** error_message)
{
    if (!instance || !exchange_value) {
        set_error_message(error_message, "module handle() received a null input");
        return garcon_module_result{.outcome = GARCON_MODULE_ERROR};
    }

    try {
        const auto& implementation =
            *reinterpret_cast<exported_module_instance<Module>*>(instance)->implementation;
        const auto result_value = implementation.handle(to_http_request(exchange_value->request));

        if (result_value.response_value) {
            auto response_value = *result_value.response_value;
            response_value.headers.insert(response_value.headers.end(),
                                          result_value.response_headers.begin(),
                                          result_value.response_headers.end());
            publish_response(response_value, *exchange_value);
        } else if (!result_value.response_headers.empty()) {
            publish_response_headers(result_value.response_headers, exchange_value->response);
        }

        return garcon_module_result{
            .outcome = static_cast<garcon_module_outcome>(result_value.next),
        };
    } catch (const std::exception& e) {
        set_error_message(error_message, e.what());
        return garcon_module_result{.outcome = GARCON_MODULE_ERROR};
    } catch (...) {
        set_error_message(error_message, "module handle() threw an unknown exception");
        return garcon_module_result{.outcome = GARCON_MODULE_ERROR};
    }
}

} // namespace detail

template <typename Module>
const garcon_module_descriptor* make_descriptor(const char* name)
{
    static const garcon_module_descriptor descriptor{
        .abi_version = GARCON_MODULE_ABI_VERSION,
        .name = name,
        .create = &detail::create<Module>,
        .destroy = &detail::destroy<Module>,
        .handle = &detail::handle<Module>,
    };

    return &descriptor;
}

} // namespace garcon::module

#define GARCON_EXPORT_MODULE(ModuleType, ModuleName)                           \
    extern "C" const garcon_module_descriptor*                                \
    garcon_get_module_descriptor(void)                                        \
    {                                                                         \
        return ::garcon::module::make_descriptor<ModuleType>(ModuleName);     \
    }
