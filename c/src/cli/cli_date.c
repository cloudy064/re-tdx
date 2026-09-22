#include "cli_date.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int cli_resolve_date(const char *requested, char out[9], uint32_t *date, tdx_error *err) {
    uint32_t parsed;
    if (requested && *requested) {
        if (!tdx_date_parse(requested, strlen(requested), 1, &parsed)) goto invalid;
        snprintf(out, 9, "%08u", (unsigned)parsed);
    } else {
        time_t now = time(NULL);
        struct tm local;
#ifdef _WIN32
        if (localtime_s(&local, &now) != 0) goto invalid;
#else
        if (!localtime_r(&now, &local)) goto invalid;
#endif
        if (strftime(out, 9, "%Y%m%d", &local) != 8 || !tdx_date_parse(out, 8, 0, &parsed)) goto invalid;
    }
    if (date) *date = parsed;
    return TDX_OK;
invalid:
    tdx_error_set(err, "cannot resolve a valid calendar date");
    return TDX_ERR;
}
