/* tdx_zst.h - TdxW zst_cache *.img decoder (historical "super order book").
 *
 * Container: 24-byte header + zlib.  header[8] is the compressed size, header[16]
 * the inflated size.
 * Payload: a tag stream.  tag 3 starts a record and carries an 8-character
 * security key; tag 2 carries a field whose value is ASCII text terminated by
 * the next control byte; tag 4 ends the record.
 * Field numbers were calibrated against live captures: 0T is HHMMSS, 08 the
 * last price, 20..24/30..34 the five bid prices/volumes, 40..44/50..54 the five
 * ask prices/volumes, and 25..29/35..39/45..49/55..59 the same for levels 6..10
 * (present only in the open/close snapshots). */
#ifndef TDX_ZST_H
#define TDX_ZST_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_ZST_LEVELS 10
#define TDX_ZST_FIELD_ID 2
#define TDX_ZST_FIELD_VALUE 48
#define TDX_ZST_MAX_FIELDS 128

typedef struct tdx_zst_field {
    char id[TDX_ZST_FIELD_ID + 1];
    char value[TDX_ZST_FIELD_VALUE];
} tdx_zst_field;

typedef struct tdx_zst_record {
    char security_key[9]; /* two ASCII market digits + six ASCII code digits */
    /* The two leading key digits, kept raw.  They are NOT the 0x0547 market id:
     * a Shenzhen file is keyed "01" (sz000623_20260612.img carries 01000623),
     * while 0x0547 calls Shenzhen 0.  Only that one pairing is attested, so this
     * decoder reports the digits it read and never translates them. */
    int key_market;
    char code[8];
    tdx_zst_field fields[TDX_ZST_MAX_FIELDS];
    size_t field_count;
    /* Convenience views built from the calibrated field numbers. */
    int time_hhmmss;
    double last_price;
    double total_volume;
    double bid_price[TDX_ZST_LEVELS];
    double bid_volume[TDX_ZST_LEVELS];
    double ask_price[TDX_ZST_LEVELS];
    double ask_volume[TDX_ZST_LEVELS];
} tdx_zst_record;

typedef struct tdx_zst_document {
    tdx_zst_record *records;
    size_t count;
    size_t capacity;
    size_t inflated_size;
    size_t compressed_size;
} tdx_zst_document;

void tdx_zst_document_init(tdx_zst_document *document);
void tdx_zst_document_free(tdx_zst_document *document);

/* Decodes one zst_cache .img file. */
int tdx_zst_decode(const uint8_t *file, size_t size, tdx_zst_document *out,
                   tdx_error *err);
int tdx_zst_decode_file(const char *path, tdx_zst_document *out, tdx_error *err);

/* Parses an already-inflated tag stream.  Exposed for tests. */
int tdx_zst_parse_stream(const uint8_t *payload, size_t size,
                         tdx_zst_document *out, tdx_error *err);

/* Raw access to a field value by its two-character id; NULL when absent. */
const char *tdx_zst_find_field(const tdx_zst_record *record, const char *id);

#ifdef __cplusplus
}
#endif

#endif /* TDX_ZST_H */
