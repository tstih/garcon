// Shared TLS error formatting utility.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <string>
#include <string_view>

#include <openssl/err.h>

namespace tls {

inline std::string ssl_error_message(std::string_view prefix)
{
    const auto code = ::ERR_get_error();
    if (code == 0)
        return std::string(prefix);

    char buf[256];
    ::ERR_error_string_n(code, buf, sizeof(buf));
    return std::string(prefix) + ": " + buf;
}

} // namespace tls
