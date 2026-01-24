#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace http {

class buffer
{
public:
    std::span<std::byte> write_span(std::size_t min_free = 2048);
    void commit(std::size_t n);

    std::string_view as_string_view() const;

private:
    std::vector<std::byte> _data;
    std::size_t _size = 0;
};

} /* namespace http */
