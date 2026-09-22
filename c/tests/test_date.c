#include <stdio.h>
#include <string.h>
#include "tdx_date.h"
#include "tdx_bond_math.h"
#include "tdx_trades.h"
#include "tdx_minute.h"
#include "tdx_zst_day.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(void) {
    static const char *invalid[] = {"20260229", "20260230", "20260431", "19000229",
                                    "21000229", "00000101", "2026-2-03", "20260-203",
                                    "2026 0203", "-20260203", "20260203-"};
    size_t index;
    uint32_t date = 0;
    uint16_t word;
    long long days = 0, next = 0;
    tdx_error err = {{0}};
    tdx_code security = {0};
    char path[128];
    memcpy(security.code, "000623", 7);
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        CHECK(!tdx_date_parse(invalid[index], strlen(invalid[index]), 1, &date));
        CHECK(!tdx_bond_civil_days(invalid[index], strlen(invalid[index]), &days));
    }
    CHECK(tdx_date_parse("2000-02-29", 10, 1, &date) && date == 20000229);
    CHECK(!tdx_date_parse("2000-02-29", 10, 0, &date));
    CHECK(tdx_date_days(19700101, &days) && days == 0);
    CHECK(tdx_date_days(20000229, &days));
    CHECK(tdx_date_days(20000301, &next) && next == days + 1);
    CHECK(tdx_date_days(21000228, &days));
    CHECK(tdx_date_days(21000301, &next) && next == days + 1);
    CHECK(tdx_trades_date_valid("20260230", &err) == TDX_ERR);
    CHECK(tdx_zst_day_build_paths(&security, "20260431", NULL, path, sizeof(path),
                                  NULL, 0, &err) == TDX_ERR);
    memcpy(security.code, "12/345", 7);
    CHECK(tdx_zst_day_build_paths(&security, "20260203", NULL, path, sizeof(path),
                                  NULL, 0, &err) == TDX_ERR);
    memcpy(security.code, "000623", 7);
    security.market_id = 99;
    CHECK(tdx_zst_day_build_paths(&security, "20260203", NULL, path, sizeof(path),
                                  NULL, 0, &err) == TDX_ERR);
    CHECK(!tdx_lc1_encode_date(20260230, &word));
    CHECK(!tdx_lc1_encode_date(20260431, &word));
    CHECK(tdx_lc1_encode_date(20240229, &word));
    CHECK(tdx_lc1_decode_date(word, &date) && date == 20240229);
    printf("date checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
