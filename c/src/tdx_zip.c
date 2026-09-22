/* tdx_zip.c - the small part of ZIP the finance packages need. */
#include "tdx_zip.h"

#include <string.h>

#include <zlib.h>

/* Signatures, little endian in the file. */
#define TDX_ZIP_EOCD 0x06054B50u
#define TDX_ZIP_CENTRAL 0x02014B50u
#define TDX_ZIP_LOCAL 0x04034B50u
/* The end-of-central-directory record is 22 bytes plus a comment of at most 65535. */
#define TDX_ZIP_EOCD_SEARCH 65557u

static uint16_t u16_at(const uint8_t *data, size_t offset) {
    return (uint16_t)((uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8));
}

static uint32_t u32_at(const uint8_t *data, size_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

int tdx_zip_entries(const uint8_t *archive, size_t size, tdx_zip_entry *out, size_t capacity,
                    size_t *out_count, tdx_error *err) {
    size_t search_start;
    size_t position;
    size_t eocd = 0;
    int found = 0;
    uint16_t entries_on_disk;
    uint16_t entry_count;
    uint32_t central_size;
    uint32_t central_offset;
    uint16_t comment_size;
    size_t cursor;
    size_t index;
    uint64_t total = 0;

    if (out_count)
        *out_count = 0;
    if (!archive || !out) {
        tdx_error_set(err, "reading a ZIP needs bytes and an output");
        return TDX_ERR;
    }
    if (size < 22) {
        tdx_error_set(err, "a %zu-byte file is too short to be a ZIP", size);
        return TDX_ERR;
    }
    /* The record is found from the end, within the span a comment could occupy. */
    search_start = size > TDX_ZIP_EOCD_SEARCH ? size - TDX_ZIP_EOCD_SEARCH : 0;
    for (position = size - 22;; --position) {
        if (u32_at(archive, position) == TDX_ZIP_EOCD) {
            eocd = position;
            found = 1;
            break;
        }
        if (position == search_start)
            break;
    }
    if (!found) {
        tdx_error_set(err, "the archive has no end-of-central-directory record");
        return TDX_ERR;
    }
    if (u16_at(archive, eocd + 4) != 0 || u16_at(archive, eocd + 6) != 0) {
        tdx_error_set(err, "the archive is split across disks");
        return TDX_ERR;
    }
    entries_on_disk = u16_at(archive, eocd + 8);
    entry_count = u16_at(archive, eocd + 10);
    if (entries_on_disk != entry_count || entry_count == 0) {
        tdx_error_set(err, "the archive names %u entries on disk and %u in total",
                      entries_on_disk, entry_count);
        return TDX_ERR;
    }
    if (entry_count > capacity || entry_count > TDX_ZIP_ENTRIES_MAX) {
        tdx_error_set(err, "the archive holds %u entries, more than %zu", entry_count,
                      capacity < TDX_ZIP_ENTRIES_MAX ? capacity : TDX_ZIP_ENTRIES_MAX);
        return TDX_ERR;
    }
    central_size = u32_at(archive, eocd + 12);
    central_offset = u32_at(archive, eocd + 16);
    comment_size = u16_at(archive, eocd + 20);
    if (eocd + 22 + comment_size != size) {
        tdx_error_set(err, "the archive's comment size does not reach the end of the file");
        return TDX_ERR;
    }
    if ((size_t)central_offset > size ||
        (size_t)central_size > size - (size_t)central_offset ||
        (size_t)central_offset + central_size > eocd) {
        tdx_error_set(err, "the archive's central directory lies outside the file");
        return TDX_ERR;
    }

    cursor = central_offset;
    for (index = 0; index < entry_count; ++index) {
        tdx_zip_entry *entry = &out[index];
        uint16_t flags;
        uint16_t name_size;
        uint16_t extra_size;
        uint16_t entry_comment;
        uint16_t disk;
        size_t span;

        if (cursor > size || size - cursor < 46) {
            tdx_error_set(err, "central directory entry %zu is past the end of the file", index);
            return TDX_ERR;
        }
        if (u32_at(archive, cursor) != TDX_ZIP_CENTRAL) {
            tdx_error_set(err, "central directory entry %zu has the wrong signature", index);
            return TDX_ERR;
        }
        flags = u16_at(archive, cursor + 8);
        memset(entry, 0, sizeof(*entry));
        entry->method = u16_at(archive, cursor + 10);
        entry->crc = u32_at(archive, cursor + 16);
        entry->compressed_size = u32_at(archive, cursor + 20);
        entry->uncompressed_size = u32_at(archive, cursor + 24);
        name_size = u16_at(archive, cursor + 28);
        extra_size = u16_at(archive, cursor + 30);
        entry_comment = u16_at(archive, cursor + 32);
        disk = u16_at(archive, cursor + 34);
        entry->local_offset = u32_at(archive, cursor + 42);
        span = 46 + (size_t)name_size + extra_size + entry_comment;
        if (span > size - cursor) {
            tdx_error_set(err, "central directory entry %zu runs past the end of the file",
                          index);
            return TDX_ERR;
        }
        /* Each of these is a shape this reader does not implement, so it says so rather
         * than producing something that looks like data. */
        if (disk != 0) {
            tdx_error_set(err, "entry %zu lives on disk %u", index, disk);
            return TDX_ERR;
        }
        if (flags & 1u) {
            tdx_error_set(err, "entry %zu is encrypted", index);
            return TDX_ERR;
        }
        if (entry->method != 0 && entry->method != 8) {
            tdx_error_set(err, "entry %zu uses compression method %u", index, entry->method);
            return TDX_ERR;
        }
        if (name_size == 0 || name_size >= TDX_ZIP_ENTRY_NAME_MAX) {
            tdx_error_set(err, "entry %zu has a %u-byte name", index, name_size);
            return TDX_ERR;
        }
        memcpy(entry->name, archive + cursor + 46, name_size);
        entry->name[name_size] = '\0';
        if (entry->uncompressed_size > TDX_ZIP_MEMBER_MAX ||
            total + entry->uncompressed_size > TDX_ZIP_MEMBER_MAX) {
            tdx_error_set(err, "the archive expands beyond the %u-byte limit",
                          (unsigned)TDX_ZIP_MEMBER_MAX);
            return TDX_ERR;
        }
        total += entry->uncompressed_size;
        cursor += span;
    }
    if (cursor != (size_t)central_offset + central_size) {
        tdx_error_set(err, "the central directory does not end where its size says");
        return TDX_ERR;
    }
    if (out_count)
        *out_count = entry_count;
    return TDX_OK;
}

int tdx_zip_find(const tdx_zip_entry *entries, size_t count, const char *name,
                 tdx_zip_entry *out) {
    size_t index;
    if (!entries || !name || !out)
        return 0;
    for (index = 0; index < count; ++index)
        if (strcmp(entries[index].name, name) == 0) {
            *out = entries[index];
            return 1;
        }
    return 0;
}

int tdx_zip_extract(const uint8_t *archive, size_t size, const tdx_zip_entry *entry,
                    tdx_buf *out, tdx_error *err) {
    size_t local;
    uint16_t name_size;
    uint16_t extra_size;
    size_t data_offset;
    uLong crc;

    if (!archive || !entry || !out) {
        tdx_error_set(err, "extracting a ZIP member needs an archive, an entry and a buffer");
        return TDX_ERR;
    }
    local = entry->local_offset;
    if (local > size || size - local < 30) {
        tdx_error_set(err, "the local header of %s is past the end of the file", entry->name);
        return TDX_ERR;
    }
    if (u32_at(archive, local) != TDX_ZIP_LOCAL) {
        tdx_error_set(err, "the local header of %s has the wrong signature", entry->name);
        return TDX_ERR;
    }
    name_size = u16_at(archive, local + 26);
    extra_size = u16_at(archive, local + 28);
    data_offset = local + 30 + (size_t)name_size + extra_size;
    if (data_offset > size || (size_t)entry->compressed_size > size - data_offset) {
        tdx_error_set(err, "the data of %s lies outside the file", entry->name);
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    if (entry->method == 0) {
        if (entry->compressed_size != entry->uncompressed_size) {
            tdx_error_set(err, "stored member %s claims %u compressed and %u uncompressed bytes",
                          entry->name, entry->compressed_size, entry->uncompressed_size);
            return TDX_ERR;
        }
        if (entry->uncompressed_size &&
            tdx_buf_append(out, archive + data_offset, entry->uncompressed_size, err) != TDX_OK)
            return TDX_ERR;
    } else {
        z_stream stream;
        int status;
        if (entry->uncompressed_size == 0)
            return TDX_OK;
        if (tdx_buf_reserve(out, entry->uncompressed_size, err) != TDX_OK)
            return TDX_ERR;
        memset(&stream, 0, sizeof(stream));
        stream.next_in = (Bytef *)(uintptr_t)(const void *)(archive + data_offset);
        stream.avail_in = entry->compressed_size;
        stream.next_out = (Bytef *)out->data;
        stream.avail_out = (uInt)entry->uncompressed_size;
        /* Raw deflate: the ZIP member has no zlib wrapper of its own. */
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            tdx_error_set(err, "cannot start the deflate decoder for %s", entry->name);
            return TDX_ERR;
        }
        status = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (status != Z_STREAM_END || stream.total_in != entry->compressed_size ||
            stream.total_out != entry->uncompressed_size) {
            tdx_error_set(err, "the deflate stream of %s did not decode to its stated size",
                          entry->name);
            return TDX_ERR;
        }
        out->len = entry->uncompressed_size;
    }
    /* The checksum, because a stream that decodes to the right length but the wrong
     * bytes would otherwise reach the caller as plausible numbers. */
    crc = crc32(0L, Z_NULL, 0);
    if (out->len)
        crc = crc32(crc, (const Bytef *)out->data, (uInt)out->len);
    if ((uint32_t)crc != entry->crc) {
        tdx_error_set(err, "the CRC of %s is %08x, not the %08x the archive states",
                      entry->name, (unsigned)(uint32_t)crc, entry->crc);
        tdx_buf_clear(out);
        return TDX_ERR;
    }
    return TDX_OK;
}
