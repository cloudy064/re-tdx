/* test_directory.c - category/board classification and 0x044D page decoding. */
#include <stdio.h>
#include <string.h>

#include "tdx_directory.h"

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

static void put_u16le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_f32le(uint8_t *dst, float value) {
    uint8_t raw[4];
    memcpy(raw, &value, sizeof(raw));
    memcpy(dst, raw, sizeof(raw));
}

static void test_categories(void) {
    static const struct {
        int market;
        const char *code;
        const char *expected;
    } cases[] = {
        {1, "600000", "a_share"},      {1, "601318", "a_share"},
        {1, "603288", "a_share"},      {1, "605368", "a_share"},
        {1, "688111", "a_share"},      {0, "000001", "a_share"},
        {0, "001979", "a_share"},      {0, "002594", "a_share"},
        {0, "300750", "a_share"},      {0, "301236", "a_share"},
        {2, "920478", "a_share"},      {1, "688981", "a_share"},
        {1, "000300", "index"},        {1, "880471", "index"},
        {0, "399001", "index"},        {2, "899050", "index"},
        {1, "510300", "etf"},          {1, "588000", "etf"},
        {0, "159915", "etf"},          {1, "113050", "convertible_bond"},
        {0, "128036", "convertible_bond"}, {2, "810001", "convertible_bond"},
        {1, "900901", "b_share"},      {0, "200011", "b_share"},
        {1, "501018", "fund"},         {0, "160105", "fund"},
        {1, "204001", "repo"},         {0, "131801", "repo"},
        {1, "019547", "bond"},         {0, "102001", "bond"},
        {2, "820001", "bond"},         {1, "999999", "index"},
    };
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const char *actual = tdx_directory_category(cases[index].market, cases[index].code);
        CHECK(strcmp(actual, cases[index].expected) == 0,
              "category(%d,%s) is %s, expected %s", cases[index].market,
              cases[index].code, actual, cases[index].expected);
    }
}

static void test_boards(void) {
    CHECK(strcmp(tdx_directory_board(1, "600000", "a_share"), "sse_main_board") == 0,
          "Shanghai main board");
    CHECK(strcmp(tdx_directory_board(1, "688111", "a_share"), "sse_star_market") == 0,
          "STAR market");
    CHECK(strcmp(tdx_directory_board(0, "000001", "a_share"), "szse_main_board") == 0,
          "Shenzhen main board");
    CHECK(strcmp(tdx_directory_board(0, "300750", "a_share"), "szse_chinext") == 0,
          "ChiNext");
    CHECK(strcmp(tdx_directory_board(2, "920478", "a_share"), "bse_listed_stock") == 0,
          "Beijing listing");
    CHECK(strcmp(tdx_directory_board(1, "510300", "etf"), "none") == 0,
          "non equity boards report none");
}

static void test_category_filter(void) {
    tdx_security security;
    memset(&security, 0, sizeof(security));
    snprintf(security.category, sizeof(security.category), "a_share");
    CHECK(tdx_security_matches_category(&security, "a_share") == 1, "a_share matches");
    CHECK(tdx_security_matches_category(&security, "all") == 1, "all matches");
    CHECK(tdx_security_matches_category(&security, "") == 1, "empty matches");
    CHECK(tdx_security_matches_category(&security, "etf") == 0, "etf does not match");
}

static void build_record(uint8_t *record, const char *code, uint16_t multiple,
                         const uint8_t *name, size_t name_length, uint8_t decimal,
                         float previous_close) {
    memset(record, 0, TDX_DIRECTORY_RECORD_SIZE);
    memcpy(record, code, 6);
    put_u16le(record + 6, multiple);
    if (name_length > 16)
        name_length = 16;
    memcpy(record + 8, name, name_length);
    put_f32le(record + 24, 1.5f);
    record[28] = decimal;
    put_f32le(record + 29, previous_close);
    record[33] = 0xDE;
    record[34] = 0xAD;
    record[35] = 0xBE;
    record[36] = 0xEF;
}

static void test_parse_page(void) {
    /* "平安银行" in GB18030. */
    static const uint8_t pingan[] = {0xC6, 0xBD, 0xB0, 0xB2, 0xD2, 0xF8, 0xD0, 0xD0};
    static const uint8_t maotai[] = {0xB9, 0xF3, 0xD6, 0xDD, 0xC3, 0xA9, 0xCC, 0xA8};
    uint8_t payload[2 + TDX_DIRECTORY_RECORD_SIZE * 2];
    tdx_security_list list;
    size_t parsed = 0;
    tdx_error error;

    error.message[0] = '\0';
    memset(payload, 0, sizeof(payload));
    put_u16le(payload, 2);
    build_record(payload + 2, "000001", 100, pingan, sizeof(pingan), 2, 11.7f);
    build_record(payload + 2 + TDX_DIRECTORY_RECORD_SIZE, "600519", 100, maotai,
                 sizeof(maotai), 2, 1500.0f);

    tdx_security_list_init(&list);
    CHECK(tdx_directory_parse_page(payload, sizeof(payload), 0, &list, &parsed, &error) ==
              TDX_OK,
          "parse failed: %s", error.message);
    CHECK(parsed == 2, "parsed %zu records, expected 2", parsed);
    CHECK(list.count == 2, "list holds %zu records", list.count);
    if (list.count == 2) {
        CHECK(strcmp(list.items[0].code, "000001") == 0, "first code is %s",
              list.items[0].code);
        CHECK(strcmp(list.items[0].name, "平安银行") == 0, "first name is %s",
              list.items[0].name);
        CHECK(strcmp(list.items[0].category, "a_share") == 0, "first category is %s",
              list.items[0].category);
        CHECK(strcmp(list.items[0].board, "szse_main_board") == 0, "first board is %s",
              list.items[0].board);
        CHECK(list.items[0].market_id == 0, "first market is %d", list.items[0].market_id);
        CHECK(list.items[0].multiple == 100, "first multiple is %u",
              (unsigned)list.items[0].multiple);
        CHECK(list.items[0].decimal == 2, "first decimal is %u",
              (unsigned)list.items[0].decimal);
        CHECK(list.items[0].previous_close_price > 11.69 &&
                  list.items[0].previous_close_price < 11.71,
              "first previous close is %.4f", list.items[0].previous_close_price);
        CHECK(strcmp(list.items[1].name, "贵州茅台") == 0, "second name is %s",
              list.items[1].name);
        CHECK(list.items[1].market_id == 0, "second market is %d",
              list.items[1].market_id);
    }
    tdx_security_list_free(&list);

    /* The parser must record the market the caller asked for, not the one in
     * the record bytes. */
    tdx_security_list_init(&list);
    CHECK(tdx_directory_parse_page(payload, sizeof(payload), 1, &list, &parsed, &error) ==
              TDX_OK,
          "parse failed: %s", error.message);
    if (list.count == 2) {
        /* Under market 1 "000001" is a Shanghai index, so the override must
         * change both the category and the board. */
        CHECK(strcmp(list.items[0].category, "index") == 0,
              "market override category is %s", list.items[0].category);
        CHECK(strcmp(list.items[0].board, "none") == 0,
              "market override board is %s", list.items[0].board);
    }
    tdx_security_list_free(&list);
}

static void test_parse_page_rejects(void) {
    uint8_t payload[2 + TDX_DIRECTORY_RECORD_SIZE];
    tdx_security_list list;
    size_t parsed = 0;
    tdx_error error;

    error.message[0] = '\0';
    tdx_security_list_init(&list);
    /* count says one record but the bytes stop short */
    memset(payload, 0, sizeof(payload));
    put_u16le(payload, 5);
    CHECK(tdx_directory_parse_page(payload, sizeof(payload), 0, &list, &parsed, &error) ==
              TDX_ERR,
          "a truncated page must be rejected");
    tdx_security_list_free(&list);

    /* a record whose code is not six digits */
    tdx_security_list_init(&list);
    memset(payload, 0, sizeof(payload));
    put_u16le(payload, 1);
    memcpy(payload + 2, "00A001", 6);
    CHECK(tdx_directory_parse_page(payload, sizeof(payload), 0, &list, &parsed, &error) ==
              TDX_ERR,
          "a non numeric code must be rejected");
    tdx_security_list_free(&list);
}

int main(void) {
    test_categories();
    test_boards();
    test_category_filter();
    test_parse_page();
    test_parse_page_rejects();
    if (failures) {
        printf("%d directory check(s) failed\n", failures);
        return 1;
    }
    printf("directory checks passed\n");
    return 0;
}
