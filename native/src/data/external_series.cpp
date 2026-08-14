#include "tdx/external_series.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::size_t index_record_size = 29;
constexpr std::size_t point_record_size = 12;
constexpr std::size_t host_point_limit = 30000;

std::uint16_t u16le(const std::uint8_t* value) {
    return static_cast<std::uint16_t>(value[0]) |
           (static_cast<std::uint16_t>(value[1]) << 8);
}

std::uint32_t u32le(const std::uint8_t* value) {
    return static_cast<std::uint32_t>(value[0]) |
           (static_cast<std::uint32_t>(value[1]) << 8) |
           (static_cast<std::uint32_t>(value[2]) << 16) |
           (static_cast<std::uint32_t>(value[3]) << 24);
}

std::int32_t i32le(const std::uint8_t* value) {
    return static_cast<std::int32_t>(u32le(value));
}

float f32le(const std::uint8_t* value) {
    const auto bits = u32le(value);
    float result{};
    static_assert(sizeof(result) == sizeof(bits));
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

int market_id_from_text(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz") return 0;
    if (value == "sh") return 1;
    if (value == "bj") return 2;
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < 0 || parsed > 65535)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error("market must be sz/sh/bj or an integer in 0..65535");
    }
}

int integer_option(const std::string& value, std::string_view name) {
    try {
        std::size_t used = 0;
        const long long parsed = std::stoll(value, &used);
        if (used != value.size() ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max())
            throw std::invalid_argument("range");
        return static_cast<int>(parsed);
    } catch (...) {
        throw Error(std::string(name) + " must be a 32-bit integer");
    }
}

std::size_t size_option(const std::string& value, std::string_view name) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoull(value, &used);
        if (used != value.size() || parsed > host_point_limit)
            throw std::invalid_argument("range");
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        throw Error(std::string(name) + " must be an integer in 0..30000");
    }
}

int compact_date(const std::string& value) {
    if (value.size() < 10) throw Error("K-line bar has an invalid date for EXTDATA_USER");
    try {
        return std::stoi(value.substr(0, 4)) * 10000 +
               std::stoi(value.substr(5, 2)) * 100 +
               std::stoi(value.substr(8, 2));
    } catch (...) {
        throw Error("K-line bar has an invalid date for EXTDATA_USER");
    }
}

int compact_time(const std::string& value) {
    if (value.empty()) return 0;
    if (value.size() < 5) throw Error("K-line bar has an invalid time for EXTDATA_USER");
    try {
        int second = 0;
        if (value.size() >= 8) second = std::stoi(value.substr(6, 2));
        return std::stoi(value.substr(0, 2)) * 10000 +
               std::stoi(value.substr(3, 2)) * 100 + second;
    } catch (...) {
        throw Error("K-line bar has an invalid time for EXTDATA_USER");
    }
}

std::int64_t stamp(int date, int time) {
    return static_cast<std::int64_t>(date) * 1000000 + time;
}

}  // namespace

ExternalSeriesSelection load_external_series(
    const fs::path& tdx_root, int dataset_id, std::optional<int> market_id,
    const std::string& code, std::size_t point_limit, int start_date,
    int end_date) {
    if (market_id.has_value() != !code.empty())
        throw Error("EXTDATA_USER selection requires both market and code");
    point_limit = std::min(point_limit, host_point_limit);
    if (start_date > end_date)
        throw Error("EXTDATA_USER start date must not exceed end date");

    ExternalSeriesSelection result;
    result.dataset_id = dataset_id;
    result.range_start_date = start_date;
    result.range_end_date = end_date;
    const auto directory = tdx_root / "T0002" / "extdata";
    const auto stem = "extdata_" + std::to_string(dataset_id);
    result.index_path = directory / (stem + ".idx");
    result.data_path = directory / (stem + ".dat");
    result.index_exists = fs::is_regular_file(result.index_path);
    result.data_exists = fs::is_regular_file(result.data_path);
    if (!result.index_exists) return result;

    const auto bytes = read_bytes(result.index_path);
    if (bytes.empty() || bytes.size() % index_record_size != 0)
        throw Error("invalid EXTDATA_USER index length: expected a positive multiple of 29 bytes");

    std::uint64_t start_point = 0;
    for (std::size_t offset = 0, source_index = 0;
         offset < bytes.size(); offset += index_record_size, ++source_index) {
        const auto* raw = bytes.data() + offset;
        const auto terminator = std::find(raw + 2, raw + 25, std::uint8_t{});
        if (terminator == raw + 25)
            throw Error("invalid EXTDATA_USER index code: missing NUL terminator in 23-byte field");
        const int count = i32le(raw + 25);
        if (count < 0)
            throw Error("invalid EXTDATA_USER index count: negative point count");
        ExternalSeriesIndexRecord record;
        record.market_id = u16le(raw);
        record.code.assign(reinterpret_cast<const char*>(raw + 2),
                           reinterpret_cast<const char*>(terminator));
        record.point_count = count;
        record.start_point = start_point;
        record.source_index = source_index;
        result.index_records.push_back(record);
        if (market_id && !result.security_found &&
            record.market_id == *market_id && record.code == code) {
            result.security_found = true;
            result.selected = record;
        }
        if (start_point > std::numeric_limits<std::uint64_t>::max() -
                              static_cast<std::uint64_t>(count))
            throw Error("EXTDATA_USER index point offset overflow");
        start_point += static_cast<std::uint64_t>(count);
    }

    if (!result.security_found) return result;
    if (!result.data_exists)
        throw Error("EXTDATA_USER data file is missing for a matching index record");

    const auto& selected = *result.selected;
    const auto requested = static_cast<std::size_t>(selected.point_count);
    const auto byte_offset = selected.start_point * point_record_size;
    const auto byte_count = static_cast<std::uint64_t>(requested) * point_record_size;
    std::error_code ec;
    const auto data_size = fs::file_size(result.data_path, ec);
    if (ec || byte_offset > data_size || byte_count > data_size - byte_offset)
        throw Error("truncated EXTDATA_USER data segment for matching index record");

    std::ifstream stream(result.data_path, std::ios::binary);
    if (!stream) throw Error("cannot open EXTDATA_USER data file: " + path_utf8(result.data_path));
    stream.seekg(static_cast<std::streamoff>(byte_offset));
    result.points.reserve(std::min(requested, point_limit));
    constexpr std::size_t chunk_points = 4096;
    Bytes raw(chunk_points * point_record_size);
    for (std::size_t consumed = 0; consumed < requested;) {
        const auto count = std::min(chunk_points, requested - consumed);
        const auto bytes_to_read = count * point_record_size;
        stream.read(reinterpret_cast<char*>(raw.data()),
                    static_cast<std::streamsize>(bytes_to_read));
        if (!stream)
            throw Error("cannot read EXTDATA_USER data segment: " +
                        path_utf8(result.data_path));
        for (std::size_t offset = 0; offset < bytes_to_read;
             offset += point_record_size) {
            ExternalSeriesPoint point{i32le(raw.data() + offset),
                                      i32le(raw.data() + offset + 4),
                                      f32le(raw.data() + offset + 8)};
            if (point.date < start_date || point.date > end_date) continue;
            ++result.range_matched_point_count;
            if (result.points.size() < point_limit)
                result.points.push_back(point);
        }
        consumed += count;
    }
    result.host_limit_truncated = result.range_matched_point_count > point_limit;
    return result;
}

Json align_external_series_to_kline(
    const Json& kline_document,
    const std::vector<ExternalSeriesPoint>& points,
    int mode) {
    if (!kline_document.is_object() || !kline_document.as_object().count("bars") ||
        !kline_document.at("bars").is_array())
        throw Error("EXTDATA_USER context requires a K-line bars array");
    const auto& bars = kline_document.at("bars").as_array();
    std::vector<std::int64_t> bar_stamps;
    std::vector<std::string> keys;
    bar_stamps.reserve(bars.size());
    keys.reserve(bars.size());
    for (const auto& bar : bars) {
        if (!bar.is_object()) throw Error("EXTDATA_USER K-line bar must be an object");
        const auto& date_value = bar.at("date");
        const auto& time_value = bar.at("time");
        if (!date_value.is_string() || !time_value.is_string())
            throw Error("EXTDATA_USER K-line bar date/time must be strings");
        bar_stamps.push_back(stamp(compact_date(date_value.as_string()),
                                   compact_time(time_value.as_string())));
        keys.push_back(date_value.as_string() + "|" + time_value.as_string());
    }
    std::vector<std::optional<double>> values(bars.size());
    if (mode == 3) {
        std::ptrdiff_t point_index = static_cast<std::ptrdiff_t>(points.size()) - 1;
        for (std::size_t reverse = bars.size(); reverse-- > 0;) {
            while (point_index >= 0 &&
                   stamp(points[static_cast<std::size_t>(point_index)].date,
                         points[static_cast<std::size_t>(point_index)].time) >
                       bar_stamps[reverse])
                --point_index;
            if (point_index >= 0 &&
                stamp(points[static_cast<std::size_t>(point_index)].date,
                      points[static_cast<std::size_t>(point_index)].time) ==
                    bar_stamps[reverse])
                values[reverse] = static_cast<double>(
                    points[static_cast<std::size_t>(point_index)].value);
            else if (reverse + 1 < bars.size())
                values[reverse] = values[reverse + 1];
        }
    } else {
        std::size_t point_index = 0;
        for (std::size_t index = 0; index < bars.size(); ++index) {
            while (point_index < points.size() &&
                   stamp(points[point_index].date, points[point_index].time) <
                       bar_stamps[index])
                ++point_index;
            if (point_index < points.size() &&
                stamp(points[point_index].date, points[point_index].time) ==
                    bar_stamps[index])
                values[index] = static_cast<double>(points[point_index].value);
            else if (mode == 1 && index > 0)
                values[index] = values[index - 1];
            else if (mode == 2)
                values[index] = 0.0;
        }
    }
    Json result = Json::object();
    for (std::size_t index = 0; index < values.size(); ++index)
        if (values[index] && std::isfinite(*values[index]))
            result[keys[index]] = *values[index];
    return result;
}

Json external_series_document(
    const fs::path& tdx_root, int dataset_id, std::optional<int> market_id,
    const std::string& code, std::size_t point_limit, int start_date,
    int end_date) {
    const auto data = load_external_series(
        tdx_root, dataset_id, market_id, code, point_limit, start_date,
        end_date);
    Json result = Json::object();
    result["schema"] = "tdx-extdata-user-v1";
    result["tdx_root"] = path_utf8(tdx_root);
    result["dataset_id"] = dataset_id;
    result["index_path"] = path_utf8(data.index_path);
    result["data_path"] = path_utf8(data.data_path);
    result["index_exists"] = data.index_exists;
    result["data_exists"] = data.data_exists;
    result["index_record_size"] = static_cast<std::uint64_t>(index_record_size);
    result["data_record_size"] = static_cast<std::uint64_t>(point_record_size);
    result["index_record_count"] = static_cast<std::uint64_t>(data.index_records.size());
    result["host_callback_selector"] = 38;
    result["host_point_limit"] = static_cast<std::uint64_t>(host_point_limit);
    result["lookup_rule"] = "exact uint16 market and case-sensitive NUL-terminated code";
    result["storage_rule"] = "dat segments concatenated in idx record order";
    if (market_id) {
        result["market_id"] = *market_id;
        result["code"] = code;
        result["security_found"] = data.security_found;
        result["host_limit_truncated"] = data.host_limit_truncated;
        result["range_start_date"] = data.range_start_date;
        result["range_end_date"] = data.range_end_date;
        result["range_matched_point_count"] =
            data.range_matched_point_count;
        result["selected_point_count"] = data.selected
            ? Json(static_cast<std::uint64_t>(data.selected->point_count)) : Json(nullptr);
        result["selected_start_point"] = data.selected
            ? Json(data.selected->start_point) : Json(nullptr);
    }
    Json index = Json::array();
    for (const auto& record : data.index_records) {
        Json row = Json::object();
        row["market_id"] = record.market_id;
        row["code"] = record.code;
        row["point_count"] = record.point_count;
        row["start_point"] = record.start_point;
        row["source_index"] = static_cast<std::uint64_t>(record.source_index);
        index.push_back(std::move(row));
    }
    result["index"] = std::move(index);
    Json points = Json::array();
    for (const auto& point : data.points) {
        Json row = Json::object();
        row["date"] = point.date;
        row["time"] = point.time;
        row["value"] = static_cast<double>(point.value);
        points.push_back(std::move(row));
    }
    result["returned"] = static_cast<std::uint64_t>(points.size());
    result["points"] = std::move(points);
    return result;
}

int command_formulas_external_series(const std::vector<std::string>& values) {
    Args arguments(values);
    if (arguments.take_flag("--help") || arguments.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas extdata-user --id N [options]\n\n"
            "  --root PATH              TDX installation root\n"
            "  --id N                   External dataset identifier\n"
            "  --market sz|sh|bj|ID     Select exact security market\n"
            "  --code CODE              Select exact security code\n"
            "  --limit N                Return at most N points (0..30000)\n"
            "  --start YYYYMMDD         Inclusive host callback date floor\n"
            "  --end YYYYMMDD           Inclusive host callback date ceiling\n"
            "  --output FILE            Write JSON instead of stdout\n"
            "  --compact                Compact JSON\n";
        return 0;
    }
    const auto root_text = arguments.take_option("--root");
    const auto id_text = arguments.take_option("--id");
    if (id_text.empty()) throw Error("--id is required");
    const int dataset_id = integer_option(id_text, "id");
    const auto market_text = arguments.take_option("--market");
    const auto code = trim(arguments.take_option("--code"));
    const auto limit_text = arguments.take_option("--limit", "30000");
    const auto start_text = arguments.take_option("--start", "0");
    const auto end_text = arguments.take_option("--end", "99991231");
    const auto output_text = arguments.take_option("--output");
    const bool compact = arguments.take_flag("--compact");
    arguments.require_empty();
    std::optional<int> market_id;
    if (!market_text.empty()) market_id = market_id_from_text(market_text);
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto document = external_series_document(
        root, dataset_id, market_id, code, size_option(limit_text, "limit"),
        integer_option(start_text, "start"), integer_option(end_text, "end"));
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = fs::u8path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "exported " << document.at("returned").as_number()
                  << " EXTDATA_USER points -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
