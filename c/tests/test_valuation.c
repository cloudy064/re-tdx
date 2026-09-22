/* test_valuation.c - index valuation: the current table and the PE/PB history.
 *
 * The master resource is small enough to embed WHOLE - 11 rows - so the current-table
 * assertions run on every row the server sends rather than on a sample.  The two histories
 * are 100 KB each, so they come in as prefixes of twenty rows that keep the SAME twenty
 * dates on both sides, and the merge's unaligned cases are built in the test instead,
 * because a capture does not promise to contain a date only one side carries.
 *
 * One property is worth stating: live, both histories hold 3,093 rows, and the merge reports
 * 3,093 points with 3,093 on both sides - so the two halves really are one series.  The
 * prefixes carry the same three-hundredths of it and assert the same thing.
 *
 * A cross-resource fact is checked live by the evidence script rather than here, since the
 * fixture keeps only the first twenty history rows: the master's current PE and PB equal the
 * history's LAST point on the same date - 20260921, PE 15.9510 and PB 1.3583 in both. */
#include <stdio.h>
#include <string.h>

#include "tdx_valuation.h"
#include "tdx_valuation_json.h"
#include "tdx_jsn.h"
#include "render_check.h"
#include "valuation_fixtures.h"

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

/* THE LABEL IS TEXT, and it is Chinese, so it is compared as the UTF-8 bytes the resource
 * carries: "\xe4\xbc\xb0\xe5\x80\xbc\xe9\x80\x82\xe4\xb8\xad" is the four characters the
 * master uses for a middling valuation.  Asserting only that it is non-empty would pass on
 * mojibake. */
#define LABEL_MODERATE "\xe4\xbc\xb0\xe5\x80\xbc\xe9\x80\x82\xe4\xb8\xad"

static int parse(const char *text, tdx_jsn_document *doc) {
    error.message[0] = '\0';
    tdx_jsn_document_init(doc);
    if (tdx_jsn_parse((const uint8_t *)text, strlen(text), doc, &error) != TDX_OK) {
        CHECK(0, "parsing a fixture: %s", error.message);
        return TDX_ERR;
    }
    return TDX_OK;
}

static const char *label_text(const tdx_bond_text *text) {
    static char scratch[128];
    if (!text->present || !text->data)
        return "(absent)";
    if (text->length >= sizeof(scratch))
        return "(too long)";
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return scratch;
}

static void test_master(void) {
    tdx_jsn_document doc = {0};
    tdx_valuation_index indices[VALUATION_MASTER_ROWS + 4];
    size_t count = 0;
    size_t skipped = 0;

    if (parse(valuation_master, &doc) != TDX_OK)
        return;
    CHECK(tdx_valuation_parse_master(&doc, &doc.groups[0], indices, VALUATION_MASTER_ROWS + 4,
                                     &count, &skipped, &error) == TDX_OK,
          "parse: %s", error.message);
    /* The whole table, not a sample: it is eleven rows. */
    CHECK(count == VALUATION_MASTER_ROWS, "%d indices, got %zu", VALUATION_MASTER_ROWS, count);
    CHECK(VALUATION_MASTER_ROWS == 11, "the live table has 11 rows, header says %d",
          VALUATION_MASTER_ROWS);
    CHECK(skipped == 0, "and none is skipped, got %zu", skipped);

    /* The Shanghai Composite, as the resource states it. */
    CHECK(strcmp(indices[0].security_id, "SH000001") == 0, "the first index is SH000001, got %s",
          indices[0].security_id);
    CHECK(strcmp(indices[0].detail_id, "000001") == 0,
          "and its detail id is 000001, got %s", indices[0].detail_id);
    CHECK(strcmp(indices[0].date, "20260921") == 0, "dated 20260921, got %s", indices[0].date);
    CHECK(indices[0].has_pe && indices[0].pe > 15.9509 && indices[0].pe < 15.9511,
          "PE 15.9510, got %f", indices[0].pe);
    CHECK(indices[0].has_pe_percentile && indices[0].pe_percentile > 79.1081 &&
              indices[0].pe_percentile < 79.1083,
          "PE percentile 79.1082, got %f", indices[0].pe_percentile);
    CHECK(indices[0].has_pb && indices[0].pb > 1.3582 && indices[0].pb < 1.3584,
          "PB 1.3583, got %f", indices[0].pb);
    CHECK(indices[0].has_pb_percentile && indices[0].pb_percentile > 77.4565 &&
              indices[0].pb_percentile < 77.4567,
          "PB percentile 77.4566, got %f", indices[0].pb_percentile);
    CHECK(indices[0].has_roe && indices[0].roe > 8.34 && indices[0].roe < 8.36,
          "ROE 8.35, got %f", indices[0].roe);
    CHECK(indices[0].label.present &&
              strcmp(label_text(&indices[0].label), LABEL_MODERATE) == 0,
          "the label is the resource's own word, got %s", label_text(&indices[0].label));
    CHECK(indices[0].has_return[0] && indices[0].returns[0] > 1.6621 &&
              indices[0].returns[0] < 1.6622,
          "the 5-day return is 1.66215, got %f", indices[0].returns[0]);
    CHECK(indices[0].has_return[3] && indices[0].returns[3] > -0.4206 &&
              indices[0].returns[3] < -0.4205,
          "and the 30-day return is -0.420512, got %f", indices[0].returns[3]);

    /* The table carries both markets, and every row names a detail id: that is what makes
     * the history reachable from a row. */
    {
        size_t index;
        size_t shenzhen = 0;
        size_t with_detail = 0;
        for (index = 0; index < count; ++index) {
            if (indices[index].market_id == 0)
                shenzhen++;
            if (indices[index].detail_id[0] != '\0')
                with_detail++;
        }
        CHECK(shenzhen > 0, "some index is on Shenzhen, got %zu", shenzhen);
        CHECK(with_detail == count, "every row carries a detail id, got %zu of %zu",
              with_detail, count);
    }
    tdx_jsn_document_free(&doc);
}

static void test_history_merge(void) {
    tdx_jsn_document pe = {0};
    tdx_jsn_document pb = {0};
    tdx_valuation_point points[VALUATION_HISTORY_PREFIX_ROWS + 4];
    tdx_valuation_merge merge;

    if (parse(valuation_pe, &pe) != TDX_OK)
        return;
    if (parse(valuation_pb, &pb) != TDX_OK) {
        tdx_jsn_document_free(&pe);
        return;
    }
    CHECK(tdx_valuation_merge_history(&pe, &pe.groups[0], &pb, &pb.groups[0], points,
                                      VALUATION_HISTORY_PREFIX_ROWS + 4, &merge,
                                      &error) == TDX_OK,
          "merge: %s", error.message);
    CHECK(merge.points == VALUATION_HISTORY_PREFIX_ROWS, "%d points, got %zu",
          VALUATION_HISTORY_PREFIX_ROWS, merge.points);
    /* THE PROPERTY: every date is on both sides. */
    CHECK(merge.both_sides == merge.points, "every date is on both sides, got %zu of %zu",
          merge.both_sides, merge.points);
    CHECK(merge.pe_only == 0 && merge.pb_only == 0, "and neither side has an extra date");
    CHECK(merge.complete == 1, "so the merge reports itself complete");

    /* The values come from the two resources, joined on the date. */
    CHECK(strcmp(points[0].date, "20140103") == 0, "the first date is 20140103, got %s",
          points[0].date);
    CHECK(points[0].has_pe && points[0].pe > 9.1695 && points[0].pe < 9.1697,
          "PE 9.1696 on that date, got %f", points[0].pe);
    CHECK(points[0].has_pb && points[0].pb > 1.2748 && points[0].pb < 1.2750,
          "and PB 1.2749 from the OTHER resource, got %f", points[0].pb);
    CHECK(points[0].has_pe_percentile && points[0].pe_percentile == 0.0,
          "the PE percentile is 0.0000 as the resource states, got %f",
          points[0].pe_percentile);
    CHECK(strcmp(points[merge.points - 1].date, "20140130") == 0,
          "and the last is 20140130, got %s", points[merge.points - 1].date);
    {
        size_t index;
        int ordered = 1;
        for (index = 1; index < merge.points; ++index)
            if (strcmp(points[index - 1].date, points[index].date) >= 0)
                ordered = 0;
        CHECK(ordered, "and the merged series is in ascending date order");
    }
    tdx_jsn_document_free(&pe);
    tdx_jsn_document_free(&pb);
}

/* The cases a capture does not promise: a date only one side carries, and out-of-order
 * input. */
static void test_merge_edges(void) {
    static const char *pe_json =
        "[{\"colheader\":[\"date\",\"pebfw\",\"pe\"],\"data\":["
        "[\"20240103\",\"10.0\",\"11.0\"],[\"20240104\",\"11.0\",\"12.0\"],"
        "[\"20240106\",\"13.0\",\"14.0\"]]}]";
    static const char *pb_json =
        "[{\"colheader\":[\"date\",\"pbbfw\",\"pb\"],\"data\":["
        "[\"20240105\",\"20.0\",\"1.5\"],[\"20240103\",\"21.0\",\"1.6\"]]}]";
    tdx_jsn_document pe = {0};
    tdx_jsn_document pb = {0};
    tdx_valuation_point points[16];
    tdx_valuation_merge merge;
    size_t index;

    if (parse(pe_json, &pe) != TDX_OK)
        return;
    if (parse(pb_json, &pb) != TDX_OK) {
        tdx_jsn_document_free(&pe);
        return;
    }
    CHECK(tdx_valuation_merge_history(&pe, &pe.groups[0], &pb, &pb.groups[0], points, 16,
                                      &merge, &error) == TDX_OK,
          "merge: %s", error.message);
    /* Three PE dates and two PB dates sharing only the third: five points, one shared, and
     * the extra dates kept rather than dropped. */
    CHECK(merge.points == 4, "four distinct dates, got %zu", merge.points);
    CHECK(merge.both_sides == 1, "one date is on both sides, got %zu", merge.both_sides);
    CHECK(merge.pe_only == 2, "two are PE only, got %zu", merge.pe_only);
    CHECK(merge.pb_only == 1, "one is PB only, got %zu", merge.pb_only);
    CHECK(merge.complete == 0, "so the merge does NOT report itself complete");
    /* Sorted, whatever order the resources used - the PB side is out of order on purpose. */
    CHECK(strcmp(points[0].date, "20240103") == 0, "first 20240103, got %s", points[0].date);
    CHECK(strcmp(points[1].date, "20240104") == 0, "then 20240104, got %s", points[1].date);
    CHECK(strcmp(points[2].date, "20240105") == 0, "then 20240105, got %s", points[2].date);
    CHECK(strcmp(points[3].date, "20240106") == 0, "then 20240106, got %s", points[3].date);
    /* The shared date carries both halves. */
    CHECK(points[0].has_pe && points[0].has_pb, "the shared date has both halves");
    /* A PB-only date carries no PE rather than a zero. */
    CHECK(points[2].has_pe == 0 && points[2].has_pb == 1,
          "the PB-only date has no PE, got has_pe=%d", points[2].has_pe);
    CHECK(points[2].has_pb && points[2].pb > 1.49 && points[2].pb < 1.51,
          "and its PB is 1.5, got %f", points[2].pb);
    for (index = 1; index < merge.points; ++index)
        CHECK(strcmp(points[index - 1].date, points[index].date) < 0, "the order holds");

    /* One side absent is not a failure: the other side's dates stand alone. */
    CHECK(tdx_valuation_merge_history(&pe, &pe.groups[0], NULL, NULL, points, 16, &merge,
                                      &error) == TDX_OK,
          "merging with no PB side works: %s", error.message);
    CHECK(merge.points == 3 && merge.pe_only == 3 && merge.complete == 0,
          "with three PE-only points, got %zu and %zu", merge.points, merge.pe_only);
    CHECK(points[0].has_pe && points[0].has_pb == 0, "PE present, PB absent");
    tdx_jsn_document_free(&pe);
    tdx_jsn_document_free(&pb);
}

static void test_funds(void) {
    tdx_jsn_document doc = {0};
    tdx_valuation_fund funds[VALUATION_FUNDS_ROWS + 4];
    size_t count = 0;
    size_t skipped = 0;

    if (parse(valuation_funds, &doc) != TDX_OK)
        return;
    CHECK(tdx_valuation_parse_funds(&doc, &doc.groups[0], funds, VALUATION_FUNDS_ROWS + 4,
                                    &count, &skipped, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == VALUATION_FUNDS_ROWS, "%d funds, got %zu", VALUATION_FUNDS_ROWS, count);
    CHECK(VALUATION_FUNDS_ROWS == 6, "the live resource has 6 rows, header says %d",
          VALUATION_FUNDS_ROWS);
    CHECK(strcmp(funds[0].security_id, "SH510210") == 0, "the first fund is SH510210, got %s",
          funds[0].security_id);
    CHECK(funds[0].has_unit_nav && funds[0].unit_nav > 1.0135 && funds[0].unit_nav < 1.0137,
          "unit NAV 1.0136, got %f", funds[0].unit_nav);
    CHECK(funds[0].has_premium_pct && funds[0].premium_pct > -0.0592 &&
              funds[0].premium_pct < -0.0591,
          "a premium of -0.059195 per cent, got %f", funds[0].premium_pct);
    CHECK(funds[0].has_size_yuan && funds[0].size_yuan > 1.34e10 && funds[0].size_yuan < 1.35e10,
          "and a size of 1.35e10 yuan, got %f", funds[0].size_yuan);
    CHECK(funds[0].fund_type.present, "the fund type is present: %s",
          funds[0].fund_type.present ? "(yes)" : "(no)");
    tdx_jsn_document_free(&doc);
}

static void test_rendering(void) {
    tdx_jsn_document doc = {0};
    tdx_valuation_index indices[VALUATION_MASTER_ROWS + 4];
    tdx_valuation_point points[4];
    tdx_valuation_merge merge;
    size_t count = 0;
    size_t skipped = 0;
    tdx_buf line = {0};
    char reason[192];

    if (parse(valuation_master, &doc) != TDX_OK)
        return;
    if (tdx_valuation_parse_master(&doc, &doc.groups[0], indices, VALUATION_MASTER_ROWS + 4,
                                   &count, &skipped, &error) != TDX_OK) {
        CHECK(0, "parse: %s", error.message);
        tdx_jsn_document_free(&doc);
        return;
    }
    tdx_buf_init(&line);
    CHECK(tdx_valuation_format_index(&line, &indices[0], 0, &error) == TDX_OK, "render: %s",
          error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the index line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"valuation_index\"") != NULL, "the type");
    CHECK(strstr(text_of(&line), "\"detail_id\":\"000001\"") != NULL,
          "the detail id, which names the history resources: %s", text_of(&line));
    CHECK(strstr(text_of(&line), "\"pe_percentile\":79.108200") != NULL, "the percentile");
    CHECK(strstr(text_of(&line), "\"returns\":{\"days_5\":") != NULL, "the returns block");

    /* A merged point and the merge report, on synthetic values so both halves show. */
    memset(points, 0, sizeof(points));
    snprintf(points[0].date, sizeof(points[0].date), "20240103");
    points[0].has_pe = 1;
    points[0].pe = 9.1696;
    points[0].has_pb = 1;
    points[0].pb = 1.2749;
    tdx_buf_clear(&line);
    CHECK(tdx_valuation_format_point(&line, &points[0], "SH000001", 0, &error) == TDX_OK,
          "point render: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "the point parses: %s", reason);
    CHECK(strstr(text_of(&line), "\"pe\":9.169600") != NULL, "both halves: %s",
          text_of(&line));

    /* A point whose PB is absent renders it as null rather than as zero. */
    tdx_buf_clear(&line);
    points[1] = points[0];
    points[1].has_pb = 0;
    CHECK(tdx_valuation_format_point(&line, &points[1], "SH000001", 1, &error) == TDX_OK,
          "render: %s", error.message);
    CHECK(strstr(text_of(&line), "\"pb\":null") != NULL, "an absent half is null: %s",
          text_of(&line));

    /* THE FUND RENDERER TOO: it was the one this test did not cover, and that is exactly
     * where the same missing brace survived. */
    {
        tdx_jsn_document funds_doc = {0};
        tdx_valuation_fund funds[VALUATION_FUNDS_ROWS + 4];
        size_t fund_count = 0;
        size_t fund_skipped = 0;
        if (parse(valuation_funds, &funds_doc) == TDX_OK) {
            if (tdx_valuation_parse_funds(&funds_doc, &funds_doc.groups[0], funds,
                                          VALUATION_FUNDS_ROWS + 4, &fund_count,
                                          &fund_skipped, &error) == TDX_OK &&
                fund_count > 0) {
                tdx_buf_clear(&line);
                CHECK(tdx_valuation_format_fund(&line, &funds[0], 0, &error) == TDX_OK,
                      "fund render: %s", error.message);
                CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
                      "the fund line parses: %s\n    %s", reason, text_of(&line));
                CHECK(strstr(text_of(&line), "\"type\":\"valuation_fund\"") != NULL,
                      "the type");
            } else {
                CHECK(0, "parsing the funds fixture: %s", error.message);
            }
            tdx_jsn_document_free(&funds_doc);
        }
    }
    memset(&merge, 0, sizeof(merge));
    merge.points = 3093;
    merge.both_sides = 3093;
    merge.complete = 1;
    tdx_buf_clear(&line);
    CHECK(tdx_valuation_format_merge(&line, &merge, "000001", "zsgz3/000001.jsn",
                                     "zsgz4/000001.jsn", &error) == TDX_OK,
          "merge render: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "the merge parses: %s",
          reason);
    CHECK(strstr(text_of(&line), "\"complete\":true") != NULL, "the verdict: %s",
          text_of(&line));
    tdx_buf_free(&line);
    tdx_jsn_document_free(&doc);
}

int main(void) {
    test_master();
    test_history_merge();
    test_merge_edges();
    test_funds();
    test_rendering();

    if (failures) {
        printf("%d valuation check(s) failed\n", failures);
        return 1;
    }
    printf("valuation checks passed\n");
    return 0;
}
