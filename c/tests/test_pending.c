/* test_pending.c - the pending convertible-bond issue list.
 *
 * Two halves, tested differently on purpose.
 *
 * The ROW MAPPING is tested against a real capture: bi/list/dfkzz201_1.jsn, reduced
 * by generator to its verbatim colheader plus the first three rows.  This list is the
 * one JSN resource in the project whose columns are LOWER CASE (zzlx, mzgm, fadj,
 * date0, byhq, zgj, gdpsl, sgrq, fxrq, zql, zqr, sgdm, sgmc, fxjg), and a pending bond
 * has no code of its own yet, so its identity is the UNDERLYING STOCK.
 *
 * The SET RECONCILIATION is tested against arrays built here, because it is pure logic
 * over row arrays and the cases that matter - a duplicate identity, an empty identity,
 * an exactly equal set - are ones a live document does not promise to contain.  The
 * live documents do exercise the ordinary case: measured, the second projection is a
 * superset of the primary (154 against 153 securities, differing by SZ300495) and the
 * first is missing nine of the primary's. */
#include <stdio.h>
#include <string.h>

#include "tdx_pending.h"
#include "tdx_pending_json.h"
#include "tdx_jsn.h"
#include "pending_fixtures.h"
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

/* The captured Chinese values, as UTF-8 escapes: a hex escape eats every following
 * hex digit, so each sequence is closed by its own literal. */
#define EXCHANGEABLE "\xe5\x8f\xaf\xe4\xba\xa4\xe6\x8d\xa2\xe5\x80\xba"
#define CONVERTIBLE "\xe5\x8f\xaf\xe8\xbd\xac\xe5\x80\xba"
#define BOARD_PLAN "\xe8\x91\xa3\xe4\xba\x8b\xe4\xbc\x9a\xe9\xa2\x84\xe6\xa1\x88"

static void test_normalize_captured(void) {
    tdx_jsn_document document;
    tdx_pending_row rows[PENDING_FIXTURE_ROWS + 2];
    size_t count = 0;
    size_t skipped = 0;

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    CHECK(tdx_jsn_parse((const uint8_t *)pending_primary, strlen(pending_primary), &document,
                        &error) == TDX_OK,
          "the fixture parses: %s", error.message);
    CHECK(document.group_count == 1, "one group");
    CHECK(document.groups[0].column_count == PENDING_FIXTURE_COLUMNS, "%d columns, got %zu",
          PENDING_FIXTURE_COLUMNS, document.groups[0].column_count);
    CHECK(document.row_count == PENDING_FIXTURE_ROWS, "%d rows, got %zu", PENDING_FIXTURE_ROWS,
          document.row_count);
    CHECK(PENDING_FIXTURE_COLUMNS == 16, "the capture had 16 columns, header says %d",
          PENDING_FIXTURE_COLUMNS);
    CHECK(PENDING_FIXTURE_SOURCE_ROWS == 153, "and 153 rows, header says %d",
          PENDING_FIXTURE_SOURCE_ROWS);

    CHECK(tdx_pending_normalize(&document, &document.groups[0], rows, PENDING_FIXTURE_ROWS + 2,
                                &count, &skipped, &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(count == PENDING_FIXTURE_ROWS, "all %d rows map, got %zu", PENDING_FIXTURE_ROWS, count);
    CHECK(skipped == 0, "and none is skipped, got %zu", skipped);

    /* Row 0 names the underlying stock, because a pending bond has no code yet. */
    CHECK(rows[0].market_id == 1 && strcmp(rows[0].market, "sh") == 0,
          "the first row's market is sh");
    CHECK(strcmp(rows[0].security_id, "SH600300") == 0,
          "its identity is the underlying SH600300, got %s", rows[0].security_id);
    CHECK(strcmp(bond_text(&rows[0].code), "600300") == 0, "the code is 600300");
    CHECK(strcmp(bond_text(&rows[0].issue_type), EXCHANGEABLE) == 0,
          "the issue type is the Chinese for exchangeable bond, got %s",
          bond_text(&rows[0].issue_type));
    CHECK(rows[0].has_planned_size_100m_yuan && rows[0].planned_size_100m_yuan == 7.0,
          "the planned size is 0.7bn yuan, got %f", rows[0].planned_size_100m_yuan);
    CHECK(strcmp(bond_text(&rows[0].plan_progress), BOARD_PLAN) == 0,
          "the progress is the Chinese for board proposal, got %s",
          bond_text(&rows[0].plan_progress));
    CHECK(strcmp(bond_text(&rows[0].progress_date), "20170414") == 0,
          "dated 20170414, got %s", bond_text(&rows[0].progress_date));
    /* byhq arrives with eighteen decimal places; the double keeps what it can. */
    CHECK(rows[0].has_stock_rights_yuan && rows[0].stock_rights_yuan > 0.4328623 &&
              rows[0].stock_rights_yuan < 0.4328624,
          "the stock rights are 0.4328623667, got %.20f", rows[0].stock_rights_yuan);
    CHECK(rows[0].has_conversion_price_yuan && rows[0].conversion_price_yuan > 3.2251 &&
              rows[0].conversion_price_yuan < 3.2253,
          "the conversion price is 3.2252, got %f", rows[0].conversion_price_yuan);
    /* The gdpsl column exists and is EMPTY, so the field must be absent rather than
     * zero - the whole point of the has_* flags. */
    CHECK(rows[0].has_shareholder_placement_ratio == 0,
          "an empty gdpsl is absent, not zero, got has=%d",
          rows[0].has_shareholder_placement_ratio);
    CHECK(rows[0].subscription_date.present == 0, "and so is the subscription date");
    CHECK(rows[0].has_lottery_rate == 0, "and the lottery rate");

    /* Row 1 is a Shenzhen stock and row 2 is a CONVERTIBLE bond rather than an
     * exchangeable one, so the two issue types are both covered. */
    CHECK(rows[1].market_id == 0 && strcmp(rows[1].security_id, "SZ000410") == 0,
          "the second row is SZ000410, got %s", rows[1].security_id);
    CHECK(rows[1].has_planned_size_100m_yuan && rows[1].planned_size_100m_yuan == 10.0,
          "planned size 1bn, got %f", rows[1].planned_size_100m_yuan);
    CHECK(strcmp(bond_text(&rows[2].issue_type), CONVERTIBLE) == 0,
          "the third row is a convertible bond, got %s", bond_text(&rows[2].issue_type));
    CHECK(strcmp(rows[2].security_id, "SH603099") == 0, "SH603099, got %s",
          rows[2].security_id);
    CHECK(rows[2].has_conversion_price_yuan && rows[2].conversion_price_yuan > 38.84 &&
              rows[2].conversion_price_yuan < 38.85,
          "its conversion price is 38.8452, got %f", rows[2].conversion_price_yuan);

    tdx_jsn_document_free(&document);
}

static void test_normalize_skips(void) {
    /* SYNTHETIC: a document whose rows cannot be attached to a stock.  This list
     * SKIPS such rows - unlike the listed view, where an unreadable identity is an
     * error - so both halves of that rule are checked here. */
    static const char *json =
        "[{\"colheader\":[\"$ZQDM\",\"$SC\",\"zzlx\"],\"data\":["
        "[\"600300\",\"1\",\"ok\"],[\"60030\",\"1\",\"short\"],[\"600300\",\"sh\",\"bad market\"],"
        "[\"600301\",\"1\",\"ok too\"]]}]";
    tdx_jsn_document document;
    tdx_pending_row rows[8];
    size_t count = 0;
    size_t skipped = 0;

    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    if (tdx_jsn_parse((const uint8_t *)json, strlen(json), &document, &error) != TDX_OK) {
        CHECK(0, "parses: %s", error.message);
        tdx_jsn_document_free(&document);
        return;
    }
    CHECK(tdx_pending_normalize(&document, &document.groups[0], rows, 8, &count, &skipped,
                                &error) == TDX_OK,
          "normalize: %s", error.message);
    CHECK(count == 2, "two rows have a usable identity, got %zu", count);
    CHECK(skipped == 2, "two are skipped and counted, got %zu", skipped);
    if (count == 2) {
        CHECK(strcmp(rows[0].security_id, "SH600300") == 0, "the first kept row");
        CHECK(strcmp(rows[1].security_id, "SH600301") == 0, "the second kept row");
    }
    /* The capacity is enforced rather than overrun. */
    CHECK(tdx_pending_normalize(&document, &document.groups[0], rows, 1, &count, &skipped,
                                &error) == TDX_ERR,
          "a one-row buffer for two rows must be refused");
    tdx_jsn_document_free(&document);
}

/* --- the set reconciliation, on arrays built here --------------------- */

static tdx_pending_row make_row(const char *security_id) {
    tdx_pending_row row;
    memset(&row, 0, sizeof(row));
    snprintf(row.security_id, sizeof(row.security_id), "%s", security_id);
    return row;
}

static void test_reconcile(void) {
    tdx_pending_reconciliation report;

    /* Identical sets: exact, nothing on either side. */
    {
        tdx_pending_row primary[3];
        tdx_pending_row projection[3];
        primary[0] = make_row("SH600300");
        primary[1] = make_row("SZ000410");
        primary[2] = make_row("SH603099");
        projection[0] = make_row("SH603099");
        projection[1] = make_row("SH600300");
        projection[2] = make_row("SZ000410");
        CHECK(tdx_pending_reconcile(primary, 3, projection, 3, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.primary_securities == 3 && report.projection_securities == 3,
              "three distinct securities each");
        CHECK(report.common_securities == 3, "all three common, got %zu",
              report.common_securities);
        CHECK(report.primary_only_count == 0 && report.projection_only_count == 0,
              "and nothing on either side");
        /* Order must not matter: an exactly equal set is equal whatever the order. */
        CHECK(report.exact_security_set == 1, "the sets are exactly equal");
        tdx_pending_reconciliation_free(&report);
    }

    /* A difference on each side, plus a DUPLICATE identity in the primary. */
    {
        tdx_pending_row primary[5];
        tdx_pending_row projection[3];
        primary[0] = make_row("SH600300");
        primary[1] = make_row("SZ000410");
        primary[2] = make_row("SH600300"); /* a duplicate row, not a second security */
        primary[3] = make_row("SH603099");
        primary[4] = make_row("SZ002997");
        projection[0] = make_row("SH600300");
        projection[1] = make_row("SZ000410");
        projection[2] = make_row("SZ300495");
        CHECK(tdx_pending_reconcile(primary, 5, projection, 3, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.primary_rows == 5 && report.projection_rows == 3,
              "the row counts are reported as given");
        CHECK(report.primary_securities == 4,
              "the duplicate collapses to four securities, got %zu", report.primary_securities);
        CHECK(report.projection_securities == 3, "three in the projection");
        CHECK(report.common_securities == 2, "two common, got %zu", report.common_securities);
        CHECK(report.primary_only_count == 2, "two only in the primary, got %zu",
              report.primary_only_count);
        CHECK(report.projection_only_count == 1, "one only in the projection, got %zu",
              report.projection_only_count);
        CHECK(report.primary_only_count == 2 &&
                  strcmp(report.primary_only[0], "SH603099") == 0 &&
                  strcmp(report.primary_only[1], "SZ002997") == 0,
              "in the primary document's order: %s then %s",
              report.primary_only_count > 0 ? report.primary_only[0] : "(none)",
              report.primary_only_count > 1 ? report.primary_only[1] : "(none)");
        CHECK(report.projection_only_count == 1 &&
                  strcmp(report.projection_only[0], "SZ300495") == 0,
              "and the projection's own, got %s",
              report.projection_only_count > 0 ? report.projection_only[0] : "(none)");
        CHECK(report.exact_security_set == 0, "the sets differ");
        tdx_pending_reconciliation_free(&report);
    }

    /* An EMPTY identity is not a security, so it is dropped from both sides. */
    {
        tdx_pending_row primary[2];
        tdx_pending_row projection[2];
        primary[0] = make_row("SH600300");
        primary[1] = make_row("");
        projection[0] = make_row("SH600300");
        projection[1] = make_row("");
        CHECK(tdx_pending_reconcile(primary, 2, projection, 2, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.primary_securities == 1 && report.projection_securities == 1,
              "one security each once the empty identity is dropped");
        CHECK(report.exact_security_set == 1, "and the sets are still equal");
        tdx_pending_reconciliation_free(&report);
    }

    /* An empty projection leaves everything on the primary's side. */
    {
        tdx_pending_row primary[2];
        primary[0] = make_row("SH600300");
        primary[1] = make_row("SZ000410");
        CHECK(tdx_pending_reconcile(primary, 2, NULL, 0, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.primary_securities == 2 && report.projection_securities == 0,
              "two against none");
        CHECK(report.primary_only_count == 2 && report.projection_only_count == 0,
              "both are primary-only");
        CHECK(report.exact_security_set == 0, "and the sets differ");
        tdx_pending_reconciliation_free(&report);
    }

    /* Two empty lists agree. */
    {
        CHECK(tdx_pending_reconcile(NULL, 0, NULL, 0, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        CHECK(report.exact_security_set == 1, "two empty sets are equal");
        tdx_pending_reconciliation_free(&report);
    }
}

static void test_rendering(void) {
    tdx_jsn_document document;
    tdx_pending_row rows[PENDING_FIXTURE_ROWS + 2];
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
    if (tdx_jsn_parse((const uint8_t *)pending_primary, strlen(pending_primary), &document,
                      &error) != TDX_OK ||
        tdx_pending_normalize(&document, &document.groups[0], rows, PENDING_FIXTURE_ROWS + 2,
                              &count, &skipped, &error) != TDX_OK) {
        CHECK(0, "prepare: %s", error.message);
        goto done;
    }
    CHECK(tdx_pending_format(&line, &rows[0], "list/dfkzz201_1.jsn", 0, &error) == TDX_OK,
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
    CHECK(depth == 0 && !in_string, "the pending row is balanced, depth %d in_string %d", depth,
          in_string);
    {
        /* Balanced is not the same as parseable: this assertion is what caught a Windows
         * path reaching the JSON through a raw %s, and a CRC printed without quotes. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"type\":\"pending_convertible_bond\"") != NULL, "the type");
    CHECK(strstr(text, "\"security_id\":\"SH600300\"") != NULL, "the underlying identity");
    CHECK(strstr(text, "\"planned_size_100m_yuan\":7.000000") != NULL, "the planned size");
    /* An absent number renders null rather than 0. */
    CHECK(strstr(text, "\"shareholder_placement_ratio\":null") != NULL,
          "an absent ratio renders null: %s", text);
    CHECK(strstr(text, "\"subscription\":{\"code\":null,\"name\":null}") != NULL,
          "and an absent subscription renders null");

    /* The reconciliation report, including its two lists. */
    {
        tdx_pending_reconciliation report;
        tdx_pending_row primary[2];
        tdx_pending_row projection[2];
        primary[0] = make_row("SH600300");
        primary[1] = make_row("SZ000002");
        projection[0] = make_row("SH600300");
        projection[1] = make_row("SZ002997");
        CHECK(tdx_pending_reconcile(primary, 2, projection, 2, &report, &error) == TDX_OK,
              "reconcile: %s", error.message);
        tdx_buf_clear(&line);
        CHECK(tdx_pending_format_reconciliation(&line, &report, "primary.jsn", "projection.jsn",
                                                "1.2.3.4:7709", &error) == TDX_OK,
              "render: %s", error.message);
        text = text_of(&line);
        CHECK(strstr(text, "\"type\":\"pending_reconciliation\"") != NULL, "the type");
        CHECK(strstr(text, "\"primary_only\":[\"SZ000002\"]") != NULL,
              "the primary-only list: %s", text);
        CHECK(strstr(text, "\"projection_only\":[\"SZ002997\"]") != NULL,
              "the projection-only list");
        CHECK(strstr(text, "\"exact_security_set\":false") != NULL, "and the verdict");
        tdx_pending_reconciliation_free(&report);
    }

    tdx_buf_clear(&line);
    CHECK(tdx_pending_format_summary(&line, 153, 0, 2, 0, "list/dfkzz201_1.jsn", "1.2.3.4:7709",
                                     &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(strstr(text_of(&line), "\"type\":\"pending_summary\"") != NULL, "the summary type");
    CHECK(strstr(text_of(&line), "\"projections_agreeing_on_every_security\":0") != NULL,
          "the agreement count");
    tdx_buf_clear(&line);
    CHECK(tdx_pending_format_summary(&line, 0, 0, 0, 0, NULL, NULL, &error) == TDX_OK,
          "an empty summary renders");
    CHECK(strstr(text_of(&line), "\"endpoint\":null") != NULL, "a missing endpoint is null");

done:
    tdx_jsn_document_free(&document);
    tdx_buf_free(&line);
}

int main(void) {
    test_normalize_captured();
    test_normalize_skips();
    test_reconcile();
    test_rendering();

    if (failures) {
        printf("%d pending check(s) failed\n", failures);
        return 1;
    }
    printf("pending checks passed\n");
    return 0;
}
