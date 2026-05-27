// Implementation of the HTTP server orchestration.
//
// This file implements the app::server class, which drives the main server
// loop. It accepts incoming TCP connections, performs HTTP framing and
// request parsing, and delegates request handling to the static file handler.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "server.h"

#include "http/buffer.h"
#include "http/framing.h"
#include "http/request.h"
#include "http/response.h"
#include "net/plain_stream.h"
#include "tls/context.h"
#include "tls/stream.h"

#include <iostream>
#include <memory>

namespace app {

namespace {

std::unique_ptr<tls::context> create_tls_context(const server_config& config)
{
    if (!config.tls)
        return nullptr;

    return std::make_unique<tls::context>(config.tls->certificate_file,
                                          config.tls->private_key_file);
}

void log_startup(const server_config& config)
{
    std::cout << "Garçon listening on "
              << config.scheme()
              << "://"
              << config.bind_address
              << ':'
              << config.port
              << "/\n"
              << std::flush;
}

} // namespace

server::server(server_config config)
    : _config(std::move(config)),
      _listener(_config.bind_address, _config.port),
      _files(_config.www_root),
      _tls_context(create_tls_context(_config))
{
    log_startup(_config);
}

void server::run()
{
    for (;;) {
        auto c = _listener.accept();
        if (!c.valid())
            continue;

        handle_connection(std::move(c));
    }
}

std::unique_ptr<net::stream> server::create_stream(net::socket client) const
{
    if (_tls_context)
        return std::make_unique<tls::stream>(*_tls_context, std::move(client));

    return std::make_unique<net::plain_stream>(std::move(client));
}

bool server::prepare_stream(net::stream& client) const
{
    if (!client.set_receive_timeout(io_timeout) ||
        !client.set_send_timeout(io_timeout)) {
        return false;
    }

    return client.handshake() == net::io_status::ok;
}

void server::serve_client(net::stream& client) const
{
    auto send = [&client](http::response response) {
        client.send_all(response.serialize());
    };

    http::buffer buf;

    const auto hdr = http::read_header_block(client, buf);
    switch (hdr.status) {
    case http::header_read_status::ok:
        break;
    case http::header_read_status::timeout:
        send(http::response::text(408, "Request Timeout", "request timeout\n"));
        return;
    case http::header_read_status::too_large:
        send(http::response::text(431,
                                  "Request Header Fields Too Large",
                                  "request header too large\n"));
        return;
    case http::header_read_status::closed:
    case http::header_read_status::error:
        return;
    }

    const auto req = http::request::parse(hdr.header);
    if (!req) {
        send(http::response::text(400, "Bad Request", "bad request\n"));
        return;
    }

    send(_files.handle(*req));
}

void server::handle_connection(net::socket client)
{
    try {
        auto stream = create_stream(std::move(client));
        if (!stream || !stream->valid())
            return;

        if (!prepare_stream(*stream))
            return;

        serve_client(*stream);
    } catch (const std::exception&) {
        return;
    }
}

} // namespace app
