/* tdx_pricing.c - the convertible-bond pricing view. */
#include "tdx_pricing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int tdx_pricing_quote_price(const tdx_quote_view *view, double *out,
                            tdx_price_source *source) {
    if (source)
        *source = TDX_PRICE_UNAVAILABLE;
    if (!view || !out || !view->available)
        return 0;
    /* A positive last price first; a previous close is the fallback, and which one was
     * used is reported because a valuation on a previous close is a different claim. */
    if (view->has_last_price && view->last_price > 0.0 && isfinite(view->last_price)) {
        *out = view->last_price;
        if (source)
            *source = TDX_PRICE_LAST;
        return 1;
    }
    if (view->has_pre_close_price && view->pre_close_price > 0.0 &&
        isfinite(view->pre_close_price)) {
        *out = view->pre_close_price;
        if (source)
            *source = TDX_PRICE_PRE_CLOSE;
        return 1;
    }
    return 0;
}

void tdx_pricing_quote_from_snapshot(const tdx_snapshot *snapshot, tdx_quote_view *out) {
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!snapshot)
        return;
    out->available = 1;
    if (isfinite(snapshot->last) && snapshot->last > 0.0) {
        out->has_last_price = 1;
        out->last_price = snapshot->last;
    }
    if (isfinite(snapshot->previous) && snapshot->previous > 0.0) {
        out->has_pre_close_price = 1;
        out->pre_close_price = snapshot->previous;
    }
    if (isfinite(snapshot->amount))
        out->has_amount = 1, out->amount = snapshot->amount;
    /* The snapshot carries no change of its own, so it is derived; without a positive
     * previous close there is no change to derive. */
    if (out->has_last_price && out->has_pre_close_price) {
        out->has_change_pct = 1;
        out->change_pct = (out->last_price - out->pre_close_price) * 100.0 /
                          out->pre_close_price;
    }
}

static const tdx_snapshot *find_quote(const tdx_snapshot *quotes, size_t count, int market,
                                       const char *code, size_t code_length) {
    size_t index;
    if (!quotes || !code)
        return NULL;
    for (index = 0; index < count; ++index) {
        if (quotes[index].security.market_id != market)
            continue;
        if (strlen(quotes[index].security.code) != code_length)
            continue;
        if (memcmp(quotes[index].security.code, code, code_length) == 0)
            return &quotes[index];
    }
    return NULL;
}

static int valid_code(const char *code, size_t length) {
    size_t index;
    if (!code || length != 6)
        return 0;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

static tdx_bond_text cell(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                          const char *key) {
    return tdx_bonds_cell_text(doc, group, row, key);
}

static int cell_number(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row,
                       const char *key, double *out) {
    return tdx_bonds_cell_number(doc, group, row, key, out);
}

/* The type text says "exchangeable" by containing the character, which is how the
 * reference decides it - not by the code prefix, which the listed view uses. */
static int is_exchangeable(const tdx_bond_text *type) {
    size_t index;
    /* The UTF-8 encoding of the character for "exchange". */
    static const char needle[] = "\xe4\xba\xa4\xe6\x8d\xa2";
    if (!type->present || !type->data || type->length < 6)
        return 0;
    for (index = 0; index + 6 <= type->length; ++index)
        if (memcmp(type->data + index, needle, 6) == 0)
            return 1;
    return 0;
}

/* A helper that reads a trigger price: conversion price times the ratio over a
 * hundred, and only when both are present. */
static void read_trigger_price(int has_conversion_price, double conversion_price,
                               int has_ratio, double ratio, int *has_out, double *out) {
    *has_out = 0;
    *out = 0.0;
    if (!has_conversion_price || !has_ratio)
        return;
    *out = conversion_price * ratio / 100.0;
    *has_out = isfinite(*out) ? 1 : 0;
}

int tdx_pricing_normalize(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                          const tdx_snapshot *quotes, size_t quote_count, const char *as_of_date,
                          size_t as_of_length, tdx_pricing_row *out, size_t capacity,
                          size_t *out_count, size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;
    long long as_of_days = 0;
    int has_as_of;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "normalizing the pricing view needs a document, a group and an output");
        return TDX_ERR;
    }
    has_as_of = tdx_bond_civil_days(as_of_date, as_of_length, &as_of_days);
    if (!has_as_of) {
        tdx_error_set(err, "the as-of date is not a date");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_pricing_row *item;
        tdx_bond_text code = cell(doc, group, row, "$ZQDM");
        tdx_bond_text market_text = cell(doc, group, row, "$SC");
        tdx_bond_text stock_code = cell(doc, group, row, "$ZQDM1");
        tdx_bond_text stock_market_text = cell(doc, group, row, "$SC1");
        int market = -1;
        int stock_market = -1;
        char prefix[16];
        const char *market_name;
        const tdx_snapshot *bond_quote;
        const tdx_snapshot *stock_quote;

        if (stored >= capacity) {
            tdx_error_set(err, "the pricing view holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        /* A row without a usable bond identity cannot be priced, so it is skipped. */
        if (!valid_code(code.data, code.length)) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_id(market_text.data, market_text.length, &market, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(market, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        item->bond_market_id = market;
        market_name = tdx_bonds_market_name(market);
        snprintf(item->bond_market, sizeof(item->bond_market), "%s",
                 market_name ? market_name : prefix);
        snprintf(item->bond_security_id, sizeof(item->bond_security_id), "%s%.*s", prefix,
                 (int)code.length, code.data);
        item->bond_code = code;
        item->bond_name = cell(doc, group, row, "ZQJC");

        /* The underlying is optional here, unlike in the listed view: a pricing row
         * without one still has terms worth reporting. */
        if (valid_code(stock_code.data, stock_code.length) &&
            tdx_bonds_market_id(stock_market_text.data, stock_market_text.length, &stock_market,
                                NULL) == TDX_OK &&
            tdx_bonds_market_prefix(stock_market, prefix, sizeof(prefix), err) == TDX_OK) {
            market_name = tdx_bonds_market_name(stock_market);
            item->has_underlying = 1;
            item->stock_market_id = stock_market;
            snprintf(item->stock_market, sizeof(item->stock_market), "%s",
                     market_name ? market_name : prefix);
            snprintf(item->stock_security_id, sizeof(item->stock_security_id), "%s%.*s", prefix,
                     (int)stock_code.length, stock_code.data);
            item->stock_code = stock_code;
            item->stock_name = cell(doc, group, row, "ZGSM");
        }
        item->bond_type = cell(doc, group, row, "ZQLX");
        item->exchangeable = is_exchangeable(&item->bond_type);
        item->maturity_date = cell(doc, group, row, "DQRQ");
        /* Active means a maturity date that is present and not already past, which is a
         * DATE comparison and not a string one. */
        if (item->maturity_date.present) {
            long long maturity_days = 0;
            if (tdx_bond_civil_days(item->maturity_date.data, item->maturity_date.length,
                                    &maturity_days) &&
                maturity_days >= as_of_days)
                item->active = 1;
        }

        /* terms */
        item->has_face_value = cell_number(doc, group, row, "MZ", &item->face_value);
        item->has_return_since_listing_pct =
            cell_number(doc, group, row, "ZF1", &item->return_since_listing_pct);
        item->has_return_5d_pct = cell_number(doc, group, row, "ZF2", &item->return_5d_pct);
        item->has_return_10d_pct = cell_number(doc, group, row, "ZF3", &item->return_10d_pct);
        item->has_conversion_price =
            cell_number(doc, group, row, "ZGJ", &item->conversion_price);
        item->conversion_start_date = cell(doc, group, row, "ZGQSR");
        item->conversion_end_date = cell(doc, group, row, "ZGJZR");
        item->listing_date = cell(doc, group, row, "SSRQ");
        item->interest_start_date = cell(doc, group, row, "QXRQ");
        item->has_remaining_years = cell_number(doc, group, row, "SYNX", &item->remaining_years);
        item->bond_rating = cell(doc, group, row, "ZQPJ");
        item->issuer_rating = cell(doc, group, row, "ZTPJ");
        item->rate_type = cell(doc, group, row, "LLLX");
        item->rate_type_code = cell(doc, group, row, "LLLXBZ");
        item->payment_dates = cell(doc, group, row, "FXRQXL");
        item->payment_rates = cell(doc, group, row, "FXLLXL");
        item->has_remaining_payment_count =
            cell_number(doc, group, row, "SYFXCS", &item->remaining_payment_count);
        item->remaining_payment_dates = cell(doc, group, row, "SYFXRQXL");
        item->remaining_payment_rates = cell(doc, group, row, "SYFXLLXL");
        item->previous_payment_date = cell(doc, group, row, "SGFXRQ");
        item->next_payment_date = cell(doc, group, row, "XGFXRQ");
        item->has_payment_frequency_months =
            cell_number(doc, group, row, "FXPL1", &item->payment_frequency_months);
        item->has_remaining_balance_pct =
            cell_number(doc, group, row, "YEZB", &item->remaining_balance_pct);
        item->has_revision_trigger_ratio_pct =
            cell_number(doc, group, row, "XXCFBL", &item->revision_trigger_ratio_pct);
        item->has_sellback_trigger_ratio_pct =
            cell_number(doc, group, row, "HSCFBL", &item->sellback_trigger_ratio_pct);
        item->has_redemption_trigger_ratio_pct =
            cell_number(doc, group, row, "QSCFBL", &item->redemption_trigger_ratio_pct);
        read_trigger_price(item->has_conversion_price, item->conversion_price,
                           item->has_revision_trigger_ratio_pct, item->revision_trigger_ratio_pct,
                           &item->has_revision_trigger_price, &item->revision_trigger_price);
        read_trigger_price(item->has_conversion_price, item->conversion_price,
                           item->has_sellback_trigger_ratio_pct, item->sellback_trigger_ratio_pct,
                           &item->has_sellback_trigger_price, &item->sellback_trigger_price);
        read_trigger_price(item->has_conversion_price, item->conversion_price,
                           item->has_redemption_trigger_ratio_pct,
                           item->redemption_trigger_ratio_pct,
                           &item->has_redemption_trigger_price,
                           &item->redemption_trigger_price);
        item->has_curve_short_years =
            cell_number(doc, group, row, "ZSQX1", &item->curve_short_years);
        item->has_curve_short_yield_pct =
            cell_number(doc, group, row, "ZSSYL1", &item->curve_short_yield_pct);
        item->has_curve_long_years =
            cell_number(doc, group, row, "ZSQX2", &item->curve_long_years);
        item->has_curve_long_yield_pct =
            cell_number(doc, group, row, "ZSSYL2", &item->curve_long_yield_pct);
        item->has_curve_yield = cell_number(doc, group, row, "SYNXSYL", &item->curve_yield);

        /* quote: the join itself. */
        bond_quote = find_quote(quotes, quote_count, item->bond_market_id, code.data,
                                code.length);
        stock_quote = item->has_underlying
                          ? find_quote(quotes, quote_count, item->stock_market_id,
                                       item->stock_code.data, item->stock_code.length)
                          : NULL;
        memset(&item->bond_quote, 0, sizeof(item->bond_quote));
        memset(&item->stock_quote, 0, sizeof(item->stock_quote));
        tdx_pricing_quote_from_snapshot(bond_quote, &item->bond_quote);
        tdx_pricing_quote_from_snapshot(stock_quote, &item->stock_quote);

        /* Is the price a bond price at all?  A convertible bond can trade far above
         * par - one reached 3000 yuan - so the threshold is generous: ten times the
         * face value.  Only a unit error reaches that, and the exchangeable bonds do. */
        item->price_plausible = 1;
        {
            double quoted = 0.0;
            if (item->has_face_value && item->face_value > 0.0 &&
                tdx_pricing_quote_price(&item->bond_quote, &quoted, NULL) &&
                quoted > item->face_value * 10.0)
                item->price_plausible = 0;
        }

        /* valuation, through tdx_bond_math. */
        item->as_of_date.data = as_of_date;
        item->as_of_date.length = as_of_length;
        item->as_of_date.present = 1;
        {
            double bond_price = 0.0;
            double stock_price = 0.0;
            int has_bond_price = tdx_pricing_quote_price(&item->bond_quote, &bond_price, NULL);
            int has_stock_price = tdx_pricing_quote_price(&item->stock_quote, &stock_price, NULL);
            tdx_bond_flow flows[TDX_BOND_FLOWS_MAX];
            size_t flow_count;

            if (item->has_face_value && item->previous_payment_date.present &&
                item->next_payment_date.present && item->remaining_payment_rates.present)
                item->has_accrued_interest = tdx_bond_accrued_interest(
                    item->face_value, item->previous_payment_date.data,
                    item->previous_payment_date.length, item->next_payment_date.data,
                    item->next_payment_date.length, as_of_date, as_of_length,
                    item->remaining_payment_rates.data, item->remaining_payment_rates.length,
                    &item->accrued_interest);
            if (has_bond_price && item->has_accrued_interest) {
                item->full_price = bond_price + item->accrued_interest;
                item->has_full_price = isfinite(item->full_price) ? 1 : 0;
            }
            if (has_stock_price && item->has_face_value && item->has_conversion_price &&
                item->conversion_price > 0.0) {
                item->conversion_value = stock_price * item->face_value / item->conversion_price;
                item->has_conversion_value = isfinite(item->conversion_value) ? 1 : 0;
            }
            if (item->has_full_price && item->has_conversion_value &&
                item->conversion_value > 0.0) {
                item->conversion_premium_pct =
                    (item->full_price / item->conversion_value - 1.0) * 100.0;
                item->has_conversion_premium_pct = isfinite(item->conversion_premium_pct) ? 1 : 0;
            }
            flow_count = 0;
            if (item->has_face_value && item->remaining_payment_dates.present &&
                item->remaining_payment_rates.present)
                flow_count = tdx_bond_remaining_flows(
                    item->face_value, item->remaining_payment_dates.data,
                    item->remaining_payment_dates.length, item->remaining_payment_rates.data,
                    item->remaining_payment_rates.length, as_of_date, as_of_length, flows,
                    TDX_BOND_FLOWS_MAX);
            item->cash_flow_count = flow_count;
            if (item->has_full_price && flow_count > 0)
                item->has_maturity_yield_pct = tdx_bond_solve_ytm(
                    flows, flow_count, item->full_price, &item->maturity_yield_pct);
            if (item->has_curve_yield && flow_count > 0)
                item->has_pure_bond_value = tdx_bond_discounted_value(
                    flows, flow_count, item->curve_yield, &item->pure_bond_value);
            if (item->has_full_price && item->has_pure_bond_value &&
                item->pure_bond_value > 0.0) {
                item->pure_bond_premium_pct =
                    (item->full_price / item->pure_bond_value - 1.0) * 100.0;
                item->has_pure_bond_premium_pct = isfinite(item->pure_bond_premium_pct) ? 1 : 0;
            }
            if (has_bond_price && item->has_conversion_premium_pct) {
                item->double_low_score = bond_price + item->conversion_premium_pct;
                item->has_double_low_score = isfinite(item->double_low_score) ? 1 : 0;
            }
            /* How far the row got, which is what a caller needs before trusting a
             * number that is present. */
            item->availability = item->has_full_price && item->has_conversion_value
                                     ? "complete"
                                     : has_bond_price ? "bond-only" : "terms-only";
        }
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}

/* --- the quote code list ---------------------------------------------- */

static int contains_code(const tdx_code *codes, size_t count, int market, const char *code,
                         size_t code_length) {
    size_t index;
    for (index = 0; index < count; ++index)
        if (codes[index].market_id == market && strlen(codes[index].code) == code_length &&
            memcmp(codes[index].code, code, code_length) == 0)
            return 1;
    return 0;
}

static int add_code(tdx_code *out, size_t capacity, size_t *count, int market, const char *code,
                    size_t code_length) {
    if (contains_code(out, *count, market, code, code_length))
        return 1;
    if (*count >= capacity)
        return 0;
    out[*count].market_id = market;
    memcpy(out[*count].code, code, code_length);
    out[*count].code[code_length] = '\0';
    (*count)++;
    return 1;
}

int tdx_pricing_collect_codes(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                              tdx_code *out, size_t capacity, size_t *out_count,
                              size_t *skipped_count, tdx_error *err) {
    size_t count = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!doc || !group || !out) {
        tdx_error_set(err, "collecting quote codes needs a document, a group and an output");
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_bond_text code = cell(doc, group, row, "$ZQDM");
        tdx_bond_text market_text = cell(doc, group, row, "$SC");
        tdx_bond_text stock_code = cell(doc, group, row, "$ZQDM1");
        tdx_bond_text stock_market_text = cell(doc, group, row, "$SC1");
        int market = -1;
        int stock_market = -1;

        if (!valid_code(code.data, code.length) ||
            tdx_bonds_market_id(market_text.data, market_text.length, &market, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (!add_code(out, capacity, &count, market, code.data, code.length)) {
            tdx_error_set(err, "more than %zu securities need quotes", capacity);
            return TDX_ERR;
        }
        /* The underlying is worth a quote even when the row is otherwise incomplete, so
         * it is added whenever it is readable. */
        if (valid_code(stock_code.data, stock_code.length) &&
            tdx_bonds_market_id(stock_market_text.data, stock_market_text.length, &stock_market,
                                NULL) == TDX_OK &&
            !add_code(out, capacity, &count, stock_market, stock_code.data, stock_code.length)) {
            tdx_error_set(err, "more than %zu securities need quotes", capacity);
            return TDX_ERR;
        }
    }
    if (out_count)
        *out_count = count;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}
