#ifndef TDX_JSN_JSON_H
#define TDX_JSN_JSON_H
#include "tdx_jsn.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Reserved envelope columns use _cell; collisions use _cell_N. All source
 * columns survive exactly once. renamed counts changed keys in this row. */
int tdx_jsn_format_row(tdx_buf *out, const tdx_jsn_document *document,
                        size_t group_index, size_t row_index, const char *resource,
                        size_t *renamed, tdx_error *err);
typedef struct tdx_jsn_summary {
    const char *resource;
    size_t bytes;
    const char *md5;
    size_t groups;
    size_t rows;
    size_t columns_renamed;
    const char *endpoint;
} tdx_jsn_summary;
int tdx_jsn_format_summary(tdx_buf *out, const tdx_jsn_summary *summary, tdx_error *err);
#ifdef __cplusplus
}
#endif
#endif
