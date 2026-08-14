#include "tdx/local_signals.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::size_t user_catalog_record_size = 60;
constexpr std::size_t user_point_record_size = 8;
constexpr std::size_t host_point_limit = 30000;

std::string namespace_name(LocalSignalNamespace value) {
    return value == LocalSignalNamespace::system ? "system" : "user";
}

int strict_int(std::string value, std::string_view field) {
    value = trim(std::move(value));
    try {
        std::size_t used = 0;
        const auto parsed = std::stoll(value, &used);
        if (used != value.size() ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max())
            throw std::invalid_argument("range");
        return static_cast<int>(parsed);
    } catch (...) {
        throw Error("invalid local signal " + std::string(field));
    }
}

float strict_float(std::string value, std::string_view field) {
    value = trim(std::move(value));
    try {
        std::size_t used = 0;
        const float parsed = std::stof(value, &used);
        if (used != value.size() || !std::isfinite(parsed))
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error("invalid local signal " + std::string(field));
    }
}

std::vector<std::string> text_lines(const Bytes& bytes) {
    std::vector<std::string> result;
    const auto text = decode_gbk(bytes);
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!trim(line).empty()) result.push_back(std::move(line));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

int host_numeric_code(const std::string& code) {
    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(code.c_str(), &end, 10);
    if (end == code.c_str()) return 0;
    if (errno == ERANGE || value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max())
        throw Error("local system signal code is outside the host 32-bit range");
    return static_cast<int>(value);
}

int compact_date(const std::string& value) {
    if (value.size() < 10)
        throw Error("K-line bar has an invalid date for local signals");
    try {
        return std::stoi(value.substr(0, 4)) * 10000 +
               std::stoi(value.substr(5, 2)) * 100 +
               std::stoi(value.substr(8, 2));
    } catch (...) {
        throw Error("K-line bar has an invalid date for local signals");
    }
}

void validate_selection(int market_id, const std::string& code,
                        std::size_t point_limit, int start_date,
                        int end_date) {
    if (market_id < 0 || market_id > 65535)
        throw Error("local signal market must be in 0..65535");
    if (code.empty()) throw Error("local signal code must not be empty");
    if (point_limit > host_point_limit)
        throw Error("local signal limit must be in 0..30000");
    if (start_date > end_date)
        throw Error("local signal start date must not exceed end date");
}

}  // namespace

LocalSignalCatalog load_local_signal_catalog(const fs::path& tdx_root) {
    LocalSignalCatalog result;
    const auto directory = tdx_root / "T0002" / "signals";
    result.system_path = directory / "datacfg.sys";
    result.user_path = directory / "datacfg.dat";
    result.system_exists = fs::is_regular_file(result.system_path);
    result.user_exists = fs::is_regular_file(result.user_path);

    if (result.system_exists) {
        const auto lines = text_lines(read_bytes(result.system_path));
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const auto fields = split(lines[index], '|');
            if (fields.size() < 4)
                throw Error("invalid datacfg.sys record at line " +
                            std::to_string(index + 1));
            LocalSignalCatalogEntry entry;
            entry.signal_namespace = LocalSignalNamespace::system;
            entry.signal_id = strict_int(fields[0], "system catalog id");
            entry.kind = strict_int(fields[1], "system catalog kind");
            entry.catalog_value = strict_int(
                fields[2], "system catalog value");
            entry.name = trim(fields[3]);
            entry.source_index = index;
            if (entry.signal_id < 10001 || entry.kind < 0 || entry.kind > 1 ||
                entry.name.empty())
                throw Error("invalid datacfg.sys field at line " +
                            std::to_string(index + 1));
            if (entry.name.size() < 40 && entry.catalog_value != 0)
                entry.name += "(" + std::to_string(entry.catalog_value) + ")";
            result.entries.push_back(std::move(entry));
        }
    }

    if (result.user_exists) {
        const auto bytes = read_bytes(result.user_path);
        if (bytes.size() % user_catalog_record_size != 0)
            throw Error("invalid datacfg.dat length: expected a multiple of 60 bytes");
        for (std::size_t offset = 0, index = 0; offset < bytes.size();
             offset += user_catalog_record_size, ++index) {
            const auto* raw = bytes.data() + offset;
            const auto terminator = std::find(raw + 8, raw + 56, std::uint8_t{});
            if (terminator == raw + 56)
                throw Error("invalid datacfg.dat name at record " +
                            std::to_string(index));
            LocalSignalCatalogEntry entry;
            entry.signal_namespace = LocalSignalNamespace::user;
            entry.signal_id = read_i32_le(raw);
            entry.kind = static_cast<int>(read_u32_le(raw + 4));
            entry.name = decode_gbk(Bytes(raw + 8, terminator));
            entry.source_index = index;
            if (entry.kind < 0 || entry.kind > 1 || entry.name.empty())
                throw Error("invalid datacfg.dat field at record " +
                            std::to_string(index));
            result.entries.push_back(std::move(entry));
        }
    }
    return result;
}

LocalSignalSelection load_local_signal_series(
    const fs::path& tdx_root, LocalSignalNamespace signal_namespace,
    int signal_id, int market_id, const std::string& code,
    std::size_t point_limit, int start_date, int end_date) {
    validate_selection(market_id, code, point_limit, start_date, end_date);
    LocalSignalSelection result;
    result.signal_namespace = signal_namespace;
    result.signal_id = signal_id;
    result.market_id = market_id;
    result.code = code;
    result.range_start_date = start_date;
    result.range_end_date = end_date;
    const auto directory = tdx_root / "T0002" / "signals";
    if (signal_namespace == LocalSignalNamespace::system) {
        result.path = directory /
                      ("signals_sys_" + std::to_string(signal_id) + ".dat");
    } else {
        result.path = directory /
                      ("signals_user_" + std::to_string(signal_id)) /
                      (std::to_string(market_id) + "_" + code + ".dat");
    }
    result.exists = fs::is_regular_file(result.path);
    if (!result.exists) return result;

    if (signal_namespace == LocalSignalNamespace::user) {
        const auto bytes = read_bytes(result.path);
        if (bytes.size() % user_point_record_size != 0)
            throw Error("invalid SIGNALS_USER data length: expected a multiple of 8 bytes");
        result.source_record_count = bytes.size() / user_point_record_size;
        for (std::size_t offset = 0; offset < bytes.size();
             offset += user_point_record_size) {
            const int date = read_i32_le(bytes.data() + offset);
            if (date < start_date || date > end_date) continue;
            ++result.matched_record_count;
            if (result.points.size() < point_limit)
                result.points.push_back(
                    {date, read_f32_le(bytes.data() + offset + 4)});
        }
    } else {
        const int numeric_code = host_numeric_code(code);
        const auto lines = text_lines(read_bytes(result.path));
        result.source_record_count = lines.size();
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const auto fields = split(lines[index], '|');
            if (fields.size() < 4)
                throw Error("invalid SIGNALS_SYS record at line " +
                            std::to_string(index + 1));
            const int row_market = strict_int(fields[0], "system market");
            const int row_code = strict_int(fields[1], "system code");
            const int date = strict_int(fields[2], "system date");
            const float value = strict_float(fields[3], "system value");
            if (static_cast<std::uint16_t>(row_market) !=
                    static_cast<std::uint16_t>(market_id) ||
                row_code != numeric_code || date < start_date ||
                date > end_date)
                continue;
            ++result.matched_record_count;
            if (result.points.size() < point_limit)
                result.points.push_back({date, value});
        }
    }
    result.host_limit_truncated = result.matched_record_count > point_limit;
    return result;
}

Json align_local_signal_to_kline(
    const Json& kline_document, const std::vector<LocalSignalPoint>& points,
    int mode) {
    if (!kline_document.is_object() ||
        !kline_document.as_object().count("bars") ||
        !kline_document.at("bars").is_array())
        throw Error("local signal context requires a K-line bars array");
    const auto& bars = kline_document.at("bars").as_array();
    std::vector<std::optional<double>> values(bars.size());
    std::vector<std::string> keys;
    keys.reserve(bars.size());
    std::size_t point_index = 0;
    for (std::size_t index = 0; index < bars.size(); ++index) {
        const auto& bar = bars[index];
        if (!bar.is_object() || !bar.at("date").is_string() ||
            !bar.at("time").is_string())
            throw Error("local signal K-line bar date/time must be strings");
        const int date = compact_date(bar.at("date").as_string());
        keys.push_back(bar.at("date").as_string() + "|" +
                       bar.at("time").as_string());
        while (point_index < points.size() && points[point_index].date < date)
            ++point_index;
        if (point_index < points.size() && points[point_index].date == date)
            values[index] = static_cast<double>(points[point_index].value);
        else if (mode == 1 && index > 0)
            values[index] = values[index - 1];
        else if (mode == 2)
            values[index] = 0.0;
    }
    Json result = Json::object();
    for (std::size_t index = 0; index < values.size(); ++index)
        if (values[index] && std::isfinite(*values[index]))
            result[keys[index]] = *values[index];
    return result;
}

Json local_signal_document(
    const fs::path& tdx_root,
    std::optional<LocalSignalNamespace> signal_namespace,
    std::optional<int> signal_id, std::optional<int> market_id,
    const std::string& code, std::size_t point_limit, int start_date,
    int end_date) {
    if (signal_id.has_value() != market_id.has_value() ||
        signal_id.has_value() != !code.empty() ||
        signal_id.has_value() != signal_namespace.has_value())
        throw Error("local signal selection requires namespace, id, market and code together");
    const auto catalog = load_local_signal_catalog(tdx_root);
    Json result = Json::object();
    result["schema"] = "tdx-local-signals-v1";
    result["tdx_root"] = path_utf8(tdx_root);
    result["system_catalog_path"] = path_utf8(catalog.system_path);
    result["user_catalog_path"] = path_utf8(catalog.user_path);
    result["system_catalog_exists"] = catalog.system_exists;
    result["user_catalog_exists"] = catalog.user_exists;
    result["system_catalog_record_format"] =
        "pipe text: id|kind|catalog_value|name";
    result["user_catalog_record_format"] =
        "60-byte little-endian: int32 id, uint32 kind, char name[48], runtime pointer[4]";
    result["system_series_record_format"] =
        "pipe text: market|numeric_code|YYYYMMDD|float32-compatible value";
    result["user_series_record_format"] =
        "8-byte little-endian: int32 YYYYMMDD, float32 value";
    result["host_callback_selectors"] = "34=SIGNALS_SYS;36=SIGNALS_USER";
    result["missing_modes"] =
        "1=forward-fill;2=zero;other=DRAWNULL;date-only matching";
    result["host_point_limit"] = static_cast<std::uint64_t>(host_point_limit);
    Json entries = Json::array();
    for (const auto& entry : catalog.entries) {
        if (signal_namespace && entry.signal_namespace != *signal_namespace)
            continue;
        Json row = Json::object();
        row["namespace"] = namespace_name(entry.signal_namespace);
        row["id"] = entry.signal_id;
        row["kind"] = entry.kind;
        row["catalog_value"] = entry.catalog_value;
        row["name"] = entry.name;
        row["source_index"] = static_cast<std::uint64_t>(entry.source_index);
        entries.push_back(std::move(row));
    }
    result["catalog_returned"] = static_cast<std::uint64_t>(entries.size());
    result["catalog"] = std::move(entries);
    if (signal_id) {
        const auto selection = load_local_signal_series(
            tdx_root, *signal_namespace, *signal_id, *market_id, code,
            point_limit, start_date, end_date);
        result["namespace"] = namespace_name(*signal_namespace);
        result["signal_id"] = *signal_id;
        result["market_id"] = *market_id;
        result["code"] = code;
        result["path"] = path_utf8(selection.path);
        result["exists"] = selection.exists;
        result["source_record_count"] = selection.source_record_count;
        result["matched_record_count"] = selection.matched_record_count;
        result["host_limit_truncated"] = selection.host_limit_truncated;
        result["range_start_date"] = start_date;
        result["range_end_date"] = end_date;
        Json points = Json::array();
        for (const auto& point : selection.points) {
            Json row = Json::object();
            row["date"] = point.date;
            row["value"] = static_cast<double>(point.value);
            points.push_back(std::move(row));
        }
        result["returned"] = static_cast<std::uint64_t>(points.size());
        result["points"] = std::move(points);
    }
    return result;
}

}  // namespace tdx
