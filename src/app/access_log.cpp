// Implementation of the default access-log strategy.
//
// This file writes one line per handled request to standard output in a
// Combined-Log-style format with an added elapsed-microseconds field.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/access_log.h"

#include <ctime>
#include <format>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

namespace app {

namespace {

std::mutex s_access_log_mutex;

std::string format_timestamp(std::chrono::system_clock::time_point timestamp)
{
    const auto time = std::chrono::system_clock::to_time_t(timestamp);

    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream out;
    out << std::put_time(&tm, "%d/%b/%Y:%H:%M:%S +0000");
    return out.str();
}

} // namespace

void stdout_access_log::log(const access_record& record)
{
    const auto line = std::format(
        "{} - - [{}] \"{} {} {}\" {} {} rt_us={}",
        record.client_ip.empty() ? "-" : record.client_ip,
        format_timestamp(record.timestamp),
        record.method,
        record.target,
        record.version,
        record.status,
        record.response_bytes,
        record.elapsed.count());

    std::lock_guard lock(s_access_log_mutex);
    std::cout << line << '\n';
}

std::unique_ptr<access_log> make_stdout_access_log()
{
    return std::make_unique<stdout_access_log>();
}

} // namespace app
