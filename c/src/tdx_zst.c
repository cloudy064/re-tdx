/* tdx_zst.c - zst_cache *.img container, tag stream and field calibration. */
#include "tdx_zst.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#define TDX_ZST_HEADER 24u

static int inflate_bytes(const uint8_t *wire, size_t wire_size, size_t decoded_size,
                         uint8_t **out, tdx_error *err) {
    uLongf produced = (uLongf)decoded_size;
    uint8_t *buffer = (uint8_t *)malloc(decoded_size ? decoded_size : 1);
    if (!buffer) {
        tdx_error_set(err, "out of memory for %zu inflated bytes", decoded_size);
        return TDX_ERR;
    }
    if (decoded_size == 0) {
        *out = buffer;
        return TDX_OK;
    }
    if (uncompress(buffer, &produced, wire, (uLong)wire_size) != Z_OK ||
        produced != (uLongf)decoded_size) {
        free(buffer);
        tdx_error_set(err, "zst payload did not inflate to the announced %zu bytes",
                      decoded_size);
        return TDX_ERR;
    }
    *out = buffer;
    return TDX_OK;
}

static double as_double(const char *text) {
    if (!text || !*text)
        return 0.0;
    return strtod(text, NULL);
}

/* Level ladders: 20..24 bid price, 30..34 bid volume, 40..44 ask price,
 * 50..54 ask volume; the 6..10 levels follow the same +5 pattern. */
static void apply_calibrated(tdx_zst_record *record) {
    size_t index;
    const char *value;

    value = tdx_zst_find_field(record, "0T");
    if (value)
        record->time_hhmmss = atoi(value);
    value = tdx_zst_find_field(record, "08");
    if (value)
        record->last_price = as_double(value);
    value = tdx_zst_find_field(record, "1H");
    if (value)
        record->total_volume = as_double(value);

    for (index = 0; index < 5; ++index) {
        char id[3];
        id[0] = (char)('2' + (int)index);
        id[1] = '\0';
        value = tdx_zst_find_field(record, id);
        if (value)
            record->bid_price[index] = as_double(value);
        id[0] = (char)('3' + (int)index);
        value = tdx_zst_find_field(record, id);
        if (value)
            record->bid_volume[index] = as_double(value);
        id[0] = (char)('4' + (int)index);
        value = tdx_zst_find_field(record, id);
        if (value)
            record->ask_price[index] = as_double(value);
        id[0] = (char)('5' + (int)index);
        value = tdx_zst_find_field(record, id);
        if (value)
            record->ask_volume[index] = as_double(value);
    }
    for (index = 0; index < 5; ++index) {
        char id[3];
        id[0] = (char)('2' + (int)index);
        id[1] = '5';
        id[2] = '\0';
        value = tdx_zst_find_field(record, id);
        if (value)
            record->bid_price[index + 5] = as_double(value);
        id[0] = (char)('3' + (int)index);
        value = tdx_zst_find_field(record, id);
        if (value)
            record->bid_volume[index + 5] = as_double(value);
        id[0] = (char)('4' + (int)index);
        value = tdx_zst_find_field(record, id);
        if (value)
            record->ask_price[index + 5] = as_double(value);
        id[0] = (char)('5' + (int)index);
        value = tdx_zst_find_field(record, id);
        if (value)
            record->ask_volume[index + 5] = as_double(value);
    }
}

static int document_push(tdx_zst_document *document, const tdx_zst_record *record,
                         tdx_error *err) {
    if (document->count == document->capacity) {
        size_t wanted = document->capacity ? document->capacity * 2 : 256;
        tdx_zst_record *grown =
            (tdx_zst_record *)realloc(document->records, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the record list to %zu", wanted);
            return TDX_ERR;
        }
        document->records = grown;
        document->capacity = wanted;
    }
    document->records[document->count++] = *record;
    return TDX_OK;
}

static void record_key(tdx_zst_record *record) {
    size_t index;
    record->key_market = (record->security_key[0] - '0') * 10 +
                         (record->security_key[1] - '0');
    for (index = 0; index < 6; ++index)
        record->code[index] = record->security_key[2 + index];
    record->code[6] = '\0';
}

int tdx_zst_parse_stream(const uint8_t *payload, size_t size,
                         tdx_zst_document *out, tdx_error *err) {
    size_t position = 0;
    tdx_zst_record current;
    int in_record = 0;

    memset(&current, 0, sizeof(current));
    while (position < size) {
        const uint8_t tag = payload[position++];
        const size_t start = position;
        size_t length;
        while (position < size && payload[position] >= 0x20)
            position++;
        length = position - start;

        if (tag == 3) {
            if (length != 8) {
                tdx_error_set(err, "zst record key has %zu characters, expected 8",
                              length);
                return TDX_ERR;
            }
            memset(&current, 0, sizeof(current));
            memcpy(current.security_key, payload + start, 8);
            current.security_key[8] = '\0';
            record_key(&current);
            in_record = 1;
        } else if (tag == 2) {
            if (!in_record) {
                tdx_error_set(err, "zst field appeared before any record key");
                return TDX_ERR;
            }
            if (length < 2) {
                tdx_error_set(err, "zst field is shorter than its two-character id");
                return TDX_ERR;
            }
            if (current.field_count < TDX_ZST_MAX_FIELDS) {
                tdx_zst_field *field = &current.fields[current.field_count++];
                memcpy(field->id, payload + start, 2);
                field->id[2] = '\0';
                {
                    size_t copy = length - 2;
                    if (copy >= TDX_ZST_FIELD_VALUE)
                        copy = TDX_ZST_FIELD_VALUE - 1;
                    memcpy(field->value, payload + start + 2, copy);
                    field->value[copy] = '\0';
                }
            }
        } else if (tag == 4) {
            if (!in_record) {
                tdx_error_set(err, "zst record terminator without a record");
                return TDX_ERR;
            }
            apply_calibrated(&current);
            if (document_push(out, &current, err) != TDX_OK)
                return TDX_ERR;
            in_record = 0;
        } else {
            tdx_error_set(err, "zst payload carries an unknown tag %u", (unsigned)tag);
            return TDX_ERR;
        }
    }
    if (in_record) {
        tdx_error_set(err, "zst payload ends inside a record");
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_zst_decode(const uint8_t *file, size_t size, tdx_zst_document *out,
                   tdx_error *err) {
    uint32_t compressed;
    uint32_t decoded;
    uint8_t *payload = NULL;
    int result;

    if (!file || !out || size < TDX_ZST_HEADER + 2) {
        tdx_error_set(err, "zst file is too short to carry a container");
        return TDX_ERR;
    }
    compressed = (uint32_t)file[8] | ((uint32_t)file[9] << 8) |
                 ((uint32_t)file[10] << 16) | ((uint32_t)file[11] << 24);
    decoded = (uint32_t)file[16] | ((uint32_t)file[17] << 8) |
              ((uint32_t)file[18] << 16) | ((uint32_t)file[19] << 24);
    if (compressed != size - TDX_ZST_HEADER) {
        tdx_error_set(err, "zst header announces %u compressed bytes, file holds %zu",
                      compressed, size - TDX_ZST_HEADER);
        return TDX_ERR;
    }
    if (file[TDX_ZST_HEADER] != 0x78) {
        tdx_error_set(err, "zst payload is not a zlib stream");
        return TDX_ERR;
    }
    if (inflate_bytes(file + TDX_ZST_HEADER, compressed, decoded, &payload, err) != TDX_OK)
        return TDX_ERR;

    out->compressed_size = compressed;
    out->inflated_size = decoded;
    result = tdx_zst_parse_stream(payload, decoded, out, err);
    free(payload);
    return result;
}

static int read_file(const char *path, uint8_t **out, size_t *out_size, tdx_error *err) {
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *buffer;
    if (!file) {
        tdx_error_set(err, "cannot open %s", path);
        return TDX_ERR;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
        fclose(file);
        tdx_error_set(err, "cannot size %s", path);
        return TDX_ERR;
    }
    rewind(file);
    buffer = (uint8_t *)malloc((size_t)size ? (size_t)size : 1);
    if (!buffer) {
        fclose(file);
        tdx_error_set(err, "out of memory reading %s", path);
        return TDX_ERR;
    }
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        tdx_error_set(err, "short read on %s", path);
        return TDX_ERR;
    }
    fclose(file);
    *out = buffer;
    *out_size = (size_t)size;
    return TDX_OK;
}

int tdx_zst_decode_file(const char *path, tdx_zst_document *out, tdx_error *err) {
    uint8_t *file = NULL;
    size_t size = 0;
    int result;
    if (!path || !out) {
        tdx_error_set(err, "decode needs a path and a document");
        return TDX_ERR;
    }
    if (read_file(path, &file, &size, err) != TDX_OK)
        return TDX_ERR;
    result = tdx_zst_decode(file, size, out, err);
    free(file);
    return result;
}

void tdx_zst_document_init(tdx_zst_document *document) {
    if (!document)
        return;
    memset(document, 0, sizeof(*document));
}

void tdx_zst_document_free(tdx_zst_document *document) {
    if (!document)
        return;
    free(document->records);
    memset(document, 0, sizeof(*document));
}

const char *tdx_zst_find_field(const tdx_zst_record *record, const char *id) {
    size_t index;
    if (!record || !id)
        return NULL;
    for (index = 0; index < record->field_count; ++index)
        if (record->fields[index].id[0] == id[0] && record->fields[index].id[1] == id[1])
            return record->fields[index].value;
    return NULL;
}
