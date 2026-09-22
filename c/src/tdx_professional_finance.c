/* tdx_professional_finance.c - the quarterly finance packages. */
#include "tdx_professional_finance.h"

#include <math.h>
#include <string.h>

/* The three-character and two-character prefixes Shanghai, Shenzhen and Beijing use, as
 * everywhere else in this project.  The packages carry no market column - the byte after
 * the code is zero for every exchange in the packages measured - so it is inferred from
 * the code, which is what the reference does and what the terminal does elsewhere. */
static int inferred_market_id(const char code[6]) {
    static const char *const shanghai[] = {"600", "601", "603", "605", "688", "689", "110",
                                           "111", "113", "118", "132", "204", "510", "511",
                                           "512", "513", "515", "516", "518", "588", "900"};
    static const char *const beijing[] = {"430", "830", "831", "832", "833", "834", "835",
                                          "836", "837", "838", "839", "870", "871", "872",
                                          "873", "874", "875", "876", "877", "920"};
    size_t index;
    for (index = 0; index < sizeof(shanghai) / sizeof(shanghai[0]); ++index)
        if (memcmp(code, shanghai[index], 3) == 0)
            return 1;
    for (index = 0; index < sizeof(beijing) / sizeof(beijing[0]); ++index)
        if (memcmp(code, beijing[index], 3) == 0)
            return 2;
    return 0; /* Shenzhen is the default, as in the rest of the project. */
}

static int valid_code(const char *code) {
    size_t index;
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9')
            return 0;
    return 1;
}

static uint16_t u16_at(const uint8_t *data, size_t offset) {
    return (uint16_t)((uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8));
}

static uint32_t u32_at(const uint8_t *data, size_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

int tdx_profinance_parse(const uint8_t *data, size_t size, tdx_profinance_document *out,
                      tdx_error *err) {
    size_t index;
    size_t expected;

    if (!data || !out) {
        tdx_error_set(err, "parsing a finance member needs bytes and an output");
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    if (size < TDX_PROFINANCE_HEADER_SIZE) {
        tdx_error_set(err, "a %zu-byte finance member is shorter than its %d-byte header",
                      size, TDX_PROFINANCE_HEADER_SIZE);
        return TDX_ERR;
    }
    out->version = u16_at(data, 0);
    out->report_date = u32_at(data, 2);
    out->record_count = u16_at(data, 6);
    out->index_size = u16_at(data, 10);
    out->data_size = u32_at(data, 12);
    out->data = data;
    out->size = size;
    out->index_offset = TDX_PROFINANCE_HEADER_SIZE;
    if (out->version != 1) {
        tdx_error_set(err, "finance member version %u is not the supported version 1",
                      out->version);
        return TDX_ERR;
    }
    if (out->record_count == 0) {
        tdx_error_set(err, "the finance member declares no records");
        return TDX_ERR;
    }
    if (out->index_size != TDX_PROFINANCE_INDEX_SIZE) {
        tdx_error_set(err, "finance index entries are %zu bytes, not %d", out->index_size,
                      TDX_PROFINANCE_INDEX_SIZE);
        return TDX_ERR;
    }
    if (out->data_size == 0 || out->data_size % 4 != 0) {
        tdx_error_set(err, "the finance record size %zu is not a positive multiple of 4",
                      out->data_size);
        return TDX_ERR;
    }
    out->field_count = out->data_size / 4;
    if (out->field_count > TDX_PROFINANCE_FIELDS_MAX) {
        tdx_error_set(err, "the finance member declares %zu fields, more than %d",
                      out->field_count, TDX_PROFINANCE_FIELDS_MAX);
        return TDX_ERR;
    }
    /* The index has to fit, and so does the data it points into. */
    if (out->record_count > TDX_PROFINANCE_RECORDS_MAX) {
        tdx_error_set(err, "the finance member declares %zu records, more than %d",
                      out->record_count, TDX_PROFINANCE_RECORDS_MAX);
        return TDX_ERR;
    }
    expected = out->index_offset + out->record_count * out->index_size;
    if (expected > size) {
        tdx_error_set(err, "the finance index of %zu records needs %zu bytes, past the %zu "
                           "in the member",
                      out->record_count, expected, size);
        return TDX_ERR;
    }
    out->data_start = expected;
    /* Every index entry is checked now rather than at read time, so a caller iterating
     * records never meets a bad one halfway through. */
    for (index = 0; index < out->record_count; ++index) {
        size_t entry = out->index_offset + index * out->index_size;
        uint32_t offset = u32_at(data, entry + 7);
        if (offset < expected || (size_t)offset > size ||
            out->data_size > size - (size_t)offset) {
            tdx_error_set(err, "finance index entry %zu points to %u, outside its data", index,
                          offset);
            return TDX_ERR;
        }
        if (!valid_code((const char *)data + entry)) {
            tdx_error_set(err, "finance index entry %zu does not hold a six-digit code",
                          index);
            return TDX_ERR;
        }
    }
    return TDX_OK;
}

int tdx_profinance_record_at(const tdx_profinance_document *document, size_t index,
                          tdx_profinance_record_view *out, tdx_error *err) {
    size_t entry;

    if (!document || !out) {
        tdx_error_set(err, "reading a finance record needs a document and an output");
        return TDX_ERR;
    }
    if (index >= document->record_count) {
        tdx_error_set(err, "finance record %zu is past the %zu the member holds", index,
                      document->record_count);
        return TDX_ERR;
    }
    entry = document->index_offset + index * document->index_size;
    memset(out, 0, sizeof(*out));
    out->data = document->data;
    out->size = document->size;
    out->data_offset = u32_at(document->data, entry + 7);
    out->field_count = document->field_count;
    memcpy(out->code, document->data + entry, 6);
    out->code[6] = '\0';
    out->market_id = inferred_market_id(out->code);
    return TDX_OK;
}

int tdx_profinance_field(const tdx_profinance_record_view *view, unsigned field, double *out) {
    float stored;
    size_t offset;

    if (!view || !out || field == 0 || field > view->field_count)
        return 0;
    offset = view->data_offset + (size_t)(field - 1) * 4;
    if (offset + 4 > view->size)
        return 0;
    /* memcpy rather than a cast: the offset is wherever the package put it, and reading
     * an unaligned float through a cast is undefined even where the hardware allows it. */
    memcpy(&stored, view->data + offset, sizeof(stored));
    if (!isfinite(stored))
        return 0;
    *out = (double)stored;
    return 1;
}

const char *tdx_profinance_field_name(unsigned field) {
    if (field == TDX_PROFINANCE_FIELD_REVENUE_YOY)
        return "\xe8\x90\xa5\xe6\x94\xb6\xe5\x90\x8c\xe6\xaf\x94";
    if (field == TDX_PROFINANCE_FIELD_PROFIT_YOY)
        return "\xe5\x87\x80\xe5\x88\xa9\xe6\xb6\xa6\xe5\x90\x8c\xe6\xaf\x94";
    /* Not published, so not named - the same rule the trading field tables follow. */
    return NULL;
}
