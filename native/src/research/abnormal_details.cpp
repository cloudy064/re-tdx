#include "tdx/abnormal_details.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto exact = row.as_object().find(key);
    if (exact != row.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name) == wanted) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

std::optional<double> parse_number(std::string value) {
    value = trim(std::move(value));
    value.erase(std::remove(value.begin(), value.end(), ','), value.end());
    if (!value.empty() && value.back() == '%') value.pop_back();
    if (value.empty() || value == "-" || value == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const double parsed = std::stod(value, &used);
        if (used == value.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

std::optional<double> between_number(const std::string& value,
                                     const std::string& left_marker,
                                     const std::string& right_marker) {
    const auto left = value.find(left_marker);
    if (left == std::string::npos) return std::nullopt;
    const auto begin = left + left_marker.size();
    const auto right = value.find(right_marker, begin);
    if (right == std::string::npos || right < begin) return std::nullopt;
    return parse_number(value.substr(begin, right - begin));
}

std::string between_text(const std::string& value,
                         const std::string& left_marker,
                         const std::string& right_marker) {
    const auto left = value.find(left_marker);
    if (left == std::string::npos) return {};
    const auto begin = left + left_marker.size();
    const auto right = value.find(right_marker, begin);
    if (right == std::string::npos || right < begin) return {};
    return trim(value.substr(begin, right - begin));
}

bool digits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(),
        [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
}

std::string compact_date(std::string value) {
    value = trim(std::move(value));
    if (value.empty() || value == "0") return "0";
    if (value.size() == 10 && value[4] == '-' && value[7] == '-')
        value = value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2);
    if (value.size() != 8 || !digits(value))
        throw Error("date must be YYYYMMDD, YYYY-MM-DD, or 0");
    std::tm parsed{};
    parsed.tm_year = std::stoi(value.substr(0, 4)) - 1900;
    parsed.tm_mon = std::stoi(value.substr(4, 2)) - 1;
    parsed.tm_mday = std::stoi(value.substr(6, 2));
    parsed.tm_hour = 12;
    const auto year = parsed.tm_year, month = parsed.tm_mon, day = parsed.tm_mday;
    if (std::mktime(&parsed) == -1 || parsed.tm_year != year ||
        parsed.tm_mon != month || parsed.tm_mday != day)
        throw Error("date is outside the supported calendar range");
    return value;
}

Json display_date(const std::string& value) {
    if (value == "0" || value.empty()) return Json(nullptr);
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
}

std::string canonical_view(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "summary") return value;
    if (value == "explanation" || value == "detail" || value == "reason")
        return "explanation";
    throw Error("view must be summary or explanation");
}

int board_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "all" || value == "0") return 0;
    if (value == "sz-main" || value == "1") return 1;
    if (value == "gem" || value == "2") return 2;
    if (value == "sh-main" || value == "3") return 3;
    if (value == "star" || value == "4") return 4;
    if (value == "bj" || value == "5") return 5;
    throw Error("board must be all, sz-main, gem, sh-main, star, or bj");
}

std::string board_name(int value) {
    return value == 0 ? "all" : value == 1 ? "sz-main" : value == 2 ? "gem"
        : value == 3 ? "sh-main" : value == 4 ? "star" : "bj";
}

const std::map<int, std::string>& type_names() {
    static const std::map<int, std::string> values{
        {101,"涨幅偏离较高"},{102,"跌幅偏离较高"},{103,"日振幅较高"},
        {104,"日换手率较高"},{105,"无价格涨跌幅限制的证券"},
        {121,"3日涨幅偏离值累计较高"},{122,"3日跌幅偏离值累计较高"},
        {123,"3日换手率累计较高"},{131,"10日内3次同正向异常波动"},
        {132,"10日内3次同负向异常波动"},{133,"10日涨幅偏离值达100%"},
        {134,"10日跌幅偏离值达50%"},{135,"30日涨幅偏离值达200%"},
        {136,"30日跌幅偏离值达70%"},{141,"出现异常波动停牌1"},
        {142,"出现异常波动停牌2"},{143,"出现异常波动停牌3"},
        {181,"退市整理的证券"},{191,"实施特别停牌的证券"}};
    return values;
}

int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

int type_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value.empty() || value == "all" || value == "0") return 0;
    const int code = bounded(value, "type", 1, 999);
    if (!type_names().count(code))
        throw Error("type is not one of the 19 client anomaly codes");
    return code;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz" || value == "0") return 0;
    if (value == "sh" || value == "1") return 1;
    if (value == "bj" || value == "2" || value == "44") return 2;
    throw Error("market must be sz, sh, or bj");
}

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : "bj";
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : "BJ";
}

Json identity_document(const BlockData& blocks, int market,
                       const std::string& code) {
    Json result = Json::object();
    const auto found = blocks.securities.find({market, code});
    const auto name = found == blocks.securities.end() ? std::string{} : found->second.name;
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name.empty() ? Json(nullptr) : Json(name);
    result["name_resolved"] = !name.empty();
    return result;
}

Json type_catalog() {
    Json result = Json::array();
    for (const auto& [code, name] : type_names()) {
        Json item = Json::object();
        item["code"] = code;
        item["name"] = name;
        item["possible_suspension_check"] =
            (code >= 121 && code <= 122) || (code >= 131 && code <= 136);
        result.push_back(std::move(item));
    }
    return result;
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

std::string cache_key(const AbnormalDetailsQuery& query) {
    return query.view + '|' + query.board + '|' + query.type + '|' + query.date +
           '|' + query.market + '|' + query.code;
}

Json source_document(const Json& upstream, const std::string& request_id,
                     std::size_t decoded_rows) {
    Json result = Json::object();
    result["transport"] = "TQLEX reqformat=2";
    result["request_id"] = request_id;
    result["entry"] = upstream.at("entry");
    result["source_file"] = upstream.at("source_file");
    result["module"] = "mod_mdsi.dll";
    const auto* sets = field(upstream.at("response"), "ResultSets");
    if (sets && sets->is_array() && sets->size()) {
        const auto& table = sets->as_array().front();
        const auto* descriptions = field(table, "ColDes");
        const auto declared_rows = parse_number(text(table, "RowNum"));
        const auto declared_columns = parse_number(text(table, "ColNum"));
        const std::size_t decoded_columns = descriptions && descriptions->is_array()
            ? descriptions->size() : 0;
        Json metadata = Json::object();
        metadata["declared_row_count"] = number_json(declared_rows);
        metadata["decoded_row_count"] = static_cast<std::uint64_t>(decoded_rows);
        metadata["declared_column_count"] = number_json(declared_columns);
        metadata["decoded_column_count"] = static_cast<std::uint64_t>(decoded_columns);
        metadata["consistent"] = declared_rows && declared_columns &&
            static_cast<std::size_t>(*declared_rows) == decoded_rows &&
            static_cast<std::size_t>(*declared_columns) == decoded_columns;
        metadata["row_count_trusted"] = "decoded-content";
        result["result_set_metadata"] = std::move(metadata);
    }
    return result;
}

Json available_views() {
    Json result = Json::array();
    result.push_back("summary");
    result.push_back("explanation");
    return result;
}

}  // namespace

Json normalize_abnormal_summary_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("abnormal summary rows must be an array");
    if (rows.size() == 0) return Json(nullptr);
    if (rows.size() != 1 || !rows.as_array().front().is_object())
        throw Error("abnormal summary must contain at most one object row");
    const auto value = text(rows.as_array().front(), "PercentInfo");
    if (value.empty()) return Json(nullptr);
    Json result = Json::object();
    auto refresh = between_text(value, "刷新时间", "预测准确率");
    if (refresh.size() == 5 && refresh[2] == ':') refresh += ":00";
    result["refresh_time"] = refresh.empty() ? Json(nullptr) : Json(refresh);
    result["predicted_accuracy_pct"] = number_json(between_number(
        value, "预测准确率", "共"));
    result["reported_count"] = number_json(between_number(value, "共", "条"));
    result["upstream_text"] = value;
    return result;
}

Json normalize_abnormal_explanation_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("abnormal explanation rows must be an array");
    if (rows.size() == 0) return Json(nullptr);
    if (rows.size() != 1 || !rows.as_array().front().is_object())
        throw Error("abnormal explanation must contain at most one object row");
    const auto value = text(rows.as_array().front(), "Detail");
    if (value.empty()) return Json(nullptr);
    Json result = Json::object();
    const auto separator = value.find(':');
    result["reason"] = separator == std::string::npos
        ? Json(value) : Json(trim(value.substr(0, separator)));
    result["cumulative_deviation_pct"] = number_json(between_number(
        value, "累计偏离值:", "%"));
    result["cumulative_volume_shares"] = number_json(between_number(
        value, "累计成交量(股):", "累计成交金额"));
    const auto amount_10k = between_number(
        value, "累计成交金额(万元):", "异常期间");
    result["cumulative_amount_10k_cny"] = number_json(amount_10k);
    result["cumulative_amount_yuan"] = amount_10k
        ? Json(*amount_10k * 10000.0) : Json(nullptr);
    auto period = trim(between_text(value, "异常期间:", "\n"));
    if (period.empty()) {
        const auto marker = value.find("异常期间:");
        if (marker != std::string::npos)
            period = trim(value.substr(marker + std::string("异常期间:").size()));
    }
    Json period_document = Json::object();
    if (period.size() == 17 && period[8] == '-' &&
        digits(period.substr(0, 8)) && digits(period.substr(9, 8))) {
        period_document["start_date"] = display_date(period.substr(0, 8));
        period_document["end_date"] = display_date(period.substr(9, 8));
    } else {
        period_document["start_date"] = Json(nullptr);
        period_document["end_date"] = Json(nullptr);
    }
    result["period"] = std::move(period_document);
    result["upstream_text"] = value;
    return result;
}

AbnormalDetailsService::AbnormalDetailsService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json AbnormalDetailsService::query(const AbnormalDetailsQuery& input) {
    AbnormalDetailsQuery query = input;
    query.view = canonical_view(query.view);
    const int board = board_id(query.board);
    const int type = type_id(query.type);
    query.board = board_name(board);
    query.type = std::to_string(type);
    query.date = compact_date(query.date.empty() ? "0" : query.date);
    int security_market = -1;
    if (query.view == "explanation") {
        security_market = market_id(query.market);
        query.market = market_name(security_market);
        query.code = trim(query.code);
        if (query.code.size() != 6 || !digits(query.code))
            throw Error("explanation view requires a six-digit code");
    } else {
        query.market.clear();
        query.code.clear();
    }
    if (query.cache_ttl_seconds < 0 || query.cache_ttl_seconds > 3600 ||
        query.timeout_ms < 100 || query.timeout_ms > 60000)
        throw Error("abnormal-details query limits are invalid");

    const auto key = cache_key(query);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!query.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        if (age < query.cache_ttl_seconds) {
            auto result = cached->second.document;
            result["cache"]["hit"] = true;
            result["cache"]["age_seconds"] = age;
            return result;
        }
    }

    const std::string request_id = query.view == "summary" ? "500109" : "500108";
    std::map<std::string, std::string> replacements{{"#2111.result", query.type}};
    if (query.view == "explanation") {
        replacements["Code"] = query.code;
        replacements["SetCode"] = std::to_string(security_market);
    }
    const std::map<std::string, std::string> overrides{
        {"DataDate", query.date}, {"Type", query.type},
        {"MarketType", std::to_string(board)}};
    Json upstream;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            upstream = execute_tqlex_config(
                root_, request_id, replacements, overrides, {}, "hq_lhb_dryc.xml",
                {"'MarketType': '0'"}, false, -1, 0, 100,
                cloud_endpoints::tqlex, query.timeout_ms);
            break;
        } catch (const Error& error) {
            const std::string message = error.what();
            if (!transient_error(message)) throw;
            if (attempt == 2) {
                if (cached != cache_.end()) {
                    auto stale = cached->second.document;
                    stale["availability"] = "stale-cache";
                    stale["cache"]["hit"] = true;
                    stale["cache"]["stale"] = true;
                    stale["cache"]["age_seconds"] = static_cast<std::uint64_t>(
                        std::max<std::time_t>(0, now - cached->second.fetched_at));
                    stale["cache"]["upstream_error"] = message;
                    return stale;
                }
                throw;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    const auto data = query.view == "summary"
        ? normalize_abnormal_summary_rows(raw_rows)
        : normalize_abnormal_explanation_rows(raw_rows);
    Json parameters = Json::object();
    parameters["board"] = query.board;
    parameters["type_code"] = type;
    parameters["type_name"] = type == 0 ? Json("全部") : Json(type_names().at(type));
    parameters["data_date"] = display_date(query.date);
    parameters["security"] = query.view == "explanation"
        ? identity_document(blocks_, security_market, query.code) : Json(nullptr);

    Json cache = Json::object();
    cache["hit"] = false; cache["stale"] = false; cache["age_seconds"] = 0;
    cache["ttl_seconds"] = query.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-abnormal-details-native-v1";
    result["availability"] = data.is_null() ? "empty" : "live";
    result["generated_at"] = now_text();
    result["view"] = query.view;
    result["view_title"] = query.view == "summary" ? "异常证券刷新摘要" : "单证券上榜原因";
    result["available_views"] = available_views();
    result["parameters"] = std::move(parameters);
    result["data"] = data;
    result["type_catalog"] = type_catalog();
    result["source"] = source_document(upstream, request_id, raw_rows.size());
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    result["investment_signal"] = false;
    cache_[key] = CachedDocument{result, std::time(nullptr)};
    return result;
}

int command_market_abnormal_details(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market abnormal-details [options]\n\n"
            "Views:\n"
            "  summary       Refresh time, reported count and upstream accuracy\n"
            "  explanation   Structured reason for one security\n\n"
            "Options:\n"
            "  --view summary|explanation --board all|sz-main|gem|sh-main|star|bj\n"
            "  --type all|CODE --date DATE\n"
            "  --market sz|sh|bj --code CODE       Required for explanation\n"
            "  --root PATH --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    AbnormalDetailsQuery query;
    query.view = args.take_option("--view", "summary");
    query.board = args.take_option("--board", "all");
    query.type = args.take_option("--type", "all");
    query.date = args.take_option("--date");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl-seconds", "60"),
        "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-abnormal-details.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    AbnormalDetailsService service(root, load_blocks(root, {}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << (document.at("data").is_null() ? 0 : 1)
              << " abnormal detail rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
