#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace net {

class socket
{
public:
    socket();
    explicit socket(int fd);

    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    socket(socket&& o) noexcept;
    socket& operator=(socket&& o) noexcept;

    ~socket();

    bool valid() const;
    int  fd() const;

    void close();

    std::ptrdiff_t recv_some(std::span<std::byte> out);
    bool send_all(std::span<const std::byte> data);
    bool send_all(std::string_view s);

private:
    int _fd;
};

} /* namespace net */
