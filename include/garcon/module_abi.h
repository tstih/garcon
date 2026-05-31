#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GARCON_MODULE_ABI_VERSION = 3U,
};

#define GARCON_MODULE_ENTRYPOINT "garcon_get_module_descriptor"

typedef struct garcon_string_view
{
    const char* data;
    size_t size;
} garcon_string_view;

typedef struct garcon_header_view
{
    garcon_string_view name;
    garcon_string_view value;
} garcon_header_view;

typedef struct garcon_request_view
{
    garcon_string_view method;
    garcon_string_view target;
    const garcon_header_view* headers;
    size_t header_count;
} garcon_request_view;

typedef struct garcon_response_view
{
    int status;
    garcon_string_view reason;
    garcon_string_view content_type;
    const garcon_header_view* headers;
    size_t header_count;
    garcon_string_view body;
    size_t content_length;
    int has_content_length;
} garcon_response_view;

typedef struct garcon_exchange
{
    garcon_request_view request;
    garcon_response_view response;
} garcon_exchange;

typedef enum garcon_module_outcome
{
    GARCON_MODULE_PASS = 0,
    GARCON_MODULE_RESPOND = 1,
    GARCON_MODULE_UPGRADE = 2,
    GARCON_MODULE_ERROR = 3,
} garcon_module_outcome;

typedef struct garcon_module_result
{
    garcon_module_outcome outcome;
} garcon_module_result;

typedef struct garcon_host_api
{
    uint32_t abi_version;
    garcon_string_view default_document_root;
    garcon_string_view config_directory;
} garcon_host_api;

typedef struct garcon_module_instance garcon_module_instance;

typedef struct garcon_module_descriptor
{
    uint32_t abi_version;
    const char* name;
    garcon_module_instance* (*create)(const garcon_host_api* host,
                                      const char* config,
                                      const char** error_message);
    void (*destroy)(garcon_module_instance* instance);
    garcon_module_result (*handle)(garcon_module_instance* instance,
                                   garcon_exchange* exchange,
                                   const char** error_message);
} garcon_module_descriptor;

typedef const garcon_module_descriptor* (*garcon_get_module_descriptor_fn)(void);

#ifdef __cplusplus
}
#endif
