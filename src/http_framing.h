#pragma once

#include "http_buffer.h"
#include "net_socket.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace http {

std::optional<std::string_view> read_header_block(net::socket& s,
                                                  buffer& b,
                                                  std::size_t max_bytes = 64 * 1024);

} /* namespace http */
