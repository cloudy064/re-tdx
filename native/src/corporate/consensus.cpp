#include "tdx/consensus.hpp"
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
#include <map>
#include <optional>
#include <set>
#include <sstream>

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
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("consensus row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json copy_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? *value : Json(nullptr);
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> numeric_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string() || value->as_string().empty() || value->as_string() == "--")
        return std::nullopt;
    try {
        std::size_t used = 0;
        const double parsed = std::stod(value->as_string(), &used);
        return used == value->as_string().size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

int canonical_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : value == 2 ? "bj"
                                                               : "m" + std::to_string(value);
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : value == 2 ? "BJ"
                                                               : "M" + std::to_string(value);
}

bool six_digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

Json security_document(int market_id, const std::string& code,
                       const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({market_id, code});
    Json result = Json::object();
    result["market"] = market_name(market_id);
    result["market_id"] = market_id;
    result["code"] = code;
    result["security_id"] = market_prefix(market_id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

Json forecast_years(const Json& row, std::string_view base_field,
                    std::string_view eps_prefix) {
    int base_year = 0;
    const auto year_text = text_value(row, base_field);
    try { if (!year_text.empty()) base_year = std::stoi(year_text); }
    catch (...) { base_year = 0; }
    Json forecasts = Json::array();
    for (int offset = 0; offset < 3; ++offset) {
        Json forecast = Json::object();
        forecast["year"] = base_year ? Json(base_year + offset) : Json(nullptr);
        forecast["eps"] = copy_value(row, std::string(eps_prefix) + std::to_string(offset));
        forecast["pe"] = copy_value(row, "PE" + std::to_string(offset));
        forecast["net_profit_yi"] = copy_value(row, "JLR" + std::to_string(offset));
        forecast["revenue_yi"] = copy_value(row, "YYSR" + std::to_string(offset));
        forecasts.push_back(std::move(forecast));
    }
    return forecasts;
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

const Json& document_for_resource(const Json& documents, const std::string& resource) {
    if (!documents.is_array()) throw Error("consensus documents must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing consensus resource: " + resource);
}

const ConsensusCategorySpec& category_spec(const std::string& id) {
    const auto& categories = consensus_categories();
    const auto found = std::find_if(categories.begin(), categories.end(),
        [&](const ConsensusCategorySpec& item) { return item.id == id; });
    if (found == categories.end())
        throw Error("category must be latest, rating-up, rating-down, first-rating, "
                    "revenue-growth, profit-growth, year-high-drawdown, "
                    "consecutive-rise, or year-low-rise");
    return *found;
}

const Json& category_document(const Json& master, const std::string& id) {
    for (const auto& category : master.at("categories").as_array())
        if (category.at("id").as_string() == id) return category;
    throw Error("consensus category is missing from master cache: " + id);
}

bool contains_query(const Json& record, const std::string& query) {
    if (query.empty()) return true;
    const auto needle = lower_ascii(query);
    for (const auto* field : {"code", "name", "security_id"}) {
        const auto value = lower_ascii(text_value(record.at("security"), field));
        if (value.find(needle) != std::string::npos) return true;
    }
    return lower_ascii(text_value(record, "industry")).find(needle) != std::string::npos;
}

Json filtered_records(const Json& records, const ConsensusQuery& options) {
    Json result = Json::array();
    for (const auto& record : records.as_array()) {
        if (!contains_query(record, options.query)) continue;
        result.push_back(record);
        if (result.size() >= static_cast<std::size_t>(options.limit)) break;
    }
    return result;
}

bool same_security(const Json& record, int market_id, const std::string& code) {
    return static_cast<int>(record.at("security").at("market_id").as_number()) == market_id &&
           record.at("security").at("code").as_string() == code;
}

Json reports_for_output(const Json& reports, const ConsensusQuery& options) {
    Json result = Json::array();
    const auto count = std::min(reports.size(), static_cast<std::size_t>(options.report_limit));
    for (std::size_t index = 0; index < count; ++index) {
        Json report = reports.as_array()[index];
        if (!options.include_report_text) report.as_object().erase("report_text");
        result.push_back(std::move(report));
    }
    return result;
}

Json report_summary(const Json& reports) {
    std::set<std::string> institutions, analysts;
    std::map<std::string, std::uint64_t> ratings;
    std::optional<double> target_minimum, target_maximum;
    double target_total = 0;
    std::uint64_t target_count = 0;
    for (const auto& report : reports.as_array()) {
        const auto institution = text_value(report, "institution");
        const auto analyst = text_value(report, "analyst");
        const auto rating = text_value(report, "rating");
        if (!institution.empty()) institutions.insert(institution);
        if (!analyst.empty()) analysts.insert(analyst);
        if (!rating.empty()) ++ratings[rating];
        const auto target = numeric_value(report, "target_price");
        if (target) {
            target_minimum = target_minimum ? std::min(*target_minimum, *target) : target;
            target_maximum = target_maximum ? std::max(*target_maximum, *target) : target;
            target_total += *target;
            ++target_count;
        }
    }
    Json result = Json::object();
    result["report_count"] = static_cast<std::uint64_t>(reports.size());
    result["institution_count"] = static_cast<std::uint64_t>(institutions.size());
    result["analyst_count"] = static_cast<std::uint64_t>(analysts.size());
    result["latest_report_date"] = reports.size()
        ? reports.as_array().front().at("report_date") : Json(nullptr);
    Json rating_counts = Json::object();
    for (const auto& [rating, count] : ratings) rating_counts[rating] = count;
    result["rating_counts"] = std::move(rating_counts);
    Json targets = Json::object();
    targets["count"] = target_count;
    targets["minimum"] = target_minimum ? Json(*target_minimum) : Json(nullptr);
    targets["maximum"] = target_maximum ? Json(*target_maximum) : Json(nullptr);
    targets["average"] = target_count ? Json(target_total / target_count) : Json(nullptr);
    result["target_price"] = std::move(targets);
    return result;
}

}  // namespace

const std::vector<ConsensusCategorySpec>& consensus_categories() {
    static const std::vector<ConsensusCategorySpec> categories{
        {"latest", "最新一致预期", "list/func_yzyq101_1.jsn"},
        {"rating-up", "最新评级调高", "list/func_yzyq102_1.jsn"},
        {"rating-down", "最新评级调低", "list/func_yzyq108_1.jsn"},
        {"first-rating", "机构首次评级", "list/func_yzyq103_1.jsn"},
        {"revenue-growth", "营收复合增速 Top100", "list/func_yzyq104_1.jsn"},
        {"profit-growth", "净利复合增速 Top100", "list/func_yzyq105_1.jsn"},
        {"year-high-drawdown", "年高点至今回撤", "list/func_yzyq106_1.jsn"},
        {"consecutive-rise", "连续上涨", "list/func_yzyq107_1.jsn"},
        {"year-low-rise", "年低点至今涨幅", "list/func_yzyq109_1.jsn"},
    };
    return categories;
}

Json normalize_consensus_master_rows(
    const Json& rows, const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("consensus master rows must be an array");
    (void)category_spec(category);
    Json result = Json::array();
    std::set<std::pair<int, std::string>> identities;
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) throw Error("consensus master security code is invalid");
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        if (!identities.emplace(market_id, code).second)
            throw Error("consensus master contains a duplicate security");
        Json value = Json::object();
        value["category"] = category;
        value["security"] = security_document(market_id, code, securities);
        value["latest_date"] = copy_value(row, "ZXRQ");
        value["industry"] = copy_value(row, "hy");
        value["institution_count"] = copy_value(row, "JGSL");
        value["rating_score"] = copy_value(row, "ZHPJ");
        value["pe"] = copy_value(row, "PE");
        value["expected_eps_growth_pct"] = copy_value(row, "YCEPS");
        value["peg"] = copy_value(row, "PEG");
        value["target_price"] = copy_value(row, "MBJ");
        // These are client-provided ranking fields. Their prices can be sampled at
        // different dates, so expose the source percentages without recomputing them.
        value["latest_close"] = copy_value(row, "zxspj");
        value["year_high_price"] = copy_value(row, "jynzgj");
        value["change_from_year_high_pct"] = copy_value(row, "zgdf");
        value["consecutive_rise_days"] = copy_value(row, "lzts");
        value["year_low_price"] = copy_value(row, "jynzdj");
        value["change_from_year_low_pct"] = copy_value(row, "zdzf");
        value["base_year"] = copy_value(row, "BGQ");
        value["forecasts"] = forecast_years(row, "BGQ", "MGSY");
        Json growth = Json::object();
        growth["revenue_cagr_pct"] = copy_value(row, "YYSRZS");
        growth["profit_cagr_pct"] = copy_value(row, "JLRZS");
        Json revenue = Json::array(), profit = Json::array();
        for (int offset = 0; offset < 3; ++offset) {
            revenue.push_back(copy_value(row, "YYSRZS" + std::to_string(offset)));
            profit.push_back(copy_value(row, "JLRZS" + std::to_string(offset)));
        }
        growth["revenue_yearly_pct"] = std::move(revenue);
        growth["profit_yearly_pct"] = std::move(profit);
        value["growth"] = std::move(growth);
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_consensus_report_rows(const Json& rows, bool include_report_text) {
    if (!rows.is_array()) throw Error("consensus report rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json report = Json::object();
        report["report_date"] = copy_value(row, "BGRQ");
        report["institution"] = copy_value(row, "YJJG");
        report["institution_grade"] = copy_value(row, "jgyxl");
        report["analyst"] = copy_value(row, "FXS");
        report["rating"] = copy_value(row, "T003");
        report["rating_change"] = copy_value(row, "T004");
        report["target_price"] = copy_value(row, "T011");
        report["base_year"] = copy_value(row, "nd");
        const auto body = text_value(row, "ybxq");
        report["report_text_length"] = static_cast<std::uint64_t>(body.size());
        if (include_report_text) report["report_text"] = body;
        report["forecasts"] = forecast_years(row, "nd", "EPS");
        result.push_back(std::move(report));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "report_date") > text_value(right, "report_date");
        });
    return result;
}

ConsensusService::ConsensusService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

ConsensusService::FetchResult ConsensusService::fetch_master(
    const ConsensusQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at && age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    std::vector<std::string> resources;
    for (const auto& category : consensus_categories()) resources.push_back(category.resource);
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    Json document = Json::object(), categories = Json::array(), sources = Json::array();
    std::set<std::pair<int, std::string>> securities;
    std::uint64_t total_rows = 0;
    for (const auto& spec : consensus_categories()) {
        const auto& source = document_for_resource(documents, spec.resource);
        auto records = normalize_consensus_master_rows(source.at("rows"), spec.id, securities_);
        for (const auto& record : records.as_array()) {
            securities.emplace(
                static_cast<int>(record.at("security").at("market_id").as_number()),
                record.at("security").at("code").as_string());
        }
        Json category = Json::object();
        category["id"] = spec.id;
        category["label"] = spec.label;
        category["resource"] = spec.resource;
        category["record_count"] = static_cast<std::uint64_t>(records.size());
        category["records"] = std::move(records);
        total_rows += static_cast<std::uint64_t>(category.at("record_count").as_number());
        categories.push_back(std::move(category));
        sources.push_back(source_summary(source));
    }
    document["categories"] = std::move(categories);
    document["sources"] = std::move(sources);
    document["total_rows"] = total_rows;
    document["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

ConsensusService::FetchResult ConsensusService::fetch_detail(
    int market_id, const std::string& code, const ConsensusQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = detail_cache_.begin(); item != detail_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_cache_.erase(item);
        else ++item;
    }
    const auto key = std::to_string(market_id) + code;
    const auto cached = detail_cache_.find(key);
    const int age = cached == detail_cache_.end() ? 0
        : static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != detail_cache_.end())
        return {cached->second.document, false, age};
    const auto source = fetch_jsn_resource_rows("yzyq/" + key + ".jsn", "bi", options.timeout_ms);
    Json document = Json::object();
    document["reports"] = normalize_consensus_report_rows(source.at("rows"), true);
    Json sources = Json::array();
    sources.push_back(source_summary(source));
    document["sources"] = std::move(sources);
    detail_cache_[key] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

Json ConsensusService::query(const ConsensusQuery& options) {
    (void)category_spec(options.category);
    if (options.limit < 1 || options.limit > 5000) throw Error("limit must be in 1..5000");
    if (options.report_limit < 1 || options.report_limit > 500)
        throw Error("report_limit must be in 1..500");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("consensus cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    if (security_mode && !six_digits(options.code)) throw Error("code must contain six digits");

    auto master = fetch_master(options);
    const auto& selected_category = category_document(master.document, options.category);
    Json records = Json::array();
    if (security_mode) {
        const int selected_market = canonical_market_id(options.market);
        for (const auto& record : selected_category.at("records").as_array())
            if (same_security(record, selected_market, options.code)) records.push_back(record);
    } else {
        records = filtered_records(selected_category.at("records"), options);
    }
    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    Json result = Json::object();
    result["schema"] = "tdx-market-consensus-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = security_mode ? "security" : "master";
    result["category"] = options.category;
    result["category_label"] = selected_category.at("label");
    result["query"] = options.query;
    Json catalog = Json::array();
    for (const auto& category : master.document.at("categories").as_array()) {
        Json item = Json::object();
        for (const auto* key : {"id", "label", "resource", "record_count"})
            item[key] = category.at(key);
        catalog.push_back(std::move(item));
    }
    result["categories"] = std::move(catalog);
    result["records"] = std::move(records);
    result["selected_security"] = Json(nullptr);
    result["selected_consensus"] = Json(nullptr);
    result["category_memberships"] = Json::array();
    result["reports"] = Json::array();
    result["report_summary"] = Json::object();
    result["sources"] = master.document.at("sources");
    Json counts = Json::object();
    counts["categories"] = static_cast<std::uint64_t>(master.document.at("categories").size());
    counts["total_rows"] = master.document.at("total_rows");
    counts["unique_securities"] = master.document.at("unique_securities");
    counts["category_rows"] = selected_category.at("record_count");
    counts["returned_records"] = static_cast<std::uint64_t>(result.at("records").size());
    counts["reports"] = 0;
    counts["full_reports"] = 0;

    if (security_mode || options.include_details) {
        int market_id = 0;
        std::string code;
        if (security_mode) {
            market_id = canonical_market_id(options.market);
            code = options.code;
        } else {
            if (!result.at("records").size())
                throw Error("cannot expand details from an empty consensus category");
            const auto& security = result.at("records").as_array().front().at("security");
            market_id = static_cast<int>(security.at("market_id").as_number());
            code = security.at("code").as_string();
        }
        result["selected_security"] = security_document(market_id, code, securities_);
        Json memberships = Json::array();
        const Json* preferred = nullptr;
        for (const auto& category : master.document.at("categories").as_array()) {
            for (const auto& record : category.at("records").as_array()) {
                if (!same_security(record, market_id, code)) continue;
                Json membership = Json::object();
                membership["category"] = category.at("id");
                membership["category_label"] = category.at("label");
                membership["record"] = record;
                memberships.push_back(std::move(membership));
                if (!preferred || category.at("id").as_string() == "latest") preferred = &record;
            }
        }
        result["category_memberships"] = std::move(memberships);
        result["selected_consensus"] = preferred ? *preferred : Json(nullptr);
        auto detail = fetch_detail(market_id, code, options);
        const auto reports = reports_for_output(detail.document.at("reports"), options);
        result["reports"] = reports;
        result["report_summary"] = report_summary(detail.document.at("reports"));
        for (const auto& source : detail.document.at("sources").as_array())
            result["sources"].push_back(source);
        counts["reports"] = static_cast<std::uint64_t>(reports.size());
        counts["full_reports"] = static_cast<std::uint64_t>(detail.document.at("reports").size());
        cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
        cache["detail_refreshed"] = detail.refreshed;
        cache["detail_age_seconds"] = detail.age_seconds;
    }
    result["counts"] = std::move(counts);
    const auto upstream_health = jsn_sources_health(result.at("sources"));
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" : "live";
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_consensus(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market consensus [options]\n\n"
            "Native consensus forecasts, rating lists, and per-security research reports.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --category NAME         latest/rating-up/rating-down/first-rating/\n"
            "                          revenue-growth/profit-growth/year-high-drawdown/\n"
            "                          consecutive-rise/year-low-rise\n"
            "  --query TEXT            Filter master rows by code, name, or industry\n"
            "  --market sz|sh|bj       Select one security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --details               Expand the first filtered master row\n"
            "  --limit N               Master rows, default 500\n"
            "  --report-limit N        Per-security reports, default 100\n"
            "  --no-report-text        Omit long research report bodies\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-consensus-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ConsensusQuery query;
    query.category = lower_ascii(trim(args.take_option("--category", "latest")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.include_details = args.take_flag("--details");
    query.include_report_text = !args.take_flag("--no-report-text");
    query.limit = bounded_integer(args.take_option("--limit", "500"), "--limit", 1, 5000);
    query.report_limit = bounded_integer(args.take_option("--report-limit", "100"),
                                         "--report-limit", 1, 500);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-consensus-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    ConsensusService service(load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string()
              << " consensus query with "
              << document.at("counts").at("returned_records").as_number()
              << " master rows and " << document.at("counts").at("reports").as_number()
              << " reports -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
