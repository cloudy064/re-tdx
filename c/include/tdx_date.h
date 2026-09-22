/* Calendar primitives. All functions return 1 on success, 0 on invalid input. */
#ifndef TDX_DATE_H
#define TDX_DATE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Gregorian date, year 1..9999. Business-specific year bounds belong to callers. */
int tdx_date_valid(uint32_t date);
/* Exactly YYYYMMDD, or YYYY-MM-DD when allow_dashes is nonzero. */
int tdx_date_parse(const char *text, size_t length, int allow_dashes, uint32_t *out);
int tdx_date_days(uint32_t date, long long *out);
#ifdef __cplusplus
}
#endif
#endif
