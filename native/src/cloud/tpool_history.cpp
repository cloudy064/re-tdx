#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace tpool_detail;

namespace {

constexpr std::array<std::string_view, 5> kEntryFields{
    "market", "code", "indate", "intime", "inprice"};
constexpr std::array<std::string_view, 14> kSnapshotFields{
    "market", "code", "indate", "intime", "inprice", "income", "now",
    "rise", "volume", "maxrate", "maxperiod", "maxtime", "maxprice",
    "idaynum"};

std::string history_kind(const std::string& source_name) {
    const auto extension = lower_ascii(
        fs::path(from_utf8(source_name)).extension().string());
    if (extension == ".dat") return "daily-snapshot";
    if (extension == ".log") return "daily-entry-log";
    throw Error("TPool history file must use .dat or .log extension");
}

bool eight_digit_date(std::string_view value) {
    if (value.size() != 8) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    });
}

std::string embedded_history_date(const fs::path& path) {
    const auto stem = path.stem().string();
    std::string result;
    for (std::size_t position = 0; position + 8 <= stem.size(); ++position) {
        const auto candidate = std::string_view(stem).substr(position, 8);
        if (!eight_digit_date(candidate)) continue;
        const bool left_boundary = position == 0 ||
            stem[position - 1] < '0' || stem[position - 1] > '9';
        const bool right_boundary = position + 8 == stem.size() ||
            stem[position + 8] < '0' || stem[position + 8] > '9';
        if (left_boundary && right_boundary) result = std::string(candidate);
    }
    return result;
}

std::string date_text(int value) {
    const auto raw = std::to_string(value);
    if (!eight_digit_date(raw)) return {};
    return raw.substr(0, 4) + "-" + raw.substr(4, 2) + "-" + raw.substr(6, 2);
}

std::string time_text(int value) {
    if (value < 0 || value > 235959) return {};
    const int hour = value / 10000;
    const int minute = value / 100 % 100;
    const int second = value % 100;
    if (hour > 23 || minute > 59 || second > 59) return {};
    std::ostringstream result;
    result << std::setfill('0') << std::setw(2) << hour << ':'
           << std::setw(2) << minute << ':' << std::setw(2) << second;
    return result.str();
}

Json integer_or_null(const Attributes& attributes, std::string_view key) {
    const auto value = attribute(attributes, key);
    if (value.empty()) return Json(nullptr);
    constexpr int sentinel = std::numeric_limits<int>::min();
    const int parsed = integer_text(value, sentinel);
    return parsed == sentinel ? Json(nullptr) : Json(parsed);
}

Json number_or_null(const Attributes& attributes, std::string_view key) {
    const auto value = attribute(attributes, key);
    if (value.empty()) return Json(nullptr);
    const double parsed = float_text(
        value, std::numeric_limits<double>::quiet_NaN());
    return std::isfinite(parsed) ? Json(parsed) : Json(nullptr);
}

bool valid_integer(const Attributes& attributes, std::string_view key) {
    const auto value = attribute(attributes, key);
    if (value.empty()) return true;
    constexpr int sentinel = std::numeric_limits<int>::min();
    return integer_text(value, sentinel) != sentinel;
}

bool valid_number(const Attributes& attributes, std::string_view key) {
    const auto value = attribute(attributes, key);
    if (value.empty()) return true;
    return std::isfinite(float_text(
        value, std::numeric_limits<double>::quiet_NaN()));
}

Json invalid_fields(const Attributes& attributes, std::string_view kind) {
    Json result = Json::array();
    const auto market = attribute(attributes, "market").empty()
        ? attribute(attributes, "setcode") : attribute(attributes, "market");
    if (!market.empty() && market_from_setcode(market).empty())
        result.push_back("market");
    const auto indate = attribute(attributes, "indate");
    if (!indate.empty() && !eight_digit_date(indate))
        result.push_back("indate");
    const auto intime = attribute(attributes, "intime");
    if (!intime.empty()) {
        constexpr int sentinel = std::numeric_limits<int>::min();
        const int value = integer_text(intime, sentinel);
        if (value == sentinel || time_text(value).empty())
            result.push_back("intime");
    }
    if (!valid_number(attributes, "inprice")) result.push_back("inprice");
    if (kind == "daily-snapshot") {
        for (const auto key : {"income", "now", "rise", "maxrate", "maxprice"})
            if (!valid_number(attributes, key)) result.push_back(key);
        for (const auto key : {"volume", "maxperiod", "idaynum"})
            if (!valid_integer(attributes, key)) result.push_back(key);
        const auto maximum_date = attribute(attributes, "maxtime");
        if (!maximum_date.empty() && maximum_date != "0" &&
            !eight_digit_date(maximum_date))
            result.push_back("maxtime");
    }
    return result;
}

template <std::size_t Size>
Json expected_fields(const std::array<std::string_view, Size>& fields) {
    Json result = Json::array();
    for (const auto field : fields) result.push_back(std::string(field));
    return result;
}

template <std::size_t Size>
Json missing_fields(const Attributes& attributes,
                    const std::array<std::string_view, Size>& fields) {
    Json result = Json::array();
    for (const auto field : fields)
        if (attribute(attributes, field).empty())
            result.push_back(std::string(field));
    return result;
}

Json history_record(const Attributes& attributes, std::string_view kind) {
    Json result = Json::object();
    const auto market_value = attribute(attributes, "market").empty()
        ? attribute(attributes, "setcode") : attribute(attributes, "market");
    const int market_id = integer_text(market_value, -1);
    const auto market = market_from_setcode(std::to_string(market_id));
    const auto code = trim(attribute(attributes, "code"));
    result["market_id"] = market_id >= 0 ? Json(market_id) : Json(nullptr);
    result["market"] = market.empty() ? Json(nullptr) : Json(market);
    result["code"] = code;
    result["security_id"] = market.empty() || code.empty()
        ? Json(nullptr) : Json(market + code);
    result["entry_date"] = integer_or_null(attributes, "indate");
    result["entry_time"] = integer_or_null(attributes, "intime");
    result["entry_price"] = number_or_null(attributes, "inprice");
    if (result.at("entry_date").is_number())
        result["entry_date_text"] = date_text(
            static_cast<int>(result.at("entry_date").as_number()));
    else
        result["entry_date_text"] = Json(nullptr);
    if (result.at("entry_time").is_number())
        result["entry_time_text"] = time_text(
            static_cast<int>(result.at("entry_time").as_number()));
    else
        result["entry_time_text"] = Json(nullptr);

    if (kind == "daily-snapshot") {
        result["income"] = number_or_null(attributes, "income");
        result["current_price"] = number_or_null(attributes, "now");
        result["rise_pct"] = number_or_null(attributes, "rise");
        result["volume"] = integer_or_null(attributes, "volume");
        result["maximum_rise_pct"] = number_or_null(attributes, "maxrate");
        result["maximum_period"] = integer_or_null(attributes, "maxperiod");
        result["maximum_date"] = integer_or_null(attributes, "maxtime");
        if (result.at("maximum_date").is_number()) {
            const auto text = date_text(
                static_cast<int>(result.at("maximum_date").as_number()));
            result["maximum_date_text"] =
                text.empty() ? Json(nullptr) : Json(text);
        } else {
            result["maximum_date_text"] = Json(nullptr);
        }
        result["maximum_price"] = number_or_null(attributes, "maxprice");
        result["day_count"] = integer_or_null(attributes, "idaynum");
        result["missing_fields"] = missing_fields(attributes, kSnapshotFields);
    } else {
        result["missing_fields"] = missing_fields(attributes, kEntryFields);
    }
    result["invalid_fields"] = invalid_fields(attributes, kind);
    result["complete"] = result.at("missing_fields").size() == 0 &&
        result.at("invalid_fields").size() == 0;
    result["raw"] = attributes_json(attributes);
    return result;
}

Json history_catalog(Json files, const Json& directories) {
    std::uint64_t record_count = 0;
    std::uint64_t complete_record_count = 0;
    std::uint64_t duplicate_record_count = 0;
    std::set<std::string, std::less<>> securities;
    for (const auto& file : files.as_array()) {
        record_count += static_cast<std::uint64_t>(
            file.at("record_count").as_number());
        complete_record_count += static_cast<std::uint64_t>(
            file.at("complete_record_count").as_number());
        duplicate_record_count += static_cast<std::uint64_t>(
            file.at("duplicate_record_count").as_number());
        for (const auto& record : file.at("records").as_array()) {
            const auto* security = optional(record, "security_id");
            if (security && security->is_string())
                securities.insert(security->as_string());
        }
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-history-catalog-v1";
    result["read_only"] = true;
    result["dll_loaded"] = false;
    result["worker_started"] = false;
    result["file_count"] = static_cast<std::uint64_t>(files.size());
    result["record_count"] = record_count;
    result["complete_record_count"] = complete_record_count;
    result["incomplete_record_count"] = record_count - complete_record_count;
    result["duplicate_record_count"] = duplicate_record_count;
    result["security_count"] = static_cast<std::uint64_t>(securities.size());
    result["directories"] = directories;
    result["files"] = std::move(files);
    return result;
}

std::vector<std::string> relative_parts(const fs::path& path) {
    std::vector<std::string> result;
    for (const auto& part : path)
        result.push_back(path_utf8(part));
    return result;
}

}  // namespace

Json parse_tpool_history_xml_document(const std::string& xml,
                                      const std::string& source_name) {
    if (xml.size() > 64ULL * 1024ULL * 1024ULL)
        throw Error("TPool history XML exceeds 64 MiB safety limit");
    if (xml.find("<root") == std::string::npos ||
        xml.find("<data") == std::string::npos)
        throw Error("TPool history XML requires root/data elements");
    const auto kind = history_kind(source_name);
    const auto stocks = scan_elements(xml, "stk");
    Json records = Json::array();
    std::set<std::string, std::less<>> seen;
    std::size_t complete = 0;
    std::size_t duplicates = 0;
    for (const auto& stock : stocks) {
        auto record = history_record(stock, kind);
        if (record.at("complete").as_bool()) ++complete;
        const auto* security = optional(record, "security_id");
        if (security && security->is_string() &&
            !seen.insert(security->as_string()).second)
            ++duplicates;
        records.push_back(std::move(record));
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-history-file-v1";
    result["source"] = source_name;
    result["kind"] = kind;
    result["read_only"] = true;
    result["normalized_encoding"] = "UTF-8";
    result["record_count"] = static_cast<std::uint64_t>(records.size());
    result["complete_record_count"] = static_cast<std::uint64_t>(complete);
    result["incomplete_record_count"] =
        static_cast<std::uint64_t>(records.size() - complete);
    result["duplicate_record_count"] = static_cast<std::uint64_t>(duplicates);
    result["deduplicate_key"] = "market+code";
    result["expected_fields"] = kind == "daily-snapshot"
        ? expected_fields(kSnapshotFields) : expected_fields(kEntryFields);
    result["records"] = std::move(records);
    result["evidence"] = kind == "daily-snapshot"
        ? "TPool.dll sub_10017CC0 root/data/stk daily .dat serializer"
        : "TPool.dll sub_10018380 root/data/stk daily .log replace-then-append serializer";
    return result;
}

Json inspect_tpool_history_file_document(const fs::path& path) {
    if (!fs::is_regular_file(path))
        throw Error("TPool history file does not exist: " + path_utf8(path));
    const auto extension = lower_ascii(path.extension().string());
    auto result = extension == ".txt"
        ? parse_tpool_history_text_document(
              decode_tpool_history_text_file(path), path_utf8(path))
        : parse_tpool_history_xml_document(
              decode_xml_file(path), path_utf8(path));
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["sha256"] = lower_ascii(sha256_file(path));
    const auto date = embedded_history_date(path);
    if (!date.empty()) {
        result["history_date"] = integer_text(date, 0);
        result["history_date_text"] = date_text(integer_text(date, 0));
    } else {
        result["history_date"] = Json(nullptr);
        result["history_date_text"] = Json(nullptr);
    }
    return result;
}

Json inspect_tpool_history_files_document(const std::vector<fs::path>& paths) {
    Json files = Json::array();
    for (const auto& path : paths)
        files.push_back(inspect_tpool_history_file_document(path));
    return history_catalog(std::move(files), Json::array());
}

Json inspect_tpool_history_root_document(
    const fs::path& root, const std::string& pool, const std::string& cell,
    const std::string& requested_kind, int from_date, int to_date,
    std::size_t limit) {
    const auto kind = lower_ascii(trim(requested_kind));
    if (kind != "all" && kind != "snapshot" && kind != "entry")
        throw Error("TPool history kind must be all, snapshot or entry");
    if (from_date && !eight_digit_date(std::to_string(from_date)))
        throw Error("TPool history from date must be YYYYMMDD");
    if (to_date && !eight_digit_date(std::to_string(to_date)))
        throw Error("TPool history to date must be YYYYMMDD");
    if (from_date && to_date && from_date > to_date)
        throw Error("TPool history from date exceeds to date");
    if (limit == 0 || limit > 10000)
        throw Error("TPool history limit must be in 1..10000");

    struct Candidate {
        fs::path path;
        std::string pool;
        std::string cell;
        int date{};
    };
    std::vector<Candidate> candidates;
    Json directories = Json::array();
    for (const auto& directory : pool_directories(root)) {
        Json info = Json::object();
        info["path"] = path_utf8(directory);
        info["exists"] = fs::is_directory(directory);
        directories.push_back(std::move(info));
        if (!fs::is_directory(directory)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(
                 directory, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            const auto extension = lower_ascii(entry.path().extension().string());
            if (extension != ".dat" && extension != ".log") continue;
            if (kind == "snapshot" && extension != ".dat") continue;
            if (kind == "entry" && extension != ".log") continue;
            const auto relative = entry.path().lexically_relative(directory);
            const auto parts = relative_parts(relative);
            if (parts.size() != 3) continue;
            if (!pool.empty() && lower_ascii(parts[0]) != lower_ascii(pool)) continue;
            if (!cell.empty() && lower_ascii(parts[1]) != lower_ascii(cell)) continue;
            const auto stem = entry.path().stem().string();
            if (!eight_digit_date(stem)) continue;
            const int date = integer_text(stem, 0);
            if (from_date && date < from_date) continue;
            if (to_date && date > to_date) continue;
            candidates.push_back(Candidate{entry.path(), parts[0], parts[1], date});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left,
                                                        const Candidate& right) {
        if (left.date != right.date) return left.date > right.date;
        return std::tie(left.pool, left.cell, left.path) <
               std::tie(right.pool, right.cell, right.path);
    });
    const auto matched = candidates.size();
    if (candidates.size() > limit) candidates.resize(limit);
    Json files = Json::array();
    for (const auto& candidate : candidates) {
        auto file = inspect_tpool_history_file_document(candidate.path);
        file["pool"] = candidate.pool;
        file["cell"] = candidate.cell;
        files.push_back(std::move(file));
    }
    auto result = history_catalog(std::move(files), directories);
    result["root"] = path_utf8(root);
    result["matched_file_count"] = static_cast<std::uint64_t>(matched);
    result["truncated"] = matched > candidates.size();
    result["order"] = "date-desc";
    result["filters"] = Json::object();
    result["filters"]["pool"] = pool.empty() ? Json(nullptr) : Json(pool);
    result["filters"]["cell"] = cell.empty() ? Json(nullptr) : Json(cell);
    result["filters"]["kind"] = kind;
    result["filters"]["from_date"] = from_date ? Json(from_date) : Json(nullptr);
    result["filters"]["to_date"] = to_date ? Json(to_date) : Json(nullptr);
    result["filters"]["limit"] = static_cast<std::uint64_t>(limit);
    return result;
}

}  // namespace tdx
