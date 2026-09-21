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
    /* A subscriber that writes nothing for this long is disconnected. */
    int write_timeout_ms;
} tdx_serve_options;

void tdx_serve_options_default(tdx_serve_options *options);

/* Blocks until the process is terminated. */
int tdx_serve_run(tdx_hub *hub, const tdx_code *universe, size_t universe_size,
                  const tdx_serve_options *options, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SERVE_H */
