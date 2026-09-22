/* tdx_hub.c - resident poller, shared diff table and per-subscriber queues.
 *
 * The poller never walks the universe blindly.  Each round it builds the list
 * of securities that are both wanted (some subscriber asked for them, or nobody
 * is subscribed at all) and due (their own tier interval has elapsed), asks the
 * injected fetcher for exactly that list, and diffs the result against the
 * shared last-known-value table.
 *
 * Two cadence controls fall out of the same option set (unchanged from the
 * caller's point of view):
 *   interval_ms       hot tier, i.e. the fastest cadence
 *   idle_interval_ms  cold tier; 0 or <= interval_ms collapses the ladder
 *   idle_rounds       consecutive quiet polls before a security is demoted
 * The warm tier sits three hot intervals below cold, clamped into range. */
#include "tdx_hub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tdx_format.h"
#include "tdx_state.h"
#include "tdx_thread.h"

typedef struct hub_subscriber {
    uint64_t id;
    int active;
    uint8_t *filter; /* NULL means "everything in the universe" */
    char **queue;
    size_t *queue_keys;
    size_t *queue_next;
    size_t *queue_previous;
    size_t *pending_slots; /* universe index -> ring slot, SIZE_MAX when absent */
    size_t queue_capacity;
    size_t queue_head;
    size_t queue_tail;
    size_t queue_free;
    size_t queue_count;
    uint64_t delivered;
    uint64_t dropped;
    uint64_t coalesced;
    int failed;
    int64_t last_event_ms;
} hub_subscriber;

struct tdx_hub {
    tdx_code *universe;
    size_t universe_size;
    tdx_hub_options options;
    /* Derived tier ladder, all >= interval_ms. */
    int tier_hot_ms;
    int tier_warm_ms;
    int tier_cold_ms;
    int demote_rounds;

    tdx_hub_fetch_fn fetch;
    void *fetch_context;
    /* What the last round cost upstream, as the fetcher reported it. */
    size_t last_fetch_batches;

    tdx_depth *records;
    tdx_state state;

    /* Per security scheduling state. */
    uint8_t *tiers;   /* 0 hot, 1 warm, 2 cold */
    uint32_t *quiet;  /* consecutive polls with no change */
    int64_t *polled;  /* last poll time, ms */
    int64_t *observed_at; /* Unix milliseconds at the last successful observation */
    size_t *due;      /* scratch list for the current round */

    hub_subscriber *subscribers;
    size_t subscriber_slots;
    size_t active_subscribers;
    uint64_t next_subscriber_id;

    tdx_thread poller;
    int poller_started;
    int poller_joining;
    int round_running;
    int stopping;

    tdx_mutex lock;
    tdx_cond cond;

    uint64_t rounds;
    uint64_t failed_rounds;
    uint64_t total_records;
    uint64_t total_events;
    int64_t last_round_ms;
    int64_t next_sequence;
    unsigned consecutive_failures;
    int64_t retry_at;
    uint32_t retry_random;
    char last_error[256];
};

void tdx_hub_options_default(tdx_hub_options *options) {
    if (!options)
        return;
    options->max_subscribers = 16;
    options->subscriber_queue_limit = TDX_HUB_DEFAULT_QUEUE_LIMIT;
    options->interval_ms = TDX_HUB_DEFAULT_INTERVAL_MS;
    options->idle_interval_ms = 0;
    options->tier_warm_ms = 0;
    options->idle_rounds = 30;
    options->heartbeat_ms = TDX_HUB_DEFAULT_HEARTBEAT_MS;
}

/* ------------------------------------------------------------------ */
/* queue                                                               */
/* ------------------------------------------------------------------ */

static void queue_destroy(hub_subscriber *subscriber) {
    size_t index;
    if (!subscriber)
        return;
    if (subscriber->queue)
        for (index = 0; index < subscriber->queue_capacity; ++index)
            free(subscriber->queue[index]);
    free(subscriber->queue);
    free(subscriber->queue_keys);
    free(subscriber->queue_next);
    free(subscriber->queue_previous);
    free(subscriber->pending_slots);
    subscriber->queue = NULL;
    subscriber->queue_keys = NULL;
    subscriber->queue_next = NULL;
    subscriber->queue_previous = NULL;
    subscriber->pending_slots = NULL;
    subscriber->queue_capacity = 0;
    subscriber->queue_head = SIZE_MAX;
    subscriber->queue_tail = SIZE_MAX;
    subscriber->queue_free = SIZE_MAX;
    subscriber->queue_count = 0;
}

/* Keep at most one complete, newest value for each security. The extra key is
 * reserved for a heartbeat, so heartbeat traffic cannot evict a quiet security.
 * A failed allocation terminates this subscription explicitly; reconnecting
 * replays the current state instead of silently leaving a reader stale. */
static int queue_push(hub_subscriber *subscriber, size_t key,
                      const char *data, size_t size) {
    char *copy;
    size_t tail;

    if (subscriber->failed)
        return TDX_ERR;
    copy = (char *)malloc(size + 1);
    if (!copy) {
        subscriber->failed = 1;
        subscriber->dropped++;
        return TDX_ERR;
    }
    memcpy(copy, data, size);
    copy[size] = '\0';

    tail = subscriber->pending_slots[key];
    if (tail != SIZE_MAX) {
        free(subscriber->queue[tail]);
        subscriber->queue[tail] = copy;
        subscriber->coalesced++;
        /* Newer events move to the back. Quiet securities stay ahead and
         * delivery preserves increasing source sequence numbers. */
        if (tail != subscriber->queue_tail) {
            size_t previous = subscriber->queue_previous[tail];
            size_t next = subscriber->queue_next[tail];
            if (previous == SIZE_MAX)
                subscriber->queue_head = next;
            else
                subscriber->queue_next[previous] = next;
            subscriber->queue_previous[next] = previous;
            subscriber->queue_next[subscriber->queue_tail] = tail;
            subscriber->queue_previous[tail] = subscriber->queue_tail;
            subscriber->queue_next[tail] = SIZE_MAX;
            subscriber->queue_tail = tail;
        }
        return TDX_OK;
    }
    if (subscriber->queue_count >= subscriber->queue_capacity) {
        free(copy);
        subscriber->failed = 1;
        subscriber->dropped++;
        return TDX_ERR;
    }
    tail = subscriber->queue_free;
    subscriber->queue_free = subscriber->queue_next[tail];
    subscriber->queue[tail] = copy;
    subscriber->queue_keys[tail] = key;
    subscriber->pending_slots[key] = tail;
    subscriber->queue_next[tail] = SIZE_MAX;
    subscriber->queue_previous[tail] = subscriber->queue_tail;
    if (subscriber->queue_tail == SIZE_MAX)
        subscriber->queue_head = tail;
    else
        subscriber->queue_next[subscriber->queue_tail] = tail;
    subscriber->queue_tail = tail;
    subscriber->queue_count++;
    subscriber->delivered++;
    return TDX_OK;
}

static char *queue_pop(hub_subscriber *subscriber) {
    char *entry;
    size_t head;
    if (subscriber->queue_count == 0)
        return NULL;
    head = subscriber->queue_head;
    entry = subscriber->queue[head];
    subscriber->pending_slots[subscriber->queue_keys[head]] = SIZE_MAX;
    subscriber->queue[head] = NULL;
    subscriber->queue_head = subscriber->queue_next[head];
    if (subscriber->queue_head == SIZE_MAX)
        subscriber->queue_tail = SIZE_MAX;
    else
        subscriber->queue_previous[subscriber->queue_head] = SIZE_MAX;
    subscriber->queue_next[head] = subscriber->queue_free;
    subscriber->queue_free = head;
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

/* Pruning: a security nobody subscribed to is not polled at all.  With no
 * subscribers the whole universe stays warm so /snapshot keeps working. */
static int any_subscriber_wants(const tdx_hub *hub, size_t index) {
    size_t slot;
    if (hub->active_subscribers == 0)
        return 1;
    for (slot = 0; slot < hub->subscriber_slots; ++slot)
        if (hub->subscribers[slot].active && wants(&hub->subscribers[slot], index))
            return 1;
    return 0;
}

static int tier_interval_ms(const tdx_hub *hub, uint8_t tier) {
    switch (tier) {
    case 0:
        return hub->tier_hot_ms;
    case 1:
        return hub->tier_warm_ms;
    default:
        return hub->tier_cold_ms;
    }
}

/* Sleeps in short slices so shutdown stays responsive. */
static void hub_sleep(tdx_hub *hub, int milliseconds) {
    tdx_mutex_lock(&hub->lock);
    if (!hub->stopping && milliseconds > 0)
        tdx_cond_wait(&hub->cond, &hub->lock, milliseconds);
    tdx_mutex_unlock(&hub->lock);
}

/* ------------------------------------------------------------------ */
/* scheduling                                                          */
/* ------------------------------------------------------------------ */

/* Fills hub->due with the wanted securities whose cadence has elapsed. */
static size_t hub_build_due(tdx_hub *hub, int64_t now, int force_all) {
    size_t count = 0;
    size_t index;
    for (index = 0; index < hub->universe_size; ++index) {
        if (!any_subscriber_wants(hub, index))
            continue;
        if (!force_all &&
            now - hub->polled[index] < tier_interval_ms(hub, hub->tiers[index]))
            continue;
        hub->due[count++] = index;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* publish                                                             */
/* ------------------------------------------------------------------ */

static void hub_publish_locked(tdx_hub *hub, const size_t *indices, size_t count) {
    tdx_buf scratch;
    tdx_error error;
    size_t position;
    int64_t now = tdx_monotonic_ms();
    struct timespec wall;
    int64_t observed_at = timespec_get(&wall, TIME_UTC) == TIME_UTC
                              ? (int64_t)wall.tv_sec * 1000 + wall.tv_nsec / 1000000
                              : 0;

    tdx_buf_init(&scratch);
    error.message[0] = '\0';
    hub->rounds++;
    hub->total_records += count;

    for (position = 0; position < count; ++position) {
        const size_t index = indices[position];
        tdx_diff_mask mask = 0;
        const tdx_state_entry *entry = NULL;
        size_t slot;

        if (tdx_state_apply(&hub->state, &hub->records[position], &mask, &entry,
                            &error) != TDX_OK) {
            snprintf(hub->last_error, sizeof(hub->last_error), "%s", error.message);
            break;
        }
        hub->polled[index] = now;
        hub->observed_at[index] = observed_at;
        if (mask == 0) {
            /* Quiet: walk one step down the ladder when a step is due. */
            if (hub->quiet[index] < UINT32_MAX)
                hub->quiet[index]++;
            if (hub->demote_rounds > 0 &&
                hub->quiet[index] >= (uint32_t)hub->demote_rounds) {
                if (hub->tiers[index] < 2)
                    hub->tiers[index]++;
                hub->quiet[index] = 0;
            }
            continue;
        }
        /* Active: straight back to the hot tier. */
        hub->tiers[index] = 0;
        hub->quiet[index] = 0;
        hub->total_events++;

        tdx_buf_clear(&scratch);
        if (tdx_format_depth_event(&scratch, (mask & TDX_DIFF_NEW) ? "snapshot" : "change",
                                   ++hub->next_sequence, &hub->records[position], mask,
                                   entry ? entry->updates : 0, &error) != TDX_OK) {
            snprintf(hub->last_error, sizeof(hub->last_error), "%s", error.message);
            continue;
        }
        for (slot = 0; slot < hub->subscriber_slots; ++slot) {
            hub_subscriber *subscriber = &hub->subscribers[slot];
            if (!subscriber->active || !wants(subscriber, index))
                continue;
            if (queue_push(subscriber, index, (const char *)scratch.data,
                           scratch.len) != TDX_OK)
                snprintf(hub->last_error, sizeof(hub->last_error),
                         "subscriber queue failed; reconnect required");
        }
    }

    for (position = 0; position < hub->subscriber_slots; ++position) {
        hub_subscriber *subscriber = &hub->subscribers[position];
        if (!subscriber->active || hub->options.heartbeat_ms <= 0)
            continue;
        if (subscriber->last_event_ms == 0)
            subscriber->last_event_ms = now;
        if (now - subscriber->last_event_ms < hub->options.heartbeat_ms)
            continue;
        tdx_buf_clear(&scratch);
        if (tdx_format_heartbeat_event(&scratch, ++hub->next_sequence,
                                       hub->universe_size, hub->total_events,
                                       &error) == TDX_OK) {
            (void)queue_push(subscriber, hub->universe_size,
                             (const char *)scratch.data, scratch.len);
            subscriber->last_event_ms = now;
        }
    }
    tdx_buf_free(&scratch);
}

/* ------------------------------------------------------------------ */
/* poller                                                              */
/* ------------------------------------------------------------------ */

static int hub_run_round(tdx_hub *hub, int force_all, int *stopped, tdx_error *err) {
    size_t count;
    size_t fetch_batches = 0;
    int64_t started = tdx_monotonic_ms();
    int result;

    *stopped = 0;
    tdx_mutex_lock(&hub->lock);
    if (hub->stopping) {
        *stopped = 1;
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    if (hub->round_running) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "a hub round is already running");
        return TDX_ERR;
    }
    if (!force_all && started < hub->retry_at) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_OK;
    }
    count = hub_build_due(hub, started, force_all);
    if (count)
        hub->round_running = 1;
    tdx_mutex_unlock(&hub->lock);
    if (count == 0)
        return TDX_OK; /* nothing due in this tick */

    result = hub->fetch(hub->fetch_context, hub->due, count, hub->records,
                        &fetch_batches, err);

    tdx_mutex_lock(&hub->lock);
    hub->last_fetch_batches = fetch_batches;
    hub->last_round_ms = tdx_monotonic_ms() - started;
    hub->round_running = 0;
    tdx_cond_broadcast(&hub->cond);
    if (result != TDX_OK) {
        unsigned shift;
        int delay;
        hub->failed_rounds++;
        if (hub->consecutive_failures < 16)
            hub->consecutive_failures++;
        shift = hub->consecutive_failures - 1;
        delay = shift < 9 ? (100 << shift) : 30000;
        if (delay > 30000)
            delay = 30000;
        /* Instance-local jitter; no shared rand() state or network retry storm. */
        hub->retry_random ^= hub->retry_random << 13;
        hub->retry_random ^= hub->retry_random >> 17;
        hub->retry_random ^= hub->retry_random << 5;
        delay -= (int)(hub->retry_random % (unsigned)(delay / 5 + 1));
        hub->retry_at = tdx_monotonic_ms() + delay;
        snprintf(hub->last_error, sizeof(hub->last_error), "%s",
                 err ? err->message : "fetch failed");
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    hub->consecutive_failures = 0;
    hub->retry_at = 0;
    hub->last_error[0] = '\0';
    if (hub->stopping) {
        *stopped = 1;
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    hub_publish_locked(hub, hub->due, count);
    tdx_cond_broadcast(&hub->cond);
    tdx_mutex_unlock(&hub->lock);
    return TDX_OK;
}

/* Sleeps until the earliest wanted security becomes due again, but never
 * longer than one hot interval so a new subscriber is noticed promptly. */
static void hub_sleep_until_due(tdx_hub *hub) {
    const int64_t now = tdx_monotonic_ms();
    int64_t earliest = -1;
    int wait_ms;
    size_t index;

    tdx_mutex_lock(&hub->lock);
    if (hub->retry_at > now) {
        wait_ms = (int)(hub->retry_at - now);
        tdx_mutex_unlock(&hub->lock);
        hub_sleep(hub, wait_ms);
        return;
    }
    for (index = 0; index < hub->universe_size; ++index) {
        int64_t due_at;
        if (!any_subscriber_wants(hub, index))
            continue;
        due_at = hub->polled[index] + tier_interval_ms(hub, hub->tiers[index]);
        if (earliest < 0 || due_at < earliest)
            earliest = due_at;
    }
    tdx_mutex_unlock(&hub->lock);

    if (earliest < 0) {
        wait_ms = hub->tier_hot_ms;
    } else {
        int64_t delta = earliest - now;
        if (delta > hub->tier_hot_ms)
            delta = hub->tier_hot_ms;
        wait_ms = delta > 0 ? (int)delta : 0;
    }
    hub_sleep(hub, wait_ms);
}

static void hub_poller_main(void *context) {
    tdx_hub *hub = (tdx_hub *)context;
    for (;;) {
        tdx_error error;
        int stopped = 0;
        error.message[0] = '\0';
        if (hub_run_round(hub, 0, &stopped, &error) != TDX_OK && stopped)
            break;
        if (stopped)
            break;
        hub_sleep_until_due(hub);
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
    if (options->subscriber_queue_limit < 1 ||
        options->subscriber_queue_limit > TDX_HUB_QUEUE_MAX) {
        tdx_error_set(err, "hub queue limit must be in 1..%d", TDX_HUB_QUEUE_MAX);
        return TDX_ERR;
    }
    if (options->interval_ms < 1 || options->interval_ms > 600000) {
        tdx_error_set(err, "hub interval must be in 1..600000 ms");
        return TDX_ERR;
    }
    if (options->tier_warm_ms < 0 || options->tier_warm_ms > 600000) {
        tdx_error_set(err, "hub warm interval must be in 0..600000 ms");
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

    hub = (tdx_hub *)calloc(1, sizeof(*hub));
    if (!hub) {
        tdx_error_set(err, "out of memory for the hub");
        return TDX_ERR;
    }
    hub->options = *options;
    /* Derive the ladder.  cold collapses onto hot when unset, and warm is three
     * hot intervals below cold, clamped into range. */
    hub->tier_hot_ms = options->interval_ms;
    hub->tier_cold_ms = options->idle_interval_ms > hub->tier_hot_ms
                            ? options->idle_interval_ms
                            : hub->tier_hot_ms;
    /* Explicit warm wins; 0 means the automatic three-hot-interval step. */
    hub->tier_warm_ms = options->tier_warm_ms > 0 ? options->tier_warm_ms
                                                   : hub->tier_hot_ms * 3;
    if (hub->tier_warm_ms > hub->tier_cold_ms)
        hub->tier_warm_ms = hub->tier_cold_ms;
    if (hub->tier_warm_ms < hub->tier_hot_ms)
        hub->tier_warm_ms = hub->tier_hot_ms;
    hub->demote_rounds = options->idle_rounds;

    hub->fetch = fetch;
    hub->fetch_context = fetch_context;
    hub->universe_size = universe_size;
    hub->universe = (tdx_code *)calloc(universe_size, sizeof(*hub->universe));
    hub->records = (tdx_depth *)calloc(universe_size, sizeof(*hub->records));
    hub->tiers = (uint8_t *)calloc(universe_size, sizeof(*hub->tiers));
    hub->quiet = (uint32_t *)calloc(universe_size, sizeof(*hub->quiet));
    hub->polled = (int64_t *)calloc(universe_size, sizeof(*hub->polled));
    hub->observed_at = (int64_t *)calloc(universe_size, sizeof(*hub->observed_at));
    hub->due = (size_t *)calloc(universe_size, sizeof(*hub->due));
    hub->subscribers = (hub_subscriber *)calloc(options->max_subscribers,
                                                sizeof(*hub->subscribers));
    hub->subscriber_slots = options->max_subscribers;
    hub->next_subscriber_id = 1;
    hub->retry_random = (uint32_t)tdx_monotonic_ms() ^ (uint32_t)(uintptr_t)hub;
    if (hub->retry_random == 0)
        hub->retry_random = 1;

    if (!hub->universe || !hub->records || !hub->tiers || !hub->quiet ||
        !hub->polled || !hub->observed_at || !hub->due || !hub->subscribers) {
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
        tdx_hub_destroy(hub);
        return TDX_ERR;
    }
    if (tdx_state_init(&hub->state, universe_size, err) != TDX_OK) {
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
    tdx_hub_stop(hub);
    if (hub->subscribers)
        for (index = 0; index < hub->subscriber_slots; ++index) {
            queue_destroy(&hub->subscribers[index]);
            free(hub->subscribers[index].filter);
        }
    free(hub->subscribers);
    tdx_state_free(&hub->state);
    tdx_cond_destroy(&hub->cond);
    tdx_mutex_destroy(&hub->lock);
    free(hub->due);
    free(hub->polled);
    free(hub->observed_at);
    free(hub->quiet);
    free(hub->tiers);
    free(hub->records);
    free(hub->universe);
    free(hub);
}

int tdx_hub_start(tdx_hub *hub, tdx_error *err) {
    if (!hub) {
        tdx_error_set(err, "hub is null");
        return TDX_ERR;
    }
    tdx_mutex_lock(&hub->lock);
    if (hub->stopping || hub->round_running) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "hub is stopped or a synchronous round is running");
        return TDX_ERR;
    }
    if (hub->poller_started) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_OK;
    }
    if (tdx_thread_start(&hub->poller, hub_poller_main, hub, err) != TDX_OK) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    hub->poller_started = 1;
    tdx_mutex_unlock(&hub->lock);
    return TDX_OK;
}

void tdx_hub_stop(tdx_hub *hub) {
    if (!hub || !tdx_mutex_is_initialized(&hub->lock))
        return;
    tdx_mutex_lock(&hub->lock);
    hub->stopping = 1;
    tdx_cond_broadcast(&hub->cond);
    while (hub->poller_joining)
        tdx_cond_wait(&hub->cond, &hub->lock, -1);
    if (hub->poller_started) {
        hub->poller_joining = 1;
        tdx_mutex_unlock(&hub->lock);
        tdx_thread_join(&hub->poller);
        tdx_mutex_lock(&hub->lock);
        hub->poller_started = 0;
        hub->poller_joining = 0;
        tdx_cond_broadcast(&hub->cond);
    }
    while (hub->round_running)
        tdx_cond_wait(&hub->cond, &hub->lock, -1);
    tdx_mutex_unlock(&hub->lock);
}

int tdx_hub_is_stopping(tdx_hub *hub) {
    int stopping;
    if (!hub)
        return 1;
    tdx_mutex_lock(&hub->lock);
    stopping = hub->stopping;
    tdx_mutex_unlock(&hub->lock);
    return stopping;
}

int tdx_hub_poll_once(tdx_hub *hub, tdx_error *err) {
    int stopped = 0;
    if (!hub) {
        tdx_error_set(err, "hub is null");
        return TDX_ERR;
    }
    tdx_mutex_lock(&hub->lock);
    if (hub->poller_started) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "hub is already running its own poller");
        return TDX_ERR;
    }
    tdx_mutex_unlock(&hub->lock);
    /* Forced: a synchronous caller wants a complete round now, not the subset
     * the tier clock happens to consider due. */
    return hub_run_round(hub, 1, &stopped, err);
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
        if (queue_push(subscriber, index, (const char *)scratch.data,
                       scratch.len) != TDX_OK)
            break;
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
    if (hub->stopping) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "the hub is stopping");
        return 0;
    }
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

    /* One newest record per subscribed security, plus one heartbeat slot. */
    wanted = filter_popcount(subscriber->filter, hub->universe_size);
    if (wanted >= TDX_HUB_QUEUE_MAX || hub->universe_size == SIZE_MAX) {
        free(subscriber->filter);
        subscriber->filter = NULL;
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "subscription exceeds the bounded latest-value queue");
        return 0;
    }
    capacity = wanted + 1 > hub->options.subscriber_queue_limit
                   ? wanted + 1
                   : hub->options.subscriber_queue_limit;
    subscriber->queue = (char **)calloc(capacity, sizeof(char *));
    subscriber->queue_keys = (size_t *)calloc(capacity, sizeof(size_t));
    subscriber->queue_next = (size_t *)calloc(capacity, sizeof(size_t));
    subscriber->queue_previous = (size_t *)calloc(capacity, sizeof(size_t));
    subscriber->pending_slots = (size_t *)calloc(hub->universe_size + 1,
                                                sizeof(size_t));
    if (!subscriber->queue || !subscriber->queue_keys || !subscriber->queue_next ||
        !subscriber->queue_previous || !subscriber->pending_slots) {
        queue_destroy(subscriber);
        free(subscriber->filter);
        subscriber->filter = NULL;
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "out of memory for a subscriber queue");
        return 0;
    }
    subscriber->queue_capacity = capacity;
    subscriber->queue_head = SIZE_MAX;
    subscriber->queue_tail = SIZE_MAX;
    subscriber->queue_free = 0;
    for (slot = 0; slot < capacity; ++slot)
        subscriber->queue_next[slot] = slot + 1 == capacity ? SIZE_MAX : slot + 1;
    for (slot = 0; slot <= hub->universe_size; ++slot)
        subscriber->pending_slots[slot] = SIZE_MAX;

    subscriber->id = hub->next_subscriber_id++;
    subscriber->active = 1;
    subscriber->last_event_ms = tdx_monotonic_ms();
    hub->active_subscribers++;
    subscriber_replay(hub, subscriber);
    if (subscriber->failed) {
        queue_destroy(subscriber);
        free(subscriber->filter);
        memset(subscriber, 0, sizeof(*subscriber));
        hub->active_subscribers--;
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "cannot enqueue the initial snapshot");
        return 0;
    }
    {
        uint64_t id = subscriber->id;
        tdx_cond_broadcast(&hub->cond);
        tdx_mutex_unlock(&hub->lock);
        return id;
    }
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
        while (subscriber->queue_count == 0 && !subscriber->failed && !hub->stopping) {
            int64_t remaining = deadline - tdx_monotonic_ms();
            if (remaining <= 0)
                break;
            tdx_cond_wait(&hub->cond, &hub->lock, (int)remaining);
            subscriber = find_subscriber(hub, id);
            if (!subscriber) {
                tdx_mutex_unlock(&hub->lock);
                tdx_error_set(err, "subscriber was cancelled while waiting");
                return TDX_ERR;
            }
        }
    }
    if (subscriber->failed) {
        tdx_mutex_unlock(&hub->lock);
        tdx_error_set(err, "subscriber queue failed; reconnect for a fresh snapshot");
        return TDX_ERR;
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
    uint64_t coalesced = 0;
    size_t tier_hot = 0;
    size_t tier_warm = 0;
    size_t tier_cold = 0;
    size_t polled_now = 0;
    int effective = 0;

    if (!hub || !out) {
        tdx_error_set(err, "hub status needs a hub and an output buffer");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    tdx_mutex_lock(&hub->lock);
    for (index = 0; index < hub->universe_size; ++index) {
        int interval;
        if (!any_subscriber_wants(hub, index))
            continue;
        polled_now++;
        switch (hub->tiers[index]) {
        case 0:
            tier_hot++;
            break;
        case 1:
            tier_warm++;
            break;
        default:
            tier_cold++;
            break;
        }
        interval = tier_interval_ms(hub, hub->tiers[index]);
        if (effective == 0 || interval < effective)
            effective = interval;
    }
    if (effective == 0)
        effective = hub->tier_hot_ms;
    for (index = 0; index < hub->subscriber_slots; ++index) {
        if (!hub->subscribers[index].active)
            continue;
        pending += hub->subscribers[index].queue_count;
        dropped += (size_t)hub->subscribers[index].dropped;
        coalesced += hub->subscribers[index].coalesced;
    }
    if (tdx_buf_append_printf(out, err,
                              "{\"schema\":\"tdx-l1-hub-status-v1\","
                              "\"universe\":%zu,\"polled\":%zu,\"rounds\":%llu,"
                              "\"failed_rounds\":%llu,\"records\":%llu,"
                              "\"events\":%llu,\"last_round_ms\":%lld,"
                              "\"interval_ms\":%d,\"effective_interval_ms\":%d,"
                              "\"tier_warm_ms\":%d,\"tier_cold_ms\":%d,"
                              "\"demote_rounds\":%d,\"tier_hot\":%zu,"
                              "\"tier_warm\":%zu,\"tier_cold\":%zu,"
                              "\"heartbeat_ms\":%d,\"subscribers\":%zu,"
                              "\"subscriber_slots\":%zu,\"queued\":%zu,"
                              "\"dropped\":%zu,\"sequence\":%lld,"
                              "\"last_round_batches\":%zu,\"coalesced\":%llu,"
                              "\"consecutive_failures\":%u,\"retry_in_ms\":%lld,"
                              "\"last_error\":",
                              hub->universe_size, polled_now,
                              (unsigned long long)hub->rounds,
                              (unsigned long long)hub->failed_rounds,
                              (unsigned long long)hub->total_records,
                              (unsigned long long)hub->total_events,
                              (long long)hub->last_round_ms, hub->tier_hot_ms,
                              effective, hub->tier_warm_ms, hub->tier_cold_ms,
                              hub->demote_rounds, tier_hot, tier_warm, tier_cold,
                              hub->options.heartbeat_ms, hub->active_subscribers,
                              hub->subscriber_slots, pending, dropped,
                              (long long)hub->next_sequence,
                              hub->last_fetch_batches,
                              (unsigned long long)coalesced,
                              hub->consecutive_failures,
                              (long long)(hub->retry_at > tdx_monotonic_ms()
                                  ? hub->retry_at - tdx_monotonic_ms() : 0)) != TDX_OK) {
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
    int any_stale = 0;
    int64_t now;

    if (!hub || !out || (!codes && count > 0)) {
        tdx_error_set(err, "snapshot needs a hub, codes and an output buffer");
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    if (tdx_buf_append_printf(out, err, "{\"requested\":%zu,\"records\":[", count) !=
        TDX_OK)
        return TDX_ERR;
    tdx_mutex_lock(&hub->lock);
    now = tdx_monotonic_ms();
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
    if (tdx_buf_append(out, "],\"freshness\":[", 15, err) != TDX_OK) {
        tdx_mutex_unlock(&hub->lock);
        return TDX_ERR;
    }
    for (index = 0; index < count; ++index) {
        int slot = universe_index(hub, &codes[index]);
        const tdx_state_entry *entry = tdx_state_find(&hub->state, &codes[index]);
        char id[16];
        int stale;
        int64_t age = slot >= 0 && entry ? now - hub->polled[slot] : -1;
        if (age < 0 && entry)
            age = 0;
        stale = !entry || slot < 0 || hub->stopping ||
                age > tier_interval_ms(hub, hub->tiers[slot]);
        any_stale |= stale;
        tdx_code_id(&codes[index], id, sizeof(id));
        if ((index && tdx_buf_push(out, ',', err) != TDX_OK) ||
            tdx_buf_append(out, "{\"security_id\":", 15, err) != TDX_OK ||
            tdx_format_json_string(out, id, err) != TDX_OK ||
            tdx_buf_append_printf(out, err, ",\"stale\":%s,\"observed_at_unix_ms\":",
                                  stale ? "true" : "false") != TDX_OK) {
            tdx_mutex_unlock(&hub->lock);
            return TDX_ERR;
        }
        if (slot < 0 || !entry) {
            if (tdx_buf_append(out, "null,\"age_ms\":null}", 19, err) != TDX_OK) {
                tdx_mutex_unlock(&hub->lock);
                return TDX_ERR;
            }
        } else if (tdx_buf_append_printf(out, err, "%lld,\"age_ms\":%lld}",
                                         (long long)hub->observed_at[slot],
                                         (long long)age) != TDX_OK) {
            tdx_mutex_unlock(&hub->lock);
            return TDX_ERR;
        }
    }
    tdx_mutex_unlock(&hub->lock);
    return tdx_buf_append_printf(out, err, "],\"stale\":%s}",
                                 any_stale ? "true" : "false");
}
