#include "pe_export_reader.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::pe_detail {
namespace {

constexpr std::uintmax_t maximum_pe_file_size = 128U * 1024U * 1024U;
constexpr std::uint32_t maximum_export_functions = 1'000'000;
constexpr std::uint32_t maximum_named_exports = 65'536;
constexpr std::size_t maximum_export_name_size = 1'024;

struct Section {
    std::uint32_t virtual_size{};
    std::uint32_t virtual_address{};
    std::uint32_t raw_size{};
    std::uint32_t raw_offset{};
};

class PeImage {
public:
    explicit PeImage(Bytes bytes) : bytes_(std::move(bytes)) {
        if (bytes_.size() < 0x40 || bytes_[0] != 'M' || bytes_[1] != 'Z')
            throw Error("not a DOS/PE image");

        const auto pe_offset = static_cast<std::size_t>(u32_at(0x3C));
        require_range(pe_offset, 24, "PE/COFF header");
        if (bytes_[pe_offset] != 'P' || bytes_[pe_offset + 1] != 'E' ||
            bytes_[pe_offset + 2] != 0 || bytes_[pe_offset + 3] != 0)
            throw Error("PE signature not found");

        const std::size_t coff = pe_offset + 4;
        const auto section_count = u16_at(coff + 2);
        const auto optional_size = static_cast<std::size_t>(u16_at(coff + 16));
        if (section_count > 96)
            throw Error("PE section count exceeds the safe limit");

        const std::size_t optional = coff + 20;
        require_range(optional, optional_size, "PE optional header");
        if (optional_size < 2)
            throw Error("PE optional header is truncated");
        const auto magic = u16_at(optional);

        std::size_t directory_count_offset{};
        std::size_t directories_offset{};
        if (magic == 0x10B) {
            directory_count_offset = 92;
            directories_offset = 96;
        } else if (magic == 0x20B) {
            directory_count_offset = 108;
            directories_offset = 112;
        } else {
            throw Error("unsupported PE optional-header magic");
        }
        require_optional_range(optional_size, 60, 4,
                               "PE SizeOfHeaders");
        size_of_headers_ = u32_at(optional + 60);
        if (!size_of_headers_ || size_of_headers_ > bytes_.size())
            throw Error("PE SizeOfHeaders is outside the file");

        require_optional_range(optional_size, directory_count_offset, 4,
                               "PE data-directory count");
        const auto directory_count =
            u32_at(optional + directory_count_offset);
        if (directory_count != 0) {
            require_optional_range(optional_size, directories_offset, 8,
                                   "PE export data directory");
            export_rva_ = u32_at(optional + directories_offset);
            export_size_ = u32_at(optional + directories_offset + 4);
        }

        const std::size_t section_table = optional + optional_size;
        if (section_count >
            (std::numeric_limits<std::size_t>::max() - section_table) / 40)
            throw Error("PE section table size overflows");
        require_range(section_table, static_cast<std::size_t>(section_count) * 40,
                      "PE section table");
        sections_.reserve(section_count);
        for (std::uint16_t index = 0; index < section_count; ++index) {
            const auto offset =
                section_table + static_cast<std::size_t>(index) * 40;
            sections_.push_back(Section{
                u32_at(offset + 8), u32_at(offset + 12),
                u32_at(offset + 16), u32_at(offset + 20)});
        }
    }

    std::vector<std::string> export_names() const {
        if (!export_rva_ && !export_size_) return {};
        if (!export_rva_ || export_size_ < 40)
            throw Error("PE export directory is invalid");

        const auto directory = rva_bytes(export_rva_, 40,
                                         "PE export directory");
        const auto function_count = u32(directory, 20);
        const auto name_count = u32(directory, 24);
        const auto functions_rva = u32(directory, 28);
        const auto names_rva = u32(directory, 32);
        const auto ordinals_rva = u32(directory, 36);
        if (function_count > maximum_export_functions)
            throw Error("PE export function count exceeds the safe limit");
        if (name_count > maximum_named_exports)
            throw Error("PE named-export count exceeds the safe limit");
        if (name_count && (!function_count || !functions_rva || !names_rva ||
                           !ordinals_rva))
            throw Error("PE named-export tables are incomplete");

        checked_table(functions_rva, function_count, 4,
                      "PE export address table");
        checked_table(names_rva, name_count, 4,
                      "PE export name-pointer table");
        checked_table(ordinals_rva, name_count, 2,
                      "PE export ordinal table");

        std::set<std::string> unique;
        for (std::uint32_t index = 0; index < name_count; ++index) {
            const auto ordinal = u16_rva(
                add_rva(ordinals_rva, static_cast<std::uint64_t>(index) * 2,
                        "PE export ordinal"),
                "PE export ordinal");
            if (ordinal >= function_count)
                throw Error("PE export ordinal exceeds the function table");
            const auto function_rva = u32_rva(
                add_rva(functions_rva,
                        static_cast<std::uint64_t>(ordinal) * 4,
                        "PE export function"),
                "PE export function");
            if (!function_rva) continue;

            const auto name_rva = u32_rva(
                add_rva(names_rva, static_cast<std::uint64_t>(index) * 4,
                        "PE export name pointer"),
                "PE export name pointer");
            if (!name_rva)
                throw Error("PE export name pointer is null");
            unique.insert(ascii_cstring(name_rva));
        }
        return {unique.begin(), unique.end()};
    }

private:
    void require_range(std::size_t offset, std::size_t size,
                       const char* label) const {
        if (offset > bytes_.size() || size > bytes_.size() - offset)
            throw Error(std::string(label) + " is outside the PE file");
    }

    static void require_optional_range(std::size_t optional_size,
                                       std::size_t offset, std::size_t size,
                                       const char* label) {
        if (offset > optional_size || size > optional_size - offset)
            throw Error(std::string(label) + " is outside the optional header");
    }

    std::uint16_t u16_at(std::size_t offset) const {
        require_range(offset, 2, "PE uint16");
        return read_u16_le(bytes_.data() + offset);
    }

    std::uint32_t u32_at(std::size_t offset) const {
        require_range(offset, 4, "PE uint32");
        return read_u32_le(bytes_.data() + offset);
    }

    static std::uint32_t u32(const Bytes& bytes, std::size_t offset) {
        return read_u32_le(bytes.data() + offset);
    }

    std::size_t rva_to_offset(std::uint32_t rva, std::size_t size,
                              const char* label) const {
        if (rva < size_of_headers_) {
            const auto header_remaining =
                static_cast<std::uint64_t>(size_of_headers_) - rva;
            if (size > header_remaining)
                throw Error(std::string(label) +
                            " crosses the PE header boundary");
            const auto offset = static_cast<std::size_t>(rva);
            require_range(offset, size, label);
            return offset;
        }

        for (const auto& section : sections_) {
            const auto span = std::max(section.virtual_size, section.raw_size);
            if (rva < section.virtual_address) continue;
            const auto relative =
                static_cast<std::uint64_t>(rva) - section.virtual_address;
            if (relative >= span) continue;
            if (relative > section.raw_size ||
                size > static_cast<std::uint64_t>(section.raw_size) - relative)
                throw Error(std::string(label) +
                            " points into unbacked PE section bytes");
            const auto file_offset =
                static_cast<std::uint64_t>(section.raw_offset) + relative;
            if (file_offset > std::numeric_limits<std::size_t>::max())
                throw Error(std::string(label) + " file offset overflows");
            const auto offset = static_cast<std::size_t>(file_offset);
            require_range(offset, size, label);
            return offset;
        }
        throw Error(std::string(label) + " has an unmapped PE RVA");
    }

    Bytes rva_bytes(std::uint32_t rva, std::size_t size,
                    const char* label) const {
        const auto offset = rva_to_offset(rva, size, label);
        return Bytes(bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
                     bytes_.begin() + static_cast<std::ptrdiff_t>(offset + size));
    }

    std::uint16_t u16_rva(std::uint32_t rva, const char* label) const {
        const auto offset = rva_to_offset(rva, 2, label);
        return read_u16_le(bytes_.data() + offset);
    }

    std::uint32_t u32_rva(std::uint32_t rva, const char* label) const {
        const auto offset = rva_to_offset(rva, 4, label);
        return read_u32_le(bytes_.data() + offset);
    }

    static std::uint32_t add_rva(std::uint32_t base, std::uint64_t addend,
                                 const char* label) {
        const auto value = static_cast<std::uint64_t>(base) + addend;
        if (value > std::numeric_limits<std::uint32_t>::max())
            throw Error(std::string(label) + " RVA overflows");
        return static_cast<std::uint32_t>(value);
    }

    void checked_table(std::uint32_t rva, std::uint32_t count,
                       std::size_t width, const char* label) const {
        if (!count) return;
        if (count > std::numeric_limits<std::size_t>::max() / width)
            throw Error(std::string(label) + " size overflows");
        (void)rva_to_offset(rva, static_cast<std::size_t>(count) * width,
                            label);
    }

    std::string ascii_cstring(std::uint32_t rva) const {
        std::string result;
        result.reserve(64);
        for (std::size_t index = 0; index < maximum_export_name_size; ++index) {
            const auto current_rva = add_rva(
                rva, index, "PE export name");
            const auto offset = rva_to_offset(current_rva, 1,
                                              "PE export name");
            const auto byte = bytes_[offset];
            if (!byte) {
                if (result.empty())
                    throw Error("PE export name is empty");
                return result;
            }
            if (byte < 0x20 || byte > 0x7E)
                throw Error("PE export name is not printable ASCII");
            result.push_back(static_cast<char>(byte));
        }
        throw Error("PE export name exceeds the safe size limit");
    }

    Bytes bytes_;
    std::uint32_t size_of_headers_{};
    std::uint32_t export_rva_{};
    std::uint32_t export_size_{};
    std::vector<Section> sections_;
};

}  // namespace

std::vector<std::string> read_pe_export_names(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) throw Error("cannot inspect PE file size");
    if (size > maximum_pe_file_size)
        throw Error("PE file exceeds the 128 MiB offline safety limit");
    return PeImage(read_bytes(path)).export_names();
}

}  // namespace tdx::pe_detail
