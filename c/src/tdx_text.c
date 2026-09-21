/* tdx_text.c - code page conversion for peer-supplied labels.
 *
 * Host names inside connect.cfg and inside the 0x000D handshake response are
 * GB18030.  The client itself decodes them with code page 54936, and we match
 * that choice so labels render identically. */
#include "tdx_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

#define TDX_GB18030_CODE_PAGE 54936

int tdx_decode_gb18030(const uint8_t *data, size_t size, char *out, size_t out_size,
                       size_t *out_length, tdx_error *err) {
    int wide_len;
    int utf8_len;
    wchar_t *wide;

    if (out_length)
        *out_length = 0;
    if (out_size == 0) {
        tdx_error_set(err, "output buffer is empty");
        return TDX_ERR;
    }
    out[0] = '\0';
    if (size == 0)
        return TDX_OK;
    if (size > (size_t)INT_MAX) {
        tdx_error_set(err, "label is too long to convert");
        return TDX_ERR;
    }

    wide_len = MultiByteToWideChar(TDX_GB18030_CODE_PAGE, 0,
                                   (const char *)data, (int)size, NULL, 0);
    if (wide_len <= 0) {
        tdx_error_set(err, "cannot decode the GB18030 label");
        return TDX_ERR;
    }
    wide = (wchar_t *)malloc((size_t)wide_len * sizeof(wchar_t));
    if (!wide) {
        tdx_error_set(err, "out of memory decoding the GB18030 label");
        return TDX_ERR;
    }
    MultiByteToWideChar(TDX_GB18030_CODE_PAGE, 0, (const char *)data, (int)size,
                        wide, wide_len);
    utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, NULL, 0, NULL, NULL);
    if (utf8_len <= 0) {
        free(wide);
        tdx_error_set(err, "cannot re-encode the label as UTF-8");
        return TDX_ERR;
    }
    if ((size_t)utf8_len + 1 > out_size) {
        free(wide);
        tdx_error_set(err, "label needs %d bytes but only %zu are available",
                      utf8_len, out_size);
        return TDX_ERR;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, out, utf8_len, NULL, NULL);
    out[utf8_len] = '\0';
    free(wide);
    if (out_length)
        *out_length = (size_t)utf8_len;
    return TDX_OK;
}

#else /* !_WIN32 */

#include <iconv.h>

int tdx_decode_gb18030(const uint8_t *data, size_t size, char *out, size_t out_size,
                       size_t *out_length, tdx_error *err) {
    iconv_t converter;
    char *in_ptr;
    char *out_ptr;
    size_t in_left;
    size_t out_left;

    if (out_size == 0) {
        tdx_error_set(err, "output buffer is empty");
        return TDX_ERR;
    }
    out[0] = '\0';
    if (size == 0)
        return TDX_OK;
    converter = iconv_open("UTF-8", "GB18030");
    if (converter == (iconv_t)-1) {
        tdx_error_set(err, "iconv cannot open GB18030");
        return TDX_ERR;
    }
    in_ptr = (char *)(uintptr_t)data;
    out_ptr = out;
    in_left = size;
    out_left = out_size - 1;
    if (iconv(converter, &in_ptr, &in_left, &out_ptr, &out_left) == (size_t)-1) {
        iconv_close(converter);
        tdx_error_set(err, "cannot decode the GB18030 label");
        return TDX_ERR;
    }
    *out_ptr = '\0';
    iconv_close(converter);
    if (out_length)
        *out_length = (size_t)(out_ptr - out);
    return TDX_OK;
}

#endif /* _WIN32 */
