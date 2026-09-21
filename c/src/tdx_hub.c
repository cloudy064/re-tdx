/* tdx_hub.c - resident poller, shared diff table and per-subscriber queues. */
#include "tdx_hub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_format.h"
#include "tdx_state.h"
#include "tdx_thread.h"

typedef struct hub_subscriber {
    uint64_t id;
    int active;
    uint8_t *filter; /* NULL means "everything in the universe" */
    char **queue;
    size_t queue_capacity;
    size_t queue_head;
    size_t queue_count;
    uint64_t delivered;
    uint64_t dropped;
    int64_t last_event_ms;
} hub_subscriber;

struct tdx_hub {
    tdx_code *universe;
    size_t universe_size;
    tdx_hub_options options;
    tdx_hub_fetch_fn fetch;
    void *fetch_context;

    tdx_depth *records;
    tdx_state state;

    hub_subscriber *subscribers;
    size_t subscriber_slots;
    size_t active_subscribers;
    uint64_t next_subscriber_id;

    tdx_thread poller;
    int poller_started;
    int stopping;

    tdx_mutex lock;
    tdx_cond cond;

    uint64_t rounds;
    uint64_t quiet_rounds;
    size_t last_round_events;
    uint64_t failed_rounds;
    uint64_t total_records;
    uint64_t total_events;
    int64_t last_round_ms;
    int64_t next_sequence;
    char last_error[256];
};

void tdx_hub_options_default(tdx_hub_options *options) {
    if (!options)
        return;
    options->max_subscribers = 16;
    options->subscriber_queue_limit = TDX_HUB_DEFAULT_QUEUE_LIMIT;
    options->interval_ms = TDX_HUB_DEFAULT_INTERVAL_MS;
    options->heartbeat_ms = TDX_HUB_DEFAULT_HEARTBEAT_MS;
    options->idle_interval_ms = 0;
    options->idle_rounds = 0;
}

/* ------------------------------------------------------------------ */
/* queue                                                               */
/* ------------------------------------------------------------------ */

static void queue_destroy(hub_subscriber *subscriber) {
    size_t index;
    if (!subscriber->queue)
        return;
    for (index = 0; index < subscriber->queue_capacity; ++index)
        free(subscriber->queue[index]);
    free(subscriber->queue);
    subscriber->queue = NULL;
    subscriber->queue_capacity = 0;
    subscriber->queue_head = 0;
    subscriber->queue_count = 0;
}

/* Copies one event onto the subscriber queue.  A full queue drops its oldest
 * entry rather than growing without bound or blocking the poller. */
static void queue_push(hub_subscriber *subscriber, const char *data, size_t size) {
    char *copy;
    size_t tail;

    if (subscriber->queue_capacity == 0)
        return;
    copy = (char *)malloc(size + 1);
    if (!copy)
        return;
    memcpy(copy, data, size);
    copy[size] = '\0';

    if (subscriber->queue_count == subscriber->queue_capacity) {
        free(subscriber->queue[subscriber->queue_head]);
        subscriber->queue[subscriber->queue_head] = NULL;
        subscriber->queue_head = (subscriber->queue_head + 1) % subscriber->queue_capacity;
        subscriber->queue_count--;
        subscriber->dropped++;
    }
    tail = (subscriber->queue_head + subscriber->queue_count) % subscriber->queue_capacity;
    subscriber->queue[tail] = copy;
    subscriber->queue_count++;
    subscriber->delivered++;
}

static char *queue_pop(hub_subscriber *subscriber) {
    char *entry;
    if (subscriber->queue_count == 0)
        return NULL;
    entry = subscriber->queue[subscriber->queue_head];
    subscriber->queue[subscriber->queue_head] = NULL;
    subscriber->queue_head = (subscriber->queue_head + 1) % subscriber->queue_capacity;
    subscriber->queue_count--;
    return entry;
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static int key_equal(const tdx_code *left, const tdx_code *right) {
    return left->market_id == right->market_id &&
           memcmp(left->code, right->code, 6) == 0;
}

static int universe_index(const tdx_hub *hub, const tdx_code *code) {
    size_t index;
    for (index = 0; index < hub->universe_size; ++index)
        if (key_equal(&hub->universe[index], code))
            return (int)index;
    return -1;
}

static hub_subscriber *find_subscriber(tdx_hub *hub, uint64_t id) {
    size_t index;
    if (id == 0)
        return NULL;
    for (index = 0; index < hub->subscriber_slots; ++index)
        if (hub->subscribers[index].active && hub->subscribers[index].id == id)
            return &hub->subscribers[index];
    return NULL;
}

static int wants(const hub_subscriber *subscriber, size_t index) {
    if (!subscriber->filter)
        return 1;
    return (subscriber->filter[index >> 3] & (uint8_t)(1u << (index & 7))) != 0;
}

/* Sleeps in short slices so shutdown stays responsive. */
static void hub_sleep(tdx_hub *hub, int milliseconds) {
    int remaining = milliseconds;
    while (remaining > 0) {
        int slice = remaining > 50 ? 50 : remaining;
        tdx_sleep_ms(slice);
        remaining -= slice;
        tdx_mutex_lock(&hub->lock);
        if (hub->stopping) {
            tdx_mutex_unlock(&hub->lock);
            return;
        }
        tdx_mutex_unlock(&hub->lock);
    }
}

/* ------------------------------------------------------------------ */
/* publish                                                             */
/* ------------------------------------------------------------------ */

static void hub_publish_locked(tdx_hub *hub, size_t count) {
    tdx_buf scratch;
    tdx_error error;
    size_t index;
    int64_t now;

    tdx_buf_init(&scratch);
    error.message[0] = '\0';
    size_t published = 0;

    hub->rounds++;
    hub->total_records += count;

    for (index = 0; index < count; ++index) {
        tdx_diff_mask mask = 0;
        const tdx_state_entry *entry = NULL;
        size_t slot;

        if (tdx_state_apply(&hub->state, &hub->records[index], &mask, &entry,
                            &error) != TDX_OK) {
            snprintf(hub->last_error, sizeof(hub->last_error), "%s", error.message);
            break;
        }
        if (mask == 0)
            continue;
        hub->total_events++;
        published++;
        tdx_buf_clear(&scratch);
        if (tdx_format_depth_event(&scratch, (mask & TDX_DIFF_NEW) ? "snapshot" : "change",
                                   ++hub->next_sequence, &hub->records[index], mask,
                                   entry ? entry->updates : 0, &error) != TDX_OK) {
            snprintf(hub->last_error, sizeof(hub->last_error), "%s", error.message);
            continue;
        }
        for (slot = 0; slot < hub->subscriber_slots; ++slot) {
            hub_subscriber *subscriber = &hub->subscribers[slot];
            if (!subscriber->active || !wants(subscriber, index))
                continue;
            queue_push(subscriber, (const char *)scratch.data, scratch.len);
        }
    }

    hub->last_round_events = published;
    if (published == 0)
        hub->quiet_rounds++;
    else
        hub->quiet_rounds = 0;

    now = tdx_monotonic_ms();
    for (index = 0; index < hub->subscriber_slots; ++index) {
        hub_subscriber *subscriber = &hub->subscribers[index];
        if (!subscriber->active)
            continue;
        if (hub->options.heartbeat_ms <= 0)
            continue;
        if (subscriber->last_event_ms == 0)
            subscriber->last_event_ms = now;
        if (now - subscriber->last_event_ms < hub->options.heartbeat_ms)
            continue;
        tdx_buf_clear(&scratch);
        if (tdx_format_heartbeat_event(&scratch, ++hub->next_sequence,
                                       hub->universe_size, hub->total_events,
                                       &error) == TDX_OK) {
            queue_push(subscriber, (const char *)scratch.data, scratch.len);
            subscriber->last_event_ms = now;
        }
    }
    tdx_buf_free(&scratch);
}

/* ------------------------------------------------------------------ */
/* poller                                                              */
/* ------------------------------------------------------------------ */

static int hub_run_round(tdx_hub *hub, int *stopped, tdx_error *err) {
    size_t count = 0;
    size_t index;
    int64_t started = tdx_monotonic_ms();
    int result;

    *stopped = 0;
    tdx_mutex_lock(&hub->lock);
    if (hub->stopping) {
        *stopped = 1;
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    tdx_mutex_unlock(&hub->lock);

    result = hub->fetch(hub->fetch_context, hub->records, hub->universe_size, &count,
                        err);

    /* A short round must never be silently treated as "nothing changed": pad the
     * remainder with the last known values so the diff stays meaningful, and
     * record the failure. */
    tdx_mutex_lock(&hub->lock);
    hub->last_round_ms = tdx_monotonic_ms() - started;
    if (result != TDX_OK) {
        hub->failed_rounds++;
        snprintf(hub->last_error, sizeof(hub->last_error), "%s", err->message);
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    if (count > hub->universe_size)
        count = hub->universe_size;
    for (index = count; index < hub->universe_size; ++index) {
        const tdx_state_entry *stored = tdx_state_find(&hub->state, &hub->universe[index]);
        if (stored)
            hub->records[index] = stored->depth;
    }
    hub_publish_locked(hub, count);
    tdx_cond_broadcast(&hub->cond);
    tdx_mutex_unlock(&hub->lock);
    return TDX_OK;
}

/* Backs off to the idle cadence once enough consecutive rounds produced no
 * change.  Returns the delay to apply after the round that just finished. */
static int hub_effective_interval_ms(const tdx_hub *hub) {
    int interval = hub->options.interval_ms;
    if (hub->options.idle_interval_ms > interval && hub->options.idle_rounds > 0 &&
        hub->quiet_rounds >= (uint64_t)hub->options.idle_rounds)
        interval = hub->options.idle_interval_ms;
    interval -= (int)hub->last_round_ms;
    return interval > 0 ? interval : 0;
}

static void hub_poller_main(void *context) {
    tdx_hub *hub = (tdx_hub *)context;
    for (;;) {
        tdx_error error;
        int stopped = 0;
        int interval;
        error.message[0] = '\0';
        if (hub_run_round(hub, &stopped, &error) != TDX_OK && stopped)
            break;
        if (stopped)
            break;
        interval = hub_effective_interval_ms(hub);
        hub_sleep(hub, interval > 0 ? interval : 0);
        tdx_mutex_lock(&hub->lock);
        if (hub->stopping) {
            tdx_mutex_unlock(&hub->lock);
            break;
        }
        tdx_mutex_unlock(&hub->lock);
    }
}

/* ------------------------------------------------------------------ */
/* lifecycle                                                           */
/* ------------------------------------------------------------------ */

int tdx_hub_create(tdx_hub **out, const tdx_code *universe, size_t universe_size,
                   const tdx_hub_options *options, tdx_hub_fetch_fn fetch,
                   void *fetch_context, tdx_error *err) {
    tdx_hub_options defaults;
    tdx_hub *hub;

    if (!out || !universe || universe_size == 0 || !fetch) {
        tdx_error_set(err, "hub needs an output slot, a universe and a fetch hook");
        return TDX_ERR;
    }
    tdx_hub_options_default(&defaults);
    if (!options)
        options = &defaults;
    if (options->max_subscribers < 1 ||
        options->max_subscribers > TDX_HUB_MAX_SUBSCRIBERS) {
        tdx_error_set(err, "hub max subscribers must be in 1..%d",
                      TDX_HUB_MAX_SUBSCRIBERS);
        return TDX_ERR;
    }
    if (options->subscriber_queue_limit < 1) {
        tdx_error_set(err, "hub queue limit must be positive");
        return TDX_ERR;
    }
    if (options->idle_interval_ms < 0 || options->idle_interval_ms > 600000) {
        tdx_error_set(err, "hub idle interval must be in 0..600000 ms");
        return TDX_ERR;
    }
    if (options->idle_rounds < 0 || options->idle_rounds > 1000000) {
        tdx_error_set(err, "hub idle rounds must be in 0..1000000");
        return TDX_ERR;
    }
    if (options->interval_ms < 0 || options->interval_ms > 600000) {
        tdx_error_set(err, "hub interval must be in 0..600000 ms");
        return TDX_ERR;
    }

    hub = (tdx_hub *)calloc(1, sizeof(*hub));
    if (!hub) {
        tdx_error_set(err, "out of memory for the hub");
        return TDX_ERR;
    }
    hub->options = *options;
    hub->fetch = fetch;
    hub->fetch_context = fetch_context;
    hub->universe_size = universe_size;
    hub->universe = (tdx_code *)calloc(universe_size, sizeof(*hub->universe));
    hub->records = (tdx_depth *)calloc(universe_size, sizeof(*hub->records));
    hub->subscribers = (hub_subscriber *)calloc(options->max_subscribers,
                                                sizeof(*hub->subscribers));
    hub->subscriber_slots = options->max_subscribers;
    hub->next_subscriber_id = 1;

    if (!hub->universe || !hub->records || !hub->subscribers) {
        tdx_error_set(err, "out of memory for the hub universe");
        tdx_hub_destroy(hub);
        return TDX_ERR;
    }
    memcpy(hub->universe, universe, universe_size * sizeof(*hub->universe));

    if (tdx_mutex_init(&hub->lock, err) != TDX_OK) {
        tdx_hub_destroy(hub);
        return TDX_ERR;
    }
    if (tdx_cond_init(&hub->cond, err) != TDX_OK) {
        tdx_mutex_destroy(&hub->lock);
        tdx_hub_destroy(hub);
        return TDX_ERR;
    }
    if (tdx_state_init(&hub->state, universe_size, err) != TDX_OK) {
        tdx_cond_destroy(&hub->cond);
        tdx_mutex_destroy(&hub->lock);
        tdx_hub_destroy(hub);
        return TDX_ERR;
    }
    *out = hub;
    return TDX_OK;
}

void tdx_hub_destroy(tdx_hub *hub) {
    size_t index;
    if (!hub)
        return;
    if (hub->poller_started) {
        tdx_mutex_lock(&hub->lock);
        hub->stopping = 1;
        tdx_cond_broadcast(&hub->cond);
        tdx_mutex_unlock(&hub->lock);
        tdx_thread_join(&hub->poller);
        hub->poller_started = 0;
    }
    for (index = 0; index < hub->subscriber_slots; ++index)
        queue_destroy(&hub->subscribers[index]);
    free(hub->subscribers);
    tdx_state_free(&hub->state);
    if (hub->lock.native)
        tdx_mutex_destroy(&hub->lock);
    if (hub->cond.native)
        tdx_cond_destroy(&hub->cond);
    free(hub->records);
    free(hub->universe);
    free(hub);
}

int tdx_hub_start(tdx_hub *hub, tdx_error *err) {
    if (!hub) {
        tdx_error_set(err, "hub is null");
        return TDX_ERR;
    }
    if (hub->poller_started)
        return TDX_OK;
    if (tdx_thread_start(&hub->poller, hub_poller_main, hub, err) != TDX_OK)
        return TDX_ERR;
    hub->poller_started = 1;
    return TDX_OK;
}

int tdx_hub_poll_once(tdx_hub *hub, tdx_error *err) {
    int stopped = 0;
    if (!hub) {
        tdx_error_set(err, "hub is null");
        return TDX_ERR;
    }
    if (hub->poller_started) {
        tdx_error_set(err, "hub is already running its own poller");
        return TDX_ERR;
    }
    return hub_run_round(hub, &stopped, err);
}

/* ------------------------------------------------------------------ */
/* subscriptions                                                       */
/* ------------------------------------------------------------------ */

static size_t filter_popcount(const uint8_t *filter, size_t universe_size) {
    size_t count = 0;
    size_t index;
    if (!filter)
        return universe_size;
    for (index = 0; index < universe_size; ++index)
        if (filter[index >> 3] & (uint8_t)(1u << (index & 7)))
            count++;
    return count;
}

/* A late subscriber must not wait for the next change to learn the current
 * state, so the stored values it asked for are replayed immediately. */
static void subscriber_replay(tdx_hub *hub, hub_subscriber *subscriber) {
    tdx_buf scratch;
    tdx_error error;
    size_t index;

    tdx_buf_init(&scratch);
    error.message[0] = '\0';
    for (index = 0; index < hub->universe_size; ++index) {
        const tdx_state_entry *entry;
        if (!wants(subscriber, index))
            continue;
        entry = tdx_state_find(&hub->state, &hub->universe[index]);
        if (!entry)
            continue;
        tdx_buf_clear(&scratch);
        if (tdx_format_depth_event(&scratch, "snapshot", ++hub->next_sequence,
                                   &entry->depth, TDX_DIFF_NEW, entry->updates,
                                   &error) != TDX_OK)
            continue;
        queue_push(subscriber, (const char *)scratch.data, scratch.len);
    }
    tdx_buf_free(&scratch);
}

static uint64_t subscribe_internal(tdx_hub *hub, const tdx_code *codes, size_t count,
                                  int all, tdx_error *err) {
    size_t slot;
    hub_subscriber *subscriber;
    size_t wanted;
    size_t capacity;

    if (!hub) {
        tdx_error_set(err, "hub is null");
        return 0;
    }
    if (!all && (!codes || count == 0)) {
        tdx_error_set(err, "a filtered subscription needs at least one code");
        return 0;
    }
    tdx_mutex_lock(&hub->lock);
    for (slot = 0; slot < hub->subscriber_slots; ++slot)
        if (!hub->subscribers[slot].active)
            break;
    if (slot == hub->subscriber_slots) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "the hub already has %zu subscribers",
                      hub->subscriber_slots);
        return 0;
    }
    subscriber = &hub->subscribers[slot];
    queue_destroy(subscriber);
    memset(subscriber, 0, sizeof(*subscriber));

    if (!all) {
        size_t index;
        size_t bytes = (hub->universe_size + 7) / 8;
        subscriber->filter = (uint8_t *)calloc(bytes ? bytes : 1, 1);
        if (!subscriber->filter) {
            tdx_mutex_unlock(&hub->lock);
            tdx_error_set(err, "out of memory for a subscription filter");
            return 0;
        }
        for (index = 0; index < count; ++index) {
            int found = universe_index(hub, &codes[index]);
            if (found < 0) {
                char id[16];
                tdx_code_id(&codes[index], id, sizeof(id));
                free(subscriber->filter);
                subscriber->filter = NULL;
                tdx_mutex_unlock(&hub->lock);
                tdx_error_set(err, "%s is not in the hub universe", id);
                return 0;
            }
            subscriber->filter[(size_t)found >> 3] |=
                (uint8_t)(1u << ((size_t)found & 7));
        }
    }

    /* The queue always has room for one full snapshot of what this subscriber
     * asked for, so the drop-oldest policy only applies to live changes. */
    wanted = filter_popcount(subscriber->filter, hub->universe_size);
    capacity = wanted > hub->options.subscriber_queue_limit
                   ? wanted
                   : hub->options.subscriber_queue_limit;
    if (capacity > TDX_HUB_QUEUE_MAX)
        capacity = TDX_HUB_QUEUE_MAX;
    subscriber->queue = (char **)calloc(capacity, sizeof(char *));
    if (!subscriber->queue) {
        free(subscriber->filter);
        subscriber->filter = NULL;
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "out of memory for a subscriber queue");
        return 0;
    }
    subscriber->queue_capacity = capacity;

    subscriber->id = hub->next_subscriber_id++;
    subscriber->active = 1;
    subscriber->last_event_ms = tdx_monotonic_ms();
    hub->active_subscribers++;
    subscriber_replay(hub, subscriber);
    tdx_mutex_unlock(&hub->lock);
    return subscriber->id;
}

uint64_t tdx_hub_subscribe_all(tdx_hub *hub, tdx_error *err) {
    return subscribe_internal(hub, NULL, 0, 1, err);
}

uint64_t tdx_hub_subscribe_codes(tdx_hub *hub, const tdx_code *codes, size_t count,
                                 tdx_error *err) {
    return subscribe_internal(hub, codes, count, 0, err);
}

int tdx_hub_unsubscribe(tdx_hub *hub, uint64_t id) {
    hub_subscriber *subscriber;
    if (!hub)
        return TDX_ERR;
    tdx_mutex_lock(&hub->lock);
    subscriber = find_subscriber(hub, id);
    if (!subscriber) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    queue_destroy(subscriber);
    free(subscriber->filter);
    memset(subscriber, 0, sizeof(*subscriber));
    hub->active_subscribers--;
    tdx_cond_broadcast(&hub->cond);
    tdx_mutex_unlock(&hub->lock);
    return TDX_OK;
}

int tdx_hub_next(tdx_hub *hub, uint64_t id, tdx_buf *out, int timeout_ms,
                 tdx_error *err) {
    hub_subscriber *subscriber;
    char *entry = NULL;

    if (!hub || !out) {
        tdx_error_set(err, "hub next needs a hub and an output buffer");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    tdx_mutex_lock(&hub->lock);
    subscriber = find_subscriber(hub, id);
    if (!subscriber) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "unknown subscriber %llu", (unsigned long long)id);
        return TDX_ERR;
    }
    if (subscriber->queue_count == 0 && timeout_ms > 0) {
        const int64_t deadline = tdx_monotonic_ms() + timeout_ms;
        while (subscriber->queue_count == 0 && !hub->stopping) {
            int64_t remaining = deadline - tdx_monotonic_ms();
            if (remaining <= 0)
                break;
            tdx_cond_wait(&hub->cond, &hub->lock, (int)remaining);
        }
    }
    entry = queue_pop(subscriber);
    tdx_mutex_unlock(&hub->lock);

    if (!entry)
        return TDX_OK; /* timeout: an empty buffer is the documented result */
    if (tdx_buf_append(out, entry, strlen(entry), err) != TDX_OK) {
        free(entry);
        return TDX_ERR;
    }
    free(entry);
    return TDX_OK;
}

/* ------------------------------------------------------------------ */
/* introspection                                                       */
/* ------------------------------------------------------------------ */

int tdx_hub_status_json(tdx_hub *hub, tdx_buf *out, tdx_error *err) {
    size_t index;
    size_t pending = 0;
    size_t dropped = 0;

    if (!hub || !out) {
        tdx_error_set(err, "hub status needs a hub and an output buffer");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    tdx_mutex_lock(&hub->lock);
    for (index = 0; index < hub->subscriber_slots; ++index) {
        if (!hub->subscribers[index].active)
            continue;
        pending += hub->subscribers[index].queue_count;
        dropped += (size_t)hub->subscribers[index].dropped;
    }
    if (tdx_buf_append_printf(out, err,
                              "{\"schema\":\"tdx-l1-hub-status-v1\","
                              "\"universe\":%zu,\"rounds\":%llu,\"failed_rounds\":%llu,"
                              "\"records\":%llu,\"events\":%llu,"
                              "\"last_round_ms\":%lld,\"interval_ms\":%d,"
                              "\"effective_interval_ms\":%d,\"quiet_rounds\":%llu,"
                              "\"heartbeat_ms\":%d,\"subscribers\":%zu,"
                              "\"subscriber_slots\":%zu,\"queued\":%zu,"
                              "\"dropped\":%zu,\"sequence\":%lld,"
                              "\"last_error\":",
                              hub->universe_size, (unsigned long long)hub->rounds,
                              (unsigned long long)hub->failed_rounds,
                              (unsigned long long)hub->total_records,
                              (unsigned long long)hub->total_events,
                              (long long)hub->last_round_ms, hub->options.interval_ms,
                              hub_effective_interval_ms(hub) + (int)hub->last_round_ms,
                              (unsigned long long)hub->quiet_rounds,
                              hub->options.heartbeat_ms, hub->active_subscribers,
                              hub->subscriber_slots, pending, dropped,
                              (long long)hub->next_sequence) != TDX_OK) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    if (tdx_format_json_string(out, hub->last_error, err) != TDX_OK) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    tdx_mutex_unlock(&hub->lock);
    return tdx_buf_push(out, '}', err);
}

int tdx_hub_snapshot_json(tdx_hub *hub, const tdx_code *codes, size_t count,
                          tdx_buf *out, tdx_error *err) {
    size_t index;
    int first = 1;

    if (!hub || !out || (!codes && count > 0)) {
        tdx_error_set(err, "snapshot needs a hub, codes and an output buffer");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    if (tdx_buf_append_printf(out, err, "{\"requested\":%zu,\"records\":[", count) !=
        TDX_OK)
        return TDX_ERR;
    tdx_mutex_lock(&hub->lock);
    for (index = 0; index < count; ++index) {
        const tdx_state_entry *entry = tdx_state_find(&hub->state, &codes[index]);
        if (!entry)
            continue;
        if (!first && tdx_buf_push(out, ',', err) != TDX_OK) {
            tdx_mutex_unlock(&hub->lock);
            return TDX_ERR;
        }
        first = 0;
        if (tdx_format_depth(out, &entry->depth, err) != TDX_OK) {
            tdx_mutex_unlock(&hub->lock);
            return TDX_ERR;
        }
    }
    tdx_mutex_unlock(&hub->lock);
    return tdx_buf_append(out, "]}", 2, err);
}
