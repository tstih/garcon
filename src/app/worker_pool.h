// Fixed-size worker pool for accepted client sockets.
//
// This pool owns worker thread lifetime only. Each worker pulls sockets from
// the shared queue and delegates per-connection handling to a supplied
// callback.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/work_queue.h"

#include <cstddef>
#include <functional>
#include <string_view>
#include <thread>
#include <vector>

namespace app {

class worker_pool
{
public:
    using handler_type = std::function<void(net::socket)>;
    using error_handler_type = std::function<void(std::string_view)>;

    worker_pool(work_queue& queue,
                handler_type handler,
                std::size_t worker_count,
                error_handler_type on_error = {});
    worker_pool(const worker_pool&) = delete;
    worker_pool& operator=(const worker_pool&) = delete;
    ~worker_pool();

    void request_stop();
    std::size_t size() const;

private:
    void start_workers(std::size_t worker_count);
    void run_worker(std::stop_token stop);

    work_queue& _queue;
    handler_type _handler;
    error_handler_type _on_error;
    std::vector<std::jthread> _workers;
};

} // namespace app
