/* tdx_professional_json.c - JSONL rendering for the professional-data files. */
#include "tdx_professional_json.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

/* Floats are printed with enough digits to round-trip, because a stored value that
 * cannot be read back is not much use.  A non-finite one is reported as null: it is in
 * the file, but it is not a value. */
static int append_float(tdx_buf *out, float value, tdx_error *err) {
    if (!isfinite(value))
        return APPEND_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "%.9g", (double)value);
}

int tdx_professional_format_record(tdx_buf *out, const tdx_professional_record *record,
                                   const char *name, size_t record_index, tdx_error *err) {
    if (!out || !record) {
        tdx_error_set(err, "professional record rendering needs a buffer and a record");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"professional_record\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"id\":%u,\"name\":", record_index,
                              record->id) != TDX_OK)
        return TDX_ERR;
    if (name) {
        if (tdx_format_json_string(out, name, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    /* A date of zero is the file saying "no date", which is not the same as a date of
     * 00000000, so it renders as null. */
    if (record->date) {
        if (tdx_buf_append_printf(out, err, ",\"date\":%u", record->date) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, ",\"date\":null") != TDX_OK) {
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, ",\"first\":") != TDX_OK ||
        append_float(out, record->first, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"second\":") != TDX_OK ||
        append_float(out, record->second, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_professional_format_summary(tdx_buf *out, const tdx_professional_id_summary *summary,
                                    tdx_professional_kind kind, tdx_error *err) {
    if (!out || !summary) {
        tdx_error_set(err, "professional summary rendering needs a buffer and a summary");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"professional_field\",\"kind\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, tdx_professional_kind_name(kind), err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"id\":%u,\"name\":", summary->id) != TDX_OK)
        return TDX_ERR;
    if (summary->name) {
        if (tdx_format_json_string(out, summary->name, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        /* An id the published table does not list.  Naming it anyway would be inventing
         * data, which is the one thing worse than admitting the gap. */
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"records\":%zu,\"named\":%s", summary->count,
                              summary->name ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (summary->first_date) {
        if (tdx_buf_append_printf(out, err, ",\"first_date\":%u,\"last_date\":%u",
                                  summary->first_date, summary->last_date) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, ",\"first_date\":null,\"last_date\":null") != TDX_OK) {
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, ",\"first_min\":") != TDX_OK ||
        append_float(out, summary->first_min, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"first_max\":") != TDX_OK ||
        append_float(out, summary->first_max, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_professional_format_document(tdx_buf *out, const char *source, tdx_professional_kind kind,
                                     size_t bytes, size_t records, size_t fields,
                                     size_t fields_named, size_t fields_unnamed,
                                     size_t selected, size_t first_date, size_t last_date,
                                     tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the professional document needs a buffer");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"professional_document\",\"source\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, source ? source : "", err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"kind\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, tdx_professional_kind_name(kind), err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"bytes\":%zu,\"record_size\":%d,\"records\":%zu,"
                              "\"fields\":%zu,\"fields_named\":%zu,\"fields_unnamed\":%zu,"
                              "\"selected\":%zu,\"table_size\":%zu",
                              bytes, TDX_PROFESSIONAL_TRADING_RECORD_SIZE, records, fields,
                              fields_named, fields_unnamed, selected,
                              tdx_professional_field_count(kind)) != TDX_OK)
        return TDX_ERR;
    if (first_date && last_date) {
        if (tdx_buf_append_printf(out, err, ",\"first_date\":%zu,\"last_date\":%zu}", first_date,
                                  last_date) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, ",\"first_date\":null,\"last_date\":null}") != TDX_OK) {
        return TDX_ERR;
    }
    return TDX_OK;
}
