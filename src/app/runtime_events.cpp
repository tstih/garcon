// Implementation of runtime diagnostics.
//
// This file provides the default stderr-backed diagnostics strategy used by
// the server runtime and worker pool.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/runtime_events.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace app {

namespace {

std::mutex& log_mutex()
{
    static std::mutex mutex;
    return mutex;
}

std::string make_accept_message(const std::error_code& error, bool fatal)
{
    std::string message = "accept() failed";
    message += fatal ? " (fatal): " : " (retryable): ";
    message += error.message();
    return message;
}

std::string make_connection_message(std::string_view stage,
                                    std::string_view detail)
{
    std::string message = "connection error during ";
    message += stage;
    message += ": ";
    message += detail;
    return message;
}

} // namespace

void stderr_runtime_events::on_accept_error(const std::error_code& error,
                                            bool fatal)
{
    write_line(fatal ? "error" : "warn",
               make_accept_message(error, fatal));
}

void stderr_runtime_events::on_connection_rejected(std::string_view reason)
{
    std::string message = "connection rejected: ";
    message += reason;
    write_line("warn", message);
}

void stderr_runtime_events::on_connection_error(std::string_view stage,
                                                std::string_view detail)
{
    write_line("warn", make_connection_message(stage, detail));
}

void stderr_runtime_events::on_worker_error(std::string_view detail)
{
    std::string message = "worker callback error: ";
    message += detail;
    write_line("error", message);
}

void stderr_runtime_events::write_line(std::string_view level,
                                       std::string_view message)
{
    std::lock_guard lock(log_mutex());
    std::clog << '[' << level << "] " << message << '\n';
}

std::unique_ptr<runtime_events> make_stderr_runtime_events()
{
    return std::make_unique<stderr_runtime_events>();
}

} // namespace app
