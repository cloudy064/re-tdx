/* test_subscription.c - convertible-bond subscription events.
 *
 * The fixture is a real capture: bi/list/func_kkzss101_1.jsn reduced by generator to
 * its verbatim colheader plus three rows.  The derived metrics are asserted against
 * the CAPTURED inputs, because the point of them is the arithmetic on real numbers
 * rather than on numbers chosen to come out round:
 *
 *     row 0: 4.950 * 100 / 6.170  = 80.226904, and (106.368 - 80.226904) * 100 /
 *            80.226904 = 32.583952
 *
 * The two divisions are guarded, and the guards are tested with inputs that are not in
 * the capture but that a caller could meet: a zero conversion price, a zero conversion
 * value, and a non-finite input.  A guard that lets an infinity through would print as
 * a number, which is why they are checked directly rather than only through a row.
 *
 * The mapping itself was cross-checked against the listed view live: the subscription
 * document's zgj and the overview's ZGJ are the same bond's conversion price in two
 * independent resources, and all 314 bonds present in both agreed exactly. */
#include <stdio.h>
#include <string.h>

#include "tdx_subscription.h"
#include "tdx_subscription_json.h"
#include "tdx_jsn.h"
#include "subscription_fixtures.h"
#include "render_check.h"

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

static tdx_error error;

static const char *text_of(const tdx_buf *buffer) {
    static char scratch[8192];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

static const char *bond_text(const tdx_bond_text *text) {
    static char scratch[256];
    if (!text->present || !text->data)
        return "(absent)";
    if (text->length >= sizeof(scratch))
        return "(too long)";
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return scratch;
}

/* The captured names, as UTF-8 escapes.  A hex escape eats every following hex digit,
 * so each sequence ends its own literal. */
#define NAME_0 "\xe5\x8d\x97\xe8\x88\xaa\xe8\xbd\xac\xe5\x80\xba"
#define NAME_1 "\xe8\xb4\xb5\xe7\x87\x83\xe8\xbd\xac\xe5\x80\xba"
#define NAME_2 "\xe5\xa4\xa9\xe4\xb8\x9a\xe8\xbd\xac\xe5\x80\xba"

static void test_derived_guards(void) {
    double value = 0.0;
    double premium = 0.0;

    /* The ordinary case, on numbers from the capture. */
    CHECK(tdx_subscription_conversion_value(4.950, 6.170, &value) == 1,
          "the conversion value is defined when both inputs are");
    CHECK(value > 80.22690 && value < 80.22691,
          "4.95 * 100 / 6.17 = 80.226904, got %.6f", value);
    CHECK(tdx_subscription_premium(106.368, value, &premium) == 1,
          "the premium is defined when both inputs are");
    CHECK(premium > 32.58395 && premium < 32.58396,
          "(106.368 - 80.226904) * 100 / 80.226904 = 32.583952, got %.6f", premium);

    /* A zero conversion price is not a price: the field must be ABSENT, not infinite
     * and not zero. */
    value = 1.0;
    CHECK(tdx_subscription_conversion_value(4.950, 0.0, &value) == 0,
          "a zero conversion price leaves the value undefined");
    CHECK(tdx_subscription_conversion_value(4.950, -0.0, &value) == 0,
          "and so does a negative zero");
    /* A zero conversion value would divide by zero in the premium. */
    CHECK(tdx_subscription_premium(106.368, 0.0, &premium) == 0,
          "a zero conversion value leaves the premium undefined");
    /* A non-finite input must not produce a finite-looking result. */
    CHECK(tdx_subscription_conversion_value(4.950, 1.0 / 0.0, &value) == 0,
          "an infinite conversion price is refused");
    CHECK(tdx_subscription_premium(1.0 / 0.0, 80.0, &premium) == 0,
          "an infinite bond close is refused");
    /* A null output is refused rather than dereferenced. */
    CHECK(tdx_subscription_conversion_value(4.95, 6.17, NULL) == 0,
          "a null output is refused");
    CHECK(tdx_subscription_premium(106.368, 80.0, NULL) == 0, "and so is a null one here");
}

static void test_normalize_captured(void) {
    tdx_jsn_document document;
    tdx_subscription_row rows[SUBSCRIPTION_FIXTURE_ROWS + 2];
    size_t count = 0;
    size_t skipped = 0;

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    CHECK(tdx_jsn_parse((const uint8_t *)subscription_list, strlen(subscription_list), &document,
                        &error) == TDX_OK,
          "the fixture parses: %s", error.message);
    CHECK(document.groups[0].column_count == SUBSCRIPTION_FIXTURE_COLUMNS, "%d columns, got %zu",
          SUBSCRIPTION_FIXTURE_COLUMNS, document.groups[0].column_count);
    /* Sixteen: the resource's own columns.  An earlier count of seventeen came from
     * including the JSONL wrapper's "type" field in the column list. */
    CHECK(SUBSCRIPTION_FIXTURE_COLUMNS == 16, "the capture had 16 columns, header says %d",
          SUBSCRIPTION_FIXTURE_COLUMNS);
    CHECK(SUBSCRIPTION_FIXTURE_SOURCE_ROWS == 319, "and 319 rows, header says %d",
          SUBSCRIPTION_FIXTURE_SOURCE_ROWS);

    CHECK(tdx_subscription_normalize(&document, &document.groups[0], rows,
                                     SUBSCRIPTION_FIXTURE_ROWS + 2, &count, &skipped,
                                     &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(count == SUBSCRIPTION_FIXTURE_ROWS, "all %d rows map, got %zu",
          SUBSCRIPTION_FIXTURE_ROWS, count);
    CHECK(skipped == 0, "and none is skipped, got %zu", skipped);

    /* Row 0: China Southern's convertible bond and the stock it converts into. */
    CHECK(strcmp(rows[0].bond_security_id, "SH110075") == 0, "the bond is SH110075, got %s",
          rows[0].bond_security_id);
    CHECK(strcmp(bond_text(&rows[0].bond_name), NAME_0) == 0,
          "the bond name decodes, got %s", bond_text(&rows[0].bond_name));
    CHECK(strcmp(rows[0].stock_security_id, "SH600029") == 0,
          "the underlying is SH600029, got %s", rows[0].stock_security_id);
    CHECK(strcmp(bond_text(&rows[0].subscription_date), "20201015") == 0,
          "the subscription date is 20201015, got %s",
          bond_text(&rows[0].subscription_date));
    CHECK(strcmp(bond_text(&rows[0].subscription_code), "733029") == 0,
          "the subscription code is 733029, got %s", bond_text(&rows[0].subscription_code));
    CHECK(rows[0].has_subscription_limit_10k_yuan &&
              rows[0].subscription_limit_10k_yuan == 100.0,
          "the limit is 1m yuan, got %f", rows[0].subscription_limit_10k_yuan);
    CHECK(strcmp(bond_text(&rows[0].conversion_start_date), "20210421") == 0,
          "conversion opens 20210421");
    CHECK(rows[0].listed == 1, "it is listed");
    CHECK(strcmp(bond_text(&rows[0].listing_date), "20201103") == 0, "listed 20201103");
    CHECK(rows[0].has_lottery_rate_pct && rows[0].lottery_rate_pct > 0.0453 &&
              rows[0].lottery_rate_pct < 0.0454,
          "the lottery rate is 0.04534376, got %.8f", rows[0].lottery_rate_pct);
    CHECK(rows[0].has_issue_size_100m_yuan && rows[0].issue_size_100m_yuan == 160.0,
          "the issue size is 16bn yuan, got %f", rows[0].issue_size_100m_yuan);

    /* THE DERIVED METRICS, from the captured inputs. */
    CHECK(rows[0].has_conversion_value_yuan && rows[0].conversion_value_yuan > 80.22690 &&
              rows[0].conversion_value_yuan < 80.22691,
          "the conversion value is 80.226904, got %.6f", rows[0].conversion_value_yuan);
    CHECK(rows[0].has_conversion_premium_pct && rows[0].conversion_premium_pct > 32.58395 &&
              rows[0].conversion_premium_pct < 32.58396,
          "the premium is 32.583952 per cent, got %.6f", rows[0].conversion_premium_pct);

    /* Row 1 and row 2 keep the arithmetic honest on different numbers. */
    CHECK(strcmp(rows[1].bond_security_id, "SH110084") == 0, "row 1 is SH110084");
    CHECK(strcmp(bond_text(&rows[1].bond_name), NAME_1) == 0, "its name decodes, got %s",
          bond_text(&rows[1].bond_name));
    CHECK(rows[1].has_conversion_value_yuan && rows[1].conversion_value_yuan > 105.7632 &&
              rows[1].conversion_value_yuan < 105.7633,
          "its conversion value is 105.763240, got %.6f", rows[1].conversion_value_yuan);
    CHECK(rows[2].has_conversion_value_yuan && rows[2].conversion_value_yuan > 83.9285 &&
              rows[2].conversion_value_yuan < 83.9286,
          "row 2's conversion value is 83.928571, got %.6f", rows[2].conversion_value_yuan);
    CHECK(strcmp(bond_text(&rows[2].bond_name), NAME_2) == 0, "row 2's name decodes");

    /* The event id, built the reference's way. */
    CHECK(strcmp(rows[0].event_id, "convertible-subscription:1:110075:20201015") == 0,
          "the event id is built from the market, the code and the date, got %s",
          rows[0].event_id);
    /* The 100 in the conversion value is the face value, so a bond whose stock trades
     * at ten times its conversion price converts to about a thousand yuan. */
    {
        double value = 0.0;
        CHECK(tdx_subscription_conversion_value(61.70, 6.17, &value) == 1 &&
                  value > 999.9 && value < 1000.1,
              "ten times the conversion price gives a value of about 1000, got %.4f", value);
    }

    tdx_jsn_document_free(&document);
}

static void test_normalize_skips(void) {
    /* SYNTHETIC: the rows a subscription event cannot survive losing.  BOTH the bond
     * and the stock identity have to be readable, so each is broken in turn. */
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"$ZQDM1\",\"$SC1\",\"sgrq\"],\"data\":["
        "[\"110075\",\"1\",\"600029\",\"1\",\"ok\"],"
        "[\"11007\",\"1\",\"600029\",\"1\",\"short bond code\"],"
        "[\"110075\",\"1\",\"600029\",\"sh\",\"bad stock market\"],"
        "[\"110075\",\"sz\",\"600029\",\"1\",\"bad bond market\"],"
        "[\"110076\",\"1\",\"600030\",\"1\",\"ok too\"]]}]";
    tdx_jsn_document document;
    tdx_subscription_row rows[8];
    size_t count = 0;
    size_t skipped = 0;

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &document, &error) != TDX_OK) {
        CHECK(0, "parses: %s", error.message);
        tdx_jsn_document_free(&document);
        return;
    }
    CHECK(tdx_subscription_normalize(&document, &document.groups[0], rows, 8, &count, &skipped,
                                     &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(count == 2, "only the two complete events survive, got %zu", count);
    CHECK(skipped == 3, "three are skipped and counted, got %zu", skipped);
    if (count == 2)
        CHECK(strcmp(rows[0].bond_security_id, "SH110075") == 0 &&
                  strcmp(rows[1].bond_security_id, "SH110076") == 0,
              "the two kept events are the complete ones");

    /* A row with no inputs at all still produces an event with absent metrics, rather
     * than being skipped: the identity is what makes it an event. */
    {
        static const char *bare =
            "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"$ZQDM1\",\"$SC1\"],"
            "\"data\":[[\"110075\",\"1\",\"600029\",\"1\"]]}]";
        tdx_jsn_document other;
        tdx_jsn_document_init(&other);
        if (tdx_jsn_parse((const uint8_t *)bare, strlen(bare), &other, &error) == TDX_OK) {
            CHECK(tdx_subscription_normalize(&other, &other.groups[0], rows, 8, &count, &skipped,
                                             &error) == TDX_OK,
                  "normalize: %s", error.message);
            CHECK(count == 1, "the identity alone is enough, got %zu", count);
            CHECK(rows[0].has_conversion_value_yuan == 0 && rows[0].has_conversion_premium_pct == 0,
                  "and both metrics are absent rather than zero");
            CHECK(rows[0].listed == 0, "and it is not listed");
            /* The event id ends in a colon when there is no date, which is what the
             * reference produces too. */
            CHECK(strcmp(rows[0].event_id, "convertible-subscription:1:110075:") == 0,
                  "an absent date leaves a trailing colon, got %s", rows[0].event_id);
        } else {
            CHECK(0, "parses: %s", error.message);
        }
        tdx_jsn_document_free(&other);
    }
    tdx_jsn_document_free(&document);
}

static void test_rendering(void) {
    tdx_jsn_document document;
    tdx_subscription_row rows[SUBSCRIPTION_FIXTURE_ROWS + 2];
    size_t count = 0;
    size_t skipped = 0;
    tdx_buf line;
    const char *text;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    tdx_buf_init(&line);
    if (tdx_jsn_parse((const uint8_t *)subscription_list, strlen(subscription_list), &document,
                      &error) != TDX_OK ||
        tdx_subscription_normalize(&document, &document.groups[0], rows,
                                   SUBSCRIPTION_FIXTURE_ROWS + 2, &count, &skipped,
                                   &error) != TDX_OK) {
        CHECK(0, "prepare: %s", error.message);
        goto done;
    }
    CHECK(tdx_subscription_format(&line, &rows[0], TDX_SUBSCRIPTION_RESOURCE, 0, &error) == TDX_OK,
          "render: %s", error.message);
    text = text_of(&line);
    for (cursor = text; *cursor; ++cursor) {
        if (in_string) {
            if (escaped)
                escaped = 0;
            else if (*cursor == '\\')
                escaped = 1;
            else if (*cursor == '"')
                in_string = 0;
            continue;
        }
        if (*cursor == '"')
            in_string = 1;
        else if (*cursor == '{')
            depth++;
        else if (*cursor == '}') {
            depth--;
            CHECK(depth >= 0, "the row must not close early");
        }
    }
    CHECK(depth == 0 && !in_string, "the subscription row is balanced, depth %d in_string %d",
          depth, in_string);
    {
        /* Balanced is not the same as parseable: this assertion is what caught a Windows
         * path reaching the JSON through a raw %s, and a CRC printed without quotes. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"type\":\"convertible_bond_subscription\"") != NULL, "the type");
    CHECK(strstr(text, "\"event_id\":\"convertible-subscription:1:110075:20201015\"") != NULL,
          "the event id");
    CHECK(strstr(text, "\"bond\":{\"market\":\"sh\"") != NULL, "the bond identity");
    CHECK(strstr(text, "\"underlying\":{\"market\":\"sh\"") != NULL, "the underlying identity");
    CHECK(strstr(text, "\"conversion_value_yuan\":80.226904") != NULL,
          "the derived conversion value: %s", text);
    CHECK(strstr(text, "\"conversion_premium_pct\":32.583952") != NULL, "the derived premium");
    CHECK(strstr(text, "\"listed\":true") != NULL, "and the listed flag");

    tdx_buf_clear(&line);
    CHECK(tdx_subscription_format_summary(&line, 319, 0, 315, 319, 319,
                                          TDX_SUBSCRIPTION_RESOURCE, "1.2.3.4:7709",
                                          &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(strstr(text_of(&line), "\"type\":\"convertible_subscription_summary\"") != NULL,
          "the summary type");
    CHECK(strstr(text_of(&line), "\"rows_with_premium\":319") != NULL, "the premium count");
    tdx_buf_clear(&line);
    CHECK(tdx_subscription_format_summary(&line, 0, 0, 0, 0, 0, NULL, NULL, &error) == TDX_OK,
          "an empty summary renders");
    CHECK(strstr(text_of(&line), "\"endpoint\":null") != NULL, "a missing endpoint is null");

done:
    tdx_jsn_document_free(&document);
    tdx_buf_free(&line);
}

int main(void) {
    test_derived_guards();
    test_normalize_captured();
    test_normalize_skips();
    test_rendering();

    if (failures) {
        printf("%d subscription check(s) failed\n", failures);
        return 1;
    }
    printf("subscription checks passed\n");
    return 0;
}
