#include "shape_match_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::shape_match_detail {
namespace {

constexpr std::uintmax_t kMaximumLibrarySize = 512ULL * 1024ULL * 1024ULL;

std::string fixed_text(const Bytes& data, std::size_t offset,
                       std::size_t width, const fs::path& path,
                       std::size_t record, const char* field) {
    if (offset > data.size() || width > data.size() - offset)
        throw Error("truncated shape " + std::string(field) + " in " +
                    path_utf8(path));
    const auto begin = data.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end = begin + static_cast<std::ptrdiff_t>(width);
    const auto zero = std::find(begin, end, 0);
    if (zero == end)
        throw Error(path_utf8(path) + " record " + std::to_string(record) +
                    " has an unterminated " + field);
    return decode_gbk(Bytes(begin, zero));
}

ShapeBar parse_bar(const std::uint8_t* data) {
    ShapeBar result;
    result.year = read_u16_le(data);
    result.month = data[2];
    result.day = data[3];
    result.hour = data[4];
    result.minute = data[5];
    result.second = data[6];
    result.open = read_f32_le(data + 7);
    result.close = read_f32_le(data + 19);
    result.volume = read_f32_le(data + 27);
    return result;
}

std::string timestamp_text(const ShapeBar& bar, std::int16_t period_code) {
    if (bar.year <= 0 || bar.month <= 0 || bar.day <= 0) return {};
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << bar.year << '-'
           << std::setw(2) << bar.month << '-' << std::setw(2) << bar.day;
    if (period_code == 0 || period_code == 1 || period_code == 2 ||
        period_code == 3 || period_code == 7 || period_code == 8 ||
        period_code == 12 || period_code == 13) {
        output << ' ' << std::setw(2) << bar.hour << ':'
               << std::setw(2) << bar.minute;
        if (period_code == 12 || period_code == 13)
            output << ':' << std::setw(2) << bar.second;
    }
    return output.str();
}

Json finite_number(float value) {
    return std::isfinite(value) ? Json(value) : Json(nullptr);
}

}  // namespace

std::string scope_name(std::int32_t code) {
    if (code == 0) return "all-a-shares";
    if (code == 1) return "all-sh-sz-bj";
    if (code == 2) return "expansion-market";
    return "unknown";
}

std::string template_kind_name(std::uint16_t code) {
    if (code == 0) return "security-bars";
    if (code == 1) return "hand-drawn";
    return "unknown";
}

std::string period_name(std::int16_t code) {
    switch (code) {
        case 0: return "5m";
        case 1: return "15m";
        case 2: return "30m";
        case 3: return "60m";
        case 4: return "day";
        case 5: return "week";
        case 6: return "month";
        case 7: return "1m";
        case 8: return "intraday-custom";
        case 12: return "seconds";
        case 13: return "seconds-custom";
        default: return "unknown";
    }
}

std::string market_name(std::int16_t market_id) {
    if (market_id == 0) return "sz";
    if (market_id == 1) return "sh";
    if (market_id == 2) return "bj";
    return std::to_string(market_id);
}

ShapeLibrary parse_shape_library(const fs::path& path) {
    ShapeLibrary result;
    result.path = path;
    std::error_code error;
    result.available = fs::is_regular_file(path, error);
    if (error || !result.available) {
        result.available = false;
        return result;
    }
    result.byte_size = fs::file_size(path, error);
    if (error) throw Error("cannot inspect shape library size: " + path_utf8(path));
    if (result.byte_size < kHeaderSize || result.byte_size > kMaximumLibrarySize ||
        (result.byte_size - kHeaderSize) % kRecordSize != 0)
        throw Error("shape library size must be 4 + N * 94195 bytes");
    const auto data = read_bytes(path);
    result.scope_code = read_i32_le(data.data());
    const auto count = (data.size() - kHeaderSize) / kRecordSize;
    result.templates.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto base = kHeaderSize + index * kRecordSize;
        const auto* record = data.data() + base;
        ShapeTemplate shape;
        shape.record_index = index;
        shape.id = read_u32_le(record);
        shape.name = fixed_text(data, base + 4, 45, path, index, "name");
        shape.template_kind = read_u16_le(record + 49);
        shape.period_code = static_cast<std::int16_t>(read_u16_le(record + 51));
        shape.point_count = read_i32_le(record + 53);
        if (shape.point_count < 0 ||
            shape.point_count > static_cast<std::int32_t>(kMaximumPointCount))
            throw Error(path_utf8(path) + " record " + std::to_string(index) +
                        " has point_count outside 0..2000");
        shape.market_id = static_cast<std::int16_t>(read_u16_le(record + 57));
        shape.code = fixed_text(data, base + 59, 23, path, index, "security code");
        shape.security_name = fixed_text(
            data, base + 82, 45, path, index, "security name");
        shape.threshold_percent = read_i32_le(record + kThresholdOffset);
        for (std::size_t component = 0; component < shape.active.size(); ++component) {
            shape.active[component] = record[kActiveOffset + component] != 0;
            shape.weights[component] = read_f32_le(
                record + kWeightOffset + component * sizeof(float));
        }
        if (shape.template_kind == 0) {
            shape.bars.reserve(static_cast<std::size_t>(shape.point_count));
            for (std::int32_t bar = 0; bar < shape.point_count; ++bar)
                shape.bars.push_back(parse_bar(
                    record + kBarOffset + static_cast<std::size_t>(bar) * kBarSize));
        } else if (shape.template_kind == 1) {
            shape.drawing_points.reserve(
                static_cast<std::size_t>(shape.point_count));
            for (std::int32_t point = 0; point < shape.point_count; ++point) {
                const auto offset = kDrawingOffset +
                    static_cast<std::size_t>(point) * kDrawingPointSize;
                shape.drawing_points.push_back({
                    read_i32_le(record + offset),
                    read_i32_le(record + offset + 4),
                    read_f32_le(record + offset + 8),
                });
            }
        }
        result.templates.push_back(std::move(shape));
    }
    return result;
}

Json template_document(const ShapeTemplate& shape) {
    static constexpr std::array<const char*, 4> component_names{
        "close", "volume", "candle-body-return", "candle-direction"};
    Json result = Json::object();
    result["record_index"] = static_cast<std::uint64_t>(shape.record_index);
    result["id"] = static_cast<std::uint64_t>(shape.id);
    result["name"] = shape.name;
    result["template_kind_code"] = static_cast<int>(shape.template_kind);
    result["template_kind"] = template_kind_name(shape.template_kind);
    result["period_code"] = static_cast<int>(shape.period_code);
    result["period"] = period_name(shape.period_code);
    result["point_count"] = shape.point_count;
    result["threshold_percent"] = shape.threshold_percent;
    result["threshold"] = static_cast<double>(shape.threshold_percent) / 100.0;
    Json security = Json::object();
    security["market_id"] = static_cast<int>(shape.market_id);
    security["market"] = market_name(shape.market_id);
    security["code"] = shape.code;
    security["name"] = shape.security_name;
    result["security"] = std::move(security);
    Json components = Json::array();
    std::size_t active_count = 0;
    for (std::size_t index = 0; index < shape.active.size(); ++index) {
        Json component = Json::object();
        component["index"] = static_cast<std::uint64_t>(index);
        component["name"] = component_names[index];
        component["active"] = shape.active[index];
        component["weight"] = finite_number(shape.weights[index]);
        if (shape.active[index]) ++active_count;
        components.push_back(std::move(component));
    }
    result["components"] = std::move(components);
    result["active_component_count"] = static_cast<std::uint64_t>(active_count);
    if (!shape.bars.empty()) {
        result["start"] = timestamp_text(shape.bars.front(), shape.period_code);
        result["end"] = timestamp_text(shape.bars.back(), shape.period_code);
    } else {
        result["start"] = Json(nullptr);
        result["end"] = Json(nullptr);
    }
    const auto active_points = std::count_if(
        shape.drawing_points.begin(), shape.drawing_points.end(),
        [](const DrawingPoint& point) { return point.x != 0 && point.y != 0; });
    result["drawing_active_point_count"] =
        static_cast<std::uint64_t>(active_points);
    return result;
}

}  // namespace tdx::shape_match_detail
