#include "disclosures_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx::disclosure_detail {

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) throw Error("disclosure row must be an object");
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json& row, std::string_view name) {
    const auto text = text_value(row, name);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto value = std::stod(text, &used);
        return used == text.size() && std::isfinite(value)
            ? std::optional<double>(value) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

Json number_or_null(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int integer_market(const Json& row) {
    const auto value = text_value(row, "$SC");
    try {
        std::size_t used = 0;
        const int result = std::stoi(value, &used);
        return used == value.size() ? result : -1;
    } catch (...) {
        return -1;
    }
}

std::string market_name(int id) {
    if (id == 0) return "sz";
    if (id == 1) return "sh";
    if (id == 2 || id == 44) return "bj";
    if (id == 31 || id == 48) return "hk";
    return "m" + std::to_string(id);
}

std::string market_prefix(int id) {
    if (id == 0) return "SZ";
    if (id == 1) return "SH";
    if (id == 2 || id == 44) return "BJ";
    if (id == 31 || id == 48) return "HK";
    return "M" + std::to_string(id);
}

Json security_document(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const int market = integer_market(row);
    const auto code = text_value(row, "$ZQDM");
    if (market < 0 || code.empty()) return Json(nullptr);
    const auto found = securities.find({market == 44 ? 2 : market, code});
    auto name = text_value(row, "ZQJC");
    if (name.empty()) name = text_value(row, "$ZQJC");
    if (name.empty() && found != securities.end()) name = found->second.name;
    Json result = Json::object();
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

std::string disclosure_status(const Json& row) {
    if (!text_value(row, "spldate").empty()) return "disclosed";
    if (!text_value(row, "date1").empty() ||
        !text_value(row, "date2").empty() ||
        !text_value(row, "date3").empty() ||
        !text_value(row, "plqk").empty()) return "rescheduled";
    return "scheduled";
}

Json change_dates(const Json& row) {
    Json result = Json::array();
    for (const auto* key : {"date1", "date2", "date3"}) {
        const auto date = text_value(row, key);
        if (!date.empty()) result.push_back(date);
    }
    return result;
}

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        result[key] = document.at(key);
    return result;
}

const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing disclosure resource: " + std::string(resource));
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

std::string today_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local date");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local date");
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d");
    return output.str();
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

int bounded(const std::string& text, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}

bool market_matches(const Json& security, const std::string& requested) {
    if (requested.empty()) return true;
    const auto normalized = lower_ascii(trim(requested));
    const int id = static_cast<int>(security.at("market_id").as_number());
    if (normalized == "hk") return id == 31 || id == 48;
    if (normalized == "sz" || normalized == "0") return id == 0;
    if (normalized == "sh" || normalized == "1") return id == 1;
    if (normalized == "bj" || normalized == "2" || normalized == "44")
        return id == 2 || id == 44;
    try {
        std::size_t used = 0;
        const int expected = std::stoi(normalized, &used);
        return used == normalized.size() && id == expected;
    } catch (...) {
        throw Error("market must be sz, sh, bj, hk, or a numeric TDX market id");
    }
}

std::string primary_date(const Json& row) {
    auto date = text_value(row, "actual_disclosure_date");
    if (date.empty()) date = text_value(row, "scheduled_disclosure_date");
    return date;
}

int mainland_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "sz" || normalized == "0") return 0;
    if (normalized == "sh" || normalized == "1") return 1;
    if (normalized == "bj" || normalized == "2" || normalized == "44") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string scalar_text(const Json& value) {
    if (value.is_null()) return {};
    if (value.is_string()) return trim(value.as_string());
    return trim(jsn_scalar_text(value));
}

std::vector<std::string> tqlex_columns(const Json& value) {
    std::vector<std::string> result;
    if (value.is_array()) {
        for (const auto& item : value.as_array()) result.push_back(scalar_text(item));
        return result;
    }
    if (!value.is_string()) throw Error("announcement TQLEX ColName is invalid");
    std::istringstream input(value.as_string());
    for (std::string item; input >> item;) result.push_back(std::move(item));
    return result;
}

Json tqlex_rows(const Json& response) {
    if (!response.is_object()) throw Error("announcement TQLEX response must be an object");
    const Json* body = &response;
    const auto wrapped = response.as_object().find("response");
    if (wrapped != response.as_object().end()) body = &wrapped->second;
    if (!body->is_object()) throw Error("announcement TQLEX response body is invalid");
    const auto sets = body->as_object().find("ResultSets");
    if (sets == body->as_object().end() || !sets->second.is_array() ||
        sets->second.as_array().empty()) return Json::array();
    const auto& set = sets->second.as_array().front();
    if (!set.is_object()) throw Error("announcement TQLEX result set is invalid");
    const auto columns = set.as_object().find("ColName");
    const auto content = set.as_object().find("Content");
    if (columns == set.as_object().end())
        throw Error("announcement TQLEX ColName is missing");
    if (content == set.as_object().end()) return Json::array();
    if (!content->second.is_array())
        throw Error("announcement TQLEX Content must be an array");
    const auto names = tqlex_columns(columns->second);
    Json result = Json::array();
    for (const auto& row : content->second.as_array()) {
        if (row.is_object()) {
            result.push_back(row);
            continue;
        }
        if (!row.is_array()) throw Error("announcement TQLEX row must be an array");
        Json item = Json::object();
        for (std::size_t index = 0; index < names.size(); ++index)
            item[names[index]] = index < row.as_array().size()
                ? row.as_array()[index] : Json(nullptr);
        result.push_back(std::move(item));
    }
    return result;
}

std::string compact_announcement_date(const std::string& value) {
    std::string result;
    for (const auto ch : value) {
        if (ch >= '0' && ch <= '9') result.push_back(ch);
        if (result.size() == 8) break;
    }
    return digits(result, 8) ? result : std::string{};
}

std::optional<PeriodicReportType> periodic_report_type(const std::string& typecode) {
    // Shenzhen, Shanghai and Beijing announcement taxonomies use different IDs.
    if (typecode == "010301" || typecode == "101" || typecode == "0101")
        return PeriodicReportType{"annual", "1231", "年度报告"};
    if (typecode == "010305" || typecode == "102" || typecode == "0105")
        return PeriodicReportType{"first-quarter", "0331", "一季度报告"};
    if (typecode == "010303" || typecode == "103" || typecode == "0103")
        return PeriodicReportType{"half-year", "0630", "半年度报告"};
    if (typecode == "010307" || typecode == "104" || typecode == "0106")
        return PeriodicReportType{"third-quarter", "0930", "三季度报告"};
    return std::nullopt;
}

std::string report_year(const std::string& title, const std::string& phrase) {
    const auto report = title.find(phrase);
    if (report == std::string::npos) return {};
    std::string selected;
    for (std::size_t index = 0; index + 4 <= report; ++index) {
        const auto candidate = title.substr(index, 4);
        if (digits(candidate, 4) && title.compare(index + 4, 3, "年") == 0)
            selected = candidate;
    }
    if (selected.empty()) return {};
    const int year = std::stoi(selected);
    return year >= 1990 && year <= 2100 ? selected : std::string{};
}

std::optional<PeriodicReportType> periodic_report_type_for_row(const Json& row) {
    if (auto exact = periodic_report_type(text_value(row, "typecode"))) return exact;
    // A small number of Shanghai full reports are stored under the generic
    // LSGG bucket while their summaries retain the exchange report type.  Only
    // accept an exact formal-report title suffix so audit, assurance and board
    // reports in the same bucket cannot unlock the finance package.
    if (text_value(row, "typecode") != "LSGG") return std::nullopt;
    const auto title = text_value(row, "title");
    const auto typename_value = text_value(row, "typename");
    const std::array<PeriodicReportType, 4> candidates{{
        {"annual", "1231", "年度报告"},
        {"first-quarter", "0331", "一季度报告"},
        {"half-year", "0630", "半年度报告"},
        {"third-quarter", "0930", "三季度报告"},
    }};
    const std::array<const char*, 4> type_names{
        "年报", "一季报", "半年报", "三季报"};
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (typename_value != type_names[index]) continue;
        const auto phrase = std::string(candidates[index].title_phrase);
        const auto position = title.find(phrase);
        if (position == std::string::npos ||
            !trim(title.substr(position + phrase.size())).empty() ||
            report_year(title, phrase).empty()) continue;
        return candidates[index];
    }
    return std::nullopt;
}

Json direct_security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

Json announcement_evidence(const Json& row) {
    Json result = Json::object();
    result["issue_date"] = text_value(row, "issue_date");
    result["title"] = text_value(row, "title");
    result["typecode"] = text_value(row, "typecode");
    result["typename"] = text_value(row, "typename");
    result["url"] = text_value(row, "url");
    result["record_id"] = text_value(row, "rec_id");
    result["source"] = text_value(row, "source");
    result["summary"] = text_value(row, "title").find("摘要") != std::string::npos;
    return result;
}

}  // namespace tdx::disclosure_detail
