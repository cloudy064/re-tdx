/* JSON rendering for decoded quarterly finance packages. */
#ifndef TDX_PROFESSIONAL_FINANCE_JSON_H
#define TDX_PROFESSIONAL_FINANCE_JSON_H

#include "tdx_bytes.h"
#include "tdx_professional_finance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Append one JSON object without a newline to an initialized buffer. */
int tdx_profinance_format_document(tdx_buf *out, const char *source,
                                  const char *member, uint32_t member_bytes,
                                  uint32_t member_crc,
                                  const tdx_profinance_document *document,
                                  tdx_error *err);
int tdx_profinance_format_record(tdx_buf *out, const tdx_profinance_record_view *record,
                                uint32_t report_date, tdx_error *err);
int tdx_profinance_format_field(tdx_buf *out, const tdx_profinance_record_view *record,
                               unsigned field, tdx_error *err);
/* extra_field == 0 omits the optional numbered field. */
int tdx_profinance_format_row(tdx_buf *out, const tdx_profinance_record_view *record,
                             uint32_t report_date, unsigned extra_field, tdx_error *err);

#ifdef __cplusplus
}
#endif
#endif
