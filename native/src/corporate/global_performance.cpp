#include "tdx/global_performance.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kMajorIndices = "list/func_zyzh101.jsn";
constexpr const char* kOverseasChina = "list/func_zgghq101.jsn";
const std::vector<std::string> kResources{kMajorIndices, kOverseasChina};

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used == value.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}

Json change_pct(const std::optional<double>& current,
                const std::optional<double>& reference) {
    return current && reference && std::abs(*reference) > 0.0000001
        ? Json((*current - *reference) * 100.0 / *reference) : Json(nullptr);
}

int integer_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : -1;
    } catch (...) { return -1; }
}

std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    if (market == 2 || market == 44) return "bj";
    return "m" + std::to_string(market);
}

std::string fallback_name(int market, const std::string& code) {
    static const GlobalInstrumentNames values{
        {{1, "000001"}, "上证指数"}, {{0, "399001"}, "深证成指"},
        {{2, "899050"}, "北证50"}, {{0, "399006"}, "创业板指"},
        {{1, "000688"}, "科创50"}, {{1, "000016"}, "上证50"},
        {{1, "000300"}, "沪深300"}, {{1, "000905"}, "中证500"},
        {{1, "000852"}, "中证1000"}, {{27, "HSI"}, "恒生指数"},
        {{27, "HZ5014"}, "恒生国企指数"},
        {{27, "HZ5017"}, "恒生科技指数"},
        {{12, "A_IXIC"}, "纳斯达克综合指数"}};
    const auto found = values.find({market, code});
    return found == values.end() ? std::string{} : found->second;
}

Json instrument_document(int market, const std::string& code,
                         const GlobalInstrumentNames& names) {
    if (market == 44) market = 2;
    if (market < 0 || code.empty()) return Json(nullptr);
    const auto found = names.find({market, code});
    const auto name = found == names.end() ? fallback_name(market, code)
                                           : found->second;
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = "M" + std::to_string(market) + ":" + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
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

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

int bounded(const std::string& value, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}

Json load_local_resource(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local global-performance resource is unavailable: " +
                    path_utf8(path));
    const auto tables = load_jsn_tables(path);
    Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group) {
        const auto& table = tables[group];
        for (const auto& cells : table.rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < table.headers.size(); ++column)
                row[table.headers[column]] = cells[column];
            row["_group"] = static_cast<std::uint64_t>(group);
            rows.push_back(std::move(row));
        }
    }
    Json result = Json::object();
    result["resource"] = resource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path);
    result["rows"] = std::move(rows);
    return result;
}

const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing global-performance resource: " + std::string(resource));
}

std::string sort_field(const std::string& sort) {
    if (sort == "return-5d") return "return_5d_pct";
    if (sort == "return-20d") return "return_20d_pct";
    if (sort == "return-60d") return "return_60d_pct";
    if (sort == "month") return "month_to_date_pct";
    if (sort == "ytd") return "year_to_date_pct";
    if (sort == "turnover-day") return "daily_turnover";
    if (sort == "turnover-5d") return "five_day_turnover";
    if (sort == "pe") return "pe";
    if (sort == "close") return "close";
    if (sort == "code") return "";
    throw Error("unsupported global-performance sort");
}

std::optional<double> normalized_number(const Json& row,
                                        const std::string& name) {
    const auto* value = field(row, name);
    return value && value->is_number() && std::isfinite(value->as_number())
        ? std::optional<double>(value->as_number()) : std::nullopt;
}

}  // namespace

Json normalize_global_performance_rows(
    const std::string& resource, const Json& rows,
    const GlobalInstrumentNames& names) {
    if (!rows.is_array()) throw Error("global-performance rows must be an array");
    if (resource != kMajorIndices && resource != kOverseasChina)
        throw Error("unknown global-performance resource: " + resource);
    const bool major = resource == kMajorIndices;
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        auto instrument = instrument_document(integer_value(raw, "$SC"),
                                              text_value(raw, "$ZQDM"), names);
        if (instrument.is_null()) continue;
        const auto close = number_value(raw, "price0");
        Json item = Json::object();
        item["kind"] = major ? "major-index" : "overseas-china";
        item["kind_label"] = major ? "主要指数" : "海外中资证券";
        item["instrument"] = std::move(instrument);
        item["quote_date"] = text_value(raw, major ? "hqrq" : "date");
        item["close"] = number(raw, "price0");
        item["daily_turnover"] = number(raw, major ? "cje" : "drcje");
        item["five_day_turnover"] = number(raw, major ? "5cje" : "cje");
        item["pe"] = number(raw, "pe");
        item["return_5d_pct"] = change_pct(
            close, number_value(raw, major ? "price5" : "price6"));
        item["return_20d_pct"] = change_pct(
            close, number_value(raw, major ? "price20" : "price21"));
        item["return_60d_pct"] = change_pct(
            close, number_value(raw, major ? "price60" : "price61"));
        item["month_to_date_pct"] = change_pct(close, number_value(raw, "price1"));
        item["year_to_date_pct"] = change_pct(
            close, number_value(raw, major ? "price2" : "price11"));
        item["source_resource"] = resource;
        item["raw"] = raw;
        item["record_id"] = resource + ":" +
            item.at("instrument").at("security_id").as_string();
        result.push_back(std::move(item));
    }
    return result;
}

GlobalPerformanceService::GlobalPerformanceService(
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : securities_(std::move(securities)), jsn_root_(std::move(jsn_root)) {}

GlobalInstrumentNames GlobalPerformanceService::load_names() const {
    GlobalInstrumentNames names;
    for (const auto& [key, security] : securities_) names[key] = security.name;
    if (jsn_root_.empty()) return names;
    for (const auto market : {27, 74}) {
        const auto path = jsn_root_.parent_path() /
            native_path("tdx-market-instruments-" + std::to_string(market) + ".json");
        if (!fs::is_regular_file(path)) continue;
        try {
            const auto document = Json::parse(read_text_utf8(path));
            const auto* instruments = field(document, "instruments");
            if (!instruments || !instruments->is_array()) continue;
            for (const auto& instrument : instruments->as_array()) {
                const auto market_id = integer_value(instrument, "market_id");
                const auto code = text_value(instrument, "code");
                const auto name = text_value(instrument, "name");
                if (market_id >= 0 && !code.empty() && !name.empty())
                    names[{market_id, code}] = name;
            }
        } catch (...) {}
    }
    return names;
}

Json GlobalPerformanceService::fetch_master(
    const GlobalPerformanceQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : kResources)
                documents.push_back(load_local_resource(jsn_root_, resource));
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);
    const auto names = load_names();
    Json records = Json::array();
    Json sources = Json::array();
    for (const auto& resource : kResources) {
        const auto& source = document_for(documents, resource);
        auto normalized = normalize_global_performance_rows(
            resource, source.at("rows"), names);
        Json summary = Json::object();
        for (const auto* key : {"resource", "size", "row_count", "endpoint"})
            summary[key] = source.at(key);
        summary["normalized_row_count"] =
            static_cast<std::uint64_t>(normalized.size());
        sources.push_back(std::move(summary));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json GlobalPerformanceService::query(const GlobalPerformanceQuery& options) {
    const std::set<std::string> views{"all", "major-indices", "overseas-china"};
    if (!views.count(options.view))
        throw Error("view must be all, major-indices, or overseas-china");
    if (options.limit < 1 || options.limit > 20000)
        throw Error("limit must be in 1..20000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");
    const auto selected_sort = sort_field(options.sort);
    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        const auto kind = text_value(row, "kind");
        if (options.view == "major-indices" && kind != "major-index") continue;
        if (options.view == "overseas-china" && kind != "overseas-china") continue;
        const auto& instrument = row.at("instrument");
        if (!options.market.empty() &&
            instrument.at("market").as_string() != lower_ascii(options.market) &&
            std::to_string(static_cast<int>(instrument.at("market_id").as_number())) !=
                options.market) continue;
        if (!options.code.empty() && instrument.at("code").as_string() != options.code)
            continue;
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        records.push_back(row);
    }
    const bool ascending = options.order == "asc";
    std::stable_sort(records.as_array().begin(), records.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto lid = left.at("record_id").as_string();
            const auto rid = right.at("record_id").as_string();
            if (selected_sort.empty()) return ascending ? lid < rid : lid > rid;
            const auto lv = normalized_number(left, selected_sort);
            const auto rv = normalized_number(right, selected_sort);
            if (lv && rv && *lv != *rv) return ascending ? *lv < *rv : *lv > *rv;
            if (lv.has_value() != rv.has_value()) return lv.has_value();
            return lid < rid;
        });
    std::map<std::string, std::uint64_t> counts;
    std::size_t unresolved = 0;
    std::set<std::string> dates;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "kind")];
        if (!row.at("instrument").at("name_resolved").as_bool()) ++unresolved;
        const auto date = text_value(row, "quote_date");
        if (!date.empty()) dates.insert(date);
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    if (!options.include_raw)
        for (auto& row : records.as_array()) row.as_object().erase("raw");
    Json summary = Json::object();
    summary["major_indices"] = counts["major-index"];
    summary["overseas_china"] = counts["overseas-china"];
    summary["unresolved_names"] = static_cast<std::uint64_t>(unresolved);
    summary["latest_quote_date"] = dates.empty() ? "" : *dates.rbegin();
    Json result = Json::object();
    result["schema"] = "tdx-market-global-performance-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["sort"] = options.sort;
    result["order"] = options.order;
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "Returns are reproduced from the CFG reference-close columns because the "
        "downloaded JSN omits calculated presentation columns. Turnover retains each "
        "source market's native currency/unit and is therefore not aggregated.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_global_performance(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market global-performance [options]\n\n"
            "Native major-index and overseas China-security performance snapshot.\n\n"
            "  --view all|major-indices|overseas-china\n"
            "  --sort return-5d|return-20d|return-60d|month|ytd|turnover-day|turnover-5d|pe|close|code\n"
            "  --order asc|desc --query TEXT --market MARKET --code CODE\n"
            "  --without-raw --refresh --root PATH --input-dir PATH --limit N\n"
            "  --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    GlobalPerformanceQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.sort = lower_ascii(trim(args.take_option("--sort", "return-5d")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "10000"),
                          "--limit", 1, 20000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-global-performance-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    GlobalPerformanceService service(std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " global-performance rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
