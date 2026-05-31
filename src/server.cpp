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

#include "app/dynamic_module.h"
#include "app/default_stream_factory.h"
#include "app/module_config.h"
#include "app/request_pipeline.h"
#include "app/worker_pool.h"
#include "config.h"
#include "http/buffer.h"
#include "http/framing.h"
#include "http/request.h"
#include "http/response.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace app {

namespace {

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

    for (const auto& module : load_configured_modules(config.module_config_dir)) {
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
      _events(make_stderr_runtime_events())
{
    log_startup(_config);
}

void server::run()
{
    work_queue queue(_config.connection_queue_capacity);
    worker_pool workers(queue,
                        [this](net::socket client) {
                            handle_connection(std::move(client));
                        },
                        _config.worker_threads,
                        [this](std::string_view detail) {
                            _events->on_worker_error(detail);
                        });

    accept_loop(queue);
}

void server::handle_accept_result(const net::accept_result& accepted) const
{
    if (accepted.status == net::accept_status::accepted)
        return;

    const bool fatal = accepted.status == net::accept_status::fatal_error;
    _events->on_accept_error(accepted.error, fatal);
    if (fatal)
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

void server::send_response(net::stream& client, http::response response) const
{
    const auto status = client.send_all(response.serialize());
    if (status == net::io_status::ok || status == net::io_status::closed)
        return;

    const auto detail = (status == net::io_status::timeout)
        ? "response write timed out"
        : "response write failed";
    _events->on_connection_error("write-response", detail);
}

bool server::prepare_stream(net::stream& client) const
{
    if (!client.set_receive_timeout(io_timeout) ||
        !client.set_send_timeout(io_timeout)) {
        _events->on_connection_error("configure-stream",
                                     "failed to apply socket timeouts");
        return false;
    }

    const auto handshake = client.handshake();
    if (handshake == net::io_status::ok)
        return true;

    if (handshake == net::io_status::timeout) {
        _events->on_connection_error("handshake", "handshake timed out");
        return false;
    }

    if (handshake == net::io_status::error)
        _events->on_connection_error("handshake", "handshake failed");

    return false;
}

void server::serve_client(net::stream& client) const
{
    http::buffer buf;

    const auto hdr = http::read_header_block(client, buf);
    switch (hdr.status) {
    case http::header_read_status::ok:
        break;
    case http::header_read_status::timeout:
        send_response(client,
                      http::response::text(408,
                                           "Request Timeout",
                                           "request timeout\n"));
        return;
    case http::header_read_status::too_large:
        send_response(client,
                      http::response::text(431,
                                           "Request Header Fields Too Large",
                                           "request header too large\n"));
        return;
    case http::header_read_status::closed:
        return;
    case http::header_read_status::error:
        _events->on_connection_error("read-header", "request read failed");
        return;
    }

    const auto req = http::request::parse(hdr.header);
    if (!req) {
        send_response(client,
                      http::response::text(400,
                                           "Bad Request",
                                           "bad request\n"));
        return;
    }

    send_response(client, _handler->handle(*req));
}

void server::handle_connection(net::socket client)
{
    try {
        auto stream = _stream_factory->create(std::move(client));
        if (!stream || !stream->is_valid()) {
            _events->on_connection_error("create-stream", "stream creation failed");
            return;
        }

        if (!prepare_stream(*stream))
            return;

        serve_client(*stream);
    } catch (const std::exception& e) {
        _events->on_connection_error("connection-handler", e.what());
    }
}

void server::accept_loop(work_queue& queue)
{
    for (;;) {
        auto accepted = _listener.accept();
        handle_accept_result(accepted);
        if (!accepted)
            continue;

        handle_queue_result(queue.try_push(std::move(accepted.client)));
    }
}

} // namespace app
