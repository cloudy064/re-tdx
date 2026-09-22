/* test_newbond.c - the new-bond projection and its reconciliation with subscriptions.
 *
 * Both fixtures are real captures: the projection whole (13 rows, small enough to embed
 * untouched) and the subscription list reduced to the rows the projection NAMES plus the
 * three whose values the mapping tests use.  That second group is what makes this test
 * able to match anything at all - a fixture without it would reconcile to zero matches
 * and pass without exercising the join.
 *
 * The assertions below are the measured live result, which the two documents produce
 * from 13 rows:
 *
 *     13 by subscription code, 0 by underlying, 0 unmatched
 *      0 subscription-date mismatches, 2 issue-size mismatches
 *
 * The first of those is the one worth stating loudly.  The projection's dates are
 * FORMATTED ("2026-06-26" then the weekday in Chinese) and the subscription's are
 * compact, so comparing the raw texts reports a mismatch on all thirteen rows.  That
 * number describes the comparison, not the bonds, and the compaction that removes it is
 * what the first assertion here pins down.
 *
 * The two size mismatches are the point of reconciling at all: two rows where the plan
 * and the event disagree, by +9.80 and -4.22 hundred million yuan. */
#include <stdio.h>
#include <string.h>

#include "tdx_newbond.h"
#include "tdx_newbond_json.h"
#include "tdx_subscription.h"
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
    static char scratch[16384];
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

static void test_compact_date(void) {
    char out[12];

    /* The projection's dates carry a weekday; the compacted form is what compares. */
    CHECK(tdx_newbond_compact_date("2026-06-26 \xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94", 19, out,
                                   sizeof(out)) == 1 &&
              strcmp(out, "20260626") == 0,
          "a formatted date compacts to eight digits, got %s", out);
    CHECK(tdx_newbond_compact_date("20260626", 8, out, sizeof(out)) == 1 &&
              strcmp(out, "20260626") == 0,
          "an already compact date is unchanged, got %s", out);
    CHECK(tdx_newbond_compact_date("2026-06-26", 10, out, sizeof(out)) == 1 &&
              strcmp(out, "20260626") == 0,
          "and so is a hyphenated one, got %s", out);
    /* Not eight digits is not a date. */
    CHECK(tdx_newbond_compact_date("2026", 4, out, sizeof(out)) == 0,
          "four digits is not a date");
    CHECK(tdx_newbond_compact_date("", 0, out, sizeof(out)) == 0, "and neither is nothing");
    CHECK(tdx_newbond_compact_date(NULL, 0, out, sizeof(out)) == 0, "nor a null");
    CHECK(tdx_newbond_compact_date("20260626", 8, NULL, 0) == 0, "nor a null output");
}

static void test_projection_mapping(void) {
    tdx_jsn_document document;
    tdx_newbond_row rows[NEWBOND_FIXTURE_ROWS + 2];
    size_t count = 0;
    size_t skipped = 0;

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    CHECK(tdx_jsn_parse((const uint8_t *)newbond_projection, strlen(newbond_projection),
                        &document, &error) == TDX_OK,
          "the projection fixture parses: %s", error.message);
    CHECK(document.groups[0].column_count == NEWBOND_FIXTURE_COLUMNS, "%d columns, got %zu",
          NEWBOND_FIXTURE_COLUMNS, document.groups[0].column_count);
    CHECK(NEWBOND_FIXTURE_COLUMNS == 20, "the capture had 20 columns, header says %d",
          NEWBOND_FIXTURE_COLUMNS);
    CHECK(NEWBOND_FIXTURE_ROWS == 13, "and 13 rows, header says %d", NEWBOND_FIXTURE_ROWS);
    CHECK(tdx_newbond_normalize(&document, &document.groups[0], rows, NEWBOND_FIXTURE_ROWS + 2,
                                &count, &skipped, &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(count == NEWBOND_FIXTURE_ROWS, "all %d rows map, got %zu", NEWBOND_FIXTURE_ROWS,
          count);
    CHECK(skipped == 0, "and none is skipped, got %zu", skipped);

    /* Row 0: the underlying is this list's fallback key, and the subscription code its
     * preferred one. */
    CHECK(strcmp(bond_text(&rows[0].subscription_code), "070422") == 0,
          "the first subscription code is 070422, got %s",
          bond_text(&rows[0].subscription_code));
    CHECK(rows[0].has_underlying && strcmp(rows[0].stock_security_id, "SZ000422") == 0,
          "its underlying is SZ000422, got %s", rows[0].stock_security_id);
    /* The date is carried BOTH ways: the raw text and the compacted form. */
    CHECK(rows[0].subscription_date_text.present &&
              rows[0].subscription_date_text.length > 8,
          "the raw date keeps the weekday text, got %s",
          bond_text(&rows[0].subscription_date_text));
    CHECK(strcmp(rows[0].subscription_date, "20260626") == 0,
          "and the compacted form is 20260626, got %s", rows[0].subscription_date);
    CHECK(strcmp(rows[0].issue_date, "20260626") == 0, "the issue date is 20260626, got %s",
          rows[0].issue_date);
    CHECK(rows[0].has_issue_size_100m_yuan && rows[0].issue_size_100m_yuan > 0.0,
          "the issue size is present, got %f", rows[0].issue_size_100m_yuan);
    CHECK(rows[0].has_conversion_price_yuan, "and the conversion price");
    CHECK(rows[0].issue_type.present, "and the issue type: %s",
          bond_text(&rows[0].issue_type));
    tdx_jsn_document_free(&document);
}

static tdx_jsn_document subscriptions_document;
static tdx_jsn_document projection_document;
static tdx_subscription_row subscriptions[TDX_SUBSCRIPTION_ROWS_MAX];
static tdx_newbond_row projections[TDX_NEWBOND_ROWS_MAX];
static size_t subscription_count = 0;
static size_t projection_count = 0;

static int load_both(void) {
    size_t skipped = 0;

    error.message[0] = '\0';
    tdx_jsn_document_init(&subscriptions_document);
    tdx_jsn_document_init(&projection_document);
    if (tdx_jsn_parse((const uint8_t *)subscription_list, strlen(subscription_list),
                      &subscriptions_document, &error) != TDX_OK ||
        tdx_jsn_parse((const uint8_t *)newbond_projection, strlen(newbond_projection),
                      &projection_document, &error) != TDX_OK) {
        CHECK(0, "parsing: %s", error.message);
        return TDX_ERR;
    }
    if (tdx_subscription_normalize(&subscriptions_document,
                                   &subscriptions_document.groups[0], subscriptions,
                                   TDX_SUBSCRIPTION_ROWS_MAX, &subscription_count, &skipped,
                                   &error) != TDX_OK ||
        tdx_newbond_normalize(&projection_document, &projection_document.groups[0], projections,
                              TDX_NEWBOND_ROWS_MAX, &projection_count, &skipped, &error) != TDX_OK) {
        CHECK(0, "normalizing: %s", error.message);
        return TDX_ERR;
    }
    return TDX_OK;
}

static void free_both(void) {
    tdx_jsn_document_free(&subscriptions_document);
    tdx_jsn_document_free(&projection_document);
}

static void test_reconciliation_on_captures(void) {
    tdx_newbond_reconciliation report;

    if (load_both() != TDX_OK)
        return;
    /* The fixture really does contain the rows the projection names - otherwise every
     * assertion below would be about a vacuous match. */
    CHECK(subscription_count >= NEWBOND_FIXTURE_ROWS,
          "the fixture holds %zu subscriptions for %zu projection rows", subscription_count,
          NEWBOND_FIXTURE_ROWS);
    CHECK(tdx_newbond_reconcile(subscriptions, subscription_count, projections,
                                projection_count, &report, &error) == TDX_OK,
          "reconcile: %s", error.message);

    /* THE MEASURED RESULT. */
    CHECK(report.projection_rows == NEWBOND_FIXTURE_ROWS, "%d projection rows, got %zu",
          NEWBOND_FIXTURE_ROWS, report.projection_rows);
    CHECK(report.exact_code_matches == NEWBOND_FIXTURE_ROWS,
          "all %d match by subscription code, got %zu", NEWBOND_FIXTURE_ROWS,
          report.exact_code_matches);
    CHECK(report.underlying_only_matches == 0, "none needs the underlying fallback, got %zu",
          report.underlying_only_matches);
    CHECK(report.unmatched_projection_rows == 0, "and none is unmatched, got %zu",
          report.unmatched_projection_rows);
    CHECK(report.unmatched_code_count == 0, "with no codes left over, got %zu",
          report.unmatched_code_count);
    /* THE COMPACTION.  Without it this counter is 13, which is what comparing the raw
     * texts produces. */
    CHECK(report.subscription_date_mismatch_count == 0,
          "no date differs once compacted, got %zu - if this is %d, the raw texts are "
          "being compared",
          report.subscription_date_mismatch_count, (int)NEWBOND_FIXTURE_ROWS);
    CHECK(report.issue_size_mismatch_count == 2,
          "two rows disagree about the issue size, got %zu", report.issue_size_mismatch_count);
    CHECK(report.hybrid_or_stale_count == 2, "and two rows are hybrid or stale, got %zu",
          report.hybrid_or_stale_count);
    CHECK(report.exact_projection == 0, "so the projection is not exact");

    /* The two disagreements, with their sizes. */
    {
        size_t index;
        size_t disagreements = 0;
        double largest = 0.0;
        for (index = 0; index < report.match_count; ++index) {
            if (!report.matches[index].size_mismatch)
                continue;
            disagreements++;
            if (report.matches[index].has_size_delta &&
                (report.matches[index].size_delta_100m_yuan > largest ||
                 -report.matches[index].size_delta_100m_yuan > largest))
                largest = report.matches[index].size_delta_100m_yuan > 0
                              ? report.matches[index].size_delta_100m_yuan
                              : -report.matches[index].size_delta_100m_yuan;
        }
        CHECK(disagreements == 2, "two rows disagree, counted again as %zu", disagreements);
        CHECK(largest > 9.79 && largest < 9.81,
              "and the larger disagreement is 9.80 hundred million yuan, got %.4f", largest);
    }
    /* Every match carries the row it matched and how. */
    {
        size_t index;
        int all_code_matches = 1;
        for (index = 0; index < report.match_count; ++index) {
            if (!report.matches[index].matched || !report.matches[index].primary)
                all_code_matches = 0;
            else if (strcmp(report.matches[index].match_method, "subscription-code") != 0)
                all_code_matches = 0;
        }
        CHECK(all_code_matches, "every match is by code and points at its primary");
    }
    tdx_newbond_reconciliation_free(&report);
    free_both();
}

static void test_match_preference(void) {
    /* SYNTHETIC rows, because the interesting case - a code that matches a DIFFERENT
     * underlying - is one a live capture does not offer.  The reference prefers the code
     * even then, and falls back to the underlying only when the code finds nothing. */
    tdx_subscription_row primary[3];
    tdx_newbond_row projection[2];
    tdx_newbond_reconciliation report;
    static const char *code_a = "070422";
    static const char *code_b = "999999";

    memset(primary, 0, sizeof(primary));
    memset(projection, 0, sizeof(projection));
    primary[0].subscription_code.data = code_a;
    primary[0].subscription_code.length = 6;
    primary[0].subscription_code.present = 1;
    snprintf(primary[0].stock_security_id, sizeof(primary[0].stock_security_id), "SZ000422");
    primary[0].has_issue_size_100m_yuan = 1;
    primary[0].issue_size_100m_yuan = 10.0;
    primary[1].subscription_code.data = code_a;
    primary[1].subscription_code.length = 6;
    primary[1].subscription_code.present = 1;
    snprintf(primary[1].stock_security_id, sizeof(primary[1].stock_security_id), "SZ000999");
    primary[1].has_issue_size_100m_yuan = 1;
    primary[1].issue_size_100m_yuan = 20.0;
    snprintf(primary[2].stock_security_id, sizeof(primary[2].stock_security_id), "SH600000");
    primary[2].has_issue_size_100m_yuan = 1;
    primary[2].issue_size_100m_yuan = 30.0;

    /* The first projection names code_a AND SZ000999, so the code matches two rows and
     * the underlying picks between them - the preferred branch inside the code branch. */
    projection[0].subscription_code.data = code_a;
    projection[0].subscription_code.length = 6;
    projection[0].subscription_code.present = 1;
    projection[0].has_underlying = 1;
    snprintf(projection[0].stock_security_id, sizeof(projection[0].stock_security_id),
             "SZ000999");
    projection[0].has_issue_size_100m_yuan = 1;
    projection[0].issue_size_100m_yuan = 20.0;
    /* The second has no code the primary knows, so it falls back to the underlying. */
    projection[1].subscription_code.data = code_b;
    projection[1].subscription_code.length = 6;
    projection[1].subscription_code.present = 1;
    projection[1].has_underlying = 1;
    snprintf(projection[1].stock_security_id, sizeof(projection[1].stock_security_id),
             "SH600000");
    projection[1].has_issue_size_100m_yuan = 1;
    projection[1].issue_size_100m_yuan = 25.0;

    error.message[0] = '\0';
    CHECK(tdx_newbond_reconcile(primary, 3, projection, 2, &report, &error) == TDX_OK,
          "reconcile: %s", error.message);
    CHECK(report.exact_code_matches == 1, "one match by code, got %zu",
          report.exact_code_matches);
    CHECK(report.underlying_only_matches == 1, "one by underlying, got %zu",
          report.underlying_only_matches);
    CHECK(report.match_count == 2, "and two matches recorded, got %zu", report.match_count);
    if (report.match_count == 2) {
        CHECK(strcmp(report.matches[0].match_method, "subscription-code") == 0,
              "the first matched by code, got %s", report.matches[0].match_method);
        /* The code matched two primary rows and the underlying chose the right one: the
         * 20.00 size, not the 10.00. */
        CHECK(report.matches[0].primary == &primary[1],
              "and the underlying chose between the two code matches");
        CHECK(report.matches[0].has_size_delta && report.matches[0].size_delta_100m_yuan == 0.0,
              "so the sizes agree, delta %f", report.matches[0].size_delta_100m_yuan);
        CHECK(strcmp(report.matches[1].match_method, "underlying-only") == 0,
              "the second fell back to the underlying, got %s", report.matches[1].match_method);
        /* 25.00 against 30.00 is a 5.00 disagreement, well beyond the 0.01 tolerance. */
        CHECK(report.matches[1].size_mismatch == 1, "and it disagrees by 5.00");
        CHECK(report.matches[1].has_size_delta &&
                  report.matches[1].size_delta_100m_yuan > -5.001 &&
                  report.matches[1].size_delta_100m_yuan < -4.999,
              "the delta is 25 - 30 = -5, got %f", report.matches[1].size_delta_100m_yuan);
    }
    tdx_newbond_reconciliation_free(&report);

    /* A projection row that matches nothing is reported as unmatched by CODE. */
    {
        tdx_newbond_row only[1];
        memset(only, 0, sizeof(only));
        only[0].subscription_code.data = code_b;
        only[0].subscription_code.length = 6;
        only[0].subscription_code.present = 1;
        only[0].has_underlying = 1;
        snprintf(only[0].stock_security_id, sizeof(only[0].stock_security_id), "SZ111111");
        CHECK(tdx_newbond_reconcile(primary, 3, only, 1, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.unmatched_projection_rows == 1, "one unmatched, got %zu",
              report.unmatched_projection_rows);
        CHECK(report.unmatched_code_count == 1 &&
                  strcmp(report.unmatched_codes[0], code_b) == 0,
              "and its code is reported, got %s",
              report.unmatched_code_count ? report.unmatched_codes[0] : "(none)");
        CHECK(report.exact_projection == 0, "so the projection is not exact");
        tdx_newbond_reconciliation_free(&report);
    }

    /* A match that agrees about everything is an exact projection. */
    {
        tdx_newbond_row only[1];
        memset(only, 0, sizeof(only));
        only[0].subscription_code.data = code_a;
        only[0].subscription_code.length = 6;
        only[0].subscription_code.present = 1;
        only[0].has_underlying = 1;
        snprintf(only[0].stock_security_id, sizeof(only[0].stock_security_id), "SZ000422");
        only[0].has_issue_size_100m_yuan = 1;
        only[0].issue_size_100m_yuan = 10.0;
        CHECK(tdx_newbond_reconcile(primary, 3, only, 1, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.exact_projection == 1,
              "a code match with the same size is an exact projection");
        CHECK(report.matches[0].has_size_delta && report.matches[0].size_delta_100m_yuan == 0.0,
              "and the delta is zero rather than absent");
        tdx_newbond_reconciliation_free(&report);
    }
}

static void test_rendering(void) {
    tdx_newbond_reconciliation report;
    tdx_buf line;
    const char *text;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;
    size_t index;

    if (load_both() != TDX_OK)
        return;
    if (tdx_newbond_reconcile(subscriptions, subscription_count, projections, projection_count,
                              &report, &error) != TDX_OK) {
        CHECK(0, "reconcile: %s", error.message);
        free_both();
        return;
    }
    tdx_buf_init(&line);
    /* Every row must balance, not just the first: a missing brace in a branch that only
     * one row takes is exactly what the balance check is for. */
    for (index = 0; index < projection_count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_newbond_format_row(&line, &projections[index], &report.matches[index], index,
                                   &error) != TDX_OK) {
            CHECK(0, "render row %zu: %s", index, error.message);
            continue;
        }
        text = text_of(&line);
        depth = 0;
        in_string = 0;
        escaped = 0;
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
                CHECK(depth >= 0, "row %zu closed early", index);
            }
        }
        CHECK(depth == 0 && !in_string, "row %zu is balanced, depth %d in_string %d", index,
              depth, in_string);
    {
        /* Balanced is not the same as parseable: this assertion is what caught a Windows
         * path reaching the JSON through a raw %s, and a CRC printed without quotes. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    }
    tdx_buf_clear(&line);
    CHECK(tdx_newbond_format_row(&line, &projections[0], &report.matches[0], 0, &error) == TDX_OK,
          "render: %s", error.message);
    text = text_of(&line);
    CHECK(strstr(text, "\"type\":\"new_convertible_bond_projection\"") != NULL, "the type");
    CHECK(strstr(text, "\"subscription_code\":\"070422\"") != NULL, "the code");
    CHECK(strstr(text, "\"security_id\":\"SZ000422\"") != NULL, "the underlying");
    CHECK(strstr(text, "\"subscription\":\"20260626\"") != NULL, "the compacted date");
    CHECK(strstr(text, "\"match_method\":\"subscription-code\"") != NULL, "the match method");
    /* The primary's BOND, not the projection's underlying: the subscription with code
     * 070422 is for SZ127114, and SZ127114's underlying is the SZ000422 the projection
     * points at.  Asserting the underlying here would have been asserting the wrong
     * thing, which is what this caught. */
    CHECK(strstr(text, "\"primary_bond\":\"SZ127114\"") != NULL,
          "the primary it matched is the bond, SZ127114: %s", text);
    {
        /* And the semantic check the match rests on: the subscription it matched really
         * does have the underlying the projection names. */
        size_t index;
        int consistent = 1;
        for (index = 0; index < report.match_count; ++index) {
            if (!report.matches[index].primary)
                continue;
            if (!projections[index].has_underlying)
                continue;
            if (strcmp(report.matches[index].primary->stock_security_id,
                       projections[index].stock_security_id) != 0 &&
                strcmp(report.matches[index].match_method, "subscription-code") == 0)
                consistent = 0;
        }
        CHECK(consistent,
              "every code match agrees about the underlying as well, on all %zu rows",
              report.match_count);
    }
    CHECK(strstr(text, "\"subscription_date_mismatch\":false") != NULL, "the date verdict");
    CHECK(strstr(text, "\"issue_size_delta_100m_yuan\":0.000000") != NULL, "the size delta");

    tdx_buf_clear(&line);
    CHECK(tdx_newbond_format_reconciliation(&line, &report, TDX_SUBSCRIPTION_RESOURCE,
                                            TDX_NEWBOND_PROJECTION_RESOURCE, "1.2.3.4:7709",
                                            &error) == TDX_OK,
          "reconciliation render: %s", error.message);
    text = text_of(&line);
    CHECK(strstr(text, "\"type\":\"new_projection_reconciliation\"") != NULL, "the type");
    CHECK(strstr(text, "\"exact_subscription_code_matches\":13") != NULL, "the code matches");
    CHECK(strstr(text, "\"issue_size_mismatch_count\":2") != NULL, "the size mismatches");
    CHECK(strstr(text, "\"exact_projection\":false") != NULL, "the verdict");
    CHECK(strstr(text, "\"unmatched_subscription_codes\":[]") != NULL, "the empty list");
    tdx_buf_free(&line);
    tdx_newbond_reconciliation_free(&report);
    free_both();
}

int main(void) {
    test_compact_date();
    test_projection_mapping();
    test_reconciliation_on_captures();
    test_match_preference();
    test_rendering();

    if (failures) {
        printf("%d new-bond check(s) failed\n", failures);
        return 1;
    }
    printf("new-bond checks passed\n");
    return 0;
}
