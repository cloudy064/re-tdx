#ifndef TDX_CLI_DATE_H
#define TDX_CLI_DATE_H
#include "tdx_error.h"
#include "tdx_date.h"
/* Resolve an explicit date or today's local date into a canonical YYYYMMDD. */
int cli_resolve_date(const char *requested, char out[9], uint32_t *date, tdx_error *err);
#endif
