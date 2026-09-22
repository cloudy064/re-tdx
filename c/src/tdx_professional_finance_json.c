/* The quarterly finance JSON schema, independent of file and CLI orchestration. */
#include "tdx_professional_finance_json.h"

#include "tdx_json_write.h"

#define LITERAL(out, err, text) tdx_buf_append((out), (text), sizeof(text) - 1, (err))

static int append_field_value(tdx_buf *out, const tdx_profinance_record_view *record,
                              unsigned field, tdx_error *err) {
    double value = 0;
    if (!tdx_profinance_field(record, field, &value))
        return LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, "%.6f", value);
}

int tdx_profinance_format_document(tdx_buf *out, const char *source,
                                  const char *member, uint32_t member_bytes,
                                  uint32_t member_crc,
                                  const tdx_profinance_document *document,
                                  tdx_error *err) {
    if (!out || !document) {
        tdx_error_set(err, "finance document rendering needs a buffer and a document");
        return TDX_ERR;
    }
    if (LITERAL(out, err, "{\"type\":\"finance_document\",\"source\":") != TDX_OK ||
        tdx_format_json_string(out, source, err) != TDX_OK ||
        LITERAL(out, err, ",\"member\":") != TDX_OK ||
        tdx_format_json_string(out, member, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_append_printf(out, err,
        ",\"member_bytes\":%u,\"member_crc\":\"%08x\","
        "\"version\":%u,\"report_date\":%u,\"records\":%zu,"
        "\"field_count\":%zu,\"index_size\":%zu,\"data_size\":%zu,"
        "\"named_fields\":[%u,%u]}",
        (unsigned)member_bytes, (unsigned)member_crc, (unsigned)document->version,
        (unsigned)document->report_date, document->record_count, document->field_count,
        document->index_size, document->data_size,
        TDX_PROFINANCE_FIELD_REVENUE_YOY, TDX_PROFINANCE_FIELD_PROFIT_YOY);
}

static int append_identity(tdx_buf *out, const char *type,
                           const tdx_profinance_record_view *record, tdx_error *err) {
    if (!out || !record) {
        tdx_error_set(err, "finance record rendering needs a buffer and a record");
        return TDX_ERR;
    }
    if (LITERAL(out, err, "{\"type\":") != TDX_OK ||
        tdx_format_json_string(out, type, err) != TDX_OK ||
        LITERAL(out, err, ",\"code\":") != TDX_OK)
        return TDX_ERR;
    return tdx_format_json_string(out, record->code, err);
}

int tdx_profinance_format_record(tdx_buf *out, const tdx_profinance_record_view *record,
                                uint32_t report_date, tdx_error *err) {
    if (append_identity(out, "finance_record", record, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_append_printf(out, err,
        ",\"market_id\":%d,\"report_date\":%u,\"field_count\":%zu}",
        record->market_id, (unsigned)report_date, record->field_count);
}

int tdx_profinance_format_field(tdx_buf *out, const tdx_profinance_record_view *record,
                               unsigned field, tdx_error *err) {
    const char *name = tdx_profinance_field_name(field);
    if (append_identity(out, "finance_field", record, err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"field\":%u,\"name\":", field) != TDX_OK)
        return TDX_ERR;
    if (name) {
        if (tdx_format_json_string(out, name, err) != TDX_OK)
            return TDX_ERR;
    } else if (LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (LITERAL(out, err, ",\"value\":") != TDX_OK ||
        append_field_value(out, record, field, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_profinance_format_row(tdx_buf *out, const tdx_profinance_record_view *record,
                             uint32_t report_date, unsigned extra_field, tdx_error *err) {
    if (append_identity(out, "finance_row", record, err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"report_date\":%u,\"revenue_yoy\":",
                              record->market_id, (unsigned)report_date) != TDX_OK ||
        append_field_value(out, record, TDX_PROFINANCE_FIELD_REVENUE_YOY, err) != TDX_OK ||
        LITERAL(out, err, ",\"profit_yoy\":") != TDX_OK ||
        append_field_value(out, record, TDX_PROFINANCE_FIELD_PROFIT_YOY, err) != TDX_OK)
        return TDX_ERR;
    if (extra_field &&
        (tdx_buf_append_printf(out, err, ",\"field_%u\":", extra_field) != TDX_OK ||
         append_field_value(out, record, extra_field, err) != TDX_OK))
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}
