/* tdx_seal_json.h - JSONL rendering for the sealed-order figure. */
#ifndef TDX_SEAL_JSON_H
#define TDX_SEAL_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_limit.h"
#include "tdx_seal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One security's limits and seal, without a trailing newline.  The inputs are echoed beside
 * the result, because a seal is a claim about a particular quote and a reader has to be able
 * to see which one. */
int tdx_seal_format(tdx_buf *out, const tdx_code *security, const tdx_limit_prices *limits,
                    const tdx_seal_result *seal, const tdx_seal_input *input,
                    const char *as_of, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_SEAL_JSON_H */
