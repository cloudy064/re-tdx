/* tdx_error.h - minimal status/error plumbing shared by every module.
 *
 * The library never aborts and never throws: every entry point returns
 * TDX_OK or TDX_ERR and, when a tdx_error* is supplied, records a short
 * human readable reason.  Callers may pass NULL when they only need the
 * status code, and every function must tolerate that. */
#ifndef TDX_ERROR_H
#define TDX_ERROR_H

#if defined(__MINGW32__)
/* Provides __MINGW_PRINTF_FORMAT so the format attribute below matches the
 * stdio implementation this build actually links. */
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_OK 0
#define TDX_ERR (-1)

typedef struct tdx_error {
    char message[256];
} tdx_error;

/* Formats into err->message.  Passing a NULL err is allowed and does nothing. */
void tdx_error_set(tdx_error *err, const char *format, ...)
#if defined(__MINGW32__) && defined(__MINGW_PRINTF_FORMAT)
    __attribute__((format(__MINGW_PRINTF_FORMAT, 2, 3)))
#elif defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

#ifdef __cplusplus
}
#endif

#endif /* TDX_ERROR_H */
