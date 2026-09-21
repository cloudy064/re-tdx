/* tdx_endpoint.h - quote server endpoints and connect.cfg discovery.
 *
 * connect.cfg ships with the client installation and lists the public
 * HQHOST pool.  We only ever read it; nothing here writes to the install. */
#ifndef TDX_ENDPOINT_H
#define TDX_ENDPOINT_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_ENDPOINT_HOST_MAX 64
#define TDX_ENDPOINT_NAME_MAX 96

typedef struct tdx_endpoint {
    char host[TDX_ENDPOINT_HOST_MAX];
    uint16_t port;
    char name[TDX_ENDPOINT_NAME_MAX];
} tdx_endpoint;

#define TDX_ENDPOINT_POOL_MAX 64

typedef struct tdx_endpoint_pool {
    tdx_endpoint items[TDX_ENDPOINT_POOL_MAX];
    size_t count;
    /* "connect.cfg:hqhost-primary-first" or "compiled-default" */
    char source[48];
    /* Number of HQHOST entries found before any limit was applied. */
    size_t configured_count;
    int primary_configured;
} tdx_endpoint_pool;

/* Parses "host", "host:port", "[v6]:port".  Defaults to port 7709. */
int tdx_endpoint_parse(const char *text, tdx_endpoint *out, tdx_error *err);

/* Renders "host:port" into out (capacity 80 bytes is enough for any
 * valid endpoint). */
void tdx_endpoint_address(const tdx_endpoint *endpoint, char *out, size_t out_size);

/* Loads up to limit endpoints from <root>/connect.cfg, ordering the
 * configured primary host first and wrapping around.  Falls back to one
 * compiled-in public node when the file is missing or unusable. */
int tdx_endpoint_pool_load(const char *root, size_t limit,
                           tdx_endpoint_pool *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_ENDPOINT_H */
