#include "stats_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace tdx {

using namespace stats_detail;

namespace {

void append_u32(Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint16_t u16_at(const Bytes& data, std::size_t offset, std::string_view context) {
    if (offset > data.size() || data.size() - offset < 2)
        throw Error(std::string(context) + " exceeds archive bounds");
    return read_u16_le(data.data() + offset);
}

std::uint32_t u32_at(const Bytes& data, std::size_t offset, std::string_view context) {
    if (offset > data.size() || data.size() - offset < 4)
        throw Error(std::string(context) + " exceeds archive bounds");
    return read_u32_le(data.data() + offset);
}


struct ZipEntry {
    std::string name;
    std::uint16_t flags{};
    std::uint16_t method{};
    std::uint32_t crc{};
    std::uint32_t compressed_size{};
    std::uint32_t uncompressed_size{};
    std::uint32_t local_offset{};
};

std::vector<ZipEntry> zip_entries(const Bytes& archive) {
    if (archive.size() < 22) throw Error("statistics resource is not a valid ZIP");
    const std::size_t search_start = archive.size() > 65557 ? archive.size() - 65557 : 0;
    std::optional<std::size_t> eocd;
    for (std::size_t position = archive.size() - 22;; --position) {
        if (u32_at(archive, position, "ZIP EOCD") == 0x06054B50) {
            eocd = position;
            break;
        }
        if (position == search_start) break;
    }
    if (!eocd) throw Error("statistics ZIP has no end-of-central-directory record");
    const auto position = *eocd;
    if (u16_at(archive, position + 4, "ZIP disk") != 0 ||
        u16_at(archive, position + 6, "ZIP central disk") != 0)
        throw Error("multi-disk statistics ZIP is unsupported");
    const auto entries_on_disk = u16_at(archive, position + 8, "ZIP entry count");
    const auto entry_count = u16_at(archive, position + 10, "ZIP entry count");
    if (entries_on_disk != entry_count || entry_count > maximum_archive_entries)
        throw Error("statistics ZIP entry count is invalid");
    const auto central_size = u32_at(archive, position + 12, "ZIP central size");
    const auto central_offset = u32_at(archive, position + 16, "ZIP central offset");
    const auto comment_size = u16_at(archive, position + 20, "ZIP comment size");
    if (position + 22 + comment_size != archive.size())
        throw Error("statistics ZIP EOCD length mismatch");
    if (central_offset > archive.size() || central_size > archive.size() - central_offset ||
        static_cast<std::size_t>(central_offset) + central_size > position)
        throw Error("statistics ZIP central directory exceeds archive bounds");
    std::vector<ZipEntry> result;
    result.reserve(entry_count);
    std::size_t cursor = central_offset;
    std::size_t total_uncompressed = 0;
    for (std::size_t index = 0; index < entry_count; ++index) {
        if (u32_at(archive, cursor, "ZIP central header") != 0x02014B50 ||
            archive.size() - cursor < 46)
            throw Error("statistics ZIP central header is invalid");
        ZipEntry entry;
        entry.flags = u16_at(archive, cursor + 8, "ZIP flags");
        entry.method = u16_at(archive, cursor + 10, "ZIP compression method");
        entry.crc = u32_at(archive, cursor + 16, "ZIP CRC");
        entry.compressed_size = u32_at(archive, cursor + 20, "ZIP compressed size");
        entry.uncompressed_size = u32_at(archive, cursor + 24, "ZIP uncompressed size");
        const auto name_size = u16_at(archive, cursor + 28, "ZIP name size");
        const auto extra_size = u16_at(archive, cursor + 30, "ZIP extra size");
        const auto entry_comment_size = u16_at(archive, cursor + 32, "ZIP comment size");
        const auto disk = u16_at(archive, cursor + 34, "ZIP entry disk");
        entry.local_offset = u32_at(archive, cursor + 42, "ZIP local offset");
        const std::size_t record_size = 46 + static_cast<std::size_t>(name_size) +
                                        extra_size + entry_comment_size;
        if (record_size > archive.size() - cursor)
            throw Error("statistics ZIP central entry exceeds archive bounds");
        entry.name.assign(reinterpret_cast<const char*>(archive.data() + cursor + 46), name_size);
        if (disk != 0 || (entry.flags & 0x1))
            throw Error("statistics ZIP contains a split or encrypted entry: " + entry.name);
        if (entry.method != 0 && entry.method != 8)
            throw Error("statistics ZIP uses an unsupported compression method");
        if (entry.uncompressed_size > maximum_entry_bytes ||
            total_uncompressed > maximum_uncompressed_bytes - entry.uncompressed_size)
            throw Error("statistics ZIP expands beyond the safety limit");
        total_uncompressed += entry.uncompressed_size;
        result.push_back(std::move(entry));
        cursor += record_size;
    }
    if (cursor != static_cast<std::size_t>(central_offset) + central_size)
        throw Error("statistics ZIP central directory size mismatch");
    return result;
}

Bytes extract_zip_entry(const Bytes& archive, const ZipEntry& entry) {
    const std::size_t local = entry.local_offset;
    if (archive.size() - std::min(local, archive.size()) < 30 ||
        u32_at(archive, local, "ZIP local header") != 0x04034B50)
        throw Error("statistics ZIP local header is invalid: " + entry.name);
    const auto name_size = u16_at(archive, local + 26, "ZIP local name size");
    const auto extra_size = u16_at(archive, local + 28, "ZIP local extra size");
    const std::size_t data_offset = local + 30 + static_cast<std::size_t>(name_size) + extra_size;
    if (data_offset > archive.size() || entry.compressed_size > archive.size() - data_offset)
        throw Error("statistics ZIP entry exceeds archive bounds: " + entry.name);
    Bytes output;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size)
            throw Error("stored statistics ZIP entry has mismatched sizes");
        output.insert(output.end(), archive.begin() + static_cast<std::ptrdiff_t>(data_offset),
                      archive.begin() + static_cast<std::ptrdiff_t>(data_offset + entry.compressed_size));
    } else {
        if (entry.uncompressed_size == 0) {
            output.clear();
        } else {
            output.resize(entry.uncompressed_size);
            z_stream stream{};
            stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(
                archive.data() + data_offset));
            stream.avail_in = entry.compressed_size;
            stream.next_out = reinterpret_cast<Bytef*>(output.data());
            stream.avail_out = entry.uncompressed_size;
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                throw Error("cannot initialize ZIP deflate decoder");
            const int status = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
            if (status != Z_STREAM_END || stream.total_out != entry.uncompressed_size ||
                stream.total_in != entry.compressed_size)
                throw Error("statistics ZIP deflate stream is invalid: " + entry.name);
        }
    }
    uLong crc = crc32(0L, Z_NULL, 0);
    if (!output.empty())
        crc = crc32(crc, reinterpret_cast<const Bytef*>(output.data()),
                    static_cast<uInt>(output.size()));
    if (static_cast<std::uint32_t>(crc) != entry.crc)
        throw Error("statistics ZIP CRC mismatch: " + entry.name);
    return output;
}

}  // namespace

Bytes build_stats_file_request_data(std::string_view raw_path,
                                    std::uint32_t offset,
                                    std::uint32_t size) {
    auto path = trim(std::string(raw_path));
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path.find('\0') != std::string::npos)
        throw Error("statistics resource path is empty or contains NUL");
    if (path.size() > file_path_size ||
        !std::all_of(path.begin(), path.end(), [](unsigned char ch) {
            return ch >= 0x20 && ch <= 0x7E;
        })) throw Error("statistics resource path must be at most 300 ASCII bytes");
    if (size < 1 || size > maximum_chunk_size)
        throw Error("statistics chunk size must be in 1..60000");
    Bytes result;
    result.reserve(8 + file_path_size);
    append_u32(result, offset);
    append_u32(result, size);
    result.insert(result.end(), path.begin(), path.end());
    result.resize(8 + file_path_size, 0);
    return result;
}

Bytes parse_stats_file_chunk(const Bytes& payload, std::uint32_t request_size) {
    if (request_size < 1 || request_size > maximum_chunk_size)
        throw Error("statistics request size must be in 1..60000");
    if (payload.size() < 4) throw Error("0x06B9 response is shorter than four bytes");
    const auto length = read_u32_le(payload.data());
    if (length > request_size) throw Error("0x06B9 chunk exceeds requested size");
    if (payload.size() != 4 + static_cast<std::size_t>(length))
        throw Error("0x06B9 response length does not match its declaration");
    return Bytes(payload.begin() + 4, payload.end());
}


TdxStatsResource parse_stats_archive(const Bytes& payload, std::string source_path) {
    if (payload.empty()) throw Error("statistics ZIP is empty");
    if (payload.size() > maximum_archive_bytes)
        throw Error("statistics ZIP exceeds the 32 MiB safety limit");
    const auto entries = zip_entries(payload);
    std::map<std::string, const ZipEntry*> required;
    const ZipEntry* tip_info = nullptr;
    for (const auto& entry : entries)
        if (entry.name == "tdxstat.cfg" || entry.name == "tdxstat2.cfg") {
            if (!required.emplace(entry.name, &entry).second)
                throw Error("statistics ZIP repeats required member: " + entry.name);
        } else if (entry.name == "tipinfo.dat") {
            if (tip_info) throw Error("statistics ZIP repeats tipinfo.dat");
            tip_info = &entry;
        }
    for (const auto* name : {"tdxstat.cfg", "tdxstat2.cfg"})
        if (!required.count(name)) throw Error(std::string("statistics ZIP is missing ") + name);
    const auto tip_payload = tip_info
        ? std::optional<Bytes>(extract_zip_entry(payload, *tip_info)) : std::nullopt;
    return parse_stats_files(extract_zip_entry(payload, *required.at("tdxstat.cfg")),
                             extract_zip_entry(payload, *required.at("tdxstat2.cfg")),
                             std::move(source_path),
                             tip_payload ? &*tip_payload : nullptr);
}


}  // namespace tdx
