/* tdx_hub.h - resident polling hub with per-subscriber change queues.
 *
 * One worker polls the whole universe on a fixed cadence and updates a single
 * last-known-value table.  Every subscriber sees the same diff and only the
 * securities it asked for, so N readers cost one poller rather than N. */
#ifndef TDX_HUB_H
#define TDX_HUB_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_HUB_MAX_SUBSCRIBERS 64
#define TDX_HUB_DEFAULT_QUEUE_LIMIT 512
#define TDX_HUB_DEFAULT_INTERVAL_MS 1000
#define TDX_HUB_DEFAULT_HEARTBEAT_MS 15000
/* Hard ceiling for one subscriber queue, so a replay can never allocate freely. */
#define TDX_HUB_QUEUE_MAX 65536

/* Asks for exactly the universe indices the poller decided are wanted and due.
 * out[i] must correspond to indices[i]. */
typedef int (*tdx_hub_fetch_fn)(void *context, const size_t *indices,
                                size_t count, tdx_depth *out, tdx_error *err);

typedef struct tdx_hub_options {
    size_t max_subscribers;
    size_t subscriber_queue_limit;
    int interval_ms;
    int heartbeat_ms;
    /* Cold tier cadence.  0 or <= interval_ms means "no cold step". */
    /* Warm tier cadence.  0 means "auto": three hot intervals, clamped into
     * the ladder.  Non-zero is used as given, after the same clamping. */
    int tier_warm_ms;
    int idle_interval_ms;
    /* Consecutive quiet polls before a security is demoted one tier. */
    int idle_rounds;
} tdx_hub_options;

typedef struct tdx_hub tdx_hub;

void tdx_hub_options_default(tdx_hub_options *options);

/* Copies the universe; the caller keeps ownership of its array. */
int tdx_hub_create(tdx_hub **out, const tdx_code *universe, size_t universe_size,
                   const tdx_hub_options *options, tdx_hub_fetch_fn fetch,
                   void *fetch_context, tdx_error *err);
void tdx_hub_destroy(tdx_hub *hub);

int tdx_hub_start(tdx_hub *hub, tdx_error *err);
/* Asks the poller to stop and wakes every blocked subscriber.  The hub stays
 * alive (status and snapshot still work) until it is destroyed, which lets a
 * server drain in-flight connections before freeing it. */
void tdx_hub_stop(tdx_hub *hub);
int tdx_hub_is_stopping(tdx_hub *hub);
/* Runs one round synchronously.  Used by tests and by single-shot callers. */
int tdx_hub_poll_once(tdx_hub *hub, tdx_error *err);

/* Both return the subscriber id, or 0 on failure. */
uint64_t tdx_hub_subscribe_all(tdx_hub *hub, tdx_error *err);
uint64_t tdx_hub_subscribe_codes(tdx_hub *hub, const tdx_code *codes, size_t count,
                                 tdx_error *err);
int tdx_hub_unsubscribe(tdx_hub *hub, uint64_t id);

/* Pops the next queued event for a subscriber.  out is cleared first; a timeout
 * returns TDX_OK with out->len == 0. */
int tdx_hub_next(tdx_hub *hub, uint64_t id, tdx_buf *out, int timeout_ms,
                 tdx_error *err);

int tdx_hub_status_json(tdx_hub *hub, tdx_buf *out, tdx_error *err);
/* Current stored value for the requested codes. */
int tdx_hub_snapshot_json(tdx_hub *hub, const tdx_code *codes, size_t count,
                          tdx_buf *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_HUB_H */
