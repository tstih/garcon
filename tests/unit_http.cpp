// Small unit tests for the HTTP parser, buffer, framing, and response helpers.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "http/buffer.h"
#include "http/framing.h"
#include "http/request.h"
#include "http/response.h"
#include "net/stream.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[noreturn]] void fail(std::string_view message)
{
    std::cerr << "unit_http failed: " << message << '\n';
    std::exit(1);
}

void expect(bool condition, std::string_view message)
{
    if (!condition)
        fail(message);
}

class scripted_stream final : public net::stream
{
public:
    scripted_stream(std::vector<std::string> chunks, net::io_status final_status)
        : _chunks(std::move(chunks)),
          _final_status(final_status)
    {
    }

    bool is_valid() const override { return true; }
    void close() override {}
    bool set_receive_timeout(std::chrono::milliseconds) override { return true; }
    bool set_send_timeout(std::chrono::milliseconds) override { return true; }
    net::io_status handshake() override { return net::io_status::ok; }

    net::read_result recv_some(std::span<std::byte> out) override
    {
        if (_index >= _chunks.size())
            return net::read_result{.status = _final_status};

        const auto& chunk = _chunks[_index++];
        const auto size = std::min(out.size(), chunk.size());
        std::memcpy(out.data(), chunk.data(), size);
        return net::read_result{
            .status = net::io_status::ok,
            .bytes_read = size,
        };
    }

    net::io_status send_all(std::span<const std::byte>) override
    {
        return net::io_status::ok;
    }

private:
    std::vector<std::string> _chunks;
    std::size_t _index = 0;
    net::io_status _final_status = net::io_status::closed;
};

void test_request_parse_valid()
{
    const auto parsed = http::request::parse(
        "GET /hello HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n");

    expect(parsed.has_value(), "valid request should parse");
    expect(parsed->method == "GET", "request method should be preserved");
    expect(parsed->target == "/hello", "request target should be preserved");
    expect(parsed->version_major == 1 && parsed->version_minor == 1,
           "HTTP/1.1 version should parse");
    expect(parsed->keep_alive_requested(), "HTTP/1.1 request should default to keep-alive");
}

void test_request_parse_rejects_folded_header()
{
    const auto parsed = http::request::parse(
        "GET / HTTP/1.1\r\nHost: localhost\r\n X-Bad: folded\r\n\r\n");

    expect(!parsed.has_value(), "header folding should be rejected");
    expect(parsed.error() == http::request_parse_error::invalid_header,
           "folded header should report invalid_header");
}

void test_buffer_growth_and_commit()
{
    http::buffer buffer;
    auto out = buffer.write_span(4);
    expect(out.size() >= 4, "buffer write span should honor minimum free space");

    const char payload[] = "test";
    std::memcpy(out.data(), payload, 4);
    buffer.commit(4);

    expect(buffer.as_string_view() == "test", "buffer should expose committed bytes");
}

void test_framing_partial_reads()
{
    scripted_stream stream({
        "GET / HTTP/1.1\r\nHost: local",
        "host\r\n\r\n",
    }, net::io_status::closed);

    http::buffer buffer;
    const auto header = http::read_header_block(stream, buffer);
    expect(header.has_value(), "framing should assemble header across partial reads");
    expect(*header == "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
           "framing should return the complete header block");
}

void test_framing_too_large()
{
    scripted_stream stream({
        "GET / HTTP/1.1\r\nHost: localhost",
    }, net::io_status::closed);

    http::buffer buffer;
    const auto header = http::read_header_block(stream, buffer, 16);
    expect(!header.has_value(), "oversized header should fail");
    expect(header.error() == http::header_read_error::too_large,
           "oversized header should report too_large");
}

void test_response_header_validation()
{
    http::response response;
    bool threw = false;

    try {
        response.add_header("X-Test", "bad\r\nvalue");
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    expect(threw, "response.add_header should reject CRLF injection");

    response.headers.push_back(http::header_field{
        .name = "X-Unsafe",
        .value = "still\r\nbad",
    });

    const auto wire = response.serialize();
    expect(wire.find("X-Unsafe") == std::string::npos,
           "serialize should drop invalid response headers");
}

} // namespace

int main()
{
    test_request_parse_valid();
    test_request_parse_rejects_folded_header();
    test_buffer_growth_and_commit();
    test_framing_partial_reads();
    test_framing_too_large();
    test_response_header_validation();
    return 0;
}
