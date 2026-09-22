/* tdx_professional_json.h - JSONL rendering for the professional-data files. */
#ifndef TDX_PROFESSIONAL_JSON_H
#define TDX_PROFESSIONAL_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_professional.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One selected record, without a trailing newline.  `name` is null when the field table
 * does not list the id, which is reported rather than papered over. */
int tdx_professional_format_record(tdx_buf *out, const tdx_professional_record *record,
                                   const char *name, size_t record_index, tdx_error *err);

/* One field's summary, without a trailing newline. */
int tdx_professional_format_summary(tdx_buf *out, const tdx_professional_id_summary *summary,
                                    tdx_professional_kind kind, tdx_error *err);

/* The file-level document: the source path, the sizes, the kind and how many ids the
 * table could and could not name.  That last count is the honest headline - it says how
 * much of the file this build can label. */
int tdx_professional_format_document(tdx_buf *out, const char *source, tdx_professional_kind kind,
                                     size_t bytes, size_t records, size_t fields,
                                     size_t fields_named, size_t fields_unnamed,
                                     size_t selected, size_t first_date, size_t last_date,
                                     tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PROFESSIONAL_JSON_H */
