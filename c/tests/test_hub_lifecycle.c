/* Resident scheduling, cancellation and latest-value delivery without a network. */
#include <stdio.h>
#include <string.h>

#include "tdx_hub.h"
#include "tdx_json.h"
#include "tdx_thread.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, m); failures++; } } while (0)

typedef struct feed {
    tdx_mutex lock;
    tdx_cond cond;
    tdx_code codes[3];
    double prices[3];
    int fail;
    int block;
    int entered;
    int calls;
    int successes;
} feed;

static void feed_init(feed *f) {
    size_t i;
    tdx_error error;
    memset(f, 0, sizeof(*f));
    CHECK(tdx_mutex_init(&f->lock, &error) == TDX_OK, "feed mutex");
    CHECK(tdx_cond_init(&f->cond, &error) == TDX_OK, "feed condition");
    for (i = 0; i < 3; ++i) {
        snprintf(f->codes[i].code, sizeof(f->codes[i].code), "%06u", (unsigned)i + 1);
        f->prices[i] = 10.0 + (double)i;
    }
}

static void feed_free(feed *f) {
    tdx_cond_destroy(&f->cond);
    tdx_mutex_destroy(&f->lock);
}

static int fetch(void *context, const size_t *indices, size_t count,
                 tdx_depth *out, size_t *batches, tdx_error *err) {
    feed *f = (feed *)context;
    size_t i;
    tdx_mutex_lock(&f->lock);
    f->calls++;
    f->entered = 1;
    tdx_cond_broadcast(&f->cond);
    while (f->block)
        tdx_cond_wait(&f->cond, &f->lock, -1);
    *batches = 7;
    if (f->fail) {
        tdx_mutex_unlock(&f->lock);
        tdx_error_set(err, "injected resident failure");
        return TDX_ERR;
    }
    for (i = 0; i < count; ++i) {
        memset(&out[i], 0, sizeof(out[i]));
        out[i].security = f->codes[indices[i]];
        out[i].last = f->prices[indices[i]];
    }
    f->successes++;
    tdx_mutex_unlock(&f->lock);
    return TDX_OK;
}

static int feed_count(feed *f, int successes) {
    int result;
    tdx_mutex_lock(&f->lock);
    result = successes ? f->successes : f->calls;
    tdx_mutex_unlock(&f->lock);
    return result;
}

static int wait_count(feed *f, int successful, int wanted) {
    int64_t deadline = tdx_monotonic_ms() + 3000;
    while (tdx_monotonic_ms() < deadline) {
        if (feed_count(f, successful) >= wanted)
            return 1;
        tdx_sleep_ms(5);
    }
    return 0;
}

static tdx_hub *make_hub(feed *f, int interval) {
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    tdx_hub_options_default(&options);
    options.interval_ms = interval;
    options.heartbeat_ms = 0;
    options.subscriber_queue_limit = 1;
    CHECK(tdx_hub_create(&hub, f->codes, 3, &options, fetch, f, &error) == TDX_OK,
          "hub create");
    return hub;
}

typedef struct reader {
    tdx_hub *hub;
    feed *f;
    int errors;
} reader;

static void read_status(void *context) {
    reader *r = (reader *)context;
    tdx_buf body;
    tdx_error error;
    tdx_json_doc doc;
    int i;
    tdx_buf_init(&body);
    tdx_json_doc_init(&doc);
    for (i = 0; i < 100; ++i) {
        if (tdx_hub_status_json(r->hub, &body, &error) != TDX_OK ||
            tdx_json_parse(body.data, body.len, &doc, &error) != TDX_OK)
            r->errors++;
        if (tdx_hub_snapshot_json(r->hub, r->f->codes, 3, &body, &error) != TDX_OK ||
            tdx_json_parse(body.data, body.len, &doc, &error) != TDX_OK)
            r->errors++;
        tdx_sleep_ms(2);
    }
    tdx_json_doc_free(&doc);
    tdx_buf_free(&body);
}

static void test_backoff_and_recovery(void) {
    feed f;
    tdx_hub *hub;
    tdx_error error;
    tdx_thread reader_thread = {0};
    reader r;
    tdx_buf body;
    tdx_json_doc doc;
    const tdx_json_node *node;
    feed_init(&f);
    f.fail = 1;
    hub = make_hub(&f, 10);
    if (!hub) { feed_free(&f); return; }
    CHECK(tdx_hub_start(hub, &error) == TDX_OK, "resident start");
    CHECK(wait_count(&f, 0, 1), "first failed fetch");
    r.hub = hub; r.f = &f; r.errors = 0;
    CHECK(tdx_thread_start(&reader_thread, read_status, &r, &error) == TDX_OK,
          "concurrent status reader");
    tdx_sleep_ms(240);
    CHECK(feed_count(&f, 0) <= 3, "failed fetches must back off rather than spin");
    tdx_mutex_lock(&f.lock);
    f.fail = 0;
    tdx_mutex_unlock(&f.lock);
    CHECK(wait_count(&f, 1, 1), "resident feed recovers after failure");
    tdx_thread_join(&reader_thread);
    CHECK(r.errors == 0, "concurrent status/snapshot JSON is complete");
    tdx_buf_init(&body);
    tdx_json_doc_init(&doc);
    CHECK(tdx_hub_status_json(hub, &body, &error) == TDX_OK, "status after recovery");
    CHECK(tdx_json_parse(body.data, body.len, &doc, &error) == TDX_OK, "status JSON");
    node = tdx_json_member(&doc, tdx_json_root(&doc), "consecutive_failures");
    CHECK(node && node->number == 0, "success clears retry state");
    node = tdx_json_member(&doc, tdx_json_root(&doc), "last_round_batches");
    CHECK(node && node->number == 7, "completed fetch publishes its batch count");
    tdx_hub_stop(hub);
    CHECK(tdx_hub_is_stopping(hub), "stop state visible");
    CHECK(tdx_hub_start(hub, &error) == TDX_ERR, "stopped hub cannot restart");
    CHECK(tdx_hub_subscribe_all(hub, &error) == 0, "stopped hub rejects subscribers");
    CHECK(tdx_hub_snapshot_json(hub, f.codes, 3, &body, &error) == TDX_OK,
          "snapshot remains readable after stop");
    CHECK(tdx_json_parse(body.data, body.len, &doc, &error) == TDX_OK, "snapshot JSON");
    node = tdx_json_member(&doc, tdx_json_root(&doc), "stale");
    CHECK(node && node->boolean, "stopped snapshots are marked stale");
    tdx_json_doc_free(&doc);
    tdx_buf_free(&body);
    tdx_hub_destroy(hub);
    feed_free(&f);
}

typedef struct stopper {
    tdx_hub *hub;
    feed *f;
    int done;
} stopper;

static void stop_hub(void *context) {
    stopper *s = (stopper *)context;
    tdx_hub_stop(s->hub);
    tdx_mutex_lock(&s->f->lock);
    s->done = 1;
    tdx_mutex_unlock(&s->f->lock);
}

static void test_stop_waits_for_fetch(void) {
    feed f;
    tdx_hub *hub;
    tdx_error error;
    tdx_thread threads[2] = {{0}, {0}};
    stopper stops[2];
    int i;
    feed_init(&f);
    f.block = 1;
    hub = make_hub(&f, 10);
    if (!hub) { feed_free(&f); return; }
    CHECK(tdx_hub_start(hub, &error) == TDX_OK, "start blocked fetch");
    CHECK(wait_count(&f, 0, 1), "fetch entered");
    for (i = 0; i < 2; ++i) {
        stops[i].hub = hub; stops[i].f = &f; stops[i].done = 0;
        CHECK(tdx_thread_start(&threads[i], stop_hub, &stops[i], &error) == TDX_OK,
              "parallel stop starts");
    }
    tdx_sleep_ms(30);
    tdx_mutex_lock(&f.lock);
    CHECK(!stops[0].done && !stops[1].done, "stop must retain fetch context until fetch returns");
    f.block = 0;
    tdx_cond_broadcast(&f.cond);
    tdx_mutex_unlock(&f.lock);
    for (i = 0; i < 2; ++i)
        tdx_thread_join(&threads[i]);
    CHECK(stops[0].done && stops[1].done, "both stop callers complete");
    CHECK(feed_count(&f, 0) == 1, "no new fetch after stop");
    tdx_hub_destroy(hub);
    feed_free(&f);
}

static void test_latest_values_and_freshness(void) {
    feed f;
    tdx_hub *hub;
    tdx_error error;
    tdx_buf body;
    tdx_json_doc doc;
    uint64_t id;
    int seen[3] = {0};
    double previous_sequence = 0;
    int i;
    const tdx_json_node *node;
    feed_init(&f);
    hub = make_hub(&f, 20);
    if (!hub) { feed_free(&f); return; }
    tdx_buf_init(&body);
    tdx_json_doc_init(&doc);
    id = tdx_hub_subscribe_all(hub, &error);
    CHECK(id != 0, "latest-value subscriber");
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "initial values");
    f.prices[0] = 20;
    for (i = 0; i < 20; ++i) {
        f.prices[1] = 30 + i;
        CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "hot security update");
    }
    for (i = 0; i < 3; ++i) {
        const tdx_json_node *record;
        const char *security;
        int slot;
        CHECK(tdx_hub_next(hub, id, &body, 0, &error) == TDX_OK && body.len,
              "all securities survive a slow subscriber");
        CHECK(tdx_json_parse(body.data, body.len, &doc, &error) == TDX_OK, "event JSON");
        node = tdx_json_member(&doc, tdx_json_root(&doc), "sequence");
        CHECK(node && node->number > previous_sequence, "coalesced events retain sequence order");
        if (node) previous_sequence = node->number;
        security = tdx_json_text(&doc, tdx_json_member(&doc, tdx_json_root(&doc), "security_id"));
        slot = security ? security[strlen(security) - 1] - '1' : -1;
        record = tdx_json_member(&doc, tdx_json_root(&doc), "record");
        node = tdx_json_member(&doc, record, "last_price");
        CHECK(slot >= 0 && slot < 3 && node, "event identity and full record");
        if (slot >= 0 && slot < 3 && node) {
            seen[slot]++;
            CHECK(node->number == f.prices[slot], "delivered value is the newest for that security");
        }
    }
    CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1, "quiet security retained exactly once");
    CHECK(tdx_hub_next(hub, id, &body, 0, &error) == TDX_OK && !body.len,
          "coalescing stays bounded to one event per security");
    tdx_sleep_ms(40);
    CHECK(tdx_hub_snapshot_json(hub, f.codes, 3, &body, &error) == TDX_OK, "cached snapshot");
    CHECK(tdx_json_parse(body.data, body.len, &doc, &error) == TDX_OK, "freshness JSON");
    node = tdx_json_member(&doc, tdx_json_root(&doc), "stale");
    CHECK(node && node->boolean, "old snapshots advertise stale data");
    node = tdx_json_at(&doc, tdx_json_member(&doc, tdx_json_root(&doc), "freshness"), 0);
    node = tdx_json_member(&doc, node, "observed_at_unix_ms");
    CHECK(node && node->number > 1000000000000.0, "freshness uses a Unix timestamp");
    tdx_json_doc_free(&doc);
    tdx_buf_free(&body);
    tdx_hub_destroy(hub);
    feed_free(&f);
}

int main(void) {
    test_backoff_and_recovery();
    test_stop_waits_for_fetch();
    test_latest_values_and_freshness();
    if (failures) printf("%d hub lifecycle check(s) failed\n", failures);
    else printf("hub lifecycle checks passed\n");
    return failures ? 1 : 0;
}
