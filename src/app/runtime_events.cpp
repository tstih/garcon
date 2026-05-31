// Implementation of runtime diagnostics.
//
// This file provides the default stderr-backed diagnostics strategy used by
// the server runtime and worker pool.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/runtime_events.h"

#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace app {

namespace {

static std::mutex s_log_mutex;

std::string make_accept_message(const std::error_code& error, bool fatal)
{
    return std::format("accept() failed ({}): {}",
                       fatal ? "fatal" : "retryable",
                       error.message());
}

std::string make_connection_message(std::string_view stage,
                                    std::string_view detail)
{
    return std::format("connection error during {}: {}", stage, detail);
}

} // namespace

void stderr_runtime_events::on_accept_error(const std::error_code& error,
                                            bool fatal,
                                            std::source_location where)
{
    write_line(fatal ? "error" : "warn",
               make_accept_message(error, fatal),
               where);
}

void stderr_runtime_events::on_connection_rejected(std::string_view reason,
                                                   std::source_location where)
{
    write_line("warn", std::format("connection rejected: {}", reason), where);
}

void stderr_runtime_events::on_connection_error(std::string_view stage,
                                                std::string_view detail,
                                                std::source_location where)
{
    write_line("warn", make_connection_message(stage, detail), where);
}

void stderr_runtime_events::on_worker_error(std::string_view detail,
                                            std::source_location where)
{
    write_line("error", std::format("worker callback error: {}", detail), where);
}

void stderr_runtime_events::write_line(std::string_view level,
                                       std::string_view message,
                                       std::source_location where)
{
    std::lock_guard lock(s_log_mutex);
    std::clog << std::format("[{}] {}:{} {}\n",
                             level,
                             where.file_name(),
                             where.line(),
                             message);
}

std::unique_ptr<runtime_events> make_stderr_runtime_events()
{
    return std::make_unique<stderr_runtime_events>();
}

} // namespace app
