/* tdx_zip.h - the small part of ZIP the finance packages need.
 *
 * Not a general ZIP library: the packages here hold a handful of members, are never
 * encrypted, never split across disks, and use only the stored and deflate methods.  So
 * this reads the end-of-central-directory record, walks the central directory, and
 * extracts one member - and REFUSES anything outside that shape rather than guessing.
 *
 * The CRC is checked, not assumed.  A deflate stream that decompresses to the right
 * length but the wrong bytes is exactly the failure that would otherwise reach a caller
 * as plausible numbers, and zlib already provides the checksum.
 *
 * Every offset is bounds-checked against the archive before it is read, because the
 * archive is a file from outside this process. */
#ifndef TDX_ZIP_H
#define TDX_ZIP_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_ZIP_ENTRY_NAME_MAX 128
#define TDX_ZIP_ENTRIES_MAX 16
/* A member larger than this is refused before any allocation happens. */
#define TDX_ZIP_MEMBER_MAX (256u * 1024u * 1024u)

typedef struct tdx_zip_entry {
    char name[TDX_ZIP_ENTRY_NAME_MAX];
    uint16_t method; /* 0 stored, 8 deflate */
    uint32_t crc;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_offset;
} tdx_zip_entry;

/* Reads the central directory.  Refuses a split archive, an encrypted entry, an
 * unsupported method, more members than the caller can hold, or a member larger than
 * TDX_ZIP_MEMBER_MAX. */
int tdx_zip_entries(const uint8_t *archive, size_t size, tdx_zip_entry *out, size_t capacity,
                    size_t *out_count, tdx_error *err);

/* Finds a member by exact name; returns 0 when absent. */
int tdx_zip_find(const tdx_zip_entry *entries, size_t count, const char *name,
                 tdx_zip_entry *out);

/* Extracts one member into out, which is cleared first.  Verifies the CRC. */
int tdx_zip_extract(const uint8_t *archive, size_t size, const tdx_zip_entry *entry,
                    tdx_buf *out, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_ZIP_H */
