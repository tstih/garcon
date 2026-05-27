// Runtime diagnostics seam for server events.
//
// This interface makes operational failures visible without coupling the
// worker pool or server runtime directly to a specific logging mechanism.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <memory>
#include <string_view>
#include <system_error>

namespace app {

class runtime_events
{
public:
    virtual ~runtime_events() = default;

    virtual void on_accept_error(const std::error_code& error,
                                 bool fatal) = 0;
    virtual void on_connection_rejected(std::string_view reason) = 0;
    virtual void on_connection_error(std::string_view stage,
                                     std::string_view detail) = 0;
    virtual void on_worker_error(std::string_view detail) = 0;
};

class stderr_runtime_events final : public runtime_events
{
public:
    void on_accept_error(const std::error_code& error,
                         bool fatal) override;
    void on_connection_rejected(std::string_view reason) override;
    void on_connection_error(std::string_view stage,
                             std::string_view detail) override;
    void on_worker_error(std::string_view detail) override;

private:
    void write_line(std::string_view level, std::string_view message);
};

std::unique_ptr<runtime_events> make_stderr_runtime_events();

} // namespace app
