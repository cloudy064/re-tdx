#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
using namespace tpool_detail;
namespace {

constexpr std::string_view kEntryHeader =
    "市场|代码|名称|进入日期|进入时间|进入价";
constexpr std::string_view kSnapshotHeader =
    "市场|代码|名称|进入日期|进入时间|进入价|最高收益率|最高周期|最高日期|最高价格";
constexpr std::array<const char*, 6> kEntryColumns{
    "market", "code", "name", "entry-date", "entry-time", "entry-price"};
constexpr std::array<const char*, 10> kSnapshotColumns{
    "market", "code", "name", "entry-date", "entry-time", "entry-price",
    "maximum-rise-pct", "maximum-period", "maximum-date", "maximum-price"};

std::vector<std::string> text_lines(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) result.push_back(std::move(line));
    }
    return result;
}

bool digits(std::string_view value) {
    return !value.empty() &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        });
}

Json number_or_null(std::string_view value) {
    const auto parsed = float_text(
        std::string(value), std::numeric_limits<double>::quiet_NaN());
    return std::isfinite(parsed) ? Json(parsed) : Json(nullptr);
}

Json integer_or_null(std::string_view value) {
    constexpr int sentinel = std::numeric_limits<int>::min();
    const int parsed = integer_text(std::string(value), sentinel);
    return parsed == sentinel ? Json(nullptr) : Json(parsed);
}

int parse_hhmm(std::string_view value) {
    if (value.size() != 5 || value[2] != ':' ||
        !digits(value.substr(0, 2)) || !digits(value.substr(3, 2)))
        return -1;
    const int hour = integer_text(std::string(value.substr(0, 2)), -1);
    const int minute = integer_text(std::string(value.substr(3, 2)), -1);
    return hour <= 23 && minute <= 59
        ? hour * 10000 + minute * 100 : -1;
}

bool month_day(std::string_view value) {
    if (value.size() != 5 || value[2] != '/' ||
        !digits(value.substr(0, 2)) || !digits(value.substr(3, 2)))
        return false;
    const int month = integer_text(std::string(value.substr(0, 2)), 0);
    const int day = integer_text(std::string(value.substr(3, 2)), 0);
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool calendar_date(std::string_view value) {
    if (value.size() != 8 || !digits(value)) return false;
    const int month = integer_text(std::string(value.substr(4, 2)), 0);
    const int day = integer_text(std::string(value.substr(6, 2)), 0);
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool known_header(std::string_view value) {
    return value.substr(0, kEntryHeader.size()) == kEntryHeader ||
        value.substr(0, kSnapshotHeader.size()) == kSnapshotHeader;
}

void append_column(Json& values, std::string_view name) {
    values.push_back(std::string(name));
}

Json text_record(const std::vector<std::string>& columns, std::string_view kind,
                 std::string_view raw_line) {
    const bool snapshot = kind == "daily-snapshot";
    const std::size_t expected = snapshot ? 10 : 6;
    Json missing = Json::array();
    Json invalid = Json::array();
    if (columns.size() != expected) {
        invalid.push_back("column-count");
        Json result = Json::object();
        result["complete"] = false;
        result["missing_columns"] = std::move(missing);
        result["invalid_columns"] = std::move(invalid);
        result["raw_line"] = std::string(raw_line);
        return result;
    }
    for (const auto index : snapshot
             ? std::vector<std::size_t>{0, 1, 3, 4, 5, 6, 7, 8, 9}
             : std::vector<std::size_t>{0, 1, 3, 4, 5}) {
        if (columns[index].empty())
            append_column(missing, snapshot
                ? kSnapshotColumns[index] : kEntryColumns[index]);
    }

    const int market_id = integer_text(columns[0], -1);
    const auto market = market_from_setcode(columns[0]);
    if (!columns[0].empty() && market.empty()) append_column(invalid, "market");
    if (!columns[3].empty() && !calendar_date(columns[3]))
        append_column(invalid, "entry-date");
    const int entry_time = parse_hhmm(columns[4]);
    if (!columns[4].empty() && entry_time < 0)
        append_column(invalid, "entry-time");
    if (!columns[5].empty() && !number_or_null(columns[5]).is_number())
        append_column(invalid, "entry-price");

    Json result = Json::object();
    result["market_id"] = market_id >= 0 ? Json(market_id) : Json(nullptr);
    result["market"] = market.empty() ? Json(nullptr) : Json(market);
    result["code"] = columns[1];
    result["security_id"] = market.empty() || columns[1].empty()
        ? Json(nullptr) : Json(market + columns[1]);
    result["name"] = columns[2];
    result["name_resolved"] = !columns[2].empty();
    result["entry_date"] = calendar_date(columns[3])
        ? integer_or_null(columns[3]) : Json(nullptr);
    result["entry_date_text"] = calendar_date(columns[3])
        ? Json(columns[3].substr(0, 4) + "-" + columns[3].substr(4, 2) +
               "-" + columns[3].substr(6, 2))
        : Json(nullptr);
    result["entry_time"] = entry_time >= 0 ? Json(entry_time) : Json(nullptr);
    result["entry_time_text"] = entry_time >= 0 ? Json(columns[4]) : Json(nullptr);
    result["entry_time_precision"] = "minute";
    result["entry_price"] = number_or_null(columns[5]);

    if (snapshot) {
        auto rate = columns[6];
        if (!rate.empty() && rate.back() == '%') rate.pop_back();
        result["maximum_rise_pct"] = number_or_null(rate);
        result["maximum_period"] = integer_or_null(columns[7]);
        result["maximum_date"] = Json(nullptr);
        result["maximum_date_month_day"] = month_day(columns[8])
            ? Json(columns[8]) : Json(nullptr);
        result["maximum_date_precision"] = "month-day";
        result["maximum_price"] = number_or_null(columns[9]);
        if (!columns[6].empty() &&
            !result.at("maximum_rise_pct").is_number())
            append_column(invalid, "maximum-rise-pct");
        if (!columns[7].empty() &&
            !result.at("maximum_period").is_number())
            append_column(invalid, "maximum-period");
        if (!columns[8].empty() && !month_day(columns[8]))
            append_column(invalid, "maximum-date");
        if (!columns[9].empty() && !result.at("maximum_price").is_number())
            append_column(invalid, "maximum-price");
    }
    result["missing_columns"] = std::move(missing);
    result["invalid_columns"] = std::move(invalid);
    result["complete"] = result.at("missing_columns").size() == 0 &&
        result.at("invalid_columns").size() == 0;
    result["raw_line"] = std::string(raw_line);
    return result;
}

}  // namespace

Json parse_tpool_history_text_document(const std::string& text,
                                       const std::string& source_name) {
    if (text.size() > 64ULL * 1024ULL * 1024ULL)
        throw Error("TPool history text exceeds 64 MiB safety limit");
    const auto lines = text_lines(text);
    if (lines.empty()) throw Error("TPool history text is empty");
    std::string kind;
    std::size_t expected_columns = 0;
    if (lines.front() == kEntryHeader) {
        kind = "daily-entry-log";
        expected_columns = 6;
    } else if (lines.front() == kSnapshotHeader) {
        kind = "daily-snapshot";
        expected_columns = 10;
    } else {
        throw Error("TPool history text has an unknown header");
    }

    Json records = Json::array();
    std::set<std::string, std::less<>> seen;
    std::size_t complete = 0;
    std::size_t duplicates = 0;
    for (std::size_t index = 1; index < lines.size(); ++index) {
        auto record = text_record(split(lines[index], '|'), kind, lines[index]);
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
    result["source_format"] = "native-pipe-text";
    result["kind"] = kind;
    result["read_only"] = true;
    result["normalized_encoding"] = "UTF-8";
    result["original_encoding"] = "GBK or UTF-8";
    result["record_count"] = static_cast<std::uint64_t>(records.size());
    result["complete_record_count"] = static_cast<std::uint64_t>(complete);
    result["incomplete_record_count"] =
        static_cast<std::uint64_t>(records.size() - complete);
    result["duplicate_record_count"] = static_cast<std::uint64_t>(duplicates);
    result["deduplicate_key"] = "market+code";
    result["expected_column_count"] = static_cast<std::uint64_t>(expected_columns);
    result["records"] = std::move(records);
    result["evidence"] = kind == "daily-snapshot"
        ? "TPool.dll sub_1003AC40 native status history export"
        : "TPool.dll sub_1003A720 native entry history export";
    return result;
}

namespace tpool_detail {

std::string decode_tpool_history_text_file(const fs::path& path) {
    auto bytes = read_bytes(path);
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB &&
        bytes[2] == 0xBF)
        return std::string(bytes.begin() + 3, bytes.end());
    const std::string raw(bytes.begin(), bytes.end());
    bool valid_utf8 = false;
    try {
        (void)utf8_to_wide(raw);
        valid_utf8 = true;
    } catch (const Error&) {
    }
    if (valid_utf8 && known_header(raw)) return raw;
    const auto gbk = decode_gbk(bytes);
    if (known_header(gbk)) return gbk;
    return valid_utf8 ? raw : gbk;
}

}  // namespace tpool_detail
}  // namespace tdx
