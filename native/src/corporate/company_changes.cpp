#include "tdx/company_changes.hpp"
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

constexpr const char* kSecurityRenames = "list/func_zqbg101_1.jsn";
constexpr const char* kCompanyRenames = "list/func_zqbg102_1.jsn";
constexpr const char* kMainlandIndex = "list/func_zqbg103_1.jsn";
constexpr const char* kMajorEquity = "list/func_zqbg104_1.jsn";
constexpr const char* kHkIndex = "list/func_zqbg105_1.jsn";
constexpr const char* kControllers = "list/func_zqbg107_1.jsn";
constexpr const char* kIndustries = "list/func_zqbg108_1.jsn";
constexpr const char* kEquityTransfers = "list/func_zqbg109_1.jsn";
constexpr const char* kNeeqIndex = "list/func_zqbg110_1.jsn";

const std::vector<std::string> kResources{
    kSecurityRenames, kCompanyRenames, kMainlandIndex, kMajorEquity,
    kHkIndex, kControllers, kIndustries, kEquityTransfers, kNeeqIndex};

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

Json multiplied_number(const Json& row, std::string_view name, double factor) {
    const auto value = number_value(row, name);
    return value ? Json(*value * factor) : Json(nullptr);
}

bool digits(const std::string& value) {
    return !value.empty() &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
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
    if (market == 2) return "bj";
    if (market == 31) return "hk";
    if (market == 44) return "neeq";
    return "m" + std::to_string(market);
}

std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    if (market == 2) return "BJ";
    if (market == 31) return "HK";
    if (market == 44) return "NEEQ";
    return "M" + std::to_string(market);
}

Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (market < 0 || !digits(code)) return Json(nullptr);
    std::string name;
    auto found = securities.find({market, code});
    if (found == securities.end() && market == 44)
        found = securities.find({2, code});
    if (found != securities.end()) name = found->second.name;
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

std::string kind_for(const std::string& resource) {
    if (resource == kSecurityRenames) return "security-renames";
    if (resource == kCompanyRenames) return "company-renames";
    if (resource == kMainlandIndex) return "mainland-index";
    if (resource == kMajorEquity) return "major-equity";
    if (resource == kHkIndex) return "hk-index";
    if (resource == kControllers) return "controllers";
    if (resource == kIndustries) return "industries";
    if (resource == kEquityTransfers) return "equity-transfers";
    if (resource == kNeeqIndex) return "neeq-index";
    throw Error("unknown company-changes resource: " + resource);
}

std::string label_for(const std::string& kind) {
    if (kind == "security-renames") return "证券更名";
    if (kind == "company-renames") return "公司更名";
    if (kind == "mainland-index") return "沪深京指数调整";
    if (kind == "major-equity") return "重大股权变更";
    if (kind == "hk-index") return "港股指数调整";
    if (kind == "controllers") return "实控人变更";
    if (kind == "industries") return "行业变更";
    if (kind == "equity-transfers") return "股权转让";
    return "新三板指数调整";
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

int bounded(const std::string& value, std::string_view name,
            int low, int high) {
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

Json load_local_resource_rows(const fs::path& root,
                              const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local company-changes resource is unavailable: " +
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
    throw Error("missing company-changes resource: " + std::string(resource));
}

Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized_rows);
    return result;
}

void add_text(Json& item, const Json& row, const char* output,
              const char* input) {
    item[output] = text_value(row, input);
}

void add_number(Json& item, const Json& row, const char* output,
                const char* input) {
    item[output] = number(row, input);
}

}  // namespace

Json normalize_company_change_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("company-changes rows must be an array");
    const auto kind = kind_for(resource);
    Json result = Json::array();
    std::uint64_t rank = 0;
    for (const auto& row : rows.as_array()) {
        ++rank;
        auto market = integer_value(row, "$SC");
        // A handful of historical NEEQ/HK index rows intentionally omit the
        // host market column even though the resource itself fixes the market.
        if (market < 0 && resource == kNeeqIndex) market = 44;
        if (market < 0 && resource == kHkIndex) market = 31;
        const auto code = text_value(row, "$ZQDM");
        auto security = security_document(market, code, securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = label_for(kind);
        item["security"] = std::move(security);
        item["source_resource"] = resource;
        item["source_rank"] = rank;
        item["raw"] = row;
        std::string event_date;

        if (kind == "security-renames") {
            add_text(item, row, "announcement_date", "ggdate");
            add_text(item, row, "effective_date", "date");
            add_text(item, row, "old_name", "oname");
            add_number(item, row, "effective_close", "price1");
            add_number(item, row, "announcement_close", "price2");
            item["rename_count"] = integer_value(row, "bgcs");
            add_text(item, row, "history", "bgls");
            event_date = text_value(row, "date");
        } else if (kind == "company-renames") {
            add_text(item, row, "announcement_date", "ggdate");
            add_text(item, row, "effective_date", "date");
            add_text(item, row, "old_name", "oname");
            add_text(item, row, "new_name", "nname");
            add_number(item, row, "effective_close", "price1");
            add_number(item, row, "announcement_close", "price2");
            event_date = text_value(row, "date");
        } else if (kind == "mainland-index" || kind == "hk-index" ||
                   kind == "neeq-index") {
            add_text(item, row, "announcement_date", "gbrq");
            add_text(item, row, "direction", "tzlj");
            add_text(item, row, "index_name", "zqbg");
            add_text(item, row, "effective_date", "tzrq");
            add_number(item, row, "announcement_close", "price1");
            add_number(item, row, "effective_close", "price2");
            event_date = text_value(row, "tzrq");
        } else if (kind == "major-equity") {
            add_text(item, row, "announcement_date", "date");
            add_text(item, row, "seller", "NBD1");
            add_text(item, row, "buyer", "NBD2");
            add_number(item, row, "shares_10k", "NBD3");
            item["shares"] = multiplied_number(row, "NBD3", 10000.0);
            add_number(item, row, "total_share_pct", "NBD4");
            add_text(item, row, "method", "NBD5");
            add_text(item, row, "details", "XQ");
            add_text(item, row, "status", "JD");
            add_text(item, row, "updated_date", "RQ");
            event_date = text_value(row, "date");
        } else if (kind == "controllers") {
            add_text(item, row, "change_date", "date");
            add_number(item, row, "pre_change_close", "price1");
            add_text(item, row, "before_controller", "kggd1");
            add_text(item, row, "after_controller", "kggd2");
            add_text(item, row, "ownership_chain", "kggx");
            add_text(item, row, "industry", "hy");
            event_date = text_value(row, "date");
        } else if (kind == "industries") {
            add_text(item, row, "change_date", "date");
            add_number(item, row, "pre_change_close", "price1");
            add_text(item, row, "before_industry", "hymc1");
            add_text(item, row, "after_industry", "hymc2");
            add_text(item, row, "cross_industry", "sfkhy");
            add_text(item, row, "business_change", "zybg");
            event_date = text_value(row, "date");
        } else {
            add_text(item, row, "announcement_date", "date");
            add_text(item, row, "status", "jd");
            add_number(item, row, "pre_announcement_close", "price1");
            add_number(item, row, "transfer_shares", "zrgb");
            add_number(item, row, "total_amount_yuan", "zkx");
            add_number(item, row, "total_share_pct", "zrgbzb");
            add_text(item, row, "seller", "zrf");
            add_text(item, row, "buyer", "jsf");
            add_number(item, row, "seller_post_transfer_pct", "zrhcgbl1");
            add_number(item, row, "buyer_post_transfer_pct", "zrhcgbl2");
            add_text(item, row, "buyer_controller", "jsfskr");
            add_text(item, row, "buyer_type", "lx");
            add_text(item, row, "controller_changed", "skr");
            const auto amount = number_value(row, "zkx");
            const auto shares = number_value(row, "zrgb");
            item["transfer_price_yuan"] = amount && shares && *shares != 0.0
                ? Json(*amount / *shares) : Json(nullptr);
            event_date = text_value(row, "date");
        }
        item["event_date"] = event_date;
        item["event_id"] = kind + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            event_date + ":" + std::to_string(rank);
        result.push_back(std::move(item));
    }
    return result;
}

CompanyChangesService::CompanyChangesService(
    fs::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json CompanyChangesService::fetch_master(
    const CompanyChangesQuery& options, bool& refreshed, int& age_seconds) {
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
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);

    Json records = Json::array();
    Json sources = Json::array();
    for (const auto& resource : kResources) {
        const auto& document = document_for(documents, resource);
        auto normalized = normalize_company_change_rows(
            resource, document.at("rows"), securities_);
        sources.push_back(source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    std::sort(records.as_array().begin(), records.as_array().end(),
              [](const Json& left, const Json& right) {
        const auto ld = text_value(left, "event_date");
        const auto rd = text_value(right, "event_date");
        if (ld != rd) return ld > rd;
        return left.at("event_id").as_string() < right.at("event_id").as_string();
    });
    Json result = Json::object();
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json CompanyChangesService::query(const CompanyChangesQuery& options) {
    const std::set<std::string> views{
        "all", "security-renames", "company-renames", "mainland-index",
        "major-equity", "hk-index", "controllers", "industries",
        "equity-transfers", "neeq-index"};
    if (!views.count(options.view)) throw Error("unsupported company-changes view");
    if (options.limit < 1 || options.limit > 20000)
        throw Error("limit must be in 1..20000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const auto selected_market = lower_ascii(trim(options.market));
    if (!selected_market.empty() && selected_market != "sz" &&
        selected_market != "sh" && selected_market != "bj" &&
        selected_market != "hk" && selected_market != "neeq")
        throw Error("market must be sz/sh/bj/hk/neeq");
    if (!options.code.empty() && !digits(options.code))
        throw Error("code must contain digits");

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "kind") != options.view)
            continue;
        if (!selected_market.empty()) {
            const auto& security = row.at("security");
            if (security.at("market").as_string() != selected_market ||
                security.at("code").as_string() != options.code) continue;
        }
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        auto projected = row;
        if (!options.include_raw) projected.as_object().erase("raw");
        records.push_back(std::move(projected));
    }

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    std::string earliest, latest;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "kind")];
        securities.insert(row.at("security").at("security_id").as_string());
        const auto event_date = text_value(row, "event_date");
        if (!event_date.empty()) {
            if (earliest.empty() || event_date < earliest) earliest = event_date;
            if (event_date > latest) latest = event_date;
        }
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json summary = Json::object();
    for (const auto& view : views)
        if (view != "all") summary[view] = counts[view];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    summary["earliest_date"] = earliest;
    summary["latest_date"] = latest;

    Json result = Json::object();
    result["schema"] = "tdx-market-company-changes-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "ZQBG company and security change facts are normalized from nine client "
        "resources. Static reference prices and source percentages are retained; "
        "host-injected current quotes and derived return-to-now columns are not fabricated. "
        "NBD3 is explicitly exposed in ten-thousand shares and shares.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_company_changes(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market company-changes [options]\n\n"
            "Native security/company renames, index changes, ownership and industry changes.\n\n"
            "  --view all|security-renames|company-renames|mainland-index|major-equity\n"
            "         hk-index|controllers|industries|equity-transfers|neeq-index\n"
            "  --query TEXT --market sz|sh|bj|hk|neeq --code CODE\n"
            "  --without-raw --refresh --root PATH --input-dir PATH --limit N\n"
            "  --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    CompanyChangesQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
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
        "--output", "output/tdx-market-company-changes-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    CompanyChangesService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " company-change rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
