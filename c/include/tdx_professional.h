/* tdx_professional.h - the professional-data .dat files.
 *
 * A family of public statistical and fundamental series, served from
 * https://data.tdx.com.cn/ as one .dat per security:
 *
 *   tdxgp/gpszsh.txt       the manifest: "<name>,<md5>,<bytes>" per line
 *   tdxgp/gp<market><code>.dat   per-security, e.g. gpsz000001.dat
 *   tdxgp/gpsh999999.dat         the market aggregate
 *   tdxfin/gpcw.txt        the finance manifest: one .zip per quarter
 *
 * This module reads the files; it does NOT fetch them.  Fetching is HTTPS, and adding
 * TLS to this project would break the deployment property it states for itself - one
 * executable plus zlib - so the file arrives from outside and is parsed here.  That is
 * the same division the JSN resources use, where the transport already existed.
 *
 * THE TRADING FORMAT is a flat array of 13-byte records with no header:
 *
 *   offset 0   u8   field id
 *   offset 1   u32  date, YYYYMMDD, little endian; zero means "no date"
 *   offset 5   f32  first value
 *   offset 9   f32  second value
 *
 * Measured, not assumed: all three local samples are exact multiples of 13 with no
 * remainder, which is what settles the record size.  A record whose date is present but
 * not a real calendar date is an ERROR rather than a skipped record, because a file of
 * fixed-size records going out of step is a decode failure and not a data quirk - the
 * reference treats it the same way.
 *
 * THE FIELD TABLES are published per kind: 44 stock fields, 42 market fields and 19
 * board fields.  The ids actually present in the samples go BEYOND them - the per-stock
 * file carries 45, 47, 48, 49 and 50, and the market file carries 43, 44, 45 and 99 - so
 * an id outside a table is reported with its NUMBER and no name.  Inventing a name for
 * an unpublished id would be the one thing worse than admitting we do not know it; the
 * reference returns an empty name for exactly the same reason. */
#ifndef TDX_PROFESSIONAL_H
#define TDX_PROFESSIONAL_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The record size, established by every sample dividing by it exactly. */
#define TDX_PROFESSIONAL_TRADING_RECORD_SIZE 13
#define TDX_PROFESSIONAL_RECORDS_MAX 200000
#define TDX_PROFESSIONAL_FIELD_NAME_MAX 64

typedef enum tdx_professional_kind {
    TDX_PROFESSIONAL_STOCK = 0,
    TDX_PROFESSIONAL_MARKET = 1,
    TDX_PROFESSIONAL_BOARD = 2
} tdx_professional_kind;

const char *tdx_professional_kind_name(tdx_professional_kind kind);
/* Parses "stock", "market" or "board". */
int tdx_professional_kind_parse(const char *text, size_t length, tdx_professional_kind *out);

/* The published name of a field id, or NULL when the table does not list it.  NULL is a
 * real answer: the samples carry ids the tables do not name. */
const char *tdx_professional_field_name(tdx_professional_kind kind, unsigned id);
/* How many ids the table lists. */
size_t tdx_professional_field_count(tdx_professional_kind kind);

typedef struct tdx_professional_record {
    unsigned id;
    uint32_t date; /* YYYYMMDD, or 0 when absent */
    float first;
    float second;
} tdx_professional_record;

/* Parses a trading file.  Returns TDX_ERR when the length is not a multiple of the
 * record size, when a record's date is neither zero nor a real date, or when the file
 * holds more records than the caller has room for. */
int tdx_professional_parse_trading(const uint8_t *data, size_t size,
                                   tdx_professional_record *out, size_t capacity,
                                   size_t *out_count, tdx_error *err);

/* The distinct field ids present, ascending, so a caller can see which series a file
 * carries without reading every record. */
typedef struct tdx_professional_id_summary {
    unsigned id;
    size_t count;
    const char *name; /* NULL when the table does not name this id */
    uint32_t first_date;
    uint32_t last_date;
    int has_values;
    float first_min;
    float first_max;
} tdx_professional_id_summary;

int tdx_professional_summarize(const tdx_professional_record *records, size_t count,
                               tdx_professional_kind kind, tdx_professional_id_summary *out,
                               size_t capacity, size_t *out_count, tdx_error *err);

/* Selection by field id and an inclusive date range.  A zero bound means "open"; an id
 * of 0 means "every id".  Records with no date are kept only when no range was asked
 * for, since a range cannot be said to contain them. */
typedef struct tdx_professional_selection {
    unsigned id;
    uint32_t from;
    uint32_t to;
} tdx_professional_selection;

int tdx_professional_select(const tdx_professional_record *records, size_t count,
                            const tdx_professional_selection *selection, size_t *indices,
                            size_t capacity, size_t *out_count, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PROFESSIONAL_H */
