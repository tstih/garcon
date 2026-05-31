// Shared HTTP header utilities.
//
// This file defines the small owning header representation shared by HTTP
// requests and responses together with ASCII-only header-name comparison.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <string>
#include <string_view>

namespace http {

struct header_field
{
    std::string name;
    std::string value;
};

inline char ascii_lower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<char>(ch - 'A' + 'a');
    return ch;
}

inline bool header_name_equals(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (ascii_lower(left[i]) != ascii_lower(right[i]))
            return false;
    }

    return true;
}

} // namespace http
