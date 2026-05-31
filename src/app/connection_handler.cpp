// Implementation of the per-connection request runtime.
//
// This file keeps stream preparation, request framing, keep-alive reuse,
// response writes, and access logging out of server.cpp so the server can stay
// focused on orchestration.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/connection_handler.h"

#include "http/buffer.h"
#include "http/framing.h"
#include "http/request.h"
#include "http/response.h"

#include <chrono>
#include <exception>
#include <memory>
#include <utility>

namespace app {

connection_handler::connection_handler(const request_handler& handler,
                                       const stream_factory& stream_factory,
                                       runtime_events& events,
                                       access_log& access_log,
                                       const server_config& config)
    : _handler(handler),
      _stream_factory(stream_factory),
      _events(events),
      _access_log(access_log),
      _https_enabled(config.https_enabled())
{
}

bool connection_handler::prepare_stream(net::stream& client) const
{
    if (!client.set_receive_timeout(io_timeout) ||
        !client.set_send_timeout(io_timeout)) {
        _events.on_connection_error("configure-stream",
                                    "failed to apply socket timeouts");
        return false;
    }

    const auto handshake = client.handshake();
    if (handshake == net::io_status::ok)
        return true;

    if (handshake == net::io_status::timeout) {
        _events.on_connection_error("handshake", "handshake timed out");
        return false;
    }

    if (handshake == net::io_status::error)
        _events.on_connection_error("handshake", "handshake failed");

    return false;
}

bool connection_handler::send_response(net::stream& client,
                                       std::string_view wire_response) const
{
    const auto status = client.send_all(wire_response);
    if (status == net::io_status::ok || status == net::io_status::closed)
        return true;

    const auto detail = (status == net::io_status::timeout)
        ? "response write timed out"
        : "response write failed";
    _events.on_connection_error("write-response", detail);
    return false;
}

void connection_handler::log_access(std::string_view client_ip,
                                    const http::request* request,
                                    const http::response& response,
                                    std::size_t wire_bytes,
                                    std::chrono::steady_clock::duration elapsed) const
{
    access_record record{
        .timestamp = std::chrono::system_clock::now(),
        .client_ip = std::string(client_ip),
        .method = request ? request->method : "-",
        .target = request ? request->target : "-",
        .version = request ? request->version_string() : "HTTP/1.1",
        .status = response.status,
        .response_bytes = wire_bytes,
        .elapsed = std::chrono::duration_cast<std::chrono::microseconds>(elapsed),
    };

    _access_log.log(record);
}

void connection_handler::handle_connection(net::socket client) const
{
    const auto client_ip = std::string(client.peer_address());

    try {
        auto stream = _stream_factory.create(std::move(client));
        if (!stream || !stream->is_valid()) {
            _events.on_connection_error("create-stream", "stream creation failed");
            return;
        }

        if (!prepare_stream(*stream))
            return;

        bool first_request = true;

        for (;;) {
            const auto receive_timeout = first_request ? io_timeout
                                                       : keep_alive_idle_timeout;
            if (!stream->set_receive_timeout(receive_timeout)) {
                _events.on_connection_error("configure-stream",
                                            "failed to update receive timeout");
                return;
            }

            http::buffer buffer;
            const auto header = http::read_header_block(*stream, buffer);
            if (!header) {
                switch (header.error()) {
                case http::header_read_error::timeout: {
                if (!first_request && buffer.as_string_view().empty())
                    return;

                const auto started = std::chrono::steady_clock::now();
                const auto response = http::response::text(408,
                                                           "Request Timeout",
                                                           "request timeout\n");
                const auto policy = http::connection_policy::close;
                const auto wire_response = response.serialize(policy);
                log_access(client_ip,
                           nullptr,
                           response,
                           wire_response.size(),
                           std::chrono::steady_clock::now() - started);
                send_response(*stream, wire_response);
                return;
            }
                case http::header_read_error::too_large: {
                const auto started = std::chrono::steady_clock::now();
                const auto response =
                    http::response::text(431,
                                         "Request Header Fields Too Large",
                                         "request header too large\n");
                const auto policy = http::connection_policy::close;
                const auto wire_response = response.serialize(policy);
                log_access(client_ip,
                           nullptr,
                           response,
                           wire_response.size(),
                           std::chrono::steady_clock::now() - started);
                send_response(*stream, wire_response);
                return;
            }
                case http::header_read_error::closed:
                    return;
                case http::header_read_error::io_error:
                    _events.on_connection_error("read-header", "request read failed");
                    return;
                }
            }

            const auto parsed = http::request::parse(*header);
            if (!parsed) {
                const auto started = std::chrono::steady_clock::now();
                const auto response = http::response::text(400,
                                                           "Bad Request",
                                                           "bad request\n");
                const auto policy = http::connection_policy::close;
                const auto wire_response = response.serialize(policy);
                log_access(client_ip,
                           nullptr,
                           response,
                           wire_response.size(),
                           std::chrono::steady_clock::now() - started);
                send_response(*stream, wire_response);
                return;
            }

            const auto started = std::chrono::steady_clock::now();
            auto response = _handler.handle(*parsed);

            if (_https_enabled &&
                !response.find_header("Strict-Transport-Security")) {
                response.set_header("Strict-Transport-Security", "max-age=31536000");
            }

            const auto policy = parsed->keep_alive_requested()
                ? http::connection_policy::keep_alive
                : http::connection_policy::close;
            const auto wire_response = response.serialize(policy);
            const auto elapsed = std::chrono::steady_clock::now() - started;
            log_access(client_ip, &*parsed, response, wire_response.size(), elapsed);

            if (!send_response(*stream, wire_response))
                return;

            if (policy == http::connection_policy::close)
                return;

            first_request = false;
        }
    } catch (const std::exception& e) {
        _events.on_connection_error("connection-handler", e.what());
    }
}

} // namespace app
