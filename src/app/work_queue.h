// Bounded queue for accepted client sockets.
//
// This queue is the single shared hand-off point between the accept loop and
// worker threads. It keeps connection buffering explicit and bounded while
// supporting cooperative shutdown.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/socket.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

namespace app {

enum class queue_push_result
{
    queued,
    full,
    closed,
};

class work_queue
{
public:
    explicit work_queue(std::size_t capacity);

    work_queue(const work_queue&) = delete;
    work_queue& operator=(const work_queue&) = delete;

    // Blocks until space is available, the queue is closed, or stop is
    // requested.
    queue_push_result push(net::socket socket, std::stop_token stop = {});

    // Attempts to enqueue immediately without blocking.
    queue_push_result try_push(net::socket socket);

    // Waits for the next socket. Returns std::nullopt when the queue is
    // closed and drained, or when stop is requested before work arrives.
    std::optional<net::socket> pop(std::stop_token stop = {});

    // Prevents further pushes and wakes blocked producers and consumers.
    void close();

    bool closed() const;
    std::size_t capacity() const;
    std::size_t size() const;

private:
    std::size_t _capacity;
    std::deque<net::socket> _sockets;
    mutable std::mutex _mutex;
    std::condition_variable_any _not_empty;
    std::condition_variable_any _not_full;
    bool _closed;
};

} // namespace app
