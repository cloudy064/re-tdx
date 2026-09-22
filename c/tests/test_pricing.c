/* test_pricing.c - the convertible-bond pricing view.
 *
 * Two halves, tested differently.
 *
 * The ROW MAPPING runs on a real capture reduced to the first bond of each shape:
 * bi/list/gxjty_zq_kzzsy101_1.jsn's 43 columns over three rows - a plain bond, one with
 * a single remaining coupon, and an exchangeable bond.
 *
 * The QUOTE JOIN and the VALUATION run on quotes the test BUILDS, because the cases
 * that matter are the ones a capture cannot be relied on to contain: a live price, a
 * previous-close fallback, a positive-but-absent last price, a missing quote, and a
 * price so far above par that it cannot be a bond price.
 *
 * The arithmetic itself is tdx_bond_math's and is tested there.  What is asserted here
 * is the ASSEMBLY: that each input reaches the calculation it belongs to, that a
 * derived field is absent when its inputs are, and that `availability` reports how far
 * a row actually got.
 *
 * One measured fact is encoded as an assertion rather than fixed: the wire price for
 * the 132xxx exchangeable bonds is 100 times par, because the shared divisor table the
 * reference uses has no "13" rule other than "1318".  SH132026 came back as 13522.20
 * where its own local day file says 134.206.  The divisor table is a protocol policy
 * reproduced from the reference and is NOT changed here; instead such a price is
 * FLAGGED, so the numbers stay visible and the reader is told not to trust them. */
#include <stdio.h>
#include <string.h>

#include "tdx_pricing.h"
#include "tdx_bonds_json.h"
#include "tdx_pricing_json.h"
#include "tdx_jsn.h"
#include "pricing_fixtures.h"
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

static tdx_bond_text view(const tdx_jsn_document *doc, size_t row, const char *key) {
    return tdx_bonds_cell_text(doc, &doc->groups[0], row, key);
}

static tdx_snapshot make_quote(int market, const char *code, double last, double previous,
                              double amount) {
    tdx_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.security.market_id = market;
    snprintf(snapshot.security.code, sizeof(snapshot.security.code), "%s", code);
    snapshot.last = last;
    snapshot.previous = previous;
    snapshot.amount = amount;
    return snapshot;
}

/* The as-of date the live run used, so the captured rows' coupon windows apply. */
#define AS_OF "20260922"

static tdx_jsn_document document;
static tdx_pricing_row rows[8];
static size_t count = 0;
static size_t skipped = 0;

static int prepare(const tdx_snapshot *quotes, size_t quote_count) {
    error.message[0] = '\0';
    tdx_jsn_document_init(&document);
    if (tdx_jsn_parse((const uint8_t *)pricing_document, strlen(pricing_document), &document,
                      &error) != TDX_OK) {
        printf("FAIL parsing the fixture: %s\n", error.message);
        failures++;
        return TDX_ERR;
    }
    if (tdx_pricing_normalize(&document, &document.groups[0], quotes, quote_count, AS_OF,
                              strlen(AS_OF), rows, 8, &count, &skipped, &error) != TDX_OK) {
        printf("FAIL normalizing: %s\n", error.message);
        failures++;
        return TDX_ERR;
    }
    return TDX_OK;
}

static tdx_snapshot live[8];

static void test_terms_and_live_valuation(void) {
    /* Live quotes for the three bonds and their underlyings. */
    live[0] = make_quote(1, "110075", 106.359, 106.368, 47933440.0);
    live[1] = make_quote(1, "600029", 4.980, 4.950, 147789136.0);
    live[2] = make_quote(1, "110076", 112.500, 112.000, 1000000.0);
    live[3] = make_quote(1, "600521", 17.000, 16.900, 2000000.0);
    live[4] = make_quote(1, "132024", 133.690, 133.575, 8764269.0);
    live[5] = make_quote(1, "600362", 25.000, 24.900, 3000000.0);
    if (prepare(live, 6) != TDX_OK)
        return;
    CHECK(count == PRICING_FIXTURE_ROWS, "all %d rows map, got %zu", PRICING_FIXTURE_ROWS,
          count);
    CHECK(skipped == 0, "and none is skipped, got %zu", skipped);
    CHECK(PRICING_FIXTURE_COLUMNS == 43, "the capture had 43 columns, header says %d",
          PRICING_FIXTURE_COLUMNS);
    CHECK(PRICING_FIXTURE_SOURCE_ROWS == 312, "and 312 rows, header says %d",
          PRICING_FIXTURE_SOURCE_ROWS);

    /* Row 0 is China Southern's convertible bond, priced live. */
    CHECK(strcmp(rows[0].bond_security_id, "SH110075") == 0, "the bond is SH110075, got %s",
          rows[0].bond_security_id);
    CHECK(rows[0].has_underlying && strcmp(rows[0].stock_security_id, "SH600029") == 0,
          "its underlying is SH600029, got %s", rows[0].stock_security_id);
    CHECK(rows[0].exchangeable == 0, "a convertible bond is not exchangeable");
    CHECK(rows[0].active == 1, "and it has not matured");
    CHECK(rows[0].has_face_value && rows[0].face_value == 100.0, "face 100");
    CHECK(rows[0].has_conversion_price && rows[0].conversion_price > 6.169 &&
              rows[0].conversion_price < 6.171,
          "conversion price 6.17, got %f", rows[0].conversion_price);

    /* The trigger prices are DERIVED from the conversion price, so they must agree with
     * it and not merely be present. */
    CHECK(rows[0].has_sellback_trigger_price && rows[0].sellback_trigger_price > 4.3189 &&
              rows[0].sellback_trigger_price < 4.3191,
          "6.17 * 70 / 100 = 4.319, got %f", rows[0].sellback_trigger_price);
    CHECK(rows[0].has_redemption_trigger_price && rows[0].redemption_trigger_price > 8.0209 &&
              rows[0].redemption_trigger_price < 8.0211,
          "6.17 * 130 / 100 = 8.021, got %f", rows[0].redemption_trigger_price);

    /* The coupon schedule, read as arrays rather than one string. */
    {
        tdx_buf line;
        tdx_buf_init(&line);
        CHECK(rows[0].payment_dates.present && rows[0].payment_rates.present,
              "the full coupon schedule is present");
        CHECK(tdx_bonds_format_comma_array(&line, &rows[0].payment_rates, 1, &error) == TDX_OK,
              "and renders: %s", error.message);
        CHECK(strstr(text_of(&line), "0.065") != NULL,
              "the final rate 6.5 per cent is in the list: %s", text_of(&line));
        CHECK(tdx_bonds_format_comma_array(&line, &rows[0].remaining_payment_rates, 1,
                                           &error) == TDX_OK,
              "the remaining rates render");
        tdx_buf_free(&line);
    }

    /* The valuation, on quotes the test chose so the arithmetic can be checked by hand.
     * accrued interest = 100 * 0.065 * days/365, and with the captured coupon window
     * that is the figure the live run produced. */
    CHECK(rows[0].has_accrued_interest && rows[0].accrued_interest > 6.0904 &&
              rows[0].accrued_interest < 6.0905,
          "the accrued interest is 6.090411, got %.6f", rows[0].accrued_interest);
    CHECK(rows[0].has_full_price && rows[0].full_price > 112.4494 &&
              rows[0].full_price < 112.4495,
          "the full price is 106.359 + 6.090411 = 112.449411, got %.6f", rows[0].full_price);
    CHECK(rows[0].has_conversion_value && rows[0].conversion_value > 80.7131 &&
              rows[0].conversion_value < 80.7132,
          "the conversion value is 4.98 * 100 / 6.17 = 80.713128, got %.6f",
          rows[0].conversion_value);
    CHECK(rows[0].has_conversion_premium_pct && rows[0].conversion_premium_pct > 39.3198 &&
              rows[0].conversion_premium_pct < 39.3199,
          "the premium is 39.319853 per cent, got %.6f", rows[0].conversion_premium_pct);
    CHECK(rows[0].cash_flow_count == 1, "one coupon remains, got %zu", rows[0].cash_flow_count);
    CHECK(rows[0].has_maturity_yield_pct, "so a yield to maturity exists");
    /* A bond above its redemption value with one coupon left has a NEGATIVE yield, and
     * that is arithmetic rather than a fault.  Asserting the sign keeps someone from
     * "fixing" it later. */
    CHECK(rows[0].maturity_yield_pct < 0.0,
          "112.45 for 106.50 in 23 days is a negative yield, got %.6f",
          rows[0].maturity_yield_pct);
    CHECK(rows[0].has_pure_bond_value && rows[0].pure_bond_value > 106.4009 &&
              rows[0].pure_bond_value < 106.4010,
          "the pure bond value is 106.400995, got %.6f", rows[0].pure_bond_value);
    CHECK(rows[0].has_double_low_score && rows[0].double_low_score > 145.6788 &&
              rows[0].double_low_score < 145.6789,
          "the double-low score is 106.359 + 39.319853 = 145.678853, got %.6f",
          rows[0].double_low_score);
    CHECK(rows[0].availability && strcmp(rows[0].availability, "complete") == 0,
          "and the row is complete, got %s", rows[0].availability);
    CHECK(rows[0].price_plausible == 1, "with a plausible price");
    /* The quote side records WHERE the price came from. */
    {
        double price = 0.0;
        tdx_price_source source = TDX_PRICE_UNAVAILABLE;
        CHECK(tdx_pricing_quote_price(&rows[0].bond_quote, &price, &source) == 1 &&
                  source == TDX_PRICE_LAST,
              "the bond price came from the last price");
        CHECK(price > 106.358 && price < 106.360, "and is 106.359, got %f", price);
        CHECK(rows[0].bond_quote.has_change_pct, "the change is derived from the two prices");
    }

    /* Row 2 is the exchangeable bond, priced at 133.69 by the test.  Its full price is
     * the sum, and its five remaining coupons give a different yield from row 0's one. */
    CHECK(rows[2].exchangeable == 1, "row 2 is an exchangeable bond");
    CHECK(rows[2].cash_flow_count == 5, "with five coupons left, got %zu",
          rows[2].cash_flow_count);
    CHECK(rows[2].has_maturity_yield_pct, "and a yield");

    tdx_jsn_document_free(&document);
}

static void test_quote_fallbacks(void) {
    /* No last price but a previous close: the price falls back and SAYS SO. */
    tdx_snapshot quotes[8];
    tdx_pricing_row row;
    size_t one = 0;
    size_t one_skipped = 0;

    quotes[0] = make_quote(1, "110075", 0.0, 106.368, 1000.0);
    quotes[1] = make_quote(1, "600029", 0.0, 4.950, 1000.0);
    if (prepare(quotes, 2) != TDX_OK)
        return;
    row = rows[0];
    {
        double price = 0.0;
        tdx_price_source source = TDX_PRICE_UNAVAILABLE;
        CHECK(tdx_pricing_quote_price(&row.bond_quote, &price, &source) == 1,
              "a previous close is usable");
        CHECK(source == TDX_PRICE_PRE_CLOSE, "and is reported as the previous close");
        CHECK(price > 106.367 && price < 106.369, "with the previous close's value, got %f",
              price);
    }
    tdx_jsn_document_free(&document);

    /* A quote whose prices are both unusable is not usable at all, and the row drops
     * back to terms-only rather than inventing a price. */
    quotes[0] = make_quote(1, "110075", 0.0, 0.0, 0.0);
    quotes[1] = make_quote(1, "600029", 0.0, 0.0, 0.0);
    if (prepare(quotes, 2) != TDX_OK)
        return;
    CHECK(rows[0].bond_quote.available == 1, "the quote is present but has no usable price");
    CHECK(rows[0].has_full_price == 0, "so no full price is produced");
    CHECK(rows[0].availability && strcmp(rows[0].availability, "terms-only") == 0,
          "and the row is terms-only, got %s", rows[0].availability);
    {
        double price = 0.0;
        tdx_price_source source = TDX_PRICE_LAST;
        CHECK(tdx_pricing_quote_price(&rows[0].bond_quote, &price, &source) == 0,
              "a zero last price and a zero previous close give nothing");
        CHECK(source == TDX_PRICE_UNAVAILABLE, "and the source says unavailable");
    }
    /* A one-row buffer for three rows is refused rather than overrun. */
    CHECK(tdx_pricing_normalize(&document, &document.groups[0], quotes, 2, AS_OF,
                                strlen(AS_OF), &row, 1, &one, &one_skipped, &error) == TDX_ERR,
          "a one-row buffer for three rows must be refused");
    tdx_jsn_document_free(&document);

    /* NO quotes at all is a legitimate way to ask for the terms alone. */
    if (prepare(NULL, 0) != TDX_OK)
        return;
    CHECK(rows[0].bond_quote.available == 0, "the bond quote is absent");
    CHECK(rows[0].has_full_price == 0, "so there is no full price");
    CHECK(rows[0].has_conversion_value == 0, "and no conversion value");
    CHECK(rows[0].has_maturity_yield_pct == 0, "and no yield");
    CHECK(rows[0].has_accrued_interest == 1,
          "but the accrued interest needs no quote and is still there");
    CHECK(rows[0].availability && strcmp(rows[0].availability, "terms-only") == 0,
          "and the row says so, got %s", rows[0].availability);
    /* The terms are all still present: this row is how a caller asks what the bond IS
     * without asking what it is worth. */
    CHECK(rows[0].has_face_value && rows[0].has_conversion_price,
          "the terms survive without quotes");
    CHECK(rows[0].payment_dates.present, "including the coupon schedule");
    tdx_jsn_document_free(&document);

    /* A bond with no quote but its underlying quoted is bond-less, not complete. */
    quotes[0] = make_quote(1, "600029", 4.980, 4.950, 1000.0);
    if (prepare(quotes, 1) != TDX_OK)
        return;
    CHECK(rows[0].bond_quote.available == 0 && rows[0].stock_quote.available == 1,
          "the underlying is quoted and the bond is not");
    /* The conversion value needs the UNDERLYING's price, the face value and the
     * conversion price - not the bond's.  So it is present here, and the row is still
     * terms-only because a full price needs the bond. */
    CHECK(rows[0].has_conversion_value == 1,
          "a conversion value needs the underlying, not the bond, so it is present");
    CHECK(rows[0].has_full_price == 0, "while the full price is not");
    CHECK(rows[0].availability && strcmp(rows[0].availability, "terms-only") == 0,
          "and the row is terms-only, got %s", rows[0].availability);
    tdx_jsn_document_free(&document);
}

static void test_implausible_price_is_flagged(void) {
    /* The measured defect: 13522.20 for a face-100 bond.  The numbers stay, the flag
     * says not to trust them. */
    tdx_snapshot quotes[8];
    quotes[0] = make_quote(1, "132024", 13369.00, 13357.50, 8764269.0);
    quotes[1] = make_quote(1, "600362", 25.000, 24.900, 1000.0);
    quotes[2] = make_quote(1, "110075", 106.359, 106.368, 1000.0);
    quotes[3] = make_quote(1, "600029", 4.980, 4.950, 1000.0);
    quotes[4] = make_quote(1, "110076", 112.500, 112.000, 1000.0);
    quotes[5] = make_quote(1, "600521", 17.000, 16.900, 1000.0);
    if (prepare(quotes, 6) != TDX_OK)
        return;
    CHECK(rows[2].price_plausible == 0,
          "13369 for a face-100 bond is flagged implausible");
    CHECK(rows[0].price_plausible == 1, "while 106.359 is not");
    /* The numbers are still reported, because hiding them would hide the cause. */
    CHECK(rows[2].has_full_price, "the full price is still reported");
    CHECK(rows[2].bond_quote.has_last_price && rows[2].bond_quote.last_price > 13368.0,
          "and the price it was built from is still there, got %.2f",
          rows[2].bond_quote.last_price);
    /* A genuinely high price is NOT flagged: convertible bonds have traded above a
     * thousand yuan, and the threshold has to leave room for that. */
    {
        tdx_snapshot high[8];
        high[0] = make_quote(1, "110075", 738.000, 723.643, 1000.0);
        tdx_jsn_document other;
        tdx_pricing_row high_rows[8];
        size_t high_count = 0;
        size_t high_skipped = 0;
        tdx_jsn_document_init(&other);
        if (tdx_jsn_parse((const uint8_t *)pricing_document, strlen(pricing_document), &other,
                          &error) == TDX_OK &&
            tdx_pricing_normalize(&other, &other.groups[0], high, 1, AS_OF, strlen(AS_OF),
                                  high_rows, 8, &high_count, &high_skipped, &error) == TDX_OK &&
            high_count == PRICING_FIXTURE_ROWS) {
            CHECK(high_rows[0].bond_quote.has_last_price,
                  "the high quote reached row 0");
            CHECK(high_rows[0].price_plausible == 1,
                  "738 yuan for a face-100 bond is high but real, so it is not flagged");
        } else {
            CHECK(0, "the high-price case prepared: %s", error.message);
        }
        tdx_jsn_document_free(&other);
    }
    tdx_jsn_document_free(&document);
}

static void test_rendering(void) {
    tdx_buf line;
    const char *text;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;

    live[0] = make_quote(1, "110075", 106.359, 106.368, 47933440.0);
    live[1] = make_quote(1, "600029", 4.980, 4.950, 147789136.0);
    live[2] = make_quote(1, "110076", 112.500, 112.000, 1000.0);
    live[3] = make_quote(1, "600521", 17.000, 16.900, 1000.0);
    live[4] = make_quote(1, "132024", 13369.00, 13357.50, 1000.0);
    live[5] = make_quote(1, "600362", 25.000, 24.900, 1000.0);
    if (prepare(live, 6) != TDX_OK)
        return;
    tdx_buf_init(&line);
    CHECK(tdx_pricing_format(&line, &rows[0], TDX_PRICING_RESOURCE, 0, &error) == TDX_OK,
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
    CHECK(depth == 0 && !in_string, "the pricing row is balanced, depth %d in_string %d", depth,
          in_string);
    {
        /* Balanced is not the same as parseable: this assertion is what caught a Windows
         * path reaching the JSON through a raw %s, and a CRC printed without quotes. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"type\":\"convertible_bond_pricing\"") != NULL, "the type");
    CHECK(strstr(text, "\"instrument_type\":\"convertible-bond\"") != NULL, "the kind");
    CHECK(strstr(text, "\"active\":true") != NULL, "the active flag");
    CHECK(strstr(text, "\"sellback_price\":4.319000") != NULL, "a derived trigger price");
    CHECK(strstr(text, "\"bond\":{\"bond_last_price\":106.359000") != NULL,
          "the quote side starts cleanly, with no stray comma");
    CHECK(strstr(text, "\"bond_price_source\":\"last-price\"") != NULL, "the price source");
    CHECK(strstr(text, "\"conversion_value\":80.713128") != NULL, "the conversion value");
    CHECK(strstr(text, "\"availability\":\"complete\"") != NULL, "the availability word");
    CHECK(strstr(text, "\"price_plausible\":true") != NULL, "the plausibility flag");

    tdx_buf_clear(&line);
    CHECK(tdx_pricing_format_summary(&line, 312, 0, 312, 0, 0, 312, 0, 312, 0,
                                     TDX_PRICING_RESOURCE, AS_OF, "1.2.3.4:7709",
                                     &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(strstr(text_of(&line), "\"type\":\"convertible_bond_pricing_summary\"") != NULL,
          "the summary type");
    CHECK(strstr(text_of(&line), "\"rows_priced_live\":312") != NULL, "the pricing counts");
    tdx_buf_free(&line);
    tdx_jsn_document_free(&document);
}

int main(void) {
    test_terms_and_live_valuation();
    test_quote_fallbacks();
    test_implausible_price_is_flagged();
    test_rendering();

    if (failures) {
        printf("%d pricing check(s) failed\n", failures);
        return 1;
    }
    printf("pricing checks passed\n");
    return 0;
}
