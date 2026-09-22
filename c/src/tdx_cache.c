#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "tdx_cache.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

tdx_cache_read_status tdx_cache_read(const char *path, tdx_buf *out, tdx_error *err) {
    unsigned char chunk[16384];
    FILE *file;
    size_t got;
    int failed = 0;
    if (!path || !out) {
        tdx_error_set(err, "reading a file needs a path and buffer");
        return TDX_CACHE_ERROR;
    }
    tdx_buf_clear(out);
    file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) return TDX_CACHE_MISSING;
        tdx_error_set(err, "cannot open %s: %s", path, strerror(errno));
        return TDX_CACHE_ERROR;
    }
    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (tdx_buf_append(out, chunk, got, err) != TDX_OK) {
            failed = 1;
            break;
        }
    }
    if (ferror(file)) {
        tdx_error_set(err, "cannot read %s: %s", path, strerror(errno));
        failed = 1;
    }
    if (fclose(file) != 0) {
        tdx_error_set(err, "cannot close %s: %s", path, strerror(errno));
        failed = 1;
    }
    if (failed) {
        tdx_buf_clear(out);
        return TDX_CACHE_ERROR;
    }
    return TDX_CACHE_FOUND;
}

static FILE *temporary_file(const char *path, char *name, size_t size) {
    int fd;
#ifdef _WIN32
    static volatile LONG sequence = 0;
    unsigned attempt;
    for (attempt = 0; attempt < 128; ++attempt) {
        unsigned long serial = (unsigned long)InterlockedIncrement(&sequence);
        if (snprintf(name, size, "%s.part.%lu.%lu", path,
                     (unsigned long)GetCurrentProcessId(), serial) >= (int)size) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        fd = _open(name, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY,
                   _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            FILE *file = _fdopen(fd, "wb");
            if (!file) { _close(fd); remove(name); }
            return file;
        }
        if (errno != EEXIST) return NULL;
    }
    errno = EEXIST;
    return NULL;
#else
    FILE *file;
    if (snprintf(name, size, "%s.part.XXXXXX", path) >= (int)size) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    fd = mkstemp(name);
    if (fd < 0) return NULL;
    file = fdopen(fd, "wb");
    if (!file) { close(fd); remove(name); }
    return file;
#endif
}

static int replace_file(const char *temporary, const char *path) {
#ifdef _WIN32
    unsigned attempt;
    for (attempt = 0; attempt < 50; ++attempt) {
        DWORD error;
        if (MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return 0;
        error = GetLastError();
        /* Another writer may briefly hold the directory entry while replacing it.
         * Retry only these sharing/permission outcomes; the old entry remains valid. */
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION &&
            error != ERROR_ACCESS_DENIED)
            return -1;
        Sleep(1);
    }
    return -1;
#else
    return rename(temporary, path);
#endif
}

int tdx_cache_write_with_ops(const char *path, const tdx_buf *data,
                             const tdx_cache_write_ops *ops, tdx_error *err) {
    char *temporary;
    FILE *file;
    size_t written, room;
    int closed, failed;
    if (!path || !data || (data->len && !data->data)) {
        tdx_error_set(err, "writing a cache entry needs a path and bytes");
        return TDX_ERR;
    }
    room = strlen(path) + 80;
    temporary = (char *)malloc(room);
    if (!temporary) {
        tdx_error_set(err, "out of memory for cache temporary path");
        return TDX_ERR;
    }
    file = temporary_file(path, temporary, room);
    if (!file) {
        tdx_error_set(err, "cannot create cache temporary for %s: %s", path, strerror(errno));
        free(temporary);
        return TDX_ERR;
    }
    written = ops && ops->write ? ops->write(ops->context, data->data, data->len, file)
                                 : fwrite(data->data, 1, data->len, file);
    failed = written != data->len || ferror(file);
    closed = ops && ops->close ? ops->close(ops->context, file) : fclose(file);
    if (failed || closed != 0) {
        tdx_error_set(err, "cannot write and close cache entry %s", path);
    } else if ((ops && ops->replace ? ops->replace(ops->context, temporary, path)
                                    : replace_file(temporary, path)) != 0) {
        tdx_error_set(err, "cannot replace cache entry %s", path);
        failed = 1;
    }
    if (failed || closed != 0) remove(temporary);
    free(temporary);
    return failed || closed != 0 ? TDX_ERR : TDX_OK;
}

int tdx_cache_write(const char *path, const tdx_buf *data, tdx_error *err) {
    return tdx_cache_write_with_ops(path, data, NULL, err);
}
