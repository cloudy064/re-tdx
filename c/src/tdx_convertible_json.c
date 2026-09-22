/* tdx_convertible_json.c - JSONL rendering for a convertible-bond overview row. */
#include "tdx_convertible_json.h"

#include "tdx_bonds_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[1024];

    if (!text->present || !text->data)
        return APPEND_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        tdx_error_set(err, "a convertible-bond text field is %zu bytes, longer than this "
                           "renderer holds",
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

int tdx_convertible_format(tdx_buf *out, const tdx_convertible_row *row, const char *resource,
                           size_t group, size_t row_index, tdx_error *err) {
    if (!out || !row) {
        tdx_error_set(err, "convertible-bond rendering needs a buffer and a row");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"convertible_bond\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"group\":%zu,\"row\":%zu", group, row_index) != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"bond\":{\"market\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->bond_market, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"security_id\":", row->bond_market_id) !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->bond_security_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        append_text(out, &row->bond_code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"name\":") != TDX_OK ||
        append_text(out, &row->bond_name, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"instrument_type\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out,
                               row->kind == TDX_CONVERTIBLE_EXCHANGEABLE ? "exchangeable-bond"
                                                                         : "convertible-bond",
                               err) != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"underlying\":") != TDX_OK)
        return TDX_ERR;
    if (!row->has_underlying) {
        if (APPEND_LITERAL(out, err, "null") != TDX_OK)
            return TDX_ERR;
    } else {
        if (APPEND_LITERAL(out, err, "{\"market\":") != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, row->underlying_market, err) != TDX_OK)
            return TDX_ERR;
        if (tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"security_id\":",
                                  row->underlying_market_id) != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, row->underlying_security_id, err) != TDX_OK)
            return TDX_ERR;
        if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
            append_text(out, &row->underlying_code, err) != TDX_OK)
            return TDX_ERR;
        /* Named for what the data shows: the reference calls this the underlying's
         * name, and every live sample is a six-digit code or empty. */
        if (APPEND_LITERAL(out, err, ",\"reference_code\":") != TDX_OK ||
            append_text(out, &row->underlying_reference_code, err) != TDX_OK)
            return TDX_ERR;
        if (APPEND_LITERAL(out, err, "}") != TDX_OK)
            return TDX_ERR;
    }

    if (APPEND_LITERAL(out, err, ",\"overview\":{\"risk_notice\":") != TDX_OK ||
        append_text(out, &row->risk_notice, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"bond_rating\":") != TDX_OK ||
        append_text(out, &row->bond_rating, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issuer_rating\":") != TDX_OK ||
        append_text(out, &row->issuer_rating, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"current_state\":") != TDX_OK ||
        append_text(out, &row->current_state, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"dates\":{\"listing\":") != TDX_OK ||
        append_text(out, &row->listing_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue\":") != TDX_OK ||
        append_text(out, &row->issue_date, err) != TDX_OK)
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
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"numbers\":{\"face_value\":") != TDX_OK ||
        append_number(out, row->has_face_value, row->face_value, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_price\":") != TDX_OK ||
        append_number(out, row->has_issue_price, row->issue_price, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"return_since_listing_pct\":") != TDX_OK ||
        append_number(out, row->has_return_since_listing_pct, row->return_since_listing_pct,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"return_5d_pct\":") != TDX_OK ||
        append_number(out, row->has_return_5d_pct, row->return_5d_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"return_10d_pct\":") != TDX_OK ||
        append_number(out, row->has_return_10d_pct, row->return_10d_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_size_100m_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_size_100m_yuan, row->issue_size_100m_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_balance_100m_yuan\":") != TDX_OK ||
        append_number(out, row->has_remaining_balance_100m_yuan,
                      row->remaining_balance_100m_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_ratio_pct\":") != TDX_OK ||
        append_number(out, row->has_remaining_ratio_pct, row->remaining_ratio_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_price\":") != TDX_OK ||
        append_number(out, row->has_conversion_price, row->conversion_price, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_years\":") != TDX_OK ||
        append_number(out, row->has_remaining_years, row->remaining_years, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"maturity_redemption_price\":") != TDX_OK ||
        append_number(out, row->has_maturity_redemption_price, row->maturity_redemption_price,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"unpaid_coupon_sum\":") != TDX_OK ||
        append_number(out, row->has_unpaid_coupon_sum, row->unpaid_coupon_sum, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"sellback_trigger_ratio_pct\":") != TDX_OK ||
        append_number(out, row->has_sellback_trigger_ratio_pct, row->sellback_trigger_ratio_pct,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"redemption_trigger_ratio_pct\":") != TDX_OK ||
        append_number(out, row->has_redemption_trigger_ratio_pct,
                      row->redemption_trigger_ratio_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    /* Close the overview object.  Forgetting this leaves the outer object open,
     * which is a mistake this project has now made four times - hence the
     * brace-balance check every JSON test in it performs. */
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    if (tdx_buf_append_printf(out, err, ",\"core_terms_complete\":%s",
                              row->core_terms_complete ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_convertible_format_summary(tdx_buf *out, size_t rows, size_t rows_complete,
                                   size_t rows_exchangeable, size_t rows_with_underlying,
                                   const char *resource, const char *endpoint, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the convertible-bond summary needs a buffer");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"convertible_bond_summary\",\"resource\":") !=
        TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"rows\":%zu,\"rows_core_terms_complete\":%zu,"
                              "\"rows_exchangeable\":%zu,\"rows_with_underlying\":%zu,"
                              "\"documents_joined\":1,\"join_note\":"
                              "\"overview only; the reference joins five more documents\"}",
                              rows, rows_complete, rows_exchangeable,
                              rows_with_underlying) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

/* --- the joined row --------------------------------------------------- */

/* The trigger documents share their shape, so one emitter serves all three. */
static int append_trigger(tdx_buf *out, const tdx_convertible_trigger *trigger, tdx_error *err) {
    if (APPEND_LITERAL(out, err, "{\"condition\":") != TDX_OK ||
        append_text(out, &trigger->condition, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"price_ratio_pct\":") != TDX_OK ||
        append_number(out, trigger->has_price_ratio_pct, trigger->price_ratio_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"start_date\":") != TDX_OK ||
        append_text(out, &trigger->start_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"trigger_price\":") != TDX_OK ||
        append_number(out, trigger->has_trigger_price, trigger->trigger_price, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_price\":") != TDX_OK ||
        append_number(out, trigger->has_conversion_price, trigger->conversion_price, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"current_days\":") != TDX_OK ||
        append_text(out, &trigger->current_days, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"current_ratio_pct\":") != TDX_OK ||
        append_number(out, trigger->has_current_ratio_pct, trigger->current_ratio_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"status\":") != TDX_OK ||
        append_text(out, &trigger->status, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"history_count\":") != TDX_OK ||
        append_number(out, trigger->has_history_count, trigger->history_count, err) != TDX_OK)
        return TDX_ERR;
    /* The history dates are a comma list in the resource; the reference turns them
     * into an array, so the count above and this array are two views of one column. */
    if (APPEND_LITERAL(out, err, ",\"history_dates\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_bonds_format_comma_array(out, &trigger->history_dates, 0, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"available_days\":") != TDX_OK ||
        append_number(out, trigger->has_available_days, trigger->available_days, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

static int append_source_flags(tdx_buf *out, const tdx_convertible_join_flags *flags,
                               tdx_error *err) {
    if (APPEND_LITERAL(out, err, "{\"overview\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, flags->from_overview ? "true" : "false",
                       flags->from_overview ? 4 : 5, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"progress\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, flags->from_progress ? "true" : "false",
                       flags->from_progress ? 4 : 5, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"coupons\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, flags->from_coupons ? "true" : "false",
                       flags->from_coupons ? 4 : 5, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"sellback\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, flags->from_sellback ? "true" : "false",
                       flags->from_sellback ? 4 : 5, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"redemption\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, flags->from_redemption ? "true" : "false",
                       flags->from_redemption ? 4 : 5, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"revision\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, flags->from_revision ? "true" : "false",
                       flags->from_revision ? 4 : 5, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_convertible_format_joined(tdx_buf *out, const tdx_convertible_row *row,
                                  const tdx_convertible_extra *extra,
                                  const tdx_convertible_join_flags *flags,
                                  const char *overview_resource, tdx_error *err) {
    tdx_buf overview;
    size_t inner;
    size_t index;
    int result;

    if (!out || !row || !extra || !flags) {
        tdx_error_set(err, "the joined convertible-bond renderer needs a row, the extras and the "
                           "flags");
        return TDX_ERR;
    }
    /* The overview group is the same object the overview-only renderer produces, so
     * it is rendered by that function and then merged: one implementation of those
     * fields, not two that could drift. */
    tdx_buf_init(&overview);
    if (tdx_convertible_format(&overview, row, overview_resource, 0, 0, err) != TDX_OK) {
        tdx_buf_free(&overview);
        return TDX_ERR;
    }
    /* Splice `,"progress":...` in before the overview object's closing brace. */
    inner = overview.len;
    while (inner > 0 && overview.data[inner - 1] != '}')
        inner--;
    if (inner == 0) {
        tdx_buf_free(&overview);
        tdx_error_set(err, "the overview renderer produced no closing brace");
        return TDX_ERR;
    }
    if (tdx_buf_append(out, overview.data, inner - 1, err) != TDX_OK)
        goto failed;

    if (APPEND_LITERAL(out, err, ",\"progress\":{\"issue_size_100m_yuan\":") != TDX_OK ||
        append_number(out, extra->has_issue_size_100m_yuan, extra->issue_size_100m_yuan, err) !=
            TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"remaining_balance_100m_yuan\":") != TDX_OK ||
        append_number(out, extra->has_remaining_balance_100m_yuan,
                      extra->remaining_balance_100m_yuan, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"conversion_progress_pct\":") != TDX_OK ||
        append_number(out, extra->has_conversion_progress_pct, extra->conversion_progress_pct,
                      err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"redeemed_amount_100m_yuan\":") != TDX_OK ||
        append_number(out, extra->has_redeemed_amount_100m_yuan,
                      extra->redeemed_amount_100m_yuan, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"sellback_amount_100m_yuan\":") != TDX_OK ||
        append_number(out, extra->has_sellback_amount_100m_yuan,
                      extra->sellback_amount_100m_yuan, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"maturity_progress_pct\":") != TDX_OK ||
        append_number(out, extra->has_maturity_progress_pct, extra->maturity_progress_pct,
                      err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        goto failed;

    if (APPEND_LITERAL(out, err, ",\"coupons\":{\"term_years\":") != TDX_OK ||
        append_number(out, extra->has_term_years, extra->term_years, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"rates_pct\":[") != TDX_OK)
        goto failed;
    for (index = 0; index < 6; ++index) {
        if (index > 0 && tdx_buf_push(out, ',', err) != TDX_OK)
            goto failed;
        if (append_number(out, extra->has_rate[index], extra->rates_pct[index], err) != TDX_OK)
            goto failed;
    }
    if (APPEND_LITERAL(out, err, "],\"compensation_rate_pct\":") != TDX_OK ||
        append_number(out, extra->has_compensation_rate_pct, extra->compensation_rate_pct,
                      err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"payment_dates\":") != TDX_OK ||
        tdx_bonds_format_comma_array(out, &extra->payment_dates, 0, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"payment_rates\":") != TDX_OK ||
        tdx_bonds_format_comma_array(out, &extra->payment_rates, 1, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        goto failed;

    if (APPEND_LITERAL(out, err, ",\"sellback\":") != TDX_OK ||
        append_trigger(out, &extra->sellback, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"redemption\":") != TDX_OK ||
        append_trigger(out, &extra->redemption, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"revision\":") != TDX_OK ||
        append_trigger(out, &extra->revision, err) != TDX_OK)
        goto failed;
    if (APPEND_LITERAL(out, err, ",\"sources\":") != TDX_OK ||
        append_source_flags(out, flags, err) != TDX_OK)
        goto failed;

    /* Close the object the overview renderer opened, which is still open here. */
    result = tdx_buf_push(out, '}', err);
    tdx_buf_free(&overview);
    return result;

failed:
    tdx_buf_free(&overview);
    return TDX_ERR;
}
