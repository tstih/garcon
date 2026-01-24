#pragma once

#include "net_listener.h"
#include "static_files.h"

#include <filesystem>

namespace app {

class server
{
public:
    server(int port, std::filesystem::path www_root);

    void run();

private:
    net::listener _listener;
    static_files  _files;

    void handle_connection(net::socket client);
};

} /* namespace app */
