// Implementation of the default connection stream factory.
//
// This file contains the small policy object that decides whether accepted
// sockets become plain TCP streams or TLS streams.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/default_stream_factory.h"

#include "net/plain_stream.h"
#include "tls/stream.h"

#include <utility>

namespace app {

namespace {

std::unique_ptr<tls::context> create_tls_context(const std::optional<tls_config>& tls)
{
    if (!tls)
        return nullptr;

    return std::make_unique<tls::context>(tls->certificate_file,
                                          tls->private_key_file);
}

} // namespace

default_stream_factory::default_stream_factory(std::optional<tls_config> tls)
    : _tls_context(create_tls_context(tls))
{
}

std::unique_ptr<net::stream> default_stream_factory::create(net::socket client) const
{
    if (_tls_context)
        return std::make_unique<tls::stream>(*_tls_context, std::move(client));

    return std::make_unique<net::plain_stream>(std::move(client));
}

} // namespace app
