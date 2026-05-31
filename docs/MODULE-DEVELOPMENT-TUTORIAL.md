# Garçon Module Development Tutorial

This tutorial shows how to write a Garçon request module in modern C++ without
having to work directly with the raw C ABI.

The main idea is:

- Garçon core owns transport, TLS, request parsing, and pipeline orchestration
- shared modules own request behavior
- the public ABI in `include/garcon/module_abi.h` keeps dynamic loading stable
- the helper layer in `include/garcon/module_cpp.h` lets module authors write
  normal C++ classes and hide the ABI glue

If you want concrete references in the tree today:

- `lib/cors/` is the simplest response-decorating gateway module example
- `lib/host_guard/` is the simplest host-based allowlist module example
- `lib/static_files/` is the simplest terminal module example and also holds
  the reusable static-file service used by that module
- `lib/route_table/` is the simplest pass-or-respond gateway-style example

## 1. Understand the module model

Garçon loads modules from a `modules.d/` directory.

- each `.conf` file defines one module instance
- `.conf` files are loaded in lexical order
- each file must contain a `path=` entry pointing at the shared library
- the rest of the file is passed as raw config text to that module
- beyond `path=`, the config format is defined by the module itself

Each module returns one of four outcomes:

- `pass`: let the next module try the request
- `respond`: return the supplied `http::response`
- `upgrade`: reserved for future upgrade/session work
- `error`: signal an internal failure to the host

The default development build demonstrates this with:

1. `03-host-guard.conf`
2. `05-route-table.conf`
3. `07-cors.conf`
4. `08-header-guard.conf`
5. `10-static-files.conf`

That means the host-guard module sees the request first, the route-table module
can answer simple endpoints, the CORS module can answer preflight requests and
decorate downstream responses, the header-guard module can protect `/api/*`,
and static files act as the fallback terminal handler.

## 2. Prefer the C++ helper layer

Most module authors should include:

```cpp
#include "garcon/module_cpp.h"
```

That header gives you:

- `garcon::module::module` for modules that may `pass` or `respond`
- `garcon::module::http_terminal_module` for modules that always produce a
  response
- `garcon::module::result` and `garcon::module::outcome`
- `garcon::module::host_context`
- `garcon::module::key_value_config`
- `GARCON_EXPORT_MODULE(...)` to expose the module through the ABI

That means the normal authoring experience is plain C++ with
`http::request` and `http::response`, not raw `garcon_exchange` structs.

`host_context` is also where the host exposes small environment details such as
the default document root and the config directory that loaded the module.

`http::request` also includes parsed header fields and small helpers such as:

- `find_header(...)`
- `header_value(...)`
- `header_equals(...)`

`http::response` supports:

- arbitrary response headers
- `add_header(...)`
- `set_header(...)`

Those helpers validate header names and values, so CR/LF injection attempts are
rejected before the response reaches the wire.

And `garcon::module::result` can also attach response headers to a `pass`
result with:

- `add_response_header(...)`

## 3. Start with a small module layout

A typical module directory looks like this:

```text
lib/hello_gate/
  CMakeLists.txt
  hello_gate_module.h
  hello_gate_module.cpp
  module.cpp
```

Use the files this way:

- `hello_gate_module.h`: class definition
- `hello_gate_module.cpp`: actual C++ logic
- `module.cpp`: one-line ABI export shim
- `CMakeLists.txt`: shared-library target

## 4. Implement a simple C++ module

This example responds to one configured path and passes everything else to the
next module.

### Header

```cpp
#pragma once

#include "garcon/module_cpp.h"

#include <string>
#include <string_view>

namespace garcon::modules {

class hello_gate_module final : public garcon::module::module
{
public:
    hello_gate_module(const garcon::module::host_context& host,
                      std::string_view config_text);

    garcon::module::result handle(const http::request& request) const override;

private:
    std::string _match_path;
    std::string _body;
};

} // namespace garcon::modules
```

### Implementation

```cpp
#include "hello_gate_module.h"

#include <string>

namespace garcon::modules {

hello_gate_module::hello_gate_module(const garcon::module::host_context&,
                                     std::string_view config_text)
{
    garcon::module::key_value_config config(config_text);
    _match_path = std::string(config.get("match").value_or("/hello"));
    _body = std::string(config.get("body").value_or("hello\n"));
}

garcon::module::result hello_gate_module::handle(const http::request& request) const
{
    if (request.method != "GET" || request.target != _match_path)
        return garcon::module::result::pass();

    http::response response;
    response.status = 200;
    response.reason = "OK";
    response.content_type = "text/plain; charset=utf-8";
    response.body = _body;

    return garcon::module::result::respond(response);
}

} // namespace garcon::modules
```

### ABI export shim

```cpp
#include "hello_gate_module.h"

GARCON_EXPORT_MODULE(garcon::modules::hello_gate_module, "hello-gate")
```

That last file is the important separation point: the module logic stays in a
cohesive C++ class, and the ABI-facing glue is reduced to the export macro.

## 5. Add a config file

Create a config file under `modules.d/`, for example:

```ini
# modules.d/20-hello-gate.conf
path=../modules/libgarcon_hello_gate_module.so
match=/hello
body=hello from a shared module
```

Two useful rules:

- keep the numeric prefix meaningful because it controls execution order
- remember that relative `path=` values are resolved relative to the config file

For example:

- `00-04` for host or early gateway policy
- `05-19` for routing and auth
- `90-...` for fallback handlers

## 6. Add the CMake target

An initial `CMakeLists.txt` can stay very small:

```cmake
add_library(garcon_hello_gate_module SHARED
    module.cpp
    hello_gate_module.cpp
    ../../src/http/response.cpp
)

target_compile_features(garcon_hello_gate_module PRIVATE cxx_std_23)

target_include_directories(garcon_hello_gate_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(garcon_hello_gate_module PRIVATE
    garcon_sanitizers
)

set_target_properties(garcon_hello_gate_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
```

Then add the directory in `lib/CMakeLists.txt`:

```cmake
add_subdirectory(hello_gate)
```

That matches the existing `cors`, `host_guard`, `header_guard`, `route_table`,
and `static_files` modules.

## 7. Choose the right base class

Use `garcon::module::module` when the module may:

- inspect the request and decide to `pass`
- respond only for selected paths or methods
- behave like an early gateway or policy module

Use `garcon::module::http_terminal_module` when the module always returns a
response and never passes the request onward.

`static_files_module` is the current example of a terminal module.
`route_table_module` is the current example of a pass-or-respond module.
`host_guard_module` and `header_guard_module` are the current examples of
early gateway policy modules.
`cors_module` is the current example of a module that decorates downstream
responses while still allowing the request to pass onward.

## 8. Keep the module thread-safe

This part matters.

Garçon creates the module instance at startup and may call `handle()`
concurrently on that same instance from multiple worker threads.

That means:

- prefer immutable state after construction
- keep request-specific data on the stack inside `handle()`
- avoid mutable shared buffers inside the class
- if you must mutate shared state, protect it explicitly

The easiest safe pattern is the one used by the current modules:

- parse config in the constructor
- store stable values in members
- do all per-request work in local variables inside `handle()`

## 9. Know the current ABI limits

Today the host passes a small HTTP model across the module boundary:

- request method
- request target
- request headers
- response status
- response reason
- response content type
- response headers
- response body
- optional explicit content length

That is enough for route matching, header checks, response decoration, static
files, health checks, and other early API-gateway style modules. It is not yet
a full upstream-proxy or WebSocket module surface.

## 10. Test the module like the others

The project currently favors black-box smoke tests.

A good pattern is:

1. add a module config file under `modules.d/`
2. start Garçon with that config
3. hit the paths that should `respond`
4. hit the paths that should `pass`
5. verify the next module in the chain still works

`tests/cors_smoke.sh`, `tests/host_guard_smoke.sh`,
`tests/route_table_smoke.sh`, and `tests/header_guard_smoke.sh` are the best
examples of this pattern today.

## 11. Where to look next

If you want a reference implementation in the repository:

- read `lib/static_files/` for the smallest terminal-style module
- read `lib/route_table/` for the smallest gateway-style module
- read `include/garcon/module_cpp.h` only when you need to understand the
  helper layer itself

The intended workflow is that most module authors spend their time in their own
module class and almost never need to think about the raw C ABI. Core concerns
such as keep-alive reuse, TLS handshakes, HSTS, and access logging stay in the
host runtime, not in individual modules.
