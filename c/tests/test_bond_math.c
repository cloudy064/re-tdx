/* test_bond_math.c - the valuation arithmetic the pricing view needs.
 *
 * This module is unusual in the project in being checkable against something outside
 * it: a calendar, closed-form arithmetic and its own fixed point.  Four independent
 * kinds of check, so a wrong branch in one of them cannot hide behind the others.
 *
 *   1. CALENDAR.  The day numbers are compared against Python's own datetime, computed
 *      by a different algorithm.  If the two disagree about the calendar, one of them
 *      is wrong.
 *   2. CLOSED FORM.  A single cash flow's yield has an exact answer,
 *      (amount / price)^(1/years) - 1, so the bisection can be checked against
 *      arithmetic rather than against another iteration.
 *   3. FIXED POINT.  Discounting the flows at the yield the solver returns must
 *      reproduce the price it was given.  That is the equation it claims to solve, so
 *      this checks the answer rather than the route to it.
 *   4. REFUSALS.  A guard that lets an infinity through is the failure that looks like
 *      data, so every refusal is asserted as a refusal: an out-of-period as-of date, a
 *      rate at or below -100 per cent, a price the interval cannot bracket, a
 *      malformed date. */
#include <stdio.h>
#include <string.h>

#include "tdx_bond_math.h"

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

static long long days_of(const char *text) {
    long long value = -1;
    if (!tdx_bond_civil_days(text, strlen(text), &value))
        return -1;
    return value;
}

static void test_calendar(void) {
    /* Against Python's datetime, which uses a different algorithm. */
    CHECK(days_of("19700101") == 0, "1970-01-01 is day 0, got %lld", days_of("19700101"));
    CHECK(days_of("19700102") == 1, "1970-01-02 is day 1, got %lld", days_of("19700102"));
    CHECK(days_of("19991110") == 10905, "1999-11-10 is day 10905, got %lld",
          days_of("19991110"));
    CHECK(days_of("20000101") == 10957, "2000-01-01 is day 10957, got %lld",
          days_of("20000101"));
    /* A leap day, which is where a hand-rolled calendar usually breaks. */
    CHECK(days_of("20240229") == 19782, "2024-02-29 is day 19782, got %lld",
          days_of("20240229"));
    CHECK(days_of("20260612") == 20616, "2026-06-12 is day 20616, got %lld",
          days_of("20260612"));
    CHECK(days_of("21000228") == 47540, "2100-02-28 is day 47540, got %lld",
          days_of("21000228"));
    /* Consecutive days differ by one across a month, a year and a leap day. */
    CHECK(days_of("20240229") - days_of("20240228") == 1, "the leap day advances by one");
    CHECK(days_of("20240301") - days_of("20240229") == 1, "and so does the day after");
    CHECK(days_of("20250101") - days_of("20241231") == 1, "and across a year boundary");
    /* Hyphens are accepted, because a caller spells dates differently from the table. */
    CHECK(days_of("2026-06-12") == days_of("20260612"), "a hyphenated date is the same day");
    /* Anything that is not a real calendar date is refused. */
    {
        long long value = 0;
        CHECK(tdx_bond_civil_days("2026", 4, &value) == 0, "a short date is refused");
        CHECK(tdx_bond_civil_days("20261301", 8, &value) == 0, "month 13 is refused");
        CHECK(tdx_bond_civil_days("20260132", 8, &value) == 0, "day 32 is refused");
        CHECK(tdx_bond_civil_days("2026ab12", 8, &value) == 0, "non-digits are refused");
        CHECK(tdx_bond_civil_days("", 0, &value) == 0, "an empty date is refused");
        CHECK(tdx_bond_civil_days(NULL, 0, &value) == 0, "a null date is refused");
    }
}

static void test_csv(void) {
    double numbers[8];
    tdx_bond_piece pieces[8];
    size_t count;
    const char *dates = "20260101,20270101, 20280101 ,";
    const char *rates = "0.01,0.02,,abc,0.05";

    count = tdx_bond_csv_numbers(rates, strlen(rates), numbers, 8);
    /* "abc" and the empty piece are skipped, which is what the reference does: a list
     * whose extra entries are blank should not shift the ones that are not. */
    CHECK(count == 3, "three of the five rate pieces are numbers, got %zu", count);
    if (count == 3)
        CHECK(numbers[0] == 0.01 && numbers[1] == 0.02 && numbers[2] == 0.05,
              "and they are the numbers in order: %f %f %f", numbers[0], numbers[1],
              numbers[2]);
    count = tdx_bond_csv_pieces(dates, strlen(dates), pieces, 8);
    CHECK(count == 3, "the trailing empty piece is not a piece, got %zu", count);
    if (count == 3) {
        CHECK(pieces[0].length == 8 && memcmp(dates + pieces[0].offset, "20260101", 8) == 0,
              "the first piece is the first date");
        /* Padding is trimmed, so a piece is its own span in the source. */
        CHECK(pieces[2].length == 8 && memcmp(dates + pieces[2].offset, "20280101", 8) == 0,
              "and the third is trimmed: got %.*s", (int)pieces[2].length,
              dates + pieces[2].offset);
    }
    /* A capacity smaller than the list stops at the capacity rather than overrunning. */
    count = tdx_bond_csv_pieces(dates, strlen(dates), pieces, 2);
    CHECK(count == 2, "a two-piece buffer takes two pieces, got %zu", count);
    CHECK(tdx_bond_csv_pieces(NULL, 0, pieces, 8) == 0, "a null list has no pieces");
    CHECK(tdx_bond_csv_numbers(rates, strlen(rates), numbers, 0) == 0,
          "a zero capacity takes nothing");
}

static void test_accrued_interest(void) {
    double value = 0.0;

    /* IA = face * rate * days / 365.  From 20260101 to 20260315 is 73 days. */
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20260315", 8,
                                    "0.01,0.02", 9, &value) == 1,
          "the interest is defined inside the period");
    CHECK(value > 0.1999999 && value < 0.2000001,
          "100 * 0.01 * 73 / 365 = 0.2, got %.10f", value);
    /* The FIRST remaining rate is the one that applies, not a later one. */
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20260315", 8,
                                    "0.02,0.01", 9, &value) == 1 &&
              value > 0.3999999 && value < 0.4000001,
          "and it is the first of the pair, got %.10f", value);
    /* On the coupon date itself the interest is zero, which is a value and not an
     * absence. */
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20260101", 8,
                                    "0.01", 4, &value) == 1 &&
              value == 0.0,
          "no days elapsed means no interest, got %.10f", value);

    /* Outside the period the formula does not apply, so the answer is absent rather
     * than extrapolated. */
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20251231", 8,
                                    "0.01", 4, &value) == 0,
          "an as-of date before the last coupon is refused");
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20270102", 8,
                                    "0.01", 4, &value) == 0,
          "and one after the next coupon is refused");
    /* Missing or unusable inputs. */
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20260315", 8, "", 0,
                                    &value) == 0,
          "no remaining rates means no interest");
    CHECK(tdx_bond_accrued_interest(100.0, "notadate", 8, "20270101", 8, "20260315", 8,
                                    "0.01", 4, &value) == 0,
          "a malformed coupon date is refused");
    CHECK(tdx_bond_accrued_interest(100.0, "20260101", 8, "20270101", 8, "20260315", 8,
                                    "0.01", 4, NULL) == 0,
          "a null output is refused");
}

static void test_remaining_flows(void) {
    tdx_bond_flow flows[8];
    size_t count;

    /* Three dates, the first already past: only the later two remain, and the last
     * repays the face. */
    /* The rate list is 14 characters: three four-character rates and two commas.
     * Passing 13 truncates the third to "0.0", which silently changes the last
     * flow - the kind of slip a length argument invites. */
    count = tdx_bond_remaining_flows(100.0, "20250101,20270101,20280101", 26,
                                     "0.01,0.02,0.03", 14, "20260612", 8, flows, 8);
    CHECK(count == 2, "two of the three coupon dates remain, got %zu", count);
    if (count == 2) {
        CHECK(flows[0].amount > 1.9999 && flows[0].amount < 2.0001,
              "the first remaining coupon pays 100 * 0.02 = 2, got %f", flows[0].amount);
        /* The last flow is the coupon PLUS the face, which is where a port that forgets
         * the redemption gets a yield that is quietly too high. */
        CHECK(flows[1].amount > 102.999 && flows[1].amount < 103.001,
              "the last pays 100 * 0.03 + 100 = 103, got %f", flows[1].amount);
        /* 203 days / 365 = 0.556164. */
        CHECK(flows[0].years > 0.5561 && flows[0].years < 0.5562,
              "2027-01-01 is 203 days away, %.6f years", flows[0].years);
        CHECK(flows[1].years > flows[0].years, "and the flows are in ascending time");
    }
    /* A capacity smaller than the list stops rather than overrunning. */
    count = tdx_bond_remaining_flows(100.0, "20270101,20280101", 17, "0.01,0.02", 9,
                                     "20260612", 8, flows, 1);
    CHECK(count == 1, "a one-flow buffer takes one flow, got %zu", count);
    /* No dates after the as-of date means no flows, which is not an error. */
    count = tdx_bond_remaining_flows(100.0, "20250101", 8, "0.01", 4, "20260612", 8, flows, 8);
    CHECK(count == 0, "all coupons past leaves nothing, got %zu", count);
    /* Lists of different lengths pair up to the shorter one, as the reference does. */
    count = tdx_bond_remaining_flows(100.0, "20270101,20280101", 17, "0.01", 4, "20260612", 8,
                                     flows, 8);
    CHECK(count == 1, "one rate pairs with one date, got %zu", count);
    if (count == 1)
        CHECK(flows[0].amount > 100.999 && flows[0].amount < 101.001,
              "and that date is the last, so it redeems: got %f", flows[0].amount);
}

static void test_discounting_and_ytm(void) {
    tdx_bond_flow flows[4];
    double value = 0.0;
    double ytm = 0.0;

    /* A single flow has a closed-form yield, so the bisection can be checked against
     * arithmetic instead of against another iteration. */
    flows[0].years = 2.0;
    flows[0].amount = 108.0;
    CHECK(tdx_bond_solve_ytm(flows, 1, 100.0, &ytm) == 1, "a single flow solves");
    CHECK(ytm > 0.0392304 && ytm < 0.0392306,
          "(108/100)^(1/2) - 1 = 0.0392304845, got %.10f", ytm);
    /* FIXED POINT: discounting at the returned yield reproduces the price. */
    CHECK(tdx_bond_discounted_value(flows, 1, ytm, &value) == 1, "and it discounts");
    CHECK(value > 99.999999 && value < 100.000001,
          "discounting at that yield returns the price, got %.10f", value);

    /* The same check on a multi-flow schedule, where no closed form exists. */
    flows[0].years = 0.5;
    flows[0].amount = 1.0;
    flows[1].years = 1.5;
    flows[1].amount = 1.0;
    flows[2].years = 2.5;
    flows[2].amount = 101.0;
    CHECK(tdx_bond_solve_ytm(flows, 3, 98.5, &ytm) == 1, "a schedule solves");
    CHECK(tdx_bond_discounted_value(flows, 3, ytm, &value) == 1, "and discounts");
    CHECK(value > 98.499999 && value < 98.500001,
          "the fixed point holds on a schedule too, got %.10f for a price of 98.5", value);

    /* A price above what the flows can produce at the lowest rate tried is not
     * bracketed, so no yield is reported rather than a wrong one. */
    flows[0].years = 1.0;
    flows[0].amount = 100.0;
    /* At the interval's high end (10 per cent) the value is 100/11 = 9.09, so a
     * price below that is reachable by no rate in the interval.  A price ABOVE the
     * flows is reachable, via a negative rate: at -99.9999 per cent the value is
     * 1e8, which is why 1000 is bracketed and this case uses 1.0. */
    CHECK(tdx_bond_solve_ytm(flows, 1, 1.0, &ytm) == 0,
          "a price below the interval's reach is refused");
    /* 9.5 sits above that 9.09 and well below the 1e8 a negative rate reaches, so this
     * one IS bracketed and does solve. */
    CHECK(tdx_bond_solve_ytm(flows, 1, 9.5, &ytm) == 1,
          "while a price above it still solves, got %.6f", ytm);
    CHECK(tdx_bond_solve_ytm(flows, 1, 0.0, &ytm) == 0, "a zero price is refused");
    CHECK(tdx_bond_solve_ytm(flows, 1, -1.0, &ytm) == 0, "a negative price is refused");
    CHECK(tdx_bond_solve_ytm(NULL, 0, 100.0, &ytm) == 0, "no flows means no yield");

    /* The discounting guards. */
    CHECK(tdx_bond_discounted_value(flows, 1, -1.0, &value) == 0,
          "a rate of exactly -100 per cent is refused");
    CHECK(tdx_bond_discounted_value(flows, 1, -1.5, &value) == 0,
          "and anything below it");
    CHECK(tdx_bond_discounted_value(flows, 0, 0.05, &value) == 0, "no flows means no value");
    CHECK(tdx_bond_discounted_value(flows, 1, 0.05, NULL) == 0, "a null output is refused");
    /* At a zero rate the value is the undiscounted sum. */
    CHECK(tdx_bond_discounted_value(flows, 1, 0.0, &value) == 1 && value == 100.0,
          "a zero rate leaves the flow undiscounted, got %f", value);
}

int main(void) {
    test_calendar();
    test_csv();
    test_accrued_interest();
    test_remaining_flows();
    test_discounting_and_ytm();

    if (failures) {
        printf("%d bond-math check(s) failed\n", failures);
        return 1;
    }
    printf("bond-math checks passed\n");
    return 0;
}
