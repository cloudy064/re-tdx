#include "professional_data_internal.hpp"

#include <algorithm>
#include <cstdint>

#include <zlib.h>

namespace tdx::professional_data_detail {

std::vector<ZipEntry> zip_entries(const Bytes& archive) {
    if (archive.size() < 22) throw Error("professional finance archive is not a valid ZIP");
    const std::size_t search_start = archive.size() > 65557 ? archive.size() - 65557 : 0;
    std::optional<std::size_t> eocd;
    for (std::size_t position = archive.size() - 22;; --position) {
        if (u32_at(archive, position, "ZIP EOCD") == 0x06054B50) { eocd = position; break; }
        if (position == search_start) break;
    }
    if (!eocd) throw Error("professional finance ZIP has no EOCD");
    const auto position = *eocd;
    if (u16_at(archive, position + 4, "ZIP disk") != 0 ||
        u16_at(archive, position + 6, "ZIP central disk") != 0)
        throw Error("professional finance ZIP is split across disks");
    const auto entries_on_disk = u16_at(archive, position + 8, "ZIP entry count");
    const auto entry_count = u16_at(archive, position + 10, "ZIP entry count");
    if (entries_on_disk != entry_count || !entry_count || entry_count > 16)
        throw Error("professional finance ZIP entry count is invalid");
    const auto central_size = u32_at(archive, position + 12, "ZIP central size");
    const auto central_offset = u32_at(archive, position + 16, "ZIP central offset");
    const auto comment_size = u16_at(archive, position + 20, "ZIP comment size");
    if (position + 22 + comment_size != archive.size() ||
        central_offset > archive.size() || central_size > archive.size() - central_offset ||
        static_cast<std::size_t>(central_offset) + central_size > position)
        throw Error("professional finance ZIP central directory is invalid");
    std::vector<ZipEntry> result;
    std::size_t cursor = central_offset, total = 0;
    for (std::size_t index = 0; index < entry_count; ++index) {
        if (cursor > archive.size() || archive.size() - cursor < 46 ||
            u32_at(archive, cursor, "ZIP central header") != 0x02014B50)
            throw Error("professional finance ZIP central header is invalid");
        ZipEntry entry;
        entry.flags = u16_at(archive, cursor + 8, "ZIP flags");
        entry.method = u16_at(archive, cursor + 10, "ZIP method");
        entry.crc = u32_at(archive, cursor + 16, "ZIP CRC");
        entry.compressed_size = u32_at(archive, cursor + 20, "ZIP compressed size");
        entry.uncompressed_size = u32_at(archive, cursor + 24, "ZIP uncompressed size");
        const auto name_size = u16_at(archive, cursor + 28, "ZIP name size");
        const auto extra_size = u16_at(archive, cursor + 30, "ZIP extra size");
        const auto comment = u16_at(archive, cursor + 32, "ZIP comment size");
        const auto disk = u16_at(archive, cursor + 34, "ZIP disk");
        entry.local_offset = u32_at(archive, cursor + 42, "ZIP local offset");
        const std::size_t size = 46 + static_cast<std::size_t>(name_size) + extra_size + comment;
        if (size > archive.size() - cursor || disk || (entry.flags & 1) ||
            (entry.method != 0 && entry.method != 8))
            throw Error("professional finance ZIP contains an unsupported entry");
        entry.name.assign(reinterpret_cast<const char*>(archive.data() + cursor + 46), name_size);
        if (entry.uncompressed_size > maximum_finance_bytes ||
            total > maximum_finance_bytes - entry.uncompressed_size)
            throw Error("professional finance ZIP expands beyond safety limit");
        total += entry.uncompressed_size; result.push_back(std::move(entry)); cursor += size;
    }
    if (cursor != static_cast<std::size_t>(central_offset) + central_size)
        throw Error("professional finance ZIP central size mismatch");
    return result;
}

Bytes extract_zip_entry(const Bytes& archive, const ZipEntry& entry) {
    const auto local = static_cast<std::size_t>(entry.local_offset);
    if (local > archive.size() || archive.size() - local < 30 ||
        u32_at(archive, local, "ZIP local header") != 0x04034B50)
        throw Error("professional finance ZIP local header is invalid");
    const auto name_size = u16_at(archive, local + 26, "ZIP local name size");
    const auto extra_size = u16_at(archive, local + 28, "ZIP local extra size");
    const auto data_offset = local + 30 + static_cast<std::size_t>(name_size) + extra_size;
    if (data_offset > archive.size() || entry.compressed_size > archive.size() - data_offset)
        throw Error("professional finance ZIP entry exceeds bounds");
    Bytes result;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size)
            throw Error("stored professional finance ZIP entry has mismatched sizes");
        result.insert(result.end(), archive.begin() + static_cast<std::ptrdiff_t>(data_offset),
                      archive.begin() + static_cast<std::ptrdiff_t>(data_offset + entry.compressed_size));
    } else if (entry.uncompressed_size) {
        result.resize(entry.uncompressed_size);
        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(archive.data() + data_offset));
        stream.avail_in = entry.compressed_size;
        stream.next_out = reinterpret_cast<Bytef*>(result.data());
        stream.avail_out = entry.uncompressed_size;
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            throw Error("cannot initialize professional finance ZIP decoder");
        const int status = inflate(&stream, Z_FINISH); inflateEnd(&stream);
        if (status != Z_STREAM_END || stream.total_in != entry.compressed_size ||
            stream.total_out != entry.uncompressed_size)
            throw Error("professional finance ZIP deflate stream is invalid");
    }
    uLong crc = crc32(0L, Z_NULL, 0);
    if (!result.empty()) crc = crc32(crc, reinterpret_cast<const Bytef*>(result.data()),
                                    static_cast<uInt>(result.size()));
    if (static_cast<std::uint32_t>(crc) != entry.crc)
        throw Error("professional finance ZIP CRC mismatch");
    return result;
}

Bytes extract_finance_member(const Bytes& archive, const std::string& wanted) {
    const auto entries = zip_entries(archive);
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& item) {
        return item.name == wanted;
    });
    if (found == entries.end()) throw Error("professional finance ZIP is missing " + wanted);
    return extract_zip_entry(archive, *found);
}

}  // namespace tdx::professional_data_detail
