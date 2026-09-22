/* tdx_professional_finance.h - the quarterly finance packages.
 *
 * One ZIP per quarter from tdxfin/gpcw.txt, holding a single member, e.g.
 * gpcw20260630.dat.  The member is a fixed-layout table:
 *
 *   header, 20 bytes
 *     0  u16  version, 1 in every package measured
 *     2  u32  report date, YYYYMMDD
 *     6  u16  record count
 *     8  u16  zero in the packages measured
 *    10  u16  index entry size, 11
 *    12  u32  bytes of floats per record, a multiple of 4
 *    18  u16  zero in the packages measured
 *
 *   index, record_count entries of index entry size bytes
 *     0  char[6]  security code
 *     6  u8       zero in the packages measured
 *     7  u32      offset of this record's data
 *
 *   data, at each index entry's offset: bytes/4 floats
 *
 * Measured on gpcw20260630.zip: header 20, 5,570 records, index entry 11, 2,336 bytes of
 * floats each, and 20 + 5570*11 + 5570*2336 is exactly the member's 13,072,810 bytes with
 * nothing left over.  That the arithmetic closes exactly is what confirms the layout.
 *
 * THE FLOATS ARE NOT COPIED.  At 584 floats per record and 5,570 records the table is
 * 13 MB, and copying it to hand out would double that for no gain; the document keeps a
 * pointer to the member and an accessor reads one field at a time through memcpy, so an
 * unaligned offset is read correctly rather than through a cast that hopes it is aligned.
 *
 * ONLY TWO FIELDS ARE NAMED, because only two are published: field 183 is revenue
 * year-on-year and 184 is net-profit year-on-year, which is what the reference names.
 * Every other field is reported by its NUMBER.  A finance field table is not something
 * this project has, and inventing labels for 582 unknown columns would be worse than
 * leaving them as the numbers the package actually contains. */
#ifndef TDX_PROFESSIONAL_FINANCE_H
#define TDX_PROFESSIONAL_FINANCE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_PROFINANCE_HEADER_SIZE 20
#define TDX_PROFINANCE_INDEX_SIZE 11
#define TDX_PROFINANCE_RECORDS_MAX 20000
/* The packages measured carry 2,336 bytes of floats per record; this is the ceiling
 * beyond which the file is not this format. */
#define TDX_PROFINANCE_FIELDS_MAX 8192

/* The two field ids the reference names. */
#define TDX_PROFINANCE_FIELD_REVENUE_YOY 183
#define TDX_PROFINANCE_FIELD_PROFIT_YOY 184

typedef struct tdx_profinance_record_view {
    const uint8_t *data; /* the member, not owned */
    size_t size;
    size_t data_offset;
    size_t field_count;
    int market_id;
    char code[8];
} tdx_profinance_record_view;

typedef struct tdx_profinance_document {
    const uint8_t *data; /* the member, not owned */
    size_t size;
    uint16_t version;
    uint32_t report_date;
    size_t record_count;
    size_t index_size;
    size_t data_size;   /* bytes of floats per record */
    size_t field_count; /* data_size / 4 */
    size_t index_offset; /* TDX_PROFINANCE_HEADER_SIZE */
    size_t data_start;   /* just past the index */
} tdx_profinance_document;

/* Validates the header and every index entry's bounds, without copying the data. */
int tdx_profinance_parse(const uint8_t *data, size_t size, tdx_profinance_document *out,
                      tdx_error *err);

/* Reads one record's identity and data location.  Returns 0 when the index is out of
 * range. */
int tdx_profinance_record_at(const tdx_profinance_document *document, size_t index,
                          tdx_profinance_record_view *out, tdx_error *err);

/* Reads one field of a record, 1-based as the packages number them.  Returns 0 when the
 * field is out of range or its stored value is not finite. */
int tdx_profinance_field(const tdx_profinance_record_view *view, unsigned field, double *out);

/* The published name of a field, or NULL - only two are published. */
const char *tdx_profinance_field_name(unsigned field);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PROFESSIONAL_FINANCE_H */
