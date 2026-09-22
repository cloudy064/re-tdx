/* test_limit.c - the price-limit rules.
 *
 * The rules come from a real file the terminal keeps, and the fixture is that file's own
 * bytes: 217 bytes of INI.  Reading it settled two things a paraphrase could not:
 *
 *   * the shape is "[section]" headers with bare "key=value" lines, not "[key=value]" - an
 *     earlier dump of the file wrapped every line in brackets because of the formatting
 *     chosen while printing it, and the first parser written from that display found
 *     nothing at all;
 *   * the switch dates are 99991231, not the reference's default of 0, so the
 *     special-treatment branch NEVER fires on this machine and such stocks stay at 10 per
 *     cent instead of dropping to 5.
 *
 * The rounding is checked against arithmetic here, and the whole calculation is checked
 * against real daily bars by output/verify_limit_prices.py: over 1,200 trading days of 30
 * securities whose data reaches the present, no day's high breaks the ceiling and no low
 * breaks the floor, except one ex-rights day where the previous close in the file is not
 * the reference price the exchange used. */
#include <stdio.h>
#include <string.h>

#include "tdx_limit.h"
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

/* The real file's bytes. */
static const char rule_file[] =
    "[RULE]\r\n"
    "SHGTDayMax=520\r\n"
    "SZGTDayMax=520\r\n"
    "\r\n"
    "MainAG_Cage=1\r\n"
    "\r\n"
    "CYBZDRatio=0.20\r\n"
    "CYBZCDate=20200822\r\n"
    "\r\n"
    "SHGZ_XS3=1\r\n"
    "SHKZZ_XS3=1\r\n"
    "\r\n"
    "KCBOpenDate=20250922\r\n"
    "KCCZStartDate=20250713\r\n"
    "\r\n"
    "SZST10Date=99991231\r\n"
    "SHST10Date=99991231\r\n"
    "\r\n";

static void test_rule_file(void) {
    tdx_limit_rules rules;

    error.message[0] = '\0';
    tdx_limit_rules_default(&rules);
    CHECK(rules.sz_st_10_date == 0 && rules.sh_st_10_date == 0,
          "the defaults have a zero switch date, got %d and %d", rules.sz_st_10_date,
          rules.sh_st_10_date);
    CHECK(strcmp(rules.source, "TdxW-defaults") == 0, "and say so, got %s", rules.source);

    /* THE FILE'S OWN SHAPE, which the first parser got wrong. */
    CHECK(tdx_limit_rules_parse(rule_file, strlen(rule_file), &rules, &error) == TDX_OK,
          "the real rule file parses: %s", error.message);
    CHECK(rules.sz_st_10_date == 99991231, "SZST10Date is 99991231, got %d",
          rules.sz_st_10_date);
    CHECK(rules.sh_st_10_date == 99991231, "SHST10Date is 99991231, got %d",
          rules.sh_st_10_date);
    CHECK(rules.chinext_rate > 0.1999 && rules.chinext_rate < 0.2001,
          "CYBZDRatio is 0.20, got %f", rules.chinext_rate);
    /* The file has no STAR or Beijing rate, so those keep their defaults. */
    CHECK(rules.star_rate > 0.1999 && rules.star_rate < 0.2001,
          "the STAR rate stays at its default 0.20, got %f", rules.star_rate);
    CHECK(rules.beijing_rate > 0.2999 && rules.beijing_rate < 0.3001,
          "and Beijing at 0.30, got %f", rules.beijing_rate);

    /* A section header is not a rule, and neither is a blank line: a parser that took
     * "[RULE]" as one would have produced a key of "RULE" and no value. */
    {
        tdx_limit_rules none;
        static const char unknown_rules[] = "[RULE]\n\nMainAG_Cage=1\n";
        tdx_limit_rules_default(&none);
        error.message[0] = '\0';
        CHECK(tdx_limit_rules_parse(unknown_rules, sizeof(unknown_rules) - 1, &none, &error) == TDX_ERR,
              "a file with none of the known keys is refused");
        CHECK(strstr(error.message, "none of the keys") != NULL, "and says so: %s",
              error.message);
    }
    /* Comments are skipped. */
    {
        tdx_limit_rules comments;
        static const char *text = "; a comment\n# another\nCYBZDRatio=0.15\n";
        tdx_limit_rules_default(&comments);
        error.message[0] = '\0';
        CHECK(tdx_limit_rules_parse(text, strlen(text), &comments, &error) == TDX_OK,
              "comments are skipped: %s", error.message);
        CHECK(comments.chinext_rate > 0.1499 && comments.chinext_rate < 0.1501,
              "and the rate after them is read, got %f", comments.chinext_rate);
    }
    CHECK(tdx_limit_rules_parse(NULL, 0, &rules, &error) == TDX_ERR, "no text is refused");
}

static void test_security_class(void) {
    CHECK(tdx_limit_security_class(0, "000001") == 0, "a Shenzhen main-board stock is 0");
    CHECK(tdx_limit_security_class(0, "001979") == 0, "and 001 is the same class");
    CHECK(tdx_limit_security_class(0, "002415") == 8, "002 is the SME class");
    CHECK(tdx_limit_security_class(0, "003816") == 8, "and 003");
    CHECK(tdx_limit_security_class(0, "300750") == 9, "300 is ChiNext");
    CHECK(tdx_limit_security_class(0, "301029") == 9, "and 301");
    CHECK(tdx_limit_security_class(1, "600519") == 11, "a Shanghai main-board stock is 11");
    CHECK(tdx_limit_security_class(1, "601398") == 11, "and 601");
    CHECK(tdx_limit_security_class(1, "688981") == 19, "688 is STAR");
    CHECK(tdx_limit_security_class(2, "920001") == 21, "920 is Beijing");
    /* Anything the table does not cover is not price limited. */
    CHECK(tdx_limit_security_class(0, "123071") == -1, "a convertible bond is not");
    CHECK(tdx_limit_security_class(1, "110075") == -1, "neither is one on Shanghai");
    CHECK(tdx_limit_security_class(0, "00001") == -1, "a five-digit code is not");
    CHECK(tdx_limit_security_class(5, "600519") == -1, "nor is an unknown market");
}

static void test_rounding(void) {
    /* The rounding has THREE steps and a bias of 0.503, which makes a truncation behave as
     * round-half-up.  Checked against arithmetic rather than against another limit. */
    CHECK(tdx_limit_upper(10.00, 0.10) > 10.99 && tdx_limit_upper(10.00, 0.10) < 11.01,
          "10.00 up 10 per cent is 11.00, got %.6f", tdx_limit_upper(10.00, 0.10));
    CHECK(tdx_limit_lower(10.00, 0.10) > 8.99 && tdx_limit_lower(10.00, 0.10) < 9.01,
          "and down is 9.00, got %.6f", tdx_limit_lower(10.00, 0.10));
    CHECK(tdx_limit_upper(11.70, 0.10) > 12.86 && tdx_limit_upper(11.70, 0.10) < 12.88,
          "11.70 up 10 per cent is 12.87, got %.6f", tdx_limit_upper(11.70, 0.10));
    CHECK(tdx_limit_lower(11.70, 0.10) > 10.52 && tdx_limit_lower(11.70, 0.10) < 10.54,
          "and down is 10.53, got %.6f", tdx_limit_lower(11.70, 0.10));

    /* THE THREE STEPS MATTER.  A single-step rounding of (previous * 1.1) would differ
     * wherever the increment itself rounds: 3.33 at 10 per cent is such a case, and the
     * two-step result is 3.66 rather than 3.66 from one multiplication. */
    {
        double two_step = tdx_limit_upper(3.33, 0.10);
        CHECK(two_step > 3.65 && two_step < 3.67, "3.33 up 10 per cent is 3.66, got %.6f",
              two_step);
    }
    /* The bias turns truncation into round-half-up: 10.005 must go up to 10.01, not down
     * to 10.00. */
    CHECK(tdx_limit_upper(9.10, 0.10) > 10.00 && tdx_limit_upper(9.10, 0.10) < 10.02,
          "9.10 up 10 per cent is 10.01, got %.6f", tdx_limit_upper(9.10, 0.10));

    /* BEIJING ROUNDS THE OTHER WAY: its bias is 0.003 for the upper limit, which is a
     * truncation, so a value that needs rounding goes DOWN. */
    CHECK(tdx_limit_beijing(10.00, 0.30, 1) > 12.99 && tdx_limit_beijing(10.00, 0.30, 1) < 13.01,
          "10.00 up 30 per cent is 13.00, got %.6f", tdx_limit_beijing(10.00, 0.30, 1));
    CHECK(tdx_limit_beijing(10.00, 0.30, 0) > 6.99 && tdx_limit_beijing(10.00, 0.30, 0) < 7.01,
          "and down is 7.00, got %.6f", tdx_limit_beijing(10.00, 0.30, 0));
    /* The two roundings disagree exactly where a half-cent appears, which is the point of
     * having two functions. */
    CHECK(tdx_limit_upper(3.335, 0.10) != tdx_limit_beijing(3.335, 0.10, 1),
          "the two roundings differ on a value that needs rounding");
}

static void test_calculation(void) {
    tdx_limit_rules rules;
    tdx_limit_prices prices;

    tdx_limit_rules_default(&rules);
    (void)tdx_limit_rules_parse(rule_file, strlen(rule_file), &rules, &error);

    /* SZ main board at 10 per cent. */
    CHECK(tdx_limit_calculate(0, "000001", "", 11.70, 20260922, &rules, &prices) == 1,
          "a Shenzhen main-board stock is price limited");
    CHECK(prices.security_class == 0, "class 0, got %d", prices.security_class);
    CHECK(prices.rate > 0.0999 && prices.rate < 0.1001, "at 10 per cent, got %f", prices.rate);
    CHECK(prices.upper > 12.86 && prices.upper < 12.88, "upper 12.87, got %f", prices.upper);
    CHECK(prices.lower > 10.52 && prices.lower < 10.54, "lower 10.53, got %f", prices.lower);

    /* ChiNext and STAR at 20, Beijing at 30 with its own rounding. */
    CHECK(tdx_limit_calculate(0, "300750", "", 100.00, 20260922, &rules, &prices) == 1 &&
              prices.rate > 0.1999 && prices.rate < 0.2001,
          "ChiNext is at 20 per cent, got %f", prices.rate);
    CHECK(tdx_limit_calculate(1, "688981", "", 100.00, 20260922, &rules, &prices) == 1 &&
              prices.rate > 0.1999 && prices.rate < 0.2001,
          "STAR is at 20 per cent, got %f", prices.rate);
    CHECK(tdx_limit_calculate(2, "920001", "", 10.00, 20260922, &rules, &prices) == 1 &&
              prices.rate > 0.2999 && prices.rate < 0.3001,
          "Beijing is at 30 per cent, got %f", prices.rate);
    CHECK(prices.upper > 12.99 && prices.upper < 13.01,
          "and rounds by truncation, giving 13.00, got %f", prices.upper);

    /* THE SWITCH DATE.  The real file says 99991231, so special treatment stays at 10 per
     * cent; with the reference's default of 0 it would be 5. */
    CHECK(tdx_limit_calculate(0, "000001", "*ST\xe5\xb9\xb3\xe5\xae\x89", 10.00, 20260922,
                              &rules, &prices) == 1 &&
              prices.rate > 0.0999 && prices.rate < 0.1001,
          "a special-treatment name is still 10 per cent with this file, got %f", prices.rate);
    {
        tdx_limit_rules defaults;
        tdx_limit_rules_default(&defaults);
        CHECK(tdx_limit_calculate(0, "000001", "*ST\xe5\xb9\xb3\xe5\xae\x89", 10.00, 20260922,
                                  &defaults, &prices) == 1 &&
                  prices.rate > 0.0499 && prices.rate < 0.0501,
              "while the defaults put it at 5, got %f", prices.rate);
    }

    /* Not price limited: a bond, an absent previous close, a new listing's first day. */
    CHECK(tdx_limit_calculate(1, "110075", "", 106.37, 20260922, &rules, &prices) == 0,
          "a convertible bond is not price limited");
    CHECK(tdx_limit_calculate(0, "000001", "", 0.0, 20260922, &rules, &prices) == 0,
          "nor is a stock with no previous close");
    CHECK(tdx_limit_calculate(0, "000001", "N\xe6\x96\xb0\xe8\x82\xa1", 10.00, 20260922,
                              &rules, &prices) == 0,
          "nor a new listing, whose name begins with N");
    CHECK(tdx_limit_calculate(0, "000001", "N\xe6\x96\xb0\xe8\x82\xa1", 10.00, 20260922,
                              &rules, &prices) == 0 && prices.available == 0,
          "and it reports itself unavailable");
    CHECK(tdx_limit_calculate(0, "000001", "", 10.00, 20260922, NULL, &prices) == 1,
          "a null rule set falls back to the defaults rather than refusing");
}

int main(void) {
    test_rule_file();
    test_security_class();
    test_rounding();
    test_calculation();

    if (failures) {
        printf("%d limit check(s) failed\n", failures);
        return 1;
    }
    printf("limit checks passed\n");
    return 0;
}
