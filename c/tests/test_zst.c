/* test_zst.c - zst container/framing plus incremental replay.
 *
 * Two halves.  The first half drives the decoder and the replayer with synthetic
 * tag streams so the semantics are pinned without a TdxW installation.  The
 * second half replays a real zst_cache sample when one is available and asserts
 * the invariants and the final state measured in output/zst_field_table.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_zst.h"
#include "tdx_zst_json.h"
#include "tdx_zst_replay.h"

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

/* --- synthetic stream builder ----------------------------------------- */

typedef struct stream_builder {
    uint8_t data[8192];
    size_t size;
} stream_builder;

static void sb_init(stream_builder *builder) {
    memset(builder, 0, sizeof(*builder));
}

static void sb_key(stream_builder *builder, const char *key) {
    builder->data[builder->size++] = 3;
    memcpy(builder->data + builder->size, key, 8);
    builder->size += 8;
}

/* One tag.  There is no per-field terminator: a value runs until the first byte
 * below 0x20, which is the next tag.  Values must therefore stay printable, and
 * an empty value writes the two-character id with nothing after it - exactly how
 * the server ships the 6..10 level placeholders. */
static void sb_field(stream_builder *builder, const char *id, const char *value) {
    size_t length = strlen(value);
    builder->data[builder->size++] = 2;
    builder->data[builder->size++] = (uint8_t)id[0];
    builder->data[builder->size++] = (uint8_t)id[1];
    memcpy(builder->data + builder->size, value, length);
    builder->size += length;
}

/* Closes the record opened by the last sb_key. */
static void sb_end(stream_builder *builder) {
    builder->data[builder->size++] = 4;
}

static void parse_or_die(const stream_builder *builder, tdx_zst_document *document) {
    tdx_error error;
    error.message[0] = '\0';
    tdx_zst_document_init(document);
    if (tdx_zst_parse_stream(builder->data, builder->size, document, &error) != TDX_OK) {
        printf("FAIL: synthetic stream did not parse: %s\n", error.message);
        failures++;
    }
}

/* --- framing ---------------------------------------------------------- */

static void test_parse_stream(void) {
    stream_builder builder;
    tdx_zst_document document;
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "83436.000");
    sb_field(&builder, "04", "16.7200");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "91500.000");
    sb_field(&builder, "25", "");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    CHECK(document.count == 2, "expected 2 records, got %zu", document.count);
    if (document.count == 2) {
        const tdx_zst_record *first = &document.records[0];
        CHECK(strcmp(first->security_key, "01000623") == 0, "key %s", first->security_key);
        CHECK(first->key_market == 1, "key market %d", first->key_market);
        CHECK(strcmp(first->code, "000623") == 0, "code %s", first->code);
        CHECK(first->field_count == 2, "first record carries %zu fields", first->field_count);
        CHECK(first->time_hhmmss == 83436, "time %d", first->time_hhmmss);
        CHECK(first->last_price == 0.0, "08 absent, last must stay 0");
        CHECK(strcmp(tdx_zst_find_field(first, "04"), "16.7200") == 0, "04 value");
        CHECK(tdx_zst_find_field(first, "08") == NULL, "08 must be absent");
        CHECK(document.records[1].field_count == 2, "second record carries %zu fields",
              document.records[1].field_count);
        CHECK(strcmp(tdx_zst_find_field(&document.records[1], "25"), "") == 0,
              "an empty level placeholder must read back as empty");
    }
    tdx_zst_document_free(&document);
}

static void test_parse_rejects(void) {
    tdx_error error;
    tdx_zst_document document;
    stream_builder builder;

    /* a key that is not eight characters wide */
    sb_init(&builder);
    builder.data[builder.size++] = 3;
    memcpy(builder.data + builder.size, "0100062", 7);
    builder.size += 7;
    sb_end(&builder);
    error.message[0] = '\0';
    tdx_zst_document_init(&document);
    CHECK(tdx_zst_parse_stream(builder.data, builder.size, &document, &error) == TDX_ERR,
          "a 7-character key must be rejected");
    CHECK(strstr(error.message, "expected 8") != NULL, "error was: %s", error.message);
    tdx_zst_document_free(&document);

    /* a field before any key */
    sb_init(&builder);
    sb_field(&builder, "0T", "91500.000");
    error.message[0] = '\0';
    tdx_zst_document_init(&document);
    CHECK(tdx_zst_parse_stream(builder.data, builder.size, &document, &error) == TDX_ERR,
          "a field without a record key must be rejected");
    tdx_zst_document_free(&document);

    /* an unknown tag */
    sb_init(&builder);
    sb_key(&builder, "01000623");
    builder.data[builder.size++] = 9;
    builder.data[builder.size++] = 4;
    error.message[0] = '\0';
    tdx_zst_document_init(&document);
    CHECK(tdx_zst_parse_stream(builder.data, builder.size, &document, &error) == TDX_ERR,
          "tag 9 must be rejected");
    CHECK(strstr(error.message, "unknown tag") != NULL, "error was: %s", error.message);
    tdx_zst_document_free(&document);

    /* a stream that ends inside a record */
    sb_init(&builder);
    sb_key(&builder, "01000623");
    builder.data[builder.size++] = 2;
    builder.data[builder.size++] = '0';
    builder.data[builder.size++] = 'T';
    memcpy(builder.data + builder.size, "91500.000", 9);
    builder.size += 9;
    error.message[0] = '\0';
    tdx_zst_document_init(&document);
    CHECK(tdx_zst_parse_stream(builder.data, builder.size, &document, &error) == TDX_ERR,
          "a truncated record must be rejected");
    tdx_zst_document_free(&document);
}

/* --- value equality --------------------------------------------------- */

static void test_value_equal(void) {
    CHECK(tdx_zst_value_equal("17.390000", "17.3900"),
          "the same price at two precisions must compare equal");
    CHECK(tdx_zst_value_equal("E0      ", "E0"),
          "0D trailing padding must compare equal");
    CHECK(tdx_zst_value_equal("", ""), "two empty values are equal");
    CHECK(!tdx_zst_value_equal("", "0"), "empty and 0 differ");
    CHECK(!tdx_zst_value_equal("17.39", "17.4"), "different prices differ");
    CHECK(!tdx_zst_value_equal("A0", "E0"), "different phase text differs");
    CHECK(tdx_zst_value_equal("23175", "23175.000"),
          "an integer and its padded form must compare equal");
}

/* --- replay ----------------------------------------------------------- */

static void test_state_apply(void) {
    tdx_zst_state state;
    tdx_error error;
    int changed = -1;
    error.message[0] = '\0';
    tdx_zst_state_init(&state);

    CHECK(tdx_zst_state_apply(&state, "0T", "91500.000", &changed, &error) == TDX_OK,
          "apply failed: %s", error.message);
    CHECK(changed == 1, "a new tag is a change");
    CHECK(state.count == 1, "one tag stored, got %zu", state.count);

    CHECK(tdx_zst_state_apply(&state, "0T", "91500.000", &changed, &error) == TDX_OK, "apply");
    CHECK(changed == 0, "an identical value is not a change");

    CHECK(tdx_zst_state_apply(&state, "0T", "91500.0", &changed, &error) == TDX_OK, "apply");
    CHECK(changed == 0, "the same time at lower precision is not a change");

    CHECK(tdx_zst_state_apply(&state, "0T", "91503.000", &changed, &error) == TDX_OK, "apply");
    CHECK(changed == 1, "a new time is a change");
    CHECK(state.count == 1, "overwrite must not append, got %zu", state.count);
    CHECK(strcmp(tdx_zst_state_find(&state, "0T"), "91503.000") == 0, "stored value");
    CHECK(tdx_zst_state_find(&state, "08") == NULL, "absent tag must read NULL");
    CHECK(tdx_zst_state_find(&state, "0") == NULL, "a one-character tag must read NULL");
}

/* Records 0/1 are the pre-open header, record 2 sets the book, record 3 moves
 * only the volume.  The snapshot at record 3 must still carry record 2's prices:
 * that forward fold is the entire point of this module. */
static void test_replay_folds_forward(void) {
    stream_builder builder;
    tdx_zst_document document;
    tdx_zst_replay replay;
    tdx_zst_snapshot snapshot;
    tdx_zst_diff_mask changed = 0;
    tdx_error error;

    error.message[0] = '\0';
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "83436.000");
    sb_field(&builder, "04", "16.7200");
    sb_field(&builder, "1E", "18.390000");
    sb_field(&builder, "1F", "15.050000");
    sb_field(&builder, "1C", "8.350000");
    sb_field(&builder, "25", "");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "91500.000");
    sb_field(&builder, "20", "16.600000");
    sb_field(&builder, "30", "700");
    sb_field(&builder, "40", "16.600000");
    sb_field(&builder, "50", "700");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "91509.000");
    sb_field(&builder, "50", "800");
    sb_field(&builder, "30", "900");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    tdx_zst_replay_init(&replay);

    CHECK(tdx_zst_replay_apply(&replay, &document.records[0], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply 0: %s", error.message);
    CHECK(changed & TDX_ZST_DIFF_NEW, "the first record must report NEW");
    CHECK(changed & TDX_ZST_DIFF_OHLC, "04 must report ohlc");
    CHECK(changed & TDX_ZST_DIFF_LIMITS, "1E/1F must report limits");
    CHECK(changed & TDX_ZST_DIFF_VALUATION, "1C must report valuation");
    CHECK(snapshot.previous_close == 16.72, "previous close %.4f", snapshot.previous_close);
    CHECK(snapshot.limit_up == 18.39, "limit up %.4f", snapshot.limit_up);
    CHECK(snapshot.pe_ratio == 8.35, "pe %.4f", snapshot.pe_ratio);
    CHECK((snapshot.present & TDX_ZST_HAVE_OPEN) == 0, "open is not known pre-auction");
    CHECK(snapshot.bid_levels == 0, "an empty 25 placeholder must not set a level");
    CHECK(snapshot.record_index == 0 && snapshot.update_index == 0, "provenance of record 0");

    CHECK(tdx_zst_replay_apply(&replay, &document.records[1], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply 1: %s", error.message);
    CHECK(!(changed & TDX_ZST_DIFF_NEW), "the second record is not new");
    CHECK(changed & TDX_ZST_DIFF_BOOK, "the book must report a change");
    CHECK(!(changed & TDX_ZST_DIFF_OHLC), "04 did not move");
    CHECK(snapshot.previous_close == 16.72, "the header must survive record 1");
    CHECK(snapshot.bids[0].price == 16.60 && snapshot.bids[0].volume == 700,
          "bid1 %.4f/%.0f", snapshot.bids[0].price, snapshot.bids[0].volume);
    CHECK(snapshot.asks[0].price == 16.60 && snapshot.asks[0].volume == 700,
          "ask1 %.4f/%.0f", snapshot.asks[0].price, snapshot.asks[0].volume);

    CHECK(tdx_zst_replay_apply(&replay, &document.records[2], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply 2: %s", error.message);
    CHECK(changed == (TDX_ZST_DIFF_TIME | TDX_ZST_DIFF_BOOK), "mask 0x%x", changed);
    CHECK(snapshot.bids[0].price == 16.60, "bid1 price must persist, got %.4f",
          snapshot.bids[0].price);
    CHECK(snapshot.bids[0].volume == 900, "bid1 volume %.0f", snapshot.bids[0].volume);
    CHECK(snapshot.asks[0].volume == 800, "ask1 volume %.0f", snapshot.asks[0].volume);
    CHECK(snapshot.limit_down == 15.05, "limit down must persist");
    CHECK(snapshot.record_index == 2 && snapshot.update_index == 2, "provenance of record 2");
    CHECK(snapshot.field_count == 10, "merged tag count %zu", snapshot.field_count);
    CHECK(snapshot.changed_id_count == 3, "three tags carried, got %zu", snapshot.changed_id_count);
    CHECK(snapshot.dropped_fields == 0, "nothing may be dropped");

    tdx_zst_replay_free(&replay);
    tdx_zst_document_free(&document);
}

/* A record that repeats the previous price at a different precision carries a
 * tag but changes nothing, and must be reported that way. */
static void test_replay_precision_noise(void) {
    stream_builder builder;
    tdx_zst_document document;
    tdx_zst_replay replay;
    tdx_zst_snapshot snapshot;
    tdx_zst_diff_mask changed = 0;
    tdx_error error;

    error.message[0] = '\0';
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "150000.000");
    sb_field(&builder, "08", "17.390000");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "150000.000");
    sb_field(&builder, "08", "17.3900");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    tdx_zst_replay_init(&replay);
    CHECK(tdx_zst_replay_apply(&replay, &document.records[0], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(changed == (TDX_ZST_DIFF_NEW | TDX_ZST_DIFF_TIME | TDX_ZST_DIFF_LAST), "mask 0x%x",
          changed);

    CHECK(tdx_zst_replay_apply(&replay, &document.records[1], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(changed == 0, "a re-typed price is not a change, mask 0x%x", changed);
    CHECK(snapshot.changed_id_count == 0,
          "no tag differs as text either, got %zu", snapshot.changed_id_count);
    CHECK(snapshot.last_price == 17.39, "last %.4f", snapshot.last_price);

    tdx_zst_replay_free(&replay);
    tdx_zst_document_free(&document);
}

static void test_replay_groups(void) {
    stream_builder builder;
    tdx_zst_document document;
    tdx_zst_replay replay;
    tdx_zst_snapshot snapshot;
    tdx_zst_diff_mask changed = 0;
    tdx_error error;

    error.message[0] = '\0';
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93000.000");
    sb_field(&builder, "10", "1000");
    sb_field(&builder, "1A", "16000.00");
    sb_field(&builder, "09", "5");
    sb_field(&builder, "1G", "16.10");
    sb_field(&builder, "1H", "9000");
    sb_field(&builder, "1I", "16.20");
    sb_field(&builder, "1J", "8000");
    sb_field(&builder, "0D", "T0      ");
    sb_field(&builder, "Z3", "91");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93003.000");
    sb_field(&builder, "1H", "9100");
    sb_field(&builder, "Z3", "92");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    tdx_zst_replay_init(&replay);
    CHECK(tdx_zst_replay_apply(&replay, &document.records[0], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(changed & TDX_ZST_DIFF_VOLUME, "10 must report volume");
    CHECK(changed & TDX_ZST_DIFF_AMOUNT, "1A must report amount");
    CHECK(changed & TDX_ZST_DIFF_TRADES, "09 must report trades");
    CHECK(changed & TDX_ZST_DIFF_AGGREGATE, "1G..1J must report aggregate");
    CHECK(changed & TDX_ZST_DIFF_PHASE, "0D must report phase");
    CHECK(changed & TDX_ZST_DIFF_OTHER, "Z3 is unclassified and must report other");
    CHECK(snapshot.average_price == 16.0, "derived average %.4f", snapshot.average_price);
    CHECK(strcmp(snapshot.phase, "T0") == 0, "phase %s", snapshot.phase);
    CHECK(snapshot.total_bid == 9000 && snapshot.total_ask == 8000, "aggregate totals");

    CHECK(tdx_zst_replay_apply(&replay, &document.records[1], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(changed == (TDX_ZST_DIFF_TIME | TDX_ZST_DIFF_AGGREGATE | TDX_ZST_DIFF_OTHER),
          "mask 0x%x", changed);
    CHECK(snapshot.average_bid == 16.10, "aggregate must persist, got %.4f", snapshot.average_bid);

    tdx_zst_replay_free(&replay);
    tdx_zst_document_free(&document);
}

static void test_replay_two_securities(void) {
    stream_builder builder;
    tdx_zst_document document;
    tdx_zst_replay replay;
    tdx_zst_snapshot snapshot;
    tdx_zst_diff_mask changed = 0;
    tdx_error error;

    error.message[0] = '\0';
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93000.000");
    sb_field(&builder, "08", "17.39");
    sb_end(&builder);
    sb_key(&builder, "00000001");
    sb_field(&builder, "0T", "93000.000");
    sb_field(&builder, "08", "11.66");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93003.000");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    tdx_zst_replay_init(&replay);
    CHECK(tdx_zst_replay_apply(&replay, &document.records[0], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(changed & TDX_ZST_DIFF_NEW, "first sight of SZ000623 is new");
    CHECK(snapshot.key_market == 1 && strcmp(snapshot.code, "000623") == 0, "identity %s",
          snapshot.security_key);

    CHECK(tdx_zst_replay_apply(&replay, &document.records[1], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(changed & TDX_ZST_DIFF_NEW, "SZ000001 is new too");
    CHECK(snapshot.key_market == 0 && strcmp(snapshot.code, "000001") == 0, "identity %s",
          snapshot.security_key);
    CHECK(snapshot.last_price == 11.66, "second security last %.4f", snapshot.last_price);

    CHECK(tdx_zst_replay_apply(&replay, &document.records[2], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(snapshot.last_price == 17.39, "the first security must keep its own state, got %.4f",
          snapshot.last_price);
    CHECK(changed == (TDX_ZST_DIFF_TIME), "mask 0x%x", changed);
    CHECK(replay.count == 2, "two securities tracked, got %zu", replay.count);

    tdx_zst_replay_free(&replay);
    tdx_zst_document_free(&document);
}

/* --- JSON rendering --------------------------------------------------- */

static void test_json_rendering(void) {
    stream_builder builder;
    tdx_zst_document document;
    tdx_zst_replay replay;
    tdx_zst_snapshot snapshot;
    tdx_zst_diff_mask changed = 0;
    tdx_buf line;
    tdx_error error;

    error.message[0] = '\0';
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "92500.000");
    sb_field(&builder, "04", "16.7200");
    sb_field(&builder, "05", "16.790000");
    sb_field(&builder, "20", "16.600000");
    sb_field(&builder, "30", "700");
    sb_field(&builder, "25", "");
    sb_field(&builder, "0D", "B0      ");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    tdx_zst_replay_init(&replay);
    tdx_buf_init(&line);
    CHECK(tdx_zst_replay_apply(&replay, &document.records[0], &snapshot, &changed, &error) ==
              TDX_OK,
          "apply: %s", error.message);
    CHECK(tdx_zst_format_snapshot(&line, &snapshot, 0, changed, 0, &error) == TDX_OK,
          "render: %s", error.message);
    if (line.len) {
        char text[2048];
        size_t copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
        memcpy(text, line.data, copy);
        text[copy] = '\0';
        /* The market comes from the caller, never from the key digits: this
         * record's key says "01" and the requested security is Shenzhen. */
        CHECK(strstr(text, "\"security_id\":\"SZ000623\"") != NULL, "identity: %s", text);
        CHECK(strstr(text, "\"time\":\"09:25:00\"") != NULL, "time text");
        CHECK(strstr(text, "\"time_hhmmss\":92500") != NULL, "raw time");
        CHECK(strstr(text, "\"pre_close_price\":16.720000") != NULL, "previous close");
        CHECK(strstr(text, "\"last_price\":null") != NULL, "an unknown field must be null");
        CHECK(strstr(text, "\"open_price\":16.790000") != NULL, "open");
        CHECK(strstr(text, "\"phase\":\"B0\"") != NULL, "phase must lose its padding");
        CHECK(strstr(text, "\"buy_levels\":[{\"price\":16.600000,\"volume\":700}]") != NULL,
              "the ladder must carry only levels the stream delivered");
        CHECK(strstr(text, "\"sell_levels\":[]") != NULL, "an empty ladder must still render");
        CHECK(strstr(text, "\"fields\"") == NULL, "raw tags are opt in");
        CHECK(strchr(text, '\n') == NULL, "the caller owns the line separator");
    }

    /* The raw form carries every merged tag, which is how a caller can see the
     * tags this build does not name. */
    tdx_buf_clear(&line);
    CHECK(tdx_zst_format_snapshot(&line, &snapshot, 0, changed, 1, &error) == TDX_OK,
          "render: %s", error.message);
    if (line.len) {
        char text[4096];
        size_t copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
        memcpy(text, line.data, copy);
        text[copy] = '\0';
        CHECK(strstr(text, "\"fields\":{") != NULL, "raw tags must appear");
        CHECK(strstr(text, "\"0T\":\"92500.000\"") != NULL, "raw tag value");
        CHECK(strstr(text, "\"25\":\"\"") != NULL, "an empty placeholder must round trip");
    }

    tdx_buf_free(&line);
    tdx_zst_replay_free(&replay);
    tdx_zst_document_free(&document);
}

/* --- document walk and real sample ------------------------------------ */

typedef struct walk_context {
    size_t visits;
    size_t stopping_visit;
} walk_context;

static int counting_visit(void *context, const tdx_zst_snapshot *snapshot,
                          const tdx_zst_record *record, tdx_zst_diff_mask changed,
                          tdx_error *err) {
    walk_context *walk = (walk_context *)context;
    (void)snapshot;
    (void)record;
    (void)changed;
    (void)err;
    walk->visits++;
    if (walk->stopping_visit && walk->visits >= walk->stopping_visit)
        return TDX_ERR;
    return TDX_OK;
}

static void test_replay_document_walk(void) {
    stream_builder builder;
    tdx_zst_document document;
    walk_context walk;
    tdx_error error;
    size_t visited = 0;

    error.message[0] = '\0';
    sb_init(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93000.000");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93003.000");
    sb_end(&builder);
    sb_key(&builder, "01000623");
    sb_field(&builder, "0T", "93006.000");
    sb_end(&builder);
    parse_or_die(&builder, &document);

    memset(&walk, 0, sizeof(walk));
    CHECK(tdx_zst_replay_document(&document, counting_visit, &walk, &visited, &error) == TDX_OK,
          "walk failed: %s", error.message);
    CHECK(visited == 3 && walk.visits == 3, "visited %zu / %zu", visited, walk.visits);

    memset(&walk, 0, sizeof(walk));
    walk.stopping_visit = 2;
    CHECK(tdx_zst_replay_document(&document, counting_visit, &walk, &visited, &error) == TDX_ERR,
          "a stopping visitor must fail the walk");
    CHECK(visited == 2, "the walk must stop at the second record, got %zu", visited);

    tdx_zst_document_free(&document);
}

typedef struct sample_audit {
    size_t visits;
    size_t new_records;
    size_t silent_records;
    size_t dropped_fields;
    size_t volume_drops;
    size_t amount_drops;
    size_t trade_drops;
    size_t time_drops;
    size_t vwap_outside_range;
    size_t aggregate_misordered;
    size_t crossed_book;
    size_t deep_levels;
    size_t merged_tags;
    int first_open_index;
    int first_open_value;
    int have_previous;
    double previous_volume;
    double previous_amount;
    double previous_trades;
    double previous_time;
    double final_volume;
    double final_amount;
    double final_last;
    double final_open;
    double final_high;
    double final_low;
    double final_trades;
    double final_bid1;
    double final_bid1_volume;
    double final_ask1;
    double final_ask1_volume;
    double final_total_bid;
    double final_total_ask;
    double final_pe;
    double final_average_price;
} sample_audit;

static int audit_visit(void *context, const tdx_zst_snapshot *snapshot,
                       const tdx_zst_record *record, tdx_zst_diff_mask changed,
                       tdx_error *err) {
    sample_audit *audit = (sample_audit *)context;
    (void)record;
    (void)err;
    audit->visits++;
    if (changed & TDX_ZST_DIFF_NEW)
        audit->new_records++;
    if (changed == 0)
        audit->silent_records++;
    if (snapshot->dropped_fields)
        audit->dropped_fields = snapshot->dropped_fields;

    if (snapshot->present & TDX_ZST_HAVE_OPEN && audit->first_open_index < 0) {
        audit->first_open_index = (int)snapshot->record_index;
        audit->first_open_value = snapshot->time_hhmmss;
    }

    if (audit->have_previous) {
        if (snapshot->present & TDX_ZST_HAVE_VOLUME && snapshot->volume < audit->previous_volume)
            audit->volume_drops++;
        if (snapshot->present & TDX_ZST_HAVE_AMOUNT && snapshot->amount < audit->previous_amount)
            audit->amount_drops++;
        if (snapshot->present & TDX_ZST_HAVE_TRADE_COUNT &&
            snapshot->trade_count < audit->previous_trades)
            audit->trade_drops++;
        if ((double)snapshot->time_hhmmss < audit->previous_time)
            audit->time_drops++;
    }
    audit->have_previous = 1;
    if (snapshot->present & TDX_ZST_HAVE_VOLUME)
        audit->previous_volume = snapshot->volume;
    if (snapshot->present & TDX_ZST_HAVE_AMOUNT)
        audit->previous_amount = snapshot->amount;
    if (snapshot->present & TDX_ZST_HAVE_TRADE_COUNT)
        audit->previous_trades = snapshot->trade_count;
    audit->previous_time = snapshot->time_hhmmss;

    if (snapshot->present & TDX_ZST_HAVE_AVERAGE_PRICE) {
        double average = snapshot->average_price;
        double low = snapshot->low_price;
        double high = snapshot->high_price;
        if (average < low - 1e-6 || average > high + 1e-6)
            audit->vwap_outside_range++;
    }
    if ((snapshot->present & (TDX_ZST_HAVE_AVERAGE_BID | TDX_ZST_HAVE_LAST |
                              TDX_ZST_HAVE_AVERAGE_ASK)) ==
            (TDX_ZST_HAVE_AVERAGE_BID | TDX_ZST_HAVE_LAST | TDX_ZST_HAVE_AVERAGE_ASK) &&
        !(snapshot->average_bid - 1e-6 <= snapshot->last_price &&
          snapshot->last_price <= snapshot->average_ask + 1e-6))
        audit->aggregate_misordered++;
    if ((snapshot->bid_levels & 1u) && (snapshot->ask_levels & 1u) && snapshot->bids[0].price > 0 &&
        snapshot->asks[0].price > 0 && snapshot->bids[0].price > snapshot->asks[0].price + 1e-6)
        audit->crossed_book++;
    if (snapshot->bid_levels & ~0x1Fu || snapshot->ask_levels & ~0x1Fu)
        audit->deep_levels++;
    audit->merged_tags = snapshot->field_count;

    audit->final_volume = snapshot->volume;
    audit->final_amount = snapshot->amount;
    audit->final_last = snapshot->last_price;
    audit->final_open = snapshot->open_price;
    audit->final_high = snapshot->high_price;
    audit->final_low = snapshot->low_price;
    audit->final_trades = snapshot->trade_count;
    audit->final_bid1 = snapshot->bids[0].price;
    audit->final_bid1_volume = snapshot->bids[0].volume;
    audit->final_ask1 = snapshot->asks[0].price;
    audit->final_ask1_volume = snapshot->asks[0].volume;
    audit->final_total_bid = snapshot->total_bid;
    audit->final_total_ask = snapshot->total_ask;
    audit->final_pe = snapshot->pe_ratio;
    audit->final_average_price = snapshot->average_price;
    return TDX_OK;
}

static int close_enough(double left, double right, double tolerance) {
    double delta = left - right;
    if (delta < 0)
        delta = -delta;
    return delta <= tolerance;
}

/* Every number below was measured with output/zst_field_table.py; they are the
 * cross-check between this C replay and the Python oracle. */
static void test_real_sample(const char *directory) {
    char path[512];
    tdx_zst_document document;
    tdx_error error;
    sample_audit audit;
    size_t visited = 0;

    snprintf(path, sizeof(path), "%s/sz000623_20260612.img", directory);
    error.message[0] = '\0';
    tdx_zst_document_init(&document);
    if (tdx_zst_decode_file(path, &document, &error) != TDX_OK) {
        printf("skip: real sample %s is not readable (%s)\n", path, error.message);
        tdx_zst_document_free(&document);
        return;
    }

    CHECK(document.count == 4647, "record count %zu", document.count);
    CHECK(document.inflated_size == 590772, "inflated %zu", document.inflated_size);
    CHECK(document.compressed_size == 141203, "compressed %zu", document.compressed_size);
    if (document.count != 4647) {
        tdx_zst_document_free(&document);
        return;
    }
    CHECK(strcmp(document.records[0].security_key, "01000623") == 0, "key %s",
          document.records[0].security_key);
    CHECK(strcmp(document.records[0].security_key,
                 document.records[document.count - 1].security_key) == 0,
          "the file must stay on one security");

    memset(&audit, 0, sizeof(audit));
    audit.first_open_index = -1;
    CHECK(tdx_zst_replay_document(&document, audit_visit, &audit, &visited, &error) == TDX_OK,
          "replay failed: %s", error.message);
    CHECK(visited == 4647, "replayed %zu records", visited);
    CHECK(audit.new_records == 1, "exactly one NEW record, got %zu", audit.new_records);

    /* invariants that a wrong field binding cannot satisfy */
    CHECK(audit.volume_drops == 0, "cumulative volume dropped %zu times", audit.volume_drops);
    CHECK(audit.amount_drops == 0, "cumulative amount dropped %zu times", audit.amount_drops);
    CHECK(audit.trade_drops == 0, "trade count dropped %zu times", audit.trade_drops);
    CHECK(audit.time_drops == 0, "time went backwards %zu times", audit.time_drops);
    CHECK(audit.vwap_outside_range == 0, "amount/volume left [low,high] %zu times",
          audit.vwap_outside_range);
    CHECK(audit.aggregate_misordered == 0, "average bid/ask did not bracket last %zu times",
          audit.aggregate_misordered);
    CHECK(audit.crossed_book == 0, "book crossed %zu times", audit.crossed_book);
    CHECK(audit.deep_levels == 0, "levels 6..10 must stay empty, %zu records disagreed",
          audit.deep_levels);
    CHECK(audit.dropped_fields == 0, "the tag map dropped %zu tags", audit.dropped_fields);
    CHECK(audit.merged_tags == 71, "merged tag count %zu", audit.merged_tags);

    /* the pre-open header, the 09:25 auction open and the closing state */
    CHECK(audit.first_open_index == 24, "open first seen at record %d, time %d",
          audit.first_open_index, audit.first_open_value);
    CHECK(audit.first_open_value == 92500, "open time %d", audit.first_open_value);
    CHECK(close_enough(audit.final_last, 17.39, 1e-9), "last %.4f", audit.final_last);
    CHECK(close_enough(audit.final_open, 16.79, 1e-9), "open %.4f", audit.final_open);
    CHECK(close_enough(audit.final_high, 17.49, 1e-9), "high %.4f", audit.final_high);
    CHECK(close_enough(audit.final_low, 16.73, 1e-9), "low %.4f", audit.final_low);
    CHECK(close_enough(audit.final_volume, 16826061, 1e-6), "volume %.0f", audit.final_volume);
    CHECK(close_enough(audit.final_amount, 290028487.9, 1e-3), "amount %.1f", audit.final_amount);
    CHECK(close_enough(audit.final_trades, 23175, 1e-9), "trade count %.0f", audit.final_trades);
    CHECK(close_enough(audit.final_pe, 8.68, 1e-9), "pe %.4f", audit.final_pe);
    CHECK(close_enough(audit.final_bid1, 17.38, 1e-9), "bid1 %.4f", audit.final_bid1);
    CHECK(close_enough(audit.final_bid1_volume, 39800, 1e-9), "bid1 volume %.0f",
          audit.final_bid1_volume);
    CHECK(close_enough(audit.final_ask1, 17.39, 1e-9), "ask1 %.4f", audit.final_ask1);
    CHECK(close_enough(audit.final_ask1_volume, 34700, 1e-9), "ask1 volume %.0f",
          audit.final_ask1_volume);
    CHECK(close_enough(audit.final_total_bid, 839685, 1e-9), "total bid %.0f",
          audit.final_total_bid);
    CHECK(close_enough(audit.final_total_ask, 2193166, 1e-9), "total ask %.0f",
          audit.final_total_ask);
    CHECK(close_enough(audit.final_average_price, 17.23691, 1e-4), "vwap %.5f",
          audit.final_average_price);

    tdx_zst_document_free(&document);
}

int main(int argc, char **argv) {
    const char *directory = argc > 1 ? argv[1] : getenv("TDX_ZST_SAMPLE_DIR");
    if (!directory || !*directory)
        directory = "C:/new_tdx/T0002/zst_cache";

    test_parse_stream();
    test_parse_rejects();
    test_value_equal();
    test_state_apply();
    test_replay_folds_forward();
    test_replay_precision_noise();
    test_replay_groups();
    test_replay_two_securities();
    test_replay_document_walk();
    test_json_rendering();
    test_real_sample(directory);

    if (failures) {
        printf("%d zst check(s) failed\n", failures);
        return 1;
    }
    printf("zst checks passed\n");
    return 0;
}
