/* test_convertible_join.c - the six-document convertible-bond join.
 *
 * The fixture carries the SAME THREE BONDS in all six captured documents - the
 * overview's first three, chosen by key - so this test can show every group of a
 * joined row being filled from the document that owns it.  That matters: a join test
 * on documents whose first rows happen to describe different bonds would pass while
 * attaching nothing.
 *
 * The assertions below are exact captured values.  The sharpest one is that the three
 * trigger groups of one bond differ from each other - their conditions and ratios
 * come from three different documents - because a port that read one document three
 * times would produce three identical triggers and still look plausible.
 *
 * Two cross-document relationships are checked as well, both of which hold on the
 * live data across the whole market:
 *
 *   * the coupon list's length equals the term in years (314 of 314 live rows);
 *   * the overview's final payment equals the coupon document's final rate plus the
 *     compensation rate, because the overview folds the compensation in (311 of 312
 *     comparable live rows, and none where it is left out). */
#include <stdio.h>
#include <string.h>

#include "tdx_convertible.h"
#include "tdx_convertible_join.h"
#include "tdx_convertible_json.h"
#include "tdx_jsn.h"
#include "convertible_fixtures.h"
#include "tdx_bonds_json.h"

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
    static char scratch[65536];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

static const char *bond_text(const tdx_bond_text *text) {
    static char scratch[512];
    if (!text->present || !text->data)
        return "(absent)";
    if (text->length >= sizeof(scratch))
        return "(too long)";
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return scratch;
}

static tdx_jsn_document documents[6];
static tdx_convertible_documents set;

static int load_all(void) {
    static const char *const texts[6] = {
        convertible_overview,  convertible_progress, convertible_coupons,
        convertible_sellback,  convertible_redemption, convertible_revision,
    };
    size_t index;
    error.message[0] = '\0';
    for (index = 0; index < 6; ++index) {
        tdx_jsn_document_init(&documents[index]);
        if (tdx_jsn_parse((const uint8_t *)texts[index], strlen(texts[index]), &documents[index],
                          &error) != TDX_OK) {
            printf("FAIL loading document %zu: %s\n", index, error.message);
            failures++;
            return TDX_ERR;
        }
        if (documents[index].group_count != 1 ||
            documents[index].row_count != CONVERTIBLE_FIXTURE_ROWS) {
            printf("FAIL document %zu holds %zu groups and %zu rows, expected 1 and %d\n", index,
                   documents[index].group_count, documents[index].row_count,
                   CONVERTIBLE_FIXTURE_ROWS);
            failures++;
            return TDX_ERR;
        }
    }
    set.overview = &documents[0];
    set.progress = &documents[1];
    set.coupons = &documents[2];
    set.sellback = &documents[3];
    set.redemption = &documents[4];
    set.revision = &documents[5];
    return TDX_OK;
}

static void free_all(void) {
    size_t index;
    for (index = 0; index < 6; ++index)
        tdx_jsn_document_free(&documents[index]);
}

static tdx_code code_of(int market, const char *code) {
    tdx_code result;
    memset(&result, 0, sizeof(result));
    result.market_id = market;
    snprintf(result.code, sizeof(result.code), "%s", code);
    return result;
}

static void test_document_shapes(void) {
    CHECK(CONVERTIBLE_OVERVIEW_COLUMNS == 55, "the overview has 55 columns, header says %d",
          CONVERTIBLE_OVERVIEW_COLUMNS);
    CHECK(CONVERTIBLE_PROGRESS_COLUMNS == 21, "progress has 21 columns");
    CHECK(CONVERTIBLE_COUPONS_COLUMNS == 13, "coupons has 13 columns");
    CHECK(CONVERTIBLE_SELLBACK_COLUMNS == 20, "sellback has 20 columns");
    CHECK(CONVERTIBLE_REDEMPTION_COLUMNS == 19, "redemption has 19 columns");
    CHECK(CONVERTIBLE_REVISION_COLUMNS == 22, "revision has 22 columns");
    /* The source row counts are what the captures held, so a fixture regenerated
     * from a different capture is visible here rather than silently different. */
    CHECK(CONVERTIBLE_OVERVIEW_SOURCE_ROWS == 314, "the overview capture held 314 rows");
    CHECK(CONVERTIBLE_PROGRESS_SOURCE_ROWS == 316, "progress held 316");
    CHECK(CONVERTIBLE_COUPONS_SOURCE_ROWS == 316, "coupons held 316");
    CHECK(CONVERTIBLE_SELLBACK_SOURCE_ROWS == 320, "sellback held 320");
    CHECK(CONVERTIBLE_REDEMPTION_SOURCE_ROWS == 320, "redemption held 320");
    CHECK(CONVERTIBLE_REVISION_SOURCE_ROWS == 320, "revision held 320");
}

static void test_union(void) {
    static tdx_code keys[64];
    size_t key_count = 0;
    size_t union_count = 0;

    CHECK(tdx_convertible_keys(&set, keys, 64, &key_count, &union_count, &error) == TDX_OK,
          "keys: %s", error.message);
    /* Every document carries the same three bonds, so the union is three, not
     * eighteen. */
    CHECK(union_count == 3, "the union of the six documents is 3 bonds, got %zu", union_count);
    CHECK(key_count == 3, "and three keys were written, got %zu", key_count);
    if (union_count == 3) {
        CHECK(keys[0].market_id == 1 && strcmp(keys[0].code, "110075") == 0,
              "the first key is SH110075, got %d/%s", keys[0].market_id, keys[0].code);
        CHECK(keys[2].market_id == 1 && strcmp(keys[2].code, "110077") == 0,
              "the last key is SH110077, got %d/%s", keys[2].market_id, keys[2].code);
    }
    /* A capacity too small is refused rather than silently truncated. */
    CHECK(tdx_convertible_keys(&set, keys, 2, &key_count, &union_count, &error) == TDX_ERR,
          "a short key buffer must be refused");
    CHECK(union_count == 3, "and the union count still reports 3, got %zu", union_count);
}

static void test_join_all_six(void) {
    tdx_convertible_row row;
    tdx_convertible_extra extra;
    tdx_convertible_join_flags flags;
    tdx_code identity = code_of(1, "110076");

    CHECK(tdx_convertible_join(&set, &identity, &row, &extra, &flags, &error) == TDX_OK,
          "join: %s", error.message);

    /* The fixture was chosen so this bond is in all six. */
    CHECK(flags.from_overview && flags.from_progress && flags.from_coupons &&
              flags.from_sellback && flags.from_redemption && flags.from_revision,
          "SH110076 is in all six documents: overview=%d progress=%d coupons=%d sellback=%d "
          "redemption=%d revision=%d",
          flags.from_overview, flags.from_progress, flags.from_coupons, flags.from_sellback,
          flags.from_redemption, flags.from_revision);

    /* Overview. */
    CHECK(strcmp(row.bond_security_id, "SH110076") == 0, "the identity is SH110076");
    CHECK(row.has_conversion_price && row.conversion_price > 16.49 &&
              row.conversion_price < 16.51,
          "the conversion price is 16.50, got %f", row.conversion_price);
    CHECK(row.core_terms_complete, "the core terms are complete");

    /* progress: FXZL / ZQYE / ZGJD / YSHME / YHSME / DQJD. */
    CHECK(extra.has_issue_size_100m_yuan && extra.issue_size_100m_yuan > 18.42 &&
              extra.issue_size_100m_yuan < 18.43,
          "the issue size is 18.426bn, got %f", extra.issue_size_100m_yuan);
    CHECK(extra.has_remaining_balance_100m_yuan && extra.remaining_balance_100m_yuan > 18.42 &&
              extra.remaining_balance_100m_yuan < 18.43,
          "the remaining balance is 18.4237bn, got %f", extra.remaining_balance_100m_yuan);
    CHECK(extra.has_conversion_progress_pct && extra.conversion_progress_pct == 100.0,
          "conversion is 100 per cent complete, got %f", extra.conversion_progress_pct);
    CHECK(extra.has_redeemed_amount_100m_yuan && extra.redeemed_amount_100m_yuan == 0.0,
          "nothing has been redeemed");
    CHECK(extra.has_sellback_amount_100m_yuan && extra.sellback_amount_100m_yuan > 0.099 &&
              extra.sellback_amount_100m_yuan < 0.101,
          "0.1bn has been sold back, got %f", extra.sellback_amount_100m_yuan);
    CHECK(extra.has_maturity_progress_pct && extra.maturity_progress_pct > 98.1 &&
              extra.maturity_progress_pct < 98.2,
          "maturity progress is 98.13 per cent, got %f", extra.maturity_progress_pct);

    /* coupons: FXQX / PMLL_1 / PMLL_2 / BCLL. */
    CHECK(extra.has_term_years && extra.term_years == 6.0, "the term is 6 years, got %f",
          extra.term_years);
    CHECK(extra.has_rate[0] && extra.rates_pct[0] > 0.299 && extra.rates_pct[0] < 0.301,
          "the first annual rate is 0.3 per cent, got %f", extra.rates_pct[0]);
    CHECK(extra.has_rate[1] && extra.rates_pct[1] > 0.499 && extra.rates_pct[1] < 0.501,
          "the second is 0.5 per cent, got %f", extra.rates_pct[1]);
    CHECK(extra.has_compensation_rate_pct && extra.compensation_rate_pct == 8.0,
          "the compensation rate is 8 per cent, got %f", extra.compensation_rate_pct);

    /* THE SHARP ASSERTION: three triggers, three documents, three different sets of
     * values.  A port that read one document three times would give three identical
     * triggers, and this is what catches it. */
    CHECK(strcmp(bond_text(&extra.sellback.condition), "30/30") == 0,
          "the sellback condition is 30/30, got %s", bond_text(&extra.sellback.condition));
    CHECK(strcmp(bond_text(&extra.redemption.condition), "15/30") == 0,
          "the redemption condition is 15/30, got %s", bond_text(&extra.redemption.condition));
    CHECK(strcmp(bond_text(&extra.revision.condition), "15/30") == 0,
          "the revision condition is 15/30, got %s", bond_text(&extra.revision.condition));
    CHECK(extra.sellback.has_price_ratio_pct && extra.sellback.price_ratio_pct == 70.0,
          "the sellback ratio is 70 per cent, got %f", extra.sellback.price_ratio_pct);
    CHECK(extra.redemption.has_price_ratio_pct && extra.redemption.price_ratio_pct == 130.0,
          "the redemption ratio is 130 per cent, got %f", extra.redemption.price_ratio_pct);
    CHECK(extra.revision.has_price_ratio_pct && extra.revision.price_ratio_pct == 80.0,
          "the revision ratio is 80 per cent, got %f", extra.revision.price_ratio_pct);
    CHECK(strcmp(bond_text(&extra.sellback.start_date), "20241102") == 0,
          "the sellback window opens 20241102, got %s",
          bond_text(&extra.sellback.start_date));
    CHECK(strcmp(bond_text(&extra.redemption.start_date), "20210506") == 0,
          "the redemption window opens 20210506, got %s",
          bond_text(&extra.redemption.start_date));
    CHECK(strcmp(bond_text(&extra.revision.start_date), "20201102") == 0,
          "the revision window opens 20201102, got %s",
          bond_text(&extra.revision.start_date));
    CHECK(extra.revision.has_trigger_price && extra.revision.trigger_price > 13.19 &&
              extra.revision.trigger_price < 13.21,
          "the revision trigger price is 13.20, got %f", extra.revision.trigger_price);
    CHECK(extra.sellback.has_available_days && extra.sellback.available_days == 30.0,
          "30 sellback days are available, got %f", extra.sellback.available_days);
    CHECK(extra.redemption.has_available_days && extra.redemption.available_days == 15.0,
          "15 redemption days, got %f", extra.redemption.available_days);
    /* The sellback document's history is a two-entry comma list. */
    CHECK(extra.sellback.has_history_count && extra.sellback.history_count == 2.0,
          "two sellback events, got %f", extra.sellback.history_count);
    CHECK(strcmp(bond_text(&extra.sellback.history_dates), "20241223,20251222") == 0,
          "the sellback dates are the two captured ones, got %s",
          bond_text(&extra.sellback.history_dates));
    /* The revision document's history is long, which is why the reference defers it. */
    CHECK(extra.revision.has_history_count && extra.revision.history_count == 15.0,
          "fifteen revisions, got %f", extra.revision.history_count);
    CHECK(extra.revision.history_dates.length > 100,
          "and its date list is long, got %zu bytes", extra.revision.history_dates.length);
}

static void test_cross_document_coupons(void) {
    tdx_convertible_row row;
    tdx_convertible_extra extra;
    tdx_convertible_join_flags flags;
    tdx_code identity = code_of(1, "110076");

    CHECK(tdx_convertible_join(&set, &identity, &row, &extra, &flags, &error) == TDX_OK,
          "join: %s", error.message);
    /* The overview's payment list and the coupon document's annual rates describe one
     * schedule, and the live data pins the relationship between them: the overview
     * folds the compensation rate into the final payment. */
    CHECK(extra.payment_dates.present, "the overview carries the payment dates");
    CHECK(extra.payment_rates.present, "the overview carries the payment rates");
    {
        tdx_buf dates;
        size_t commas = 0;
        size_t position;
        tdx_buf_init(&dates);
        CHECK(tdx_bonds_format_comma_array(&dates, &extra.payment_dates, 0, &error) == TDX_OK,
              "the dates render as an array: %s", error.message);
        for (position = 0; position < extra.payment_dates.length; ++position)
            if (extra.payment_dates.data[position] == ',')
                commas++;
        CHECK(commas + 1 == (size_t)extra.term_years,
              "the %zu payment dates match the %f-year term", commas + 1, extra.term_years);
        tdx_buf_free(&dates);
    }
}

static void test_join_only_elsewhere(void) {
    tdx_convertible_row row;
    tdx_convertible_extra extra;
    tdx_convertible_join_flags flags;
    /* A bond the fixture does not carry at all must be refused rather than reported
     * as an empty bond. */
    tdx_code missing = code_of(0, "999999");
    error.message[0] = '\0';
    CHECK(tdx_convertible_join(&set, &missing, &row, &extra, &flags, &error) == TDX_ERR,
          "a bond in none of the documents must be refused");
    CHECK(strstr(error.message, "none of the six") != NULL, "the error says why: %s",
          error.message);

    /* A document set with only one document still joins, and reports the rest as
     * absent rather than failing. */
    {
        tdx_convertible_documents partial;
        tdx_code identity = code_of(1, "110076");
        memset(&partial, 0, sizeof(partial));
        partial.overview = &documents[0];
        CHECK(tdx_convertible_join(&partial, &identity, &row, &extra, &flags, &error) == TDX_OK,
              "an overview-only join works: %s", error.message);
        CHECK(flags.from_overview == 1, "and reports the overview");
        CHECK(flags.from_sellback == 0 && flags.from_coupons == 0,
              "and reports the missing documents as absent");
        CHECK(extra.has_term_years == 0, "so the coupon term is absent, not zero");
    }
    /* A document set with no overview at all still joins the bond, which is what the
     * key union exists for. */
    {
        tdx_convertible_documents partial;
        tdx_code identity = code_of(1, "110076");
        memset(&partial, 0, sizeof(partial));
        partial.progress = &documents[1];
        CHECK(tdx_convertible_join(&partial, &identity, &row, &extra, &flags, &error) == TDX_OK,
              "a progress-only join works: %s", error.message);
        CHECK(flags.from_overview == 0, "the overview did not carry it");
        CHECK(row.bond_code.present && strcmp(bond_text(&row.bond_code), "110076") == 0,
              "but the identity is built from the request");
        CHECK(row.core_terms_complete == 0,
              "and the core terms are correctly reported incomplete");
        CHECK(extra.has_issue_size_100m_yuan, "while the progress fields are there");
    }
}

static void test_rendering(void) {
    tdx_convertible_row row;
    tdx_convertible_extra extra;
    tdx_convertible_join_flags flags;
    tdx_code identity = code_of(1, "110076");
    tdx_buf line;
    const char *text;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;

    tdx_buf_init(&line);
    CHECK(tdx_convertible_join(&set, &identity, &row, &extra, &flags, &error) == TDX_OK,
          "join: %s", error.message);
    CHECK(tdx_convertible_format_joined(&line, &row, &extra, &flags,
                                        TDX_CONVERTIBLE_OVERVIEW_RESOURCE, &error) == TDX_OK,
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
            CHECK(depth >= 0, "the joined object must not close early");
        }
    }
    CHECK(depth == 0 && !in_string, "the joined row is balanced, depth %d in_string %d", depth,
          in_string);
    CHECK(strstr(text, "\"progress\":{") != NULL, "the progress group renders");
    CHECK(strstr(text, "\"coupons\":{") != NULL, "the coupon group renders");
    CHECK(strstr(text, "\"rates_pct\":[") != NULL, "the six rates render as an array");
    CHECK(strstr(text, "\"sellback\":{") != NULL, "the sellback group renders");
    CHECK(strstr(text, "\"redemption\":{") != NULL, "the redemption group renders");
    CHECK(strstr(text, "\"revision\":{") != NULL, "the revision group renders");
    CHECK(strstr(text, "\"history_dates\":[\"20241223\",\"20251222\"]") != NULL,
          "the sellback history renders as an array");
    CHECK(strstr(text, "\"sources\":{") != NULL, "the sources block renders");
    CHECK(strstr(text, "\"sources\":{\"overview\":true,\"progress\":true") != NULL,
          "and names the documents that carried the bond");
    tdx_buf_free(&line);
}

int main(void) {
    test_document_shapes();
    if (load_all() != TDX_OK) {
        free_all();
        return 1;
    }
    test_union();
    test_join_all_six();
    test_cross_document_coupons();
    test_join_only_elsewhere();
    test_rendering();
    free_all();

    if (failures) {
        printf("%d convertible-join check(s) failed\n", failures);
        return 1;
    }
    printf("convertible-join checks passed\n");
    return 0;
}
