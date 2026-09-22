/* tdx_serve.h - read-only HTTP surface in front of the resident hub.
 *
 * Routes:
 *   GET /                       one page of usage
 *   GET /health                 liveness
 *   GET /status                 hub counters
 *   GET /api/v1/market/snapshot one-shot current value
 *   GET /api/v1/market/stream   Server-Sent Events, change-only
 */
#ifndef TDX_SERVE_H
#define TDX_SERVE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"
#include "tdx_hub.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdx_serve_options {
    int port;
    int backlog;
    /* Timeout for each blocking send to a slow reader, not an idle timeout. */
    int write_timeout_ms;
    /* Total time to receive a complete HTTP header, including partial reads. */
    int read_timeout_ms;
    /* Bounds all HTTP handlers, including clients which have not sent headers. */
    size_t max_connections;
} tdx_serve_options;

void tdx_serve_options_default(tdx_serve_options *options);

typedef struct tdx_server tdx_server;

/* The server borrows hub until destroy and binds only IPv4 127.0.0.1.
 * Port 0 selects an ephemeral port. Zero read_timeout_ms/max_connections
 * select defaults for older callers. */
int tdx_server_create(tdx_server **out, tdx_hub *hub,
                      const tdx_serve_options *options, tdx_error *err);
int tdx_server_start(tdx_server *server, tdx_error *err);
int tdx_server_port(const tdx_server *server);
/* Terminal, idempotent stop: shutdown all clients and join every handler.
 * Concurrent stop calls are safe. The borrowed hub remains running. */
void tdx_server_stop(tdx_server *server);
/* Stops and joins owned handlers. All external API users, including concurrent
 * stop callers, must have returned before destroy. */
void tdx_server_destroy(tdx_server *server);

/* Compatibility wrapper: runs until SIGINT/SIGTERM, drains the server, then
 * stops the hub. Installs process signal handlers; use the instance API for
 * embedding or multiple servers. */
int tdx_serve_run(tdx_hub *hub, const tdx_code *universe, size_t universe_size,
                  const tdx_serve_options *options, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SERVE_H */
