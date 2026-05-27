// Default connection stream factory.
//
// This factory creates either plain TCP streams or TLS streams depending on
// whether TLS configuration is present in the runtime configuration.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/stream_factory.h"
#include "server_config.h"
#include "tls/context.h"

#include <memory>
#include <optional>

namespace app {

class default_stream_factory final : public stream_factory
{
public:
    explicit default_stream_factory(std::optional<tls_config> tls);

    std::unique_ptr<net::stream> create(net::socket client) const override;

private:
    std::unique_ptr<tls::context> _tls_context;
};

} // namespace app
