/* tdx_hub.h - resident polling hub with per-subscriber change queues.
 *
 * One worker polls wanted and due securities and updates a single
 * last-known-value table. Subscriber queues coalesce pending events by security;
 * each event carries its complete newest record, not a lossless tick history.
 * `changed` describes the latest observation's diff, not the accumulated diff
 * since delivery; readers must replace the complete record after coalescing.
 * N readers cost one poller rather than N. */
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
 * out[i] must correspond to indices[i].
 *
 * batches_out reports how many upstream requests the work became.  The hub decides how
 * much work there is, so the cost of that work belongs in the hub's status - without it
 * there is no way to tell a round that needed one request from one that needed twenty.
 * A fetcher that cannot say should write 0, which reads as "not reported" rather than
 * as a claim of zero. */
typedef int (*tdx_hub_fetch_fn)(void *context, const size_t *indices,
                                size_t count, tdx_depth *out, size_t *batches_out,
                                tdx_error *err);

typedef struct tdx_hub_options {
    size_t max_subscribers;
    /* Queue capacity floor (1..TDX_HUB_QUEUE_MAX); actual capacity is the larger
     * of this value and subscribed securities + 1 heartbeat slot. */
    size_t subscriber_queue_limit;
    /* Hot cadence, 1..600000 milliseconds. */
    int interval_ms;
    int heartbeat_ms;
    /* Warm tier cadence.  0 means "auto": three hot intervals, clamped into
     * the ladder.  Non-zero is used as given, after the same clamping. */
    int tier_warm_ms;
    /* Cold tier cadence.  0 or <= interval_ms means "no cold step". */
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
/* Stops the worker and frees the hub. All external API users, including stop
 * callers and servers borrowing this hub, must have finished before destroy. */
void tdx_hub_destroy(tdx_hub *hub);

int tdx_hub_start(tdx_hub *hub, tdx_error *err);
/* Asks the poller to stop and wakes every blocked subscriber. Waits for any
 * synchronous fetch in progress to return; a fetch callback must be bounded.
 * Concurrent stop calls are allowed. Start/stop are terminal (no restart).
 * The hub stays alive (status and snapshot still work) until destroyed, letting a
 * server drain in-flight connections before freeing it. */
void tdx_hub_stop(tdx_hub *hub);
int tdx_hub_is_stopping(tdx_hub *hub);
/* Runs one round synchronously.  Used by tests and by single-shot callers. */
int tdx_hub_poll_once(tdx_hub *hub, tdx_error *err);

/* Both return the subscriber id, or 0 on failure. At most TDX_HUB_QUEUE_MAX - 1
 * unique securities per subscriber. Pending replay can be replaced by a newer
 * change event for the same security before delivery. */
uint64_t tdx_hub_subscribe_all(tdx_hub *hub, tdx_error *err);
uint64_t tdx_hub_subscribe_codes(tdx_hub *hub, const tdx_code *codes, size_t count,
                                 tdx_error *err);
int tdx_hub_unsubscribe(tdx_hub *hub, uint64_t id);

/* Pops the next queued event for a subscriber.  out is cleared first; a timeout
 * returns TDX_OK with out->len == 0. */
int tdx_hub_next(tdx_hub *hub, uint64_t id, tdx_buf *out, int timeout_ms,
                 tdx_error *err);

/* `polled` counts currently wanted securities, not the last round's due work.
 * `dropped`/`coalesced` sum active subscribers and can decrease on unsubscribe. */
int tdx_hub_status_json(tdx_hub *hub, tdx_buf *out, tdx_error *err);
/* Cached values, without an on-demand upstream fetch. `records` omits missing
 * values; `freshness` includes every requested code in input order. Its
 * observed_at_unix_ms is local observation time after fetch, not exchange time;
 * age_ms is monotonic. Missing values have null timestamps. Stale means older
 * than that security's current poll interval, missing, or stopped; root stale
 * is true if any requested value is stale. Subscriptions can prune other
 * securities. Stop alone keeps queries valid. */
int tdx_hub_snapshot_json(tdx_hub *hub, const tdx_code *codes, size_t count,
                          tdx_buf *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_HUB_H */
