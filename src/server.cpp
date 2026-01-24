#include "server.h"

#include "http_framing.h"

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
    if (!hdr) return;

    const auto req = http::request::parse(*hdr);
    if (!req) {
        client.send_all(http::response::text(400, "Bad Request", "bad request\n")
                            .serialize());
        return;
    }

    const auto resp = _files.handle(*req);
    client.send_all(resp.serialize());
}

} /* namespace app */
