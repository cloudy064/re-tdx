#include "formulas_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <zlib.h>

namespace tdx::formulas_detail {
namespace {

void append_u32_be(Bytes& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    output.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void append_png_chunk(Bytes& output, const char type[4], const Bytes& payload) {
    append_u32_be(output, static_cast<std::uint32_t>(payload.size()));
    const auto begin = output.size();
    output.insert(output.end(), type, type + 4);
    output.insert(output.end(), payload.begin(), payload.end());
    const auto checksum = crc32(0L, output.data() + begin,
                                static_cast<uInt>(4 + payload.size()));
    append_u32_be(output, static_cast<std::uint32_t>(checksum));
}

}  // namespace

Bytes bitmap_dib_to_transparent_png(const Bytes& dib) {
    if (dib.size() < 40 || read_u32_le(dib.data()) != 40)
        throw Error("TCalc DRAWICON resource is not a BITMAPINFOHEADER DIB");
    const auto width = read_u32_le(dib.data() + 4);
    const auto height = read_u32_le(dib.data() + 8);
    const auto planes = read_u16_le(dib.data() + 12);
    const auto bits = read_u16_le(dib.data() + 14);
    const auto compression = read_u32_le(dib.data() + 16);
    if (width != 1800 || height != 18 || planes != 1 || bits != 24 ||
        compression != 0)
        throw Error("TCalc DRAWICON bitmap geometry/format mismatch");
    const std::size_t stride =
        ((static_cast<std::size_t>(width) * 3 + 3) / 4) * 4;
    constexpr std::size_t pixels_offset = 40;
    if (stride * height > dib.size() - pixels_offset)
        throw Error("TCalc DRAWICON bitmap pixels are truncated");

    Bytes scanlines;
    scanlines.reserve((static_cast<std::size_t>(width) * 4 + 1) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        scanlines.push_back(0);
        const auto source_y = height - 1 - y;
        const auto* row = dib.data() + pixels_offset + source_y * stride;
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto blue = row[x * 3];
            const auto green = row[x * 3 + 1];
            const auto red = row[x * 3 + 2];
            scanlines.push_back(red);
            scanlines.push_back(green);
            scanlines.push_back(blue);
            scanlines.push_back(
                red == 255 && green == 255 && blue == 255 ? 0 : 255);
        }
    }
    uLongf compressed_size = compressBound(
        static_cast<uLong>(scanlines.size()));
    Bytes compressed(compressed_size);
    const auto status = compress2(
        compressed.data(), &compressed_size, scanlines.data(),
        static_cast<uLong>(scanlines.size()), Z_BEST_COMPRESSION);
    if (status != Z_OK) throw Error("failed to compress DRAWICON PNG");
    compressed.resize(compressed_size);

    Bytes png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    Bytes header;
    append_u32_be(header, width);
    append_u32_be(header, height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    append_png_chunk(png, "IHDR", header);
    append_png_chunk(png, "IDAT", compressed);
    append_png_chunk(png, "IEND", {});
    return png;
}

Bytes bitmap_file_from_dib(const Bytes& dib) {
    Bytes bitmap;
    bitmap.reserve(14 + dib.size());
    bitmap.insert(bitmap.end(), {'B', 'M'});
    const auto append_le = [&](std::uint32_t value) {
        bitmap.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bitmap.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bitmap.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bitmap.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    };
    append_le(static_cast<std::uint32_t>(14 + dib.size()));
    append_le(0);
    append_le(54);
    bitmap.insert(bitmap.end(), dib.begin(), dib.end());
    return bitmap;
}

}  // namespace tdx::formulas_detail

namespace tdx {

FormulaIconSprite extract_formula_icon_sprite(
    const std::filesystem::path& dll) {
    using namespace formulas_detail;
    const auto digest = validate_formula_dll(dll);
    const auto& profile = supported_profile();
    PEImage image(read_bytes(dll));
    const auto dib = image.read_resource(2, 2060);
    auto bitmap = bitmap_file_from_dib(dib);
    auto png = bitmap_dib_to_transparent_png(dib);

    Json manifest = Json::object();
    manifest["schema"] = "tdx-formula-icon-sprite-v1";
    manifest["source_file"] = path_utf8(dll.filename());
    manifest["source_sha256"] = digest;
    manifest["profile"] = profile.name;
    manifest["resource_type"] = 2;
    manifest["resource_type_name"] = "RT_BITMAP";
    manifest["resource_id"] = 2060;
    manifest["resource_dib_bytes"] = static_cast<std::uint64_t>(dib.size());
    manifest["bitmap_bytes"] = static_cast<std::uint64_t>(bitmap.size());
    manifest["png_bytes"] = static_cast<std::uint64_t>(png.size());
    manifest["bitmap_md5"] = md5_bytes(bitmap);
    manifest["png_md5"] = md5_bytes(png);
    manifest["width"] = 1800;
    manifest["height"] = 18;
    manifest["cell_width"] = 18;
    manifest["cell_height"] = 18;
    manifest["cell_count"] = 100;
    manifest["cell_indexing"] = "one-based-left-to-right";
    manifest["official_type_min"] = 1;
    manifest["official_type_max"] = 51;
    manifest["transparent_color"] = "#ffffff";
    manifest["png_endpoint"] = "/api/v1/formulas/drawicon-strip.png";
    manifest["bitmap_endpoint"] = "/api/v1/formulas/drawicon-strip.bmp";
    return FormulaIconSprite{
        std::move(bitmap), std::move(png), std::move(manifest)};
}

}  // namespace tdx
