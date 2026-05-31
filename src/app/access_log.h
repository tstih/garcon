// Access logging seam for per-request observability.
//
// This interface captures one access record per handled HTTP request while
// keeping the runtime decoupled from any concrete logging destination.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

namespace app {

struct access_record
{
    std::chrono::system_clock::time_point timestamp;
    std::string client_ip;
    std::string method;
    std::string target;
    std::string version;
    int status = 0;
    std::size_t response_bytes = 0;
    std::chrono::microseconds elapsed = std::chrono::microseconds::zero();
};

class access_log
{
public:
    virtual ~access_log() = default;

    virtual void log(const access_record& record) = 0;
};

class stdout_access_log final : public access_log
{
public:
    void log(const access_record& record) override;
};

[[nodiscard]] std::unique_ptr<access_log> make_stdout_access_log();

} // namespace app
