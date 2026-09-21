/* test_state.c - change detection and the last-known-value table. */
#include <stdio.h>
#include <string.h>

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

static void make_depth(tdx_depth *depth, int market, const char *code) {
    size_t level;
    memset(depth, 0, sizeof(*depth));
    depth->security.market_id = market;
    memcpy(depth->security.code, code, 6);
    depth->active = 1684;
    depth->last = 11.66;
    depth->previous = 11.70;
    depth->open = 11.70;
    depth->high = 11.72;
    depth->low = 11.60;
    depth->time_raw = 0;
    depth->auxiliary_price_delta_raw = 0;
    depth->total_hand = 385417;
    depth->current_hand = 73;
    depth->amount = 449187776.0;
    depth->inside = 174059;
    depth->outside = 211359;
    depth->auction_imbalance_hand = 0;
    depth->open_amount = 2630160.0;
    depth->update_time = 105736;
    depth->status = 0;
    depth->unknown_after_outer = 0;
    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        depth->buys[level].price = 11.65 - (double)level * 0.01;
        depth->buys[level].volume_hand = 4119 + (int64_t)level;
        depth->sells[level].price = 11.66 + (double)level * 0.01;
        depth->sells[level].volume_hand = 282 + (int64_t)level;
    }
    depth->tail_size = 46;
}

static void test_diff_identical(void) {
    tdx_depth left;
    tdx_depth right;
    make_depth(&left, 0, "000001");
    right = left;
    CHECK(tdx_depth_diff(&left, &right) == 0, "identical records must not differ");
}

static void test_diff_groups(void) {
    tdx_depth base;
    tdx_depth probe;

    make_depth(&base, 0, "000001");

    probe = base;
    probe.last = 11.67;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_LAST, "last price change");

    probe = base;
    probe.high = 11.80;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_OHLC, "high price change");

    probe = base;
    probe.previous = 11.71;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_OHLC, "previous close change");

    probe = base;
    probe.amount = 1.0;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_AMOUNT, "turnover change");

    probe = base;
    probe.total_hand = 1;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_VOLUME, "total hand change");

    probe = base;
    probe.current_hand = 1;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_VOLUME, "current hand change");

    probe = base;
    probe.outside = 1;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_DISHES, "outer disc change");

    probe = base;
    probe.open_amount = 1.0;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_DISHES, "open amount change");

    probe = base;
    probe.buys[0].volume_hand += 1;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_BOOK, "bid volume change");

    probe = base;
    probe.sells[4].price += 0.01;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_BOOK, "ask price change");

    probe = base;
    probe.status = 3;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_STATUS, "status change");

    probe = base;
    probe.update_time = 1;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_STATUS, "update time change");

    probe = base;
    probe.tail_size = 0;
    CHECK(tdx_depth_diff(&base, &probe) == TDX_DIFF_STATUS, "tail size change");

    /* Several groups at once must union, not replace. */
    probe = base;
    probe.last += 0.01;
    probe.total_hand += 1;
    probe.buys[2].volume_hand += 1;
    CHECK(tdx_depth_diff(&base, &probe) ==
              (TDX_DIFF_LAST | TDX_DIFF_VOLUME | TDX_DIFF_BOOK),
          "combined mask is 0x%04X", tdx_depth_diff(&base, &probe));
}

static void test_diff_names(void) {
    const char *names[8];
    size_t count;

    count = tdx_diff_names(0, names, 8);
    CHECK(count == 0, "an empty mask has no names");

    count = tdx_diff_names(TDX_DIFF_NEW, names, 8);
    CHECK(count == 1 && strcmp(names[0], "new") == 0, "new name");

    count = tdx_diff_names(TDX_DIFF_LAST | TDX_DIFF_BOOK, names, 8);
    CHECK(count == 2, "two names expected, got %zu", count);
    CHECK(count == 2 && strcmp(names[0], "last") == 0 && strcmp(names[1], "book") == 0,
          "names are %s/%s", count > 0 ? names[0] : "-", count > 1 ? names[1] : "-");

    count = tdx_diff_names(0xFFFFu, names, 3);
    CHECK(count == 3, "the capacity must cap the output, got %zu", count);
}

static void test_state_lifecycle(void) {
    tdx_state state;
    tdx_error error;
    tdx_depth first;
    tdx_depth second;
    tdx_diff_mask mask = 0;
    const tdx_state_entry *entry = NULL;

    error.message[0] = '\0';
    make_depth(&first, 0, "000001");
    CHECK(tdx_state_init(&state, 16, &error) == TDX_OK, "init failed: %s", error.message);
    CHECK(tdx_state_count(&state) == 0, "a fresh table is empty");

    CHECK(tdx_state_apply(&state, &first, &mask, &entry, &error) == TDX_OK,
          "first apply failed: %s", error.message);
    CHECK(mask == TDX_DIFF_NEW, "first apply mask is 0x%04X", mask);
    CHECK(tdx_state_count(&state) == 1, "table holds %zu rows", tdx_state_count(&state));
    CHECK(entry != NULL && entry->updates == 1, "first apply must count one update");

    /* Re-observing the same values is not a change. */
    mask = 0xFFFF;
    CHECK(tdx_state_apply(&state, &first, &mask, &entry, &error) == TDX_OK,
          "second apply failed");
    CHECK(mask == 0, "an identical record must not report a change (0x%04X)", mask);
    CHECK(entry != NULL && entry->updates == 1 && entry->observations == 2,
          "identical apply changed the counters");
    CHECK(tdx_state_count(&state) == 1, "table grew on a repeat observation");

    /* A real change is detected and counted once. */
    second = first;
    second.last = 11.99;
    second.buys[1].volume_hand += 5;
    mask = 0;
    CHECK(tdx_state_apply(&state, &second, &mask, &entry, &error) == TDX_OK,
          "third apply failed");
    CHECK(mask == (TDX_DIFF_LAST | TDX_DIFF_BOOK), "change mask is 0x%04X", mask);
    CHECK(entry != NULL && entry->updates == 2 && entry->observations == 3,
          "change counters are wrong");

    {
        const tdx_state_entry *found = tdx_state_find(&state, &second.security);
        CHECK(found != NULL, "find must locate the stored row");
        CHECK(found && found->depth.last == 11.99, "stored value is stale");
    }
    {
        tdx_code other;
        other.market_id = 0;
        memcpy(other.code, "000002", 6);
        CHECK(tdx_state_find(&state, &other) == NULL, "unknown key must not be found");
    }
    tdx_state_free(&state);
    CHECK(tdx_state_count(&state) == 0, "freed table must be empty");
}

static void test_state_growth_and_markets(void) {
    tdx_state state;
    tdx_error error;
    size_t index;
    const size_t total = 5000;
    int mismatches = 0;

    error.message[0] = '\0';
    CHECK(tdx_state_init(&state, 8, &error) == TDX_OK, "init failed: %s", error.message);
    for (index = 0; index < total; ++index) {
        tdx_depth depth;
        char code[8];
        tdx_diff_mask mask = 0;
        snprintf(code, sizeof(code), "%06u", (unsigned)(100000 + index));
        make_depth(&depth, (int)(index % 3), code);
        depth.total_hand = (int64_t)index;
        if (tdx_state_apply(&state, &depth, &mask, NULL, &error) != TDX_OK) {
            printf("FAIL apply %zu: %s\n", index, error.message);
            failures++;
            tdx_state_free(&state);
            return;
        }
        if (mask != TDX_DIFF_NEW) {
            printf("FAIL apply %zu reported 0x%04X, expected NEW\n", index, mask);
            failures++;
            break;
        }
    }
    CHECK(tdx_state_count(&state) == total, "table holds %zu of %zu rows",
          tdx_state_count(&state), total);
    for (index = 0; index < total; ++index) {
        tdx_code code;
        char text[8];
        const tdx_state_entry *found;
        snprintf(text, sizeof(text), "%06u", (unsigned)(100000 + index));
        code.market_id = (int)(index % 3);
        memcpy(code.code, text, 6);
        found = tdx_state_find(&state, &code);
        if (!found || found->depth.total_hand != (int64_t)index)
            mismatches++;
    }
    CHECK(mismatches == 0, "%d rows were lost or corrupted after growth", mismatches);

    /* The same code in different markets must be three distinct rows. */
    {
        tdx_code code;
        memcpy(code.code, "000001", 6);
        code.market_id = 0;
        CHECK(tdx_state_find(&state, &code) == NULL ||
                  tdx_state_find(&state, &code)->depth.total_hand != 100001,
              "market 0 key collided with a later row");
    }
    tdx_state_free(&state);
}

int main(void) {
    test_diff_identical();
    test_diff_groups();
    test_diff_names();
    test_state_lifecycle();
    test_state_growth_and_markets();
    if (failures) {
        printf("%d state check(s) failed\n", failures);
        return 1;
    }
    printf("state checks passed\n");
    return 0;
}
