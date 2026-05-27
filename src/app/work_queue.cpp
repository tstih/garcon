// Implementation of the bounded accepted-socket queue.
//
// This file contains the queue synchronization logic shared by the accept loop
// and worker threads. The queue stays intentionally small and explicit.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/work_queue.h"

#include <stdexcept>
#include <utility>

namespace app {

work_queue::work_queue(std::size_t capacity)
    : _capacity(capacity),
      _closed(false)
{
    if (_capacity == 0)
        throw std::invalid_argument("work_queue capacity must be greater than zero");
}

queue_push_result work_queue::push(net::socket socket, std::stop_token stop)
{
    std::unique_lock lock(_mutex);
    const auto ready = [this] {
        return _closed || _sockets.size() < _capacity;
    };

    if (!ready() && !_not_full.wait(lock, stop, ready))
        return queue_push_result::closed;

    if (_closed)
        return queue_push_result::closed;

    _sockets.push_back(std::move(socket));
    lock.unlock();
    _not_empty.notify_one();
    return queue_push_result::queued;
}

queue_push_result work_queue::try_push(net::socket socket)
{
    std::unique_lock lock(_mutex);
    if (_closed || _sockets.size() >= _capacity)
        return _closed ? queue_push_result::closed : queue_push_result::full;

    _sockets.push_back(std::move(socket));
    lock.unlock();
    _not_empty.notify_one();
    return queue_push_result::queued;
}

std::optional<net::socket> work_queue::pop(std::stop_token stop)
{
    std::unique_lock lock(_mutex);
    const auto ready = [this] {
        return _closed || !_sockets.empty();
    };

    if (!ready() && !_not_empty.wait(lock, stop, ready))
        return std::nullopt;

    if (_sockets.empty())
        return std::nullopt;

    net::socket socket = std::move(_sockets.front());
    _sockets.pop_front();
    lock.unlock();
    _not_full.notify_one();
    return std::move(socket);
}

void work_queue::close()
{
    std::lock_guard lock(_mutex);
    _closed = true;
    _not_full.notify_all();
    _not_empty.notify_all();
}

bool work_queue::closed() const
{
    std::lock_guard lock(_mutex);
    return _closed;
}

std::size_t work_queue::capacity() const
{
    return _capacity;
}

std::size_t work_queue::size() const
{
    std::lock_guard lock(_mutex);
    return _sockets.size();
}

} // namespace app
