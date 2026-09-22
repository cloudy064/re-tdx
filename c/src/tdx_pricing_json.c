/* tdx_pricing_json.c - JSONL rendering for the pricing view. */
#include "tdx_pricing_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_bonds_json.h"
#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

const char *tdx_pricing_price_source_name(tdx_price_source source) {
    switch (source) {
    case TDX_PRICE_LAST:
        return "last-price";
    case TDX_PRICE_PRE_CLOSE:
        return "pre-close";
    default:
        return "unavailable";
    }
}

static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[512];

    if (!text->present || !text->data)
        return APPEND_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        tdx_error_set(err, "a pricing text field is %zu bytes, longer than this renderer holds",
                      text->length);
        return TDX_ERR;
    }
    memcpy(scratch, text->data, text->length);
    scratch[text->length] = '\0';
    return tdx_format_json_string(out, scratch, err);
}

static int append_number(tdx_buf *out, int has, double value, tdx_error *err) {
    if (!has)
        return APPEND_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "%.6f", value);
}

static int append_identity(tdx_buf *out, const char *market, int market_id,
                           const char *security_id, const tdx_bond_text *code,
                           const tdx_bond_text *name, tdx_error *err) {
    if (APPEND_LITERAL(out, err, "{\"market\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, market, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"security_id\":", market_id) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        append_text(out, code, err) != TDX_OK)
        return TDX_ERR;
    if (name) {
        if (APPEND_LITERAL(out, err, ",\"name\":") != TDX_OK ||
            append_text(out, name, err) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}

/* One side of the quote join: the price, WHERE it came from, the change and the
 * amount.  The source travels with the price because a valuation built on a previous
 * close is a different claim.
 *
 * No leading comma: the caller has just written the opening brace, so the first key
 * follows it directly.  Prepending one produced `{"bond":{,` - which the brace-balance
 * check in the test does not catch but a JSON parser does, so the live run is what
 * found it. */
static int append_quote_side(tdx_buf *out, const tdx_quote_view *view, const char *prefix,
                             tdx_error *err) {
    double price = 0.0;
    tdx_price_source source = TDX_PRICE_UNAVAILABLE;

    if (tdx_buf_append_printf(out, err, "\"%s_last_price\":", prefix) != TDX_OK)
        return TDX_ERR;
    if (!tdx_pricing_quote_price(view, &price, &source)) {
        if (APPEND_LITERAL(out, err, "null") != TDX_OK)
            return TDX_ERR;
    } else if (tdx_buf_append_printf(out, err, "%.6f", price) != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"%s_price_source\":\"%s\"", prefix,
                              tdx_pricing_price_source_name(source)) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"%s_change_pct\":", prefix) != TDX_OK)
        return TDX_ERR;
    if (append_number(out, view->available && view->has_change_pct, view->change_pct, err) !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"%s_amount_yuan\":", prefix) != TDX_OK)
        return TDX_ERR;
    if (append_number(out, view->available && view->has_amount, view->amount, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"%s_available\":%s", prefix,
                              view->available ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_pricing_format(tdx_buf *out, const tdx_pricing_row *row, const char *resource,
                       size_t row_index, tdx_error *err) {
    tdx_bond_text no_name;

    if (!out || !row) {
        tdx_error_set(err, "pricing rendering needs a buffer and a row");
        return TDX_ERR;
    }
    memset(&no_name, 0, sizeof(no_name));
    if (APPEND_LITERAL(out, err, "{\"type\":\"convertible_bond_pricing\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"row\":%zu,\"instrument_type\":\"%s\",\"active\":%s",
                              row_index,
                              row->exchangeable ? "exchangeable-bond" : "convertible-bond",
                              row->active ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"bond\":") != TDX_OK ||
        append_identity(out, row->bond_market, row->bond_market_id, row->bond_security_id,
                        &row->bond_code, &row->bond_name, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"underlying\":") != TDX_OK)
        return TDX_ERR;
    if (!row->has_underlying) {
        if (APPEND_LITERAL(out, err, "null") != TDX_OK)
            return TDX_ERR;
    } else if (append_identity(out, row->stock_market, row->stock_market_id,
                               row->stock_security_id, &row->stock_code, &row->stock_name,
                               err) != TDX_OK) {
        return TDX_ERR;
    }

    /* terms */
    if (APPEND_LITERAL(out, err, ",\"terms\":{\"face_value\":") != TDX_OK ||
        append_number(out, row->has_face_value, row->face_value, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_price\":") != TDX_OK ||
        append_number(out, row->has_conversion_price, row->conversion_price, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_years\":") != TDX_OK ||
        append_number(out, row->has_remaining_years, row->remaining_years, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"return_since_listing_pct\":") != TDX_OK ||
        append_number(out, row->has_return_since_listing_pct, row->return_since_listing_pct,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"dates\":{\"listing\":") != TDX_OK ||
        append_text(out, &row->listing_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"interest_start\":") != TDX_OK ||
        append_text(out, &row->interest_start_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_start\":") != TDX_OK ||
        append_text(out, &row->conversion_start_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_end\":") != TDX_OK ||
        append_text(out, &row->conversion_end_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"maturity\":") != TDX_OK ||
        append_text(out, &row->maturity_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"previous_payment\":") != TDX_OK ||
        append_text(out, &row->previous_payment_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"next_payment\":") != TDX_OK ||
        append_text(out, &row->next_payment_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"ratings\":{\"bond\":") != TDX_OK ||
        append_text(out, &row->bond_rating, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issuer\":") != TDX_OK ||
        append_text(out, &row->issuer_rating, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"coupons\":{\"bond_type\":") != TDX_OK ||
        append_text(out, &row->bond_type, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"rate_type\":") != TDX_OK ||
        append_text(out, &row->rate_type, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"rate_type_code\":") != TDX_OK ||
        append_text(out, &row->rate_type_code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"payment_dates\":") != TDX_OK ||
        tdx_bonds_format_comma_array(out, &row->payment_dates, 0, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"payment_rates\":") != TDX_OK ||
        tdx_bonds_format_comma_array(out, &row->payment_rates, 1, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_payment_dates\":") != TDX_OK ||
        tdx_bonds_format_comma_array(out, &row->remaining_payment_dates, 0, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_payment_rates\":") != TDX_OK ||
        tdx_bonds_format_comma_array(out, &row->remaining_payment_rates, 1, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_payment_count\":") != TDX_OK ||
        append_number(out, row->has_remaining_payment_count, row->remaining_payment_count,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"payment_frequency_months\":") != TDX_OK ||
        append_number(out, row->has_payment_frequency_months, row->payment_frequency_months,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    /* The trigger ratios AND the prices derived from them, side by side: a reader has
     * to be able to see that the price came from the ratio. */
    if (APPEND_LITERAL(out, err, ",\"triggers\":{\"sellback_ratio_pct\":") != TDX_OK ||
        append_number(out, row->has_sellback_trigger_ratio_pct, row->sellback_trigger_ratio_pct,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"sellback_price\":") != TDX_OK ||
        append_number(out, row->has_sellback_trigger_price, row->sellback_trigger_price, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"redemption_ratio_pct\":") != TDX_OK ||
        append_number(out, row->has_redemption_trigger_ratio_pct,
                      row->redemption_trigger_ratio_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"redemption_price\":") != TDX_OK ||
        append_number(out, row->has_redemption_trigger_price, row->redemption_trigger_price,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"revision_ratio_pct\":") != TDX_OK ||
        append_number(out, row->has_revision_trigger_ratio_pct, row->revision_trigger_ratio_pct,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"revision_price\":") != TDX_OK ||
        append_number(out, row->has_revision_trigger_price, row->revision_trigger_price, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_balance_pct\":") != TDX_OK ||
        append_number(out, row->has_remaining_balance_pct, row->remaining_balance_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"curve\":{\"short_years\":") != TDX_OK ||
        append_number(out, row->has_curve_short_years, row->curve_short_years, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"short_yield_pct\":") != TDX_OK ||
        append_number(out, row->has_curve_short_yield_pct, row->curve_short_yield_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"long_years\":") != TDX_OK ||
        append_number(out, row->has_curve_long_years, row->curve_long_years, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"long_yield_pct\":") != TDX_OK ||
        append_number(out, row->has_curve_long_yield_pct, row->curve_long_yield_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"discount_yield_pct\":") != TDX_OK ||
        append_number(out, row->has_curve_yield, row->curve_yield * 100.0, err) != TDX_OK)
        return TDX_ERR;
    /* Two braces: one closes the curve, one closes the terms object.  Closing only the
     * curve leaves every row a brace short, which is the sixth time this project has
     * made that mistake and why the JSON tests all check balance. */
    if (APPEND_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;

    /* quote */
    if (APPEND_LITERAL(out, err, ",\"quote\":{\"bond\":{") != TDX_OK)
        return TDX_ERR;
    if (append_quote_side(out, &row->bond_quote, "bond", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "},\"underlying\":{") != TDX_OK)
        return TDX_ERR;
    if (append_quote_side(out, &row->stock_quote, "underlying", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;

    /* valuation */
    if (APPEND_LITERAL(out, err, ",\"valuation\":{\"as_of_date\":") != TDX_OK ||
        append_text(out, &row->as_of_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"accrued_interest\":") != TDX_OK ||
        append_number(out, row->has_accrued_interest, row->accrued_interest, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"full_price\":") != TDX_OK ||
        append_number(out, row->has_full_price, row->full_price, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_value\":") != TDX_OK ||
        append_number(out, row->has_conversion_value, row->conversion_value, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_premium_pct\":") != TDX_OK ||
        append_number(out, row->has_conversion_premium_pct, row->conversion_premium_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"maturity_yield_pct\":") != TDX_OK ||
        append_number(out, row->has_maturity_yield_pct, row->maturity_yield_pct * 100.0, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"pure_bond_value\":") != TDX_OK ||
        append_number(out, row->has_pure_bond_value, row->pure_bond_value, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"pure_bond_premium_pct\":") != TDX_OK ||
        append_number(out, row->has_pure_bond_premium_pct, row->pure_bond_premium_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"double_low_score\":") != TDX_OK ||
        append_number(out, row->has_double_low_score, row->double_low_score, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"cash_flow_count\":%zu,\"availability\":\"%s\","
                              "\"price_plausible\":%s}",
                              row->cash_flow_count,
                              row->availability ? row->availability : "terms-only",
                              row->price_plausible ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_pricing_format_summary(tdx_buf *out, size_t rows, size_t skipped, size_t complete,
                               size_t bond_only, size_t terms_only, size_t with_yield,
                               size_t with_pure_value, size_t live_priced, size_t pre_close_priced,
                               const char *resource, const char *as_of, const char *endpoint,
                               tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the pricing summary needs a buffer");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"convertible_bond_pricing_summary\","
                                "\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"as_of_date\":") != TDX_OK)
        return TDX_ERR;
    if (as_of) {
        if (tdx_format_json_string(out, as_of, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, ",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"rows\":%zu,\"rows_skipped\":%zu,\"rows_complete\":%zu,"
                              "\"rows_bond_only\":%zu,\"rows_terms_only\":%zu,"
                              "\"rows_with_maturity_yield\":%zu,\"rows_with_pure_bond_value\":%zu,"
                              "\"rows_priced_live\":%zu,\"rows_priced_from_pre_close\":%zu}",
                              rows, skipped, complete, bond_only, terms_only, with_yield,
                              with_pure_value, live_priced, pre_close_priced) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
