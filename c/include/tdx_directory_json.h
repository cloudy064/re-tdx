#ifndef TDX_DIRECTORY_JSON_H
#define TDX_DIRECTORY_JSON_H

#include "tdx_bytes.h"
#include "tdx_directory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Append one security JSON object, without a newline, to initialized out. */
int tdx_directory_format_security(tdx_buf *out, const tdx_security *security,
                                   tdx_error *err);

#ifdef __cplusplus
}
#endif
#endif
