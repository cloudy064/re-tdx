/* tdx_jsn.h - the JSN resource table format.
 *
 * A JSN resource is GBK-encoded JSON of one shape:
 *
 *   [ { "colheader": ["name", ...], "data": [ ["cell", ...], ... ] }, ... ]
 *
 * The root is an array of groups; each group has a column-name array and a row
 * array whose every row must be exactly as wide as the header.  That width check
 * is the format's only real invariant and the reference enforces it, so this does
 * too - a row that does not match would otherwise be silently mislabelled by
 * column.
 *
 * Text is GBK on the wire and UTF-8 here.  The conversion uses the Windows code
 * page rather than an embedded table: 936 is the system's own GBK, so the mapping
 * is exact and costs no table.  On a platform without it the conversion reports
 * that plainly instead of guessing.
 *
 * The parser does not interpret the columns.  What each column means is domain
 * knowledge that belongs to the caller; this layer only guarantees that a cell is
 * the cell its header names. */
#ifndef TDX_JSN_H
#define TDX_JSN_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_JSN_PREFIX_LENGTH 2 /* the "bi" the reference prepends */
#define TDX_JSN_RESOURCE_MAX 256

typedef struct tdx_jsn_group {
    size_t json_index; /* the group's node in the JSON document */
    size_t first_row;  /* first row of this group in the flattened numbering */
    size_t row_count;
    size_t column_count;
} tdx_jsn_group;

typedef struct tdx_jsn_document {
    tdx_json_doc json;
    tdx_jsn_group *groups;
    size_t group_count;
    size_t group_capacity;
    size_t row_count; /* across all groups */
} tdx_jsn_document;

void tdx_jsn_document_init(tdx_jsn_document *doc);
void tdx_jsn_document_free(tdx_jsn_document *doc);

/* Parses already-decoded UTF-8 JSON.  Use tdx_jsn_gbk_to_utf8 first when the bytes
 * came straight off the wire. */
int tdx_jsn_parse(const uint8_t *data, size_t size, tdx_jsn_document *doc, tdx_error *err);

size_t tdx_jsn_group_count(const tdx_jsn_document *doc);
/* The group a flattened row belongs to, or NULL. */
const tdx_jsn_group *tdx_jsn_group_of_row(const tdx_jsn_document *doc, size_t row);
const char *tdx_jsn_column_name(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                size_t column);
/* One cell as a JSON value written into out, so a caller can emit it verbatim.
 * A missing cell is written as null. */
int tdx_jsn_cell_json(const tdx_jsn_document *doc, const tdx_jsn_group *group, size_t row_in_group,
                      size_t column, tdx_buf *out, tdx_error *err);
/* One cell as UTF-8 text; a number is formatted, a string is returned as-is. */
int tdx_jsn_cell_text(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                      size_t row_in_group, size_t column, tdx_buf *out, tdx_error *err);

/* A ZERO-COPY view of a string cell: *data points into the document's own arena and
 * stays valid exactly as long as the document does.  A cell that exists but is not
 * a JSON string reports present=1 with data=NULL, so a caller knows to format it
 * instead.  A missing cell reports present=0. */
int tdx_jsn_cell_view(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                      size_t row_in_group, size_t column, const char **data, size_t *length,
                      int *present, tdx_error *err);

/* GBK to UTF-8 through the platform code page. */
int tdx_jsn_gbk_to_utf8(const uint8_t *data, size_t size, tdx_buf *out, tdx_error *err);

/* The remote path for a resource: "<prefix>/<resource>", with the leading slash and
 * a leading prefix in the resource normalised away. */
int tdx_jsn_remote_path(const char *resource, const char *prefix, char *out, size_t capacity,
                        tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_JSN_H */
