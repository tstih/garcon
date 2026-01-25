// Implementation of the HTTP server orchestration.
//
// This file implements the app::server class, which drives the main server
// loop. It accepts incoming TCP connections, performs HTTP framing and
// request parsing, and delegates request handling to the static file handler.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "server.h"

#include "http/framing.h"
#include "http/buffer.h"
#include "http/request.h"
#include "http/response.h"

#include <iostream>

namespace app {

server::server(int port, std::filesystem::path www_root)
    : _listener(port),
      _files(std::move(www_root))
{
    std::cout << "Garçon listening on http://127.0.0.1:" << port << "/\n";
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

void server::handle_connection(net::socket client)
{
    http::buffer buf;

    const auto hdr = http::read_header_block(client, buf);
    if (!hdr)
        return;

    const auto req = http::request::parse(*hdr);
    if (!req) {
        client.send(http::response::text(400,
                                         "Bad Request",
                                         "bad request\n")
                        .serialize());
        return;
    }

    const auto resp = _files.handle(*req);
    client.send(resp.serialize());
}

} // namespace app
