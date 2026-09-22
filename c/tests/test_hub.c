/* test_hub.c - deterministic hub tests with an injected feed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_hub.h"
#include "tdx_state.h"

static int failures = 0;

#define CHECK(condition, ...)                                                        \
    do {                                                                             \
        if (!(condition)) {                                                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                              \
            printf(__VA_ARGS__);                                                     \
            printf("\n");                                                            \
            failures++;                                                              \
        }                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/* injected feed                                                       */
/* ------------------------------------------------------------------ */

#define FEED_CODES 8

typedef struct fake_feed {
    tdx_code codes[FEED_CODES];
    size_t count;
    int64_t last_price[FEED_CODES]; /* in units of 0.01 */
    int64_t total_hand[FEED_CODES];
    int calls;
    int fail_next;
} fake_feed;

static void make_depth(tdx_depth *depth, const tdx_code *code) {
    size_t level;
    memset(depth, 0, sizeof(*depth));
    depth->security = *code;
    depth->active = 100;
    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        depth->buys[level].price = 10.0 - (double)level * 0.01;
        depth->buys[level].volume_hand = 100 + (int64_t)level;
        depth->sells[level].price = 10.01 + (double)level * 0.01;
        depth->sells[level].volume_hand = 200 + (int64_t)level;
    }
}

static int feed_fetch(void *context, const size_t *indices, size_t count,
                      tdx_depth *out, size_t *batches_out, tdx_error *err) {
    fake_feed *feed = (fake_feed *)context;
    size_t position;

    if (!feed) {
        tdx_error_set(err, "feed is null");
        return TDX_ERR;
    }
    if (feed->fail_next) {
        feed->fail_next = 0;
        tdx_error_set(err, "injected fetch failure");
        return TDX_ERR;
    }
    for (position = 0; position < count; ++position) {
        const size_t slot = indices[position];
        if (slot >= feed->count) {
            tdx_error_set(err, "feed index %zu is out of range", slot);
            return TDX_ERR;
        }
        make_depth(&out[position], &feed->codes[slot]);
        out[position].last = (double)feed->last_price[slot] / 100.0;
        out[position].total_hand = feed->total_hand[slot];
        out[position].current_hand =
            feed->total_hand[slot] - feed->total_hand[slot] / 10;
    }
    feed->calls++;
    /* One upstream request per ten securities: the test only needs a
     * recognisable number, so the hub can be checked for reporting it. */
    if (batches_out)
        *batches_out = (count + 9) / 10;
    return TDX_OK;
}

static void feed_init(fake_feed *feed, size_t count) {
    size_t index;
    static const char *codes[FEED_CODES] = {"000001", "000002", "600000", "600519",
                                            "300750", "601318", "920478", "159915"};
    memset(feed, 0, sizeof(*feed));
    feed->count = count;
    for (index = 0; index < count; ++index) {
        feed->codes[index].market_id = (index < 5 || index == 7) ? (index < 5 ? 0 : 0) : 1;
        memcpy(feed->codes[index].code, codes[index], 6);
        feed->last_price[index] = 1000 + (int64_t)index;
        feed->total_hand[index] = 1000 + (int64_t)index * 10;
    }
    /* Give the Shanghai names the right market so subscriptions are honest. */
    feed->codes[2].market_id = 1;
    feed->codes[3].market_id = 1;
    feed->codes[5].market_id = 1;
    feed->codes[6].market_id = 2;
}

/* ------------------------------------------------------------------ */

static size_t count_events(tdx_hub *hub, uint64_t id, size_t limit) {
    tdx_buf buffer;
    size_t seen = 0;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&buffer);
    while (seen < limit) {
        if (tdx_hub_next(hub, id, &buffer, 0, &error) != TDX_OK)
            break;
        if (buffer.len == 0)
            break;
        seen++;
    }
    tdx_buf_free(&buffer);
    return seen;
}

static void test_first_round_emits_snapshots(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    uint64_t id;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 4;
    CHECK(tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                         &error) == TDX_OK,
          "create failed: %s", error.message);
    if (!hub)
        return;
    id = tdx_hub_subscribe_all(hub, &error);
    CHECK(id != 0, "subscribe failed: %s", error.message);
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed: %s", error.message);
    CHECK(count_events(hub, id, 16) == 4, "first round must emit one snapshot each");
    tdx_hub_destroy(hub);
}

static void test_identical_round_is_silent(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    uint64_t id;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 4;
    options.heartbeat_ms = 0; /* keep heartbeats out of this assertion */
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    id = tdx_hub_subscribe_all(hub, &error);
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed");
    (void)count_events(hub, id, 64);
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "second poll failed");
    CHECK(count_events(hub, id, 64) == 0,
          "an unchanged round must not produce any event");
    tdx_hub_destroy(hub);
}

static void test_change_is_reported_once(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    tdx_buf buffer;
    uint64_t id;
    int64_t mask_seen = 0;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 4;
    options.heartbeat_ms = 0;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    id = tdx_hub_subscribe_all(hub, &error);
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed");
    (void)count_events(hub, id, 64);

    /* One security moves. */
    feed.last_price[2] += 1;
    feed.total_hand[2] += 100;
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "third poll failed");
    tdx_buf_init(&buffer);
    CHECK(tdx_hub_next(hub, id, &buffer, 0, &error) == TDX_OK, "next failed");
    CHECK(buffer.len > 0, "a changed security must produce an event");
    if (buffer.len > 0) {
        char *text = (char *)malloc(buffer.len + 1);
        if (text) {
            memcpy(text, buffer.data, buffer.len);
            text[buffer.len] = '\0';
            if (strstr(text, "\"type\":\"change\""))
                mask_seen |= 1;
            if (strstr(text, "\"changed\":[\"last\",\"volume\"]"))
                mask_seen |= 2;
            if (strstr(text, "SH600000"))
                mask_seen |= 4;
            free(text);
        }
    }
    tdx_buf_free(&buffer);
    CHECK((mask_seen & 1) != 0, "the event must be typed as a change");
    CHECK((mask_seen & 2) != 0, "the changed list must name last and volume");
    CHECK((mask_seen & 4) != 0, "the event must carry SH600000");
    CHECK(count_events(hub, id, 8) == 0, "only one security changed");
    tdx_hub_destroy(hub);
}

static void test_filtered_subscription(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    tdx_code wanted[1];
    uint64_t all_id, filtered_id;
    size_t index;
    int saw_unwanted = 0;
    int saw_wanted = 0;
    tdx_buf buffer;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 4;
    options.heartbeat_ms = 0;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    wanted[0] = feed.codes[2]; /* SH600000 */
    all_id = tdx_hub_subscribe_all(hub, &error);
    filtered_id = tdx_hub_subscribe_codes(hub, wanted, 1, &error);
    CHECK(all_id != 0 && filtered_id != 0, "subscribe failed: %s", error.message);

    /* Every security moves so the filters are actually exercised. */
    for (index = 0; index < feed.count; ++index)
        feed.last_price[index] += 5;
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed");
    CHECK(count_events(hub, all_id, 32) == 4, "the all-subscriber missed events");

    tdx_buf_init(&buffer);
    for (index = 0; index < 8; ++index) {
        if (tdx_hub_next(hub, filtered_id, &buffer, 0, &error) != TDX_OK)
            break;
        if (buffer.len == 0)
            break;
        {
            char *text = (char *)malloc(buffer.len + 1);
            if (text) {
                memcpy(text, buffer.data, buffer.len);
                text[buffer.len] = '\0';
                if (strstr(text, "SH600000"))
                    saw_wanted = 1;
                else
                    saw_unwanted = 1;
                free(text);
            }
        }
    }
    tdx_buf_free(&buffer);
    CHECK(saw_wanted == 1, "the filtered subscriber missed its security");
    CHECK(saw_unwanted == 0, "the filtered subscriber received a foreign security");
    CHECK(count_events(hub, filtered_id, 8) == 0,
          "the filtered subscriber received more than one event");
    tdx_hub_destroy(hub);
}

static void test_unknown_code_is_rejected(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    tdx_code bogus;
    uint64_t id;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 4;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    bogus.market_id = 0;
    memcpy(bogus.code, "999999", 6);
    id = tdx_hub_subscribe_codes(hub, &bogus, 1, &error);
    CHECK(id == 0, "a code outside the universe must be rejected");
    CHECK(strstr(error.message, "not in the hub universe") != NULL,
          "rejection reason is %s", error.message);
    tdx_hub_destroy(hub);
}

static void test_queue_overflow_drops_oldest(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    uint64_t id;
    size_t index;
    size_t received;
    tdx_buf status;

    error.message[0] = '\0';
    feed_init(&feed, 8);
    tdx_hub_options_default(&options);
    options.max_subscribers = 2;
    /* Smaller than the subscription, so the queue is exactly one snapshot deep
     * and every later change must displace an older entry. */
    options.subscriber_queue_limit = 3;
    options.heartbeat_ms = 0;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    id = tdx_hub_subscribe_all(hub, &error);
    CHECK(count_events(hub, id, 64) == 0, "nothing should be queued yet");
    /* Round one fills the queue with the eight snapshots. */
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "first poll failed");

    /* Round two changes everything while that queue is still full, so every
     * push must displace an older entry. */
    for (index = 0; index < feed.count; ++index)
        feed.last_price[index] += 3;
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "second poll failed");
    received = count_events(hub, id, 64);
    CHECK(received == 8, "a bounded queue must keep only its capacity, got %zu",
          received);

    tdx_buf_init(&status);
    CHECK(tdx_hub_status_json(hub, &status, &error) == TDX_OK, "status failed: %s",
          error.message);
    if (status.len > 0) {
        char *text = (char *)malloc(status.len + 1);
        if (text) {
            memcpy(text, status.data, status.len);
            text[status.len] = '\0';
            CHECK(strstr(text, "\"dropped\":8") != NULL,
                  "expected eight dropped events in %s", text);
            CHECK(strstr(text, "\"universe\":8") != NULL, "status must report the universe");
            CHECK(strstr(text, "\"subscribers\":1") != NULL, "status subscriber count");
            /* The hub must report the cost its fetcher reported: it decides how much
             * work a round is, so the number of upstream requests that work became is
             * its business, and a fetcher that cannot say must not be read as zero. */
            CHECK(strstr(text, "\"last_round_batches\":") != NULL,
                  "status must carry the last round's batch count: %s", text);
            if (strstr(text, "\"last_round_batches\":") != NULL) {
                const char *field = strstr(text, "\"last_round_batches\":");
                CHECK(field != NULL && strncmp(field + strlen("\"last_round_batches\":"),
                                               "0", 1) != 0,
                      "and it must not be zero after a round that fetched: %s", field);
            }
            free(text);
        }
    }
    tdx_buf_free(&status);
    tdx_hub_destroy(hub);
}

static void test_failed_round_is_counted(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    tdx_buf status;
    uint64_t id;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 2;
    options.heartbeat_ms = 0;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    id = tdx_hub_subscribe_all(hub, &error);
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed");
    CHECK(count_events(hub, id, 16) == 4, "first round events");

    feed.fail_next = 1;
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_ERR, "an injected failure must surface");
    CHECK(count_events(hub, id, 16) == 0, "a failed round must not emit events");

    tdx_buf_init(&status);
    CHECK(tdx_hub_status_json(hub, &status, &error) == TDX_OK, "status failed");
    if (status.len > 0) {
        char *text = (char *)malloc(status.len + 1);
        if (text) {
            memcpy(text, status.data, status.len);
            text[status.len] = '\0';
            CHECK(strstr(text, "\"failed_rounds\":1") != NULL,
                  "expected one failed round in %s", text);
            CHECK(strstr(text, "injected fetch failure") != NULL,
                  "the failure reason must be reported");
            free(text);
        }
    }
    tdx_buf_free(&status);
    tdx_hub_destroy(hub);
}

static void test_snapshot_json(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    tdx_code wanted[2];
    tdx_buf buffer;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 2;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed");
    wanted[0] = feed.codes[0];
    wanted[1] = feed.codes[2];
    tdx_buf_init(&buffer);
    CHECK(tdx_hub_snapshot_json(hub, wanted, 2, &buffer, &error) == TDX_OK,
          "snapshot failed: %s", error.message);
    if (buffer.len > 0) {
        char *text = (char *)malloc(buffer.len + 1);
        if (text) {
            memcpy(text, buffer.data, buffer.len);
            text[buffer.len] = '\0';
            CHECK(strstr(text, "\"requested\":2") != NULL, "requested count");
            CHECK(strstr(text, "SZ000001") != NULL, "first security");
            CHECK(strstr(text, "SH600000") != NULL, "second security");
            free(text);
        }
    }
    tdx_buf_free(&buffer);
    tdx_hub_destroy(hub);
}

static int status_int(tdx_hub *hub, const char *key, long *out) {
    tdx_buf status;
    tdx_error error;
    char *text;
    char needle[64];
    const char *found;
    error.message[0] = '\0';
    tdx_buf_init(&status);
    if (tdx_hub_status_json(hub, &status, &error) != TDX_OK) {
        tdx_buf_free(&status);
        return 0;
    }
    text = (char *)malloc(status.len + 1);
    if (!text) {
        tdx_buf_free(&status);
        return 0;
    }
    memcpy(text, status.data, status.len);
    text[status.len] = '\0';
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    found = strstr(text, needle);
    if (found) {
        *out = strtol(found + strlen(needle), NULL, 10);
        free(text);
        tdx_buf_free(&status);
        return 1;
    }
    free(text);
    tdx_buf_free(&status);
    return 0;
}

static void test_idle_backoff(void) {
    fake_feed feed;
    tdx_hub *hub = NULL;
    tdx_hub_options options;
    tdx_error error;
    uint64_t id;
    long effective = -1;
    long warm = -1;
    long cold = -1;
    size_t index;

    error.message[0] = '\0';
    feed_init(&feed, 4);
    tdx_hub_options_default(&options);
    options.max_subscribers = 2;
    options.heartbeat_ms = 0;
    options.interval_ms = 1000;
    options.idle_interval_ms = 5000;
    options.tier_warm_ms = 2500; /* explicit warm step, not the auto 3x */
    options.idle_rounds = 2;
    if (tdx_hub_create(&hub, feed.codes, feed.count, &options, feed_fetch, &feed,
                       &error) != TDX_OK) {
        printf("FAIL create: %s\n", error.message);
        failures++;
        return;
    }
    id = tdx_hub_subscribe_all(hub, &error);

    /* Round 1 changes everything, so the active cadence applies. */
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "poll failed");
    CHECK(status_int(hub, "effective_interval_ms", &effective),
          "status has no effective interval");
    CHECK(effective == 1000, "after a change the cadence is %ld, expected 1000",
          effective);
    (void)count_events(hub, id, 64);

    /* Two quiet rounds walk one step down the ladder. */
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "second poll failed");
    CHECK(status_int(hub, "effective_interval_ms", &effective), "status read failed");
    CHECK(effective == 1000, "one quiet round must not demote yet (%ld)", effective);

    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "third poll failed");
    CHECK(status_int(hub, "effective_interval_ms", &effective), "status read failed");
    CHECK(effective == 2500, "two quiet rounds must demote to the configured warm (%ld)",
          effective);
    CHECK(status_int(hub, "tier_warm", &warm) && warm == 4,
          "expected four warm securities, got %ld", warm);

    /* Another two quiet rounds reach the cold step. */
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "fourth poll failed");
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "fifth poll failed");
    CHECK(status_int(hub, "effective_interval_ms", &effective), "status read failed");
    CHECK(effective == 5000, "four quiet rounds must reach cold (%ld)", effective);
    CHECK(status_int(hub, "tier_cold", &cold) && cold == 4,
          "expected four cold securities, got %ld", cold);

    /* Any change snaps straight back to hot. */
    for (index = 0; index < feed.count; ++index)
        feed.last_price[index] += 7;
    CHECK(tdx_hub_poll_once(hub, &error) == TDX_OK, "sixth poll failed");
    CHECK(status_int(hub, "effective_interval_ms", &effective), "status read failed");
    CHECK(effective == 1000, "a change must restore the hot cadence (%ld)", effective);
    tdx_hub_destroy(hub);
}

int main(void) {
    test_first_round_emits_snapshots();
    test_identical_round_is_silent();
    test_change_is_reported_once();
    test_filtered_subscription();
    test_unknown_code_is_rejected();
    test_queue_overflow_drops_oldest();
    test_failed_round_is_counted();
    test_snapshot_json();
    test_idle_backoff();
    if (failures) {
        printf("%d hub check(s) failed\n", failures);
        return 1;
    }
    printf("hub checks passed\n");
    return 0;
}
