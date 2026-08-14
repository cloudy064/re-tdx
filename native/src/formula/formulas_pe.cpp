#include "formulas_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>

namespace tdx::formulas_detail {

PEImage::PEImage(Bytes bytes) : data_(std::move(bytes)) {
    if (data_.size() < 0x40 || data_[0] != 'M' || data_[1] != 'Z')
        throw Error("not a DOS/PE image");
    const auto pe_offset = u32_at(0x3C);
    if (slice(pe_offset, 4) != Bytes{'P', 'E', 0, 0})
        throw Error("PE signature not found");
    const std::size_t coff = pe_offset + 4;
    const auto number_of_sections = u16_at(coff + 2);
    const auto optional_size = u16_at(coff + 16);
    const std::size_t optional = coff + 20;
    const auto magic = u16_at(optional);
    std::size_t data_directories = 0;
    std::uint32_t directory_count = 0;
    if (magic == 0x10B) {
        image_base_ = u32_at(optional + 28);
        directory_count = u32_at(optional + 92);
        data_directories = optional + 96;
    } else if (magic == 0x20B) {
        image_base_ = u64_at(optional + 24);
        directory_count = u32_at(optional + 108);
        data_directories = optional + 112;
    } else {
        throw Error("unsupported PE optional-header magic");
    }
    if (directory_count > 2 &&
        data_directories + 24 <= optional + optional_size) {
        resource_rva_ = u32_at(data_directories + 16);
        resource_size_ = u32_at(data_directories + 20);
    }
    size_of_headers_ = u32_at(optional + 60);
    const std::size_t section_offset = optional + optional_size;
    for (std::uint16_t index = 0; index < number_of_sections; ++index) {
        const auto offset = section_offset + static_cast<std::size_t>(index) * 40;
        const auto header = slice(offset, 40);
        const auto zero = std::find(header.begin(), header.begin() + 8, 0);
        const std::string name(header.begin(), zero);
        sections_.push_back(Section{
            name, read_u32_le(header.data() + 12),
            read_u32_le(header.data() + 8), read_u32_le(header.data() + 20),
            read_u32_le(header.data() + 16),
        });
    }
}

Bytes PEImage::read_rva(std::uint32_t rva, std::size_t size) const {
    return slice(rva_to_offset(rva, size), size);
}

std::uint16_t PEImage::u16(std::uint32_t rva) const {
    const auto value = read_rva(rva, 2);
    return read_u16_le(value.data());
}

std::uint32_t PEImage::u32(std::uint32_t rva) const {
    const auto value = read_rva(rva, 4);
    return read_u32_le(value.data());
}

std::string PEImage::decode_va_cstring(std::uint32_t va,
                                       std::size_t maximum) const {
    if (!va) return {};
    if (va < image_base_ ||
        static_cast<std::uint64_t>(va) - image_base_ > UINT32_MAX)
        throw Error("formula source pointer is below the preferred PE image");
    const auto rva = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(va) - image_base_);
    Bytes encoded;
    encoded.reserve(512);
    for (std::size_t index = 0; index < maximum; ++index) {
        if (static_cast<std::uint64_t>(rva) + index > UINT32_MAX)
            throw Error("formula source pointer exceeds PE address space");
        const auto current = read_rva(
            rva + static_cast<std::uint32_t>(index), 1).front();
        if (!current) return decode_gbk(encoded);
        encoded.push_back(current);
    }
    throw Error("formula source text exceeds the safe size limit");
}

std::size_t PEImage::rva_to_offset(std::uint32_t rva,
                                   std::size_t size) const {
    if (rva < size_of_headers_) {
        slice(rva, size);
        return rva;
    }
    for (const auto& section : sections_) {
        const auto span = std::max(section.virtual_size, section.raw_size);
        if (rva < section.virtual_address ||
            static_cast<std::uint64_t>(rva) >=
                static_cast<std::uint64_t>(section.virtual_address) + span)
            continue;
        const auto relative = rva - section.virtual_address;
        if (static_cast<std::uint64_t>(relative) + size > section.raw_size)
            throw Error("RVA points into unbacked section bytes: " + section.name);
        const auto offset = static_cast<std::size_t>(section.raw_offset) + relative;
        slice(offset, size);
        return offset;
    }
    throw Error("unmapped PE RVA: " + std::to_string(rva));
}

Bytes PEImage::read_resource(std::uint32_t type, std::uint32_t id) const {
    if (!resource_rva_ || resource_size_ < 16)
        throw Error("PE image has no resource directory");
    const auto type_entry = numeric_resource_entry(0, type);
    if (!type_entry.directory)
        throw Error("PE resource type entry is not a directory");
    const auto id_entry = numeric_resource_entry(type_entry.offset, id);
    if (!id_entry.directory)
        throw Error("PE resource ID entry is not a language directory");
    const auto language_entry = first_resource_entry(id_entry.offset);
    if (language_entry.directory)
        throw Error("PE resource language entry is unexpectedly a directory");
    if (language_entry.offset > resource_size_ ||
        16 > resource_size_ - language_entry.offset)
        throw Error("PE resource data entry is outside the resource directory");
    const auto descriptor = read_rva(
        resource_rva_ + language_entry.offset, 16);
    const auto data_rva = read_u32_le(descriptor.data());
    const auto size = read_u32_le(descriptor.data() + 4);
    if (!size || size > 2 * 1024 * 1024)
        throw Error("PE resource payload exceeds the safe size limit");
    return read_rva(data_rva, size);
}

PEImage::ResourceEntry PEImage::resource_entry_at(
    std::uint32_t directory_offset, std::size_t index) const {
    if (directory_offset > resource_size_ ||
        16 > resource_size_ - directory_offset)
        throw Error("PE resource directory is out of bounds");
    const auto directory = read_rva(resource_rva_ + directory_offset, 16);
    const auto named = read_u16_le(directory.data() + 12);
    const auto ids = read_u16_le(directory.data() + 14);
    const auto count = static_cast<std::size_t>(named) + ids;
    if (count > 4096 || index >= count)
        throw Error("PE resource directory entry is out of bounds");
    const auto relative = static_cast<std::uint64_t>(directory_offset) +
        16 + index * 8;
    if (relative > resource_size_ || 8 > resource_size_ - relative)
        throw Error("PE resource directory table is out of bounds");
    const auto entry = read_rva(
        resource_rva_ + static_cast<std::uint32_t>(relative), 8);
    const auto target = read_u32_le(entry.data() + 4);
    return ResourceEntry{
        (target & 0x80000000U) != 0, target & 0x7FFFFFFFU};
}

PEImage::ResourceEntry PEImage::numeric_resource_entry(
    std::uint32_t directory_offset, std::uint32_t wanted) const {
    if (directory_offset > resource_size_ ||
        16 > resource_size_ - directory_offset)
        throw Error("PE resource directory is out of bounds");
    const auto directory = read_rva(resource_rva_ + directory_offset, 16);
    const auto named = read_u16_le(directory.data() + 12);
    const auto ids = read_u16_le(directory.data() + 14);
    if (static_cast<std::size_t>(named) + ids > 4096)
        throw Error("PE resource directory contains too many entries");
    for (std::size_t index = named;
         index < static_cast<std::size_t>(named) + ids; ++index) {
        const auto relative = static_cast<std::uint64_t>(directory_offset) +
            16 + index * 8;
        if (relative > resource_size_ || 8 > resource_size_ - relative)
            throw Error("PE resource directory table is out of bounds");
        const auto entry = read_rva(
            resource_rva_ + static_cast<std::uint32_t>(relative), 8);
        const auto name = read_u32_le(entry.data());
        if ((name & 0x80000000U) == 0 && name == wanted)
            return resource_entry_at(directory_offset, index);
    }
    throw Error("PE resource not found: type/id " + std::to_string(wanted));
}

PEImage::ResourceEntry PEImage::first_resource_entry(
    std::uint32_t directory_offset) const {
    return resource_entry_at(directory_offset, 0);
}

Bytes PEImage::slice(std::size_t offset, std::size_t size) const {
    if (offset > data_.size() || size > data_.size() - offset)
        throw Error("PE file range is out of bounds");
    return Bytes(data_.begin() + static_cast<std::ptrdiff_t>(offset),
                 data_.begin() + static_cast<std::ptrdiff_t>(offset + size));
}

std::uint16_t PEImage::u16_at(std::size_t offset) const {
    const auto value = slice(offset, 2);
    return read_u16_le(value.data());
}

std::uint32_t PEImage::u32_at(std::size_t offset) const {
    const auto value = slice(offset, 4);
    return read_u32_le(value.data());
}

std::uint64_t PEImage::u64_at(std::size_t offset) const {
    const auto value = slice(offset, 8);
    return static_cast<std::uint64_t>(read_u32_le(value.data())) |
           (static_cast<std::uint64_t>(read_u32_le(value.data() + 4)) << 32);
}

std::string validate_formula_dll(const std::filesystem::path& dll) {
    if (!std::filesystem::is_regular_file(dll))
        throw Error("TCalc.dll does not exist: " + path_utf8(dll));
    const auto digest = lower_ascii(sha256_file(dll));
    const auto& profile = supported_profile();
    if (digest != profile.sha256)
        throw Error("unsupported TCalc.dll SHA-256 " + digest +
                    "; known profile: " + profile.name);
    return digest;
}

}  // namespace tdx::formulas_detail
