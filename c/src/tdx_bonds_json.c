/* tdx_bonds_json.c - JSONL rendering for a normalized bond reference row. */
#include "tdx_bonds_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

const char *tdx_bonds_scale_name(tdx_bond_scale scale) {
    switch (scale) {
    case TDX_BOND_SCALE_ISSUE_YUAN:
        return "issue-size-yuan";
    case TDX_BOND_SCALE_ISSUE_100M_YUAN:
        return "issue-size-100m-yuan";
    case TDX_BOND_SCALE_OUTSTANDING_100M_YUAN:
        return "outstanding-balance-100m-yuan";
    case TDX_BOND_SCALE_CLIENT_MASTER_HIDDEN:
        return "client-master-hidden-unit";
    default:
        return "unknown";
    }
}

/* A text field: the document's own bytes, escaped, or null when absent. */
static int append_text(tdx_buf *out, const tdx_bond_text *text, tdx_error *err) {
    char scratch[1024];

    if (!text->present || !text->data)
        return APPEND_LITERAL(out, err, "null");
    if (text->length >= sizeof(scratch)) {
        /* Escaping is the only reason to copy at all, and a field this long is not
         * one of the resource's scalars. */
        tdx_error_set(err, "a bond text field is %zu bytes, longer than this renderer holds",
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

int tdx_bonds_format(tdx_buf *out, const tdx_bond_row *row, const char *resource, size_t group,
                     size_t row_index, tdx_error *err) {
    if (!out || !row) {
        tdx_error_set(err, "bond rendering needs a buffer and a row");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"bond\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"group\":%zu,\"row\":%zu", group, row_index) !=
        TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"market\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->market, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"market_id\":%d", row->market_id) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, row->security_id, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        append_text(out, &row->code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"name\":") != TDX_OK ||
        append_text(out, &row->name, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"name_resolved\":%s",
                              row->name_resolved ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"client_instrument_id\":") != TDX_OK ||
        append_text(out, &row->client_instrument_id, err) != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"classification\":{\"bond_type\":") != TDX_OK ||
        append_text(out, &row->bond_type, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"bond_credit_rating\":") != TDX_OK ||
        append_text(out, &row->bond_credit_rating, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issuer_credit_rating\":") != TDX_OK ||
        append_text(out, &row->issuer_credit_rating, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"rate_type\":") != TDX_OK ||
        append_text(out, &row->rate_type, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"rate_type_flag\":") != TDX_OK ||
        append_text(out, &row->rate_type_flag, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"guarantee_status\":") != TDX_OK ||
        append_text(out, &row->guarantee_status, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"dates\":{\"accrual_start\":") != TDX_OK ||
        append_text(out, &row->accrual_start_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"maturity\":") != TDX_OK ||
        append_text(out, &row->maturity_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"next_coupon\":") != TDX_OK ||
        append_text(out, &row->next_coupon_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"last_coupon\":") != TDX_OK ||
        append_text(out, &row->last_coupon_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"listing\":") != TDX_OK ||
        append_text(out, &row->listing_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_start\":") != TDX_OK ||
        append_text(out, &row->conversion_start_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_end\":") != TDX_OK ||
        append_text(out, &row->conversion_end_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    if (APPEND_LITERAL(out, err, ",\"numbers\":{\"remaining_years\":") != TDX_OK ||
        append_number(out, row->has_remaining_years, row->remaining_years, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"current_coupon_rate_pct\":") != TDX_OK ||
        append_number(out, row->has_current_coupon_rate_pct, row->current_coupon_rate_pct,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"coupon_frequency_months\":") != TDX_OK ||
        append_number(out, row->has_coupon_frequency_months, row->coupon_frequency_months,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"face_value_yuan\":") != TDX_OK ||
        append_number(out, row->has_face_value_yuan, row->face_value_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_price_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_price_yuan, row->issue_price_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"remaining_coupon_count\":") != TDX_OK ||
        append_number(out, row->has_remaining_coupon_count, row->remaining_coupon_count,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"conversion_price_yuan\":") != TDX_OK ||
        append_number(out, row->has_conversion_price_yuan, row->conversion_price_yuan, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"revision_trigger_pct\":") != TDX_OK ||
        append_number(out, row->has_revision_trigger_pct, row->revision_trigger_pct, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"put_trigger_pct\":") != TDX_OK ||
        append_number(out, row->has_put_trigger_pct, row->put_trigger_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"call_trigger_pct\":") != TDX_OK ||
        append_number(out, row->has_call_trigger_pct, row->call_trigger_pct, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
        return TDX_ERR;

    /* The size column, with the semantics it was read under.  Only the field those
     * semantics justify is filled, so a caller never has to guess the unit. */
    if (APPEND_LITERAL(out, err, ",\"size\":{\"scale\":\"") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, tdx_bonds_scale_name(row->scale),
                       strlen(tdx_bonds_scale_name(row->scale)), err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "\",\"source_raw\":") != TDX_OK ||
        append_number(out, row->has_source_scale, row->source_scale_raw, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_size_source_100m\":") != TDX_OK ||
        append_number(out, row->has_issue_size_source_100m, row->issue_size_source_100m, err) !=
            TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"issue_size_yuan\":") != TDX_OK ||
        append_number(out, row->has_issue_size_yuan, row->issue_size_yuan, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"outstanding_balance_source_100m\":") != TDX_OK ||
        append_number(out, row->has_outstanding_balance_source_100m,
                      row->outstanding_balance_source_100m, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"outstanding_balance_yuan\":") != TDX_OK ||
        append_number(out, row->has_outstanding_balance_yuan, row->outstanding_balance_yuan,
                      err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "}") != TDX_OK)
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
        if (APPEND_LITERAL(out, err, "}") != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}

int tdx_bonds_format_schedule(tdx_buf *out, const tdx_jsn_document *doc,
                              const tdx_jsn_group *group, size_t row_in_group,
                              const char *dates_key, const char *rates_key, size_t capacity,
                              tdx_error *err) {
    tdx_bond_coupon *entries;
    size_t count = 0;
    size_t list_length = 0;
    size_t index;
    int result = TDX_ERR;

    if (!out) {
        tdx_error_set(err, "a coupon schedule needs a buffer");
        return TDX_ERR;
    }
    if (capacity == 0)
        return tdx_buf_append(out, "[]", 2, err);
    entries = (tdx_bond_coupon *)malloc(capacity * sizeof(*entries));
    if (!entries) {
        tdx_error_set(err, "out of memory for %zu coupon entries", capacity);
        return TDX_ERR;
    }
    if (tdx_bonds_coupon_schedule(doc, group, row_in_group, dates_key, rates_key, entries,
                                  capacity, &count, &list_length, err) != TDX_OK)
        goto done;
    if (tdx_buf_push(out, '[', err) != TDX_OK)
        goto done;
    for (index = 0; index < count; ++index) {
        if (index > 0 && tdx_buf_push(out, ',', err) != TDX_OK)
            goto done;
        if (tdx_buf_append_printf(out, err, "{\"date\":%.*s,\"rate_pct\":",
                                  (int)entries[index].date.length,
                                  entries[index].date.data ? entries[index].date.data : "")
            != TDX_OK)
            goto done;
        if (!entries[index].has_rate) {
            if (tdx_buf_append(out, "null", 4, err) != TDX_OK)
                goto done;
        } else if (tdx_buf_append_printf(out, err, "%.6f", entries[index].rate_pct) != TDX_OK) {
            goto done;
        }
        if (tdx_buf_push(out, '}', err) != TDX_OK)
            goto done;
    }
    result = tdx_buf_push(out, ']', err);

done:
    free(entries);
    return result;
}

int tdx_bonds_format_summary(tdx_buf *out, size_t rows, size_t rows_with_name,
                             size_t rows_with_underlying, size_t rows_with_size,
                             const char *resource, const char *scale_name, const char *endpoint,
                             tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the bond summary needs a buffer");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"bond_summary\",\"resource\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"scale\":\"") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append(out, scale_name ? scale_name : "", strlen(scale_name ? scale_name : ""),
                       err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, "\",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"rows\":%zu,\"rows_with_name\":%zu,\"rows_with_underlying\":%zu,"
                              "\"rows_with_size\":%zu}",
                              rows, rows_with_name, rows_with_underlying, rows_with_size) !=
        TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_bonds_format_comma_array(tdx_buf *out, const tdx_bond_text *text, int numeric_as_number,
                                 tdx_error *err) {
    size_t index = 0;
    size_t emitted = 0;

    if (!out || !text) {
        tdx_error_set(err, "a comma list needs a buffer and a value");
        return TDX_ERR;
    }
    if (tdx_buf_push(out, '[', err) != TDX_OK)
        return TDX_ERR;
    while (text->present && index < text->length) {
        size_t start;
        size_t stop;
        /* Skip separators and padding. */
        while (index < text->length &&
               (text->data[index] == ',' || text->data[index] == ' ' ||
                text->data[index] == '\t'))
            index++;
        start = index;
        while (index < text->length && text->data[index] != ',')
            index++;
        stop = index;
        while (stop > start && (text->data[stop - 1] == ' ' || text->data[stop - 1] == '\t'))
            stop--;
        if (stop == start)
            continue;
        if (emitted > 0 && tdx_buf_push(out, ',', err) != TDX_OK)
            return TDX_ERR;
        emitted++;
        if (numeric_as_number) {
            char scratch[64];
            char *end = NULL;
            double value;
            size_t length = stop - start;
            if (length < sizeof(scratch)) {
                memcpy(scratch, text->data + start, length);
                scratch[length] = '\0';
                value = strtod(scratch, &end);
                if (end && end != scratch && *end == '\0') {
                    if (tdx_buf_append_printf(out, err, "%.10g", value) != TDX_OK)
                        return TDX_ERR;
                    continue;
                }
            }
            /* A piece that is not a number is kept as a string: the reference's
             * numeric_array does the same, and dropping it would shorten the list
             * without saying so. */
        }
        if (tdx_buf_push(out, '"', err) != TDX_OK)
            return TDX_ERR;
        {
            size_t position;
            for (position = start; position < stop; ++position) {
                char ch = text->data[position];
                if (ch == '"' || ch == '\\') {
                    char pair[2];
                    pair[0] = '\\';
                    pair[1] = ch;
                    if (tdx_buf_append(out, pair, 2, err) != TDX_OK)
                        return TDX_ERR;
                } else if (tdx_buf_push(out, (uint8_t)ch, err) != TDX_OK) {
                    return TDX_ERR;
                }
            }
        }
        if (tdx_buf_push(out, '"', err) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, ']', err);
}
