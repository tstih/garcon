// Implementation of the fixed-size worker pool.
//
// Workers wait on the shared queue, process one socket at a time, and stay
// alive across per-connection failures so one bad client cannot collapse the
// pool.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/worker_pool.h"

#include <stdexcept>
#include <utility>

namespace app {

worker_pool::worker_pool(work_queue& queue,
                         handler_type handler,
                         std::size_t worker_count,
                         error_handler_type on_error)
    : _queue(queue),
      _handler(std::move(handler)),
      _on_error(std::move(on_error))
{
    if (!_handler)
        throw std::invalid_argument("worker_pool handler must not be empty");
    if (worker_count == 0)
        throw std::invalid_argument("worker_pool must have at least one worker");

    start_workers(worker_count);
}

worker_pool::~worker_pool()
{
    request_stop();
}

void worker_pool::request_stop()
{
    _queue.close();

    for (auto& worker : _workers)
        worker.request_stop();
}

std::size_t worker_pool::size() const noexcept
{
    return _workers.size();
}

void worker_pool::start_workers(std::size_t worker_count)
{
    _workers.reserve(worker_count);

    for (std::size_t i = 0; i < worker_count; ++i) {
        _workers.emplace_back([this](std::stop_token stop) {
            run_worker(stop);
        });
    }
}

void worker_pool::run_worker(std::stop_token stop)
{
    for (;;) {
        auto socket = _queue.pop(stop);
        if (!socket)
            return;

        try {
            _handler(std::move(*socket));
        } catch (const std::exception& e) {
            if (_on_error)
                _on_error(e.what());
        } catch (...) {
            if (_on_error)
                _on_error("unknown worker exception");
        }
    }
}

} // namespace app
