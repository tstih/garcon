#pragma once

#include "net_socket.h"

namespace net {

class listener
{
public:
    explicit listener(int port);

    socket accept();

private:
    socket _sock;
};

} /* namespace net */
