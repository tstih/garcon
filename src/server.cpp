// Implementation of the HTTP server orchestration.
//
// This file implements the app::server class, which drives the main server
// loop. It accepts incoming TCP connections, performs HTTP framing and
// request parsing, and delegates request handling to the configured request
// handler.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "server.h"

#include "app/access_log.h"
#include "app/connection_handler.h"
#include "app/default_stream_factory.h"
#include "app/dynamic_module.h"
#include "app/ip_connection_limiter.h"
#include "app/module_config.h"
#include "app/request_pipeline.h"
#include "app/worker_pool.h"
#include "config.h"

#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace app {

namespace {

class ip_limit_release_guard
{
public:
    ip_limit_release_guard(ip_connection_limiter& limiter, std::string peer_address)
        : _limiter(limiter),
          _peer_address(std::move(peer_address))
    {
    }

    ~ip_limit_release_guard()
    {
        _limiter.release(_peer_address);
    }

private:
    ip_connection_limiter& _limiter;
    std::string _peer_address;
};

void log_startup(const server_config& config)
{
    std::cout << "Garçon listening on "
              << config.scheme()
              << "://"
              << config.bind_address
              << ':'
              << config.port
              << "/"
              << " with "
              << config.worker_threads
              << " worker"
              << (config.worker_threads == 1 ? "" : "s")
              << " and queue capacity "
              << config.connection_queue_capacity
              << "\n"
              << std::flush;
}

std::unique_ptr<request_handler> create_request_handler(const server_config& config)
{
    auto pipeline = std::make_unique<request_pipeline>();
    const auto configured_modules = load_configured_modules(config.module_config_dir);
    if (!configured_modules)
        throw std::runtime_error(configured_modules.error());

    for (const auto& module : *configured_modules) {
        std::cout << "Loading module from " << module.library_file << "\n";
        pipeline->add_module(std::make_unique<dynamic_module>(module.library_file,
                                                              module.config_directory,
                                                              module.config_text,
                                                              config));
    }

    return pipeline;
}

} // namespace

server::server(server_config config)
    : _config(std::move(config)),
      _listener(_config.bind_address, _config.port),
      _handler(create_request_handler(_config)),
      _stream_factory(std::make_unique<default_stream_factory>(_config.tls)),
      _events(make_stderr_runtime_events()),
      _access_log(make_stdout_access_log()),
      _connection_handler(std::make_unique<connection_handler>(*_handler,
                                                               *_stream_factory,
                                                               *_events,
                                                               *_access_log,
                                                               _config)),
      _ip_connection_limiter(garcon::config::max_connections_per_ip)
{
    log_startup(_config);
}

void server::run()
{
    work_queue queue(_config.connection_queue_capacity);
    worker_pool workers(queue,
                        [this](net::socket client) {
                            const auto peer_address = std::string(client.peer_address());
                            const ip_limit_release_guard release(_ip_connection_limiter,
                                                                 peer_address);
                            _connection_handler->handle_connection(std::move(client));
                        },
                        _config.worker_threads,
                        [this](std::string_view detail) {
                            _events->on_worker_error(detail);
                        });

    accept_loop(queue);
}

void server::handle_accept_result(const net::accept_error& error) const
{
    _events->on_accept_error(error.error, error.fatal);
    if (error.fatal)
        throw std::runtime_error("fatal accept() failure");

    std::this_thread::sleep_for(garcon::config::accept_retry_delay);
}

void server::handle_queue_result(queue_push_result result) const
{
    switch (result) {
    case queue_push_result::queued:
        return;
    case queue_push_result::full:
        _events->on_connection_rejected("accepted-connection queue full");
        return;
    case queue_push_result::closed:
        _events->on_connection_rejected("accepted-connection queue closed");
        return;
    }
}

void server::accept_loop(work_queue& queue)
{
    for (;;) {
        auto accepted = _listener.accept();
        if (!accepted) {
            handle_accept_result(accepted.error());
            continue;
        }

        const auto peer_address = std::string(accepted->peer_address());
        if (!_ip_connection_limiter.try_acquire(peer_address)) {
            _events->on_connection_rejected(std::format(
                "per-IP concurrent connection limit exceeded: {}",
                peer_address.empty() ? "<unknown>" : peer_address));
            continue;
        }

        const auto result = queue.try_push(std::move(*accepted));
        if (result != queue_push_result::queued)
            _ip_connection_limiter.release(peer_address);

        handle_queue_result(result);
    }
}

} // namespace app
