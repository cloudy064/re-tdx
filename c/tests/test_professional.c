/* test_professional.c - the professional-data .dat files.
 *
 * The fixture is a verbatim PREFIX of two real files the reference implementation left
 * in its own cache: the per-stock file and a board file.  Each prefix carries its source
 * file's md5 and size, and the values asserted below are the values those bytes encode -
 * read out of the file, not chosen.
 *
 * The two files are picked for what they show together:
 *
 *   stock prefix   ids the 44-field table names AND ids it does not (45, 47..50)
 *   board prefix   every id named, since the board table covers 5..19 completely
 *
 * The full files are 380 KB to 1.1 MB, so the counts and date ranges of the whole files
 * live in the evidence file; what the fixture can honestly carry is the prefix, and the
 * tests below say so rather than pretending otherwise.
 *
 * The format itself was settled by measurement, not guesswork: all three samples divide
 * by 13 exactly.  A file that does NOT divide is an error, not a partial trailing record
 * to ignore. */
#include <stdio.h>
#include <string.h>

#include "tdx_professional.h"
#include "tdx_professional_json.h"
#include "professional_fixtures.h"

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

static void test_tables(void) {
    tdx_professional_kind kind;

    CHECK(tdx_professional_field_count(TDX_PROFESSIONAL_STOCK) == 44,
          "the stock table lists 44 fields");
    CHECK(tdx_professional_field_count(TDX_PROFESSIONAL_MARKET) == 42,
          "the market table lists 42 fields");
    CHECK(tdx_professional_field_count(TDX_PROFESSIONAL_BOARD) == 15,
          "the board table lists 15 fields");
    /* A named id resolves to a name; an id outside the table resolves to NOTHING, which
     * is a real answer - the samples carry such ids. */
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_STOCK, 1) != NULL &&
              tdx_professional_field_name(TDX_PROFESSIONAL_STOCK, 44) != NULL,
          "the first and last stock ids are named");
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_STOCK, 45) == NULL,
          "id 45 is past the stock table and names nothing");
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_MARKET, 43) == NULL,
          "and 43 is past the market table");
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_MARKET, 99) == NULL,
          "as is 99, which the market file also carries");
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_BOARD, 5) != NULL &&
              tdx_professional_field_name(TDX_PROFESSIONAL_BOARD, 19) != NULL,
          "the board table covers 5..19");
    /* The three tables are not the same table: an id can be named in one and not another. */
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_BOARD, 4) == NULL,
          "id 4 is a stock and market field but not a board field");
    CHECK(tdx_professional_field_name(TDX_PROFESSIONAL_STOCK, 19) != NULL,
          "while 19 is named in both stock and board");
    CHECK(tdx_professional_kind_parse("stock", 5, &kind) && kind == TDX_PROFESSIONAL_STOCK,
          "kind parsing: stock");
    CHECK(tdx_professional_kind_parse("market", 6, &kind) && kind == TDX_PROFESSIONAL_MARKET,
          "kind parsing: market");
    CHECK(tdx_professional_kind_parse("board", 5, &kind) && kind == TDX_PROFESSIONAL_BOARD,
          "kind parsing: board");
    CHECK(!tdx_professional_kind_parse("equity", 6, &kind), "and anything else is refused");
    CHECK(!tdx_professional_kind_parse("Stock", 5, &kind), "case matters");
}

static void test_parse_prefix(void) {
    tdx_professional_record records[PROFESSIONAL_STOCK_PREFIX_RECORDS + 4];
    size_t count = 0;

    error.message[0] = '\0';
    CHECK(tdx_professional_parse_trading(professional_stock_prefix,
                                         PROFESSIONAL_STOCK_PREFIX_BYTES, records,
                                         PROFESSIONAL_STOCK_PREFIX_RECORDS + 4, &count,
                                         &error) == TDX_OK,
          "the prefix parses: %s", error.message);
    CHECK(count == PROFESSIONAL_STOCK_PREFIX_RECORDS, "%d records, got %zu",
          PROFESSIONAL_STOCK_PREFIX_RECORDS, count);

    /* The first four records, exactly as the source bytes encode them. */
    CHECK(records[0].id == 1 && records[0].date == 19971231,
          "record 0 is id 1 dated 19971231, got id %u date %u", records[0].id,
          records[0].date);
    CHECK(records[0].first == 833471.0f && records[0].second == 0.0f,
          "with values 833471 and 0, got %f and %f", records[0].first, records[0].second);
    CHECK(records[1].date == 19980630 && records[1].first == 918331.0f,
          "record 1 is dated 19980630 with 918331");
    CHECK(records[3].date == 19991231 && records[3].first == 705443.0f,
          "record 3 is dated 19991231 with 705443");
    /* The prefix holds only id 1, so the id set is a single entry. */
    {
        tdx_professional_id_summary fields[64];
        size_t field_count = 0;
        CHECK(tdx_professional_summarize(records, count, TDX_PROFESSIONAL_STOCK, fields, 64,
                                         &field_count, &error) == TDX_OK,
              "summarize: %s", error.message);
        CHECK(field_count == 1, "the prefix carries one field, got %zu", field_count);
        if (field_count == 1) {
            CHECK(fields[0].id == 1 && fields[0].count == count,
                  "id 1 with every record, got id %u count %zu", fields[0].id, fields[0].count);
            CHECK(fields[0].name != NULL, "and it is a named id");
            /* The span of the whole 24-record prefix, read from the file rather than
             * extrapolated from its first few records. */
            CHECK(fields[0].first_date == 19971231 && fields[0].last_date == 20060630,
                  "spanning 19971231..20060630, got %u..%u", fields[0].first_date,
                  fields[0].last_date);
        }
    }
}

static void test_board_prefix_is_fully_named(void) {
    tdx_professional_record records[PROFESSIONAL_BOARD_PREFIX_RECORDS + 4];
    tdx_professional_id_summary fields[64];
    size_t count = 0;
    size_t field_count = 0;

    error.message[0] = '\0';
    CHECK(tdx_professional_parse_trading(professional_board_prefix,
                                         PROFESSIONAL_BOARD_PREFIX_BYTES, records,
                                         PROFESSIONAL_BOARD_PREFIX_RECORDS + 4, &count,
                                         &error) == TDX_OK,
          "the board prefix parses: %s", error.message);
    CHECK(records[0].id == 5 && records[0].date == 20180517,
          "its first record is id 5 dated 20180517, got id %u date %u", records[0].id,
          records[0].date);
    /* A float's stored bits are asserted exactly, because the parser reads four bytes
     * into a float and any rounding it introduced would show here. */
    CHECK(records[0].first == 7.110000133514404f,
          "and the first value is the file's 7.1100001, got %.9g", (double)records[0].first);
    CHECK(tdx_professional_summarize(records, count, TDX_PROFESSIONAL_BOARD, fields, 64,
                                     &field_count, &error) == TDX_OK,
          "summarize: %s", error.message);
    CHECK(field_count == 1 && fields[0].id == 5, "the prefix carries id 5, got %zu",
          field_count);
    if (field_count == 1)
        CHECK(fields[0].name != NULL,
              "and the board table names it, unlike the stock file's extra ids");
}

static void test_refusals(void) {
    tdx_professional_record records[8];
    size_t count = 0;
    unsigned char truncated[PROFESSIONAL_STOCK_PREFIX_BYTES + 1];
    static unsigned char bad_date[26];
    static unsigned char bad_day[26];

    error.message[0] = '\0';
    /* Not a multiple of 13: the record size is settled by measurement, so a remainder
     * means this is not that format. */
    memcpy(truncated, professional_stock_prefix, PROFESSIONAL_STOCK_PREFIX_BYTES);
    truncated[PROFESSIONAL_STOCK_PREFIX_BYTES] = 0x2a;
    CHECK(tdx_professional_parse_trading(truncated, PROFESSIONAL_STOCK_PREFIX_BYTES + 1, records,
                                         8, &count, &error) == TDX_ERR,
          "a file that is not a multiple of 13 is refused");
    CHECK(strstr(error.message, "multiple") != NULL, "and says why: %s", error.message);

    /* A date that is present but impossible is an error, not a skipped record. */
    memset(bad_date, 0, sizeof(bad_date));
    bad_date[0] = 1;
    bad_date[1] = 31; /* 2026-13-31 in little endian below */
    bad_date[2] = 0x0d;
    bad_date[3] = 0x20;
    bad_date[4] = 0x07;
    error.message[0] = '\0';
    CHECK(tdx_professional_parse_trading(bad_date, sizeof(bad_date), records, 8, &count,
                                         &error) == TDX_ERR,
          "month 13 is refused: %s", error.message);
    CHECK(strstr(error.message, "date") != NULL, "and the message names the field");

    /* February 31 does not exist even though the reference's day <= 31 check allows it. */
    memset(bad_day, 0, sizeof(bad_day));
    bad_day[0] = 1;
    bad_day[1] = 31;
    bad_day[2] = 0x02;
    bad_day[3] = 0x20;
    bad_day[4] = 0x07; /* 20260231 */
    error.message[0] = '\0';
    CHECK(tdx_professional_parse_trading(bad_day, sizeof(bad_day), records, 8, &count,
                                         &error) == TDX_ERR,
          "20260231 is refused: %s", error.message);

    /* A date of zero is the file saying "no date", which is allowed. */
    {
        unsigned char undated[13];
        memset(undated, 0, sizeof(undated));
        undated[0] = 7;
        undated[5] = 0x00;
        undated[6] = 0x00;
        undated[7] = 0x80;
        undated[8] = 0x3f; /* 1.0f */
        CHECK(tdx_professional_parse_trading(undated, sizeof(undated), records, 8, &count,
                                             &error) == TDX_OK,
              "an undated record is allowed: %s", error.message);
        CHECK(count == 1 && records[0].date == 0, "and its date reads as absent");
        /* Selecting with a range leaves it out, because a range cannot be said to
         * contain a record with no date. */
        {
            tdx_professional_selection selection;
            size_t indices[4];
            size_t selected = 0;
            selection.id = 0;
            selection.from = 20000101;
            selection.to = 20300101;
            CHECK(tdx_professional_select(records, 1, &selection, indices, 4, &selected,
                                          &error) == TDX_OK,
                  "select: %s", error.message);
            CHECK(selected == 0, "an undated record is left out of a date range, got %zu",
                  selected);
            selection.from = 0;
            selection.to = 0;
            CHECK(tdx_professional_select(records, 1, &selection, indices, 4, &selected,
                                          &error) == TDX_OK && selected == 1,
                  "but kept when no range was asked for");
        }
    }
    /* Capacity is enforced rather than overrun. */
    CHECK(tdx_professional_parse_trading(professional_stock_prefix,
                                         PROFESSIONAL_STOCK_PREFIX_BYTES, records, 1, &count,
                                         &error) == TDX_ERR,
          "a one-record buffer for a 24-record prefix must be refused");
    CHECK(tdx_professional_parse_trading(NULL, 0, records, 8, &count, &error) == TDX_ERR,
          "no bytes is refused");
    CHECK(tdx_professional_summarize(records, 8, TDX_PROFESSIONAL_STOCK, NULL, 8, &count,
                                     &error) == TDX_ERR,
          "summarizing without room is refused");
}

static void test_selection(void) {
    /* Synthetic records, because selection is arithmetic on dates and ids and the corner
     * cases - a bound that is only one-sided, an id that matches nothing - are easier to
     * state than to find in a capture. */
    tdx_professional_record records[6];
    tdx_professional_selection selection;
    size_t indices[8];
    size_t selected = 0;
    size_t index;

    /* Explicit dates: the first version computed them as 20240101 + index * 100, which
     * adds a month and not a day, so the range expectations below described data the
     * test never built. */
    {
        static const uint32_t dates[6] = {20240101, 20240102, 20240103, 20240104, 20240105,
                                          20240106};
        for (index = 0; index < 6; ++index) {
            records[index].id = (unsigned)(index < 3 ? 3 : 7);
            records[index].date = dates[index];
            records[index].first = (float)index;
            records[index].second = 0.0f;
        }
    }
    error.message[0] = '\0';

    /* Everything. */
    selection.id = 0;
    selection.from = 0;
    selection.to = 0;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 6,
          "an empty selection takes everything, got %zu", selected);

    /* One id. */
    selection.id = 3;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 3,
          "id 3 matches three records, got %zu", selected);
    for (index = 0; index < selected; ++index)
        CHECK(records[indices[index]].id == 3, "and every match has that id");

    /* An id nothing carries. */
    selection.id = 9;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 0,
          "an id nothing carries matches nothing, got %zu", selected);

    /* The bounds are inclusive on both ends. */
    selection.id = 0;
    selection.from = 20240101;
    selection.to = 20240103;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 3,
          "the range includes both bounds, got %zu", selected);
    selection.from = 20240102;
    selection.to = 20240102;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 1 && records[indices[0]].date == 20240102,
          "a single-day range takes one record");

    /* One-sided bounds. */
    selection.from = 20240104;
    selection.to = 0;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 3,
          "a from-only range takes the later records, got %zu", selected);
    selection.from = 0;
    selection.to = 20240103;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 3,
          "a to-only range takes the earlier ones, got %zu", selected);

    /* A range that matches nothing. */
    selection.from = 20300101;
    selection.to = 20301231;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 8, &selected, &error) ==
              TDX_OK && selected == 0,
          "a future range matches nothing, got %zu", selected);

    /* Capacity is enforced. */
    selection.from = 0;
    selection.to = 0;
    CHECK(tdx_professional_select(records, 6, &selection, indices, 2, &selected, &error) ==
              TDX_ERR,
          "a two-slot buffer for six matches must be refused");
}

static void test_rendering(void) {
    tdx_professional_record records[PROFESSIONAL_STOCK_PREFIX_RECORDS + 4];
    size_t count = 0;
    tdx_buf line;
    const char *text;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;

    error.message[0] = '\0';
    CHECK(tdx_professional_parse_trading(professional_stock_prefix,
                                         PROFESSIONAL_STOCK_PREFIX_BYTES, records,
                                         PROFESSIONAL_STOCK_PREFIX_RECORDS + 4, &count,
                                         &error) == TDX_OK,
          "parse: %s", error.message);
    tdx_buf_init(&line);

    CHECK(tdx_professional_format_record(&line, &records[0],
                                         tdx_professional_field_name(TDX_PROFESSIONAL_STOCK,
                                                                     records[0].id),
                                         0, &error) == TDX_OK,
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
            CHECK(depth >= 0, "the record must not close early");
        }
    }
    CHECK(depth == 0 && !in_string, "the record is balanced, depth %d in_string %d", depth,
          in_string);
    CHECK(strstr(text, "\"type\":\"professional_record\"") != NULL, "the type");
    CHECK(strstr(text, "\"id\":1,") != NULL, "the field id");
    CHECK(strstr(text, "\"date\":19971231") != NULL, "the date");
    CHECK(strstr(text, "\"first\":833471") != NULL, "the first value: %s", text);
    /* An unnamed id renders a null name rather than a made-up one. */
    {
        tdx_professional_record unknown;
        memset(&unknown, 0, sizeof(unknown));
        unknown.id = 45;
        unknown.date = 20240101;
        unknown.first = 1.5f;
        tdx_buf_clear(&line);
        CHECK(tdx_professional_format_record(&line, &unknown,
                                             tdx_professional_field_name(TDX_PROFESSIONAL_STOCK,
                                                                         45),
                                             0, &error) == TDX_OK,
              "an unnamed id renders: %s", error.message);
        CHECK(strstr(text_of(&line), "\"id\":45,\"name\":null") != NULL,
              "with a null name, not an invented one: %s", text_of(&line));
    }
    /* A non-finite stored value renders null: it is in the file, but it is not a value. */
    {
        tdx_professional_record infinite;
        memset(&infinite, 0, sizeof(infinite));
        infinite.id = 1;
        infinite.date = 20240101;
        infinite.first = 1.0f / 0.0f;
        tdx_buf_clear(&line);
        CHECK(tdx_professional_format_record(&line, &infinite, NULL, 0, &error) == TDX_OK,
              "a non-finite value renders: %s", error.message);
        CHECK(strstr(text_of(&line), "\"first\":null") != NULL,
              "as null rather than as a number: %s", text_of(&line));
        CHECK(strstr(text_of(&line), "\"await") == NULL, "and nothing else is invented");
    }
    /* A summary and the document line. */
    {
        tdx_professional_id_summary summary;
        memset(&summary, 0, sizeof(summary));
        summary.id = 3;
        summary.name = tdx_professional_field_name(TDX_PROFESSIONAL_STOCK, 3);
        summary.count = 2000;
        summary.first_date = 20180517;
        summary.last_date = 20260812;
        summary.first_min = 1.0f;
        summary.first_max = 2.0f;
        tdx_buf_clear(&line);
        CHECK(tdx_professional_format_summary(&line, &summary, TDX_PROFESSIONAL_STOCK,
                                              &error) == TDX_OK,
              "summary: %s", error.message);
        CHECK(strstr(text_of(&line), "\"records\":2000") != NULL, "the record count");
        CHECK(strstr(text_of(&line), "\"named\":true") != NULL, "and that it is named");

        tdx_buf_clear(&line);
        CHECK(tdx_professional_format_document(&line, "x.dat", TDX_PROFESSIONAL_STOCK, 397397,
                                               30569, 46, 41, 5, 242, 19900228, 20260812,
                                               &error) == TDX_OK,
              "document: %s", error.message);
        CHECK(strstr(text_of(&line), "\"records\":30569") != NULL, "the record count");
        CHECK(strstr(text_of(&line), "\"fields_unnamed\":5") != NULL,
              "and the count of ids the table cannot name: %s", text_of(&line));
        CHECK(strstr(text_of(&line), "\"table_size\":44") != NULL, "and the table's size");
    }
    tdx_buf_free(&line);
}

int main(void) {
    test_tables();
    test_parse_prefix();
    test_board_prefix_is_fully_named();
    test_refusals();
    test_selection();
    test_rendering();

    if (failures) {
        printf("%d professional check(s) failed\n", failures);
        return 1;
    }
    printf("professional checks passed\n");
    return 0;
}
