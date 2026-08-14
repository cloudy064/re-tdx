#include "tdx/tender_offers.hpp"
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
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* resource_name = "list/func_yysg101_1.jsn";

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto exact = value.as_object().find(key);
    if (exact != value.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto raw = text_value(value, key);
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
}

std::string json_text(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json scaled_number(const std::optional<double>& value, double scale) {
    return value ? Json(*value * scale) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string iso_date(const std::string& value) {
    if (value.empty()) return {};
    if (!digits(value, 8)) return value;
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" +
           value.substr(6, 2);
}

std::string compact_date(std::string value, const std::string& name) {
    value = trim(std::move(value));
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    if (!value.empty() && !digits(value, 8))
        throw Error(name + " must be YYYYMMDD or YYYY-MM-DD");
    return value;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : "bj";
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ";
}

Json security_document(
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

std::string status_category(const std::string& status) {
    if (status == "要约准备中") return "preparing";
    if (status == "要约进行中") return "active";
    if (status == "要约暂停中") return "paused";
    if (status == "要约完成") return "completed";
    if (status == "要约失败") return "failed";
    return "other";
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) {
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    } else if (value.is_object()) {
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    }
    return false;
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

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

Json summarize(const Json& rows) {
    std::set<std::string> securities;
    std::map<std::string, std::uint64_t> statuses;
    std::uint64_t names = 0, delist = 0;
    std::string first, latest;
    double planned_shares = 0, planned_funds = 0, actual_shares = 0;
    for (const auto& row : rows.as_array()) {
        const auto& security = row.at("security");
        securities.insert(security.at("security_id").as_string());
        if (security.at("name_resolved").as_bool()) ++names;
        ++statuses[row.at("status_category").as_string()];
        if (row.at("delisting_flag").as_bool()) ++delist;
        const auto date = row.at("announcement_date").as_string();
        if (first.empty() || date < first) first = date;
        if (date > latest) latest = date;
        if (const auto value = json_number(row, "planned_shares")) planned_shares += *value;
        if (const auto value = json_number(row, "planned_funds_yuan")) planned_funds += *value;
        if (const auto value = json_number(row, "actual_shares")) actual_shares += *value;
    }
    Json by_status = Json::object();
    for (const auto& [name, count] : statuses) by_status[name] = count;
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["names_resolved"] = names;
    result["by_status"] = std::move(by_status);
    result["live_rows"] = statuses["preparing"] + statuses["active"] + statuses["paused"];
    result["delisting_rows"] = delist;
    result["first_announcement_date"] = first.empty() ? Json(nullptr) : Json(first);
    result["latest_announcement_date"] = latest.empty() ? Json(nullptr) : Json(latest);
    result["planned_shares"] = planned_shares;
    result["planned_funds_yuan"] = planned_funds;
    result["actual_shares"] = actual_shares;
    return result;
}

}  // namespace

Json normalize_tender_offer_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("tender-offer rows must be an array");
    Json result = Json::array();
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto code = text_value(raw, "$ZQDM");
        const auto announcement_raw = text_value(raw, "date");
        if (!digits(code, 6) || !digits(announcement_raw, 8)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        const auto planned_10k = number_value(raw, "ngs");
        const auto planned_funds_10k = number_value(raw, "nzzj");
        const auto actual_10k = number_value(raw, "sjgs");
        std::optional<double> completion;
        if (planned_10k && actual_10k && *planned_10k != 0)
            completion = *actual_10k / *planned_10k * 100.0;
        const auto status = text_value(raw, "jd");
        Json row = Json::object();
        row["offer_id"] = market_prefix(id) + code + ":" + announcement_raw +
            ":" + std::to_string(source_rank);
        row["source_rank"] = source_rank;
        row["security"] = security_document(id, code, securities);
        row["announcement_date"] = iso_date(announcement_raw);
        row["acquirer"] = text_value(raw, "sgr");
        row["status"] = status;
        row["status_category"] = status_category(status);
        row["share_type"] = text_value(raw, "gflx");
        row["offer_price"] = number_json(number_value(raw, "njg"));
        row["planned_shares_10k"] = number_json(planned_10k);
        row["planned_shares"] = scaled_number(planned_10k, 10000.0);
        row["planned_total_pct"] = number_json(number_value(raw, "nbl"));
        row["planned_funds_10k_yuan"] = number_json(planned_funds_10k);
        row["planned_funds_yuan"] = scaled_number(planned_funds_10k, 10000.0);
        row["currency"] = text_value(raw, "bz");
        row["start_date"] = iso_date(text_value(raw, "qsr"));
        row["end_date"] = iso_date(text_value(raw, "zzr"));
        row["actual_shares_10k"] = number_json(actual_10k);
        row["actual_shares"] = scaled_number(actual_10k, 10000.0);
        row["actual_total_pct"] = number_json(number_value(raw, "sjbl"));
        row["actual_to_planned_pct"] = number_json(completion);
        row["transfer_date"] = iso_date(text_value(raw, "ghr"));
        row["delisting"] = text_value(raw, "ts");
        row["delisting_flag"] = text_value(raw, "ts") == "是";
        row["purpose"] = text_value(raw, "md");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

void sort_tender_offer_rows(Json& rows, const std::string& sort_value,
                            const std::string& order_value) {
    if (!rows.is_array()) throw Error("tender-offer sort requires an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    const std::set<std::string> allowed{"announcement-date", "start-date", "end-date",
        "offer-price", "planned-shares", "planned-pct", "planned-funds",
        "actual-shares", "actual-pct", "completion", "code", "source-rank"};
    if (!allowed.count(sort))
        throw Error("sort must be announcement-date, start-date, end-date, offer-price, planned-shares, planned-pct, planned-funds, actual-shares, actual-pct, completion, code, or source-rank");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    const std::map<std::string, std::string> number_keys{
        {"offer-price", "offer_price"}, {"planned-shares", "planned_shares"},
        {"planned-pct", "planned_total_pct"}, {"planned-funds", "planned_funds_yuan"},
        {"actual-shares", "actual_shares"}, {"actual-pct", "actual_total_pct"},
        {"completion", "actual_to_planned_pct"}, {"source-rank", "source_rank"}};
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "announcement-date" || sort == "start-date" || sort == "end-date") {
                const auto key = sort == "announcement-date" ? "announcement_date" :
                    sort == "start-date" ? "start_date" : "end_date";
                const auto a = json_text(left, key), b = json_text(right, key);
                if (a != b) return descending ? a > b : a < b;
            } else if (sort == "code") {
                const auto a = left.at("security").at("security_id").as_string();
                const auto b = right.at("security").at("security_id").as_string();
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto a = json_number(left, number_keys.at(sort));
                const auto b = json_number(right, number_keys.at(sort));
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            return left.at("offer_id").as_string() < right.at("offer_id").as_string();
        });
}

TenderOfferService::TenderOfferService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

TenderOfferService::FetchResult TenderOfferService::fetch(
    const TenderOfferQuery& options) {
    const auto now = std::time(nullptr);
    const int age = cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_.fetched_at)) : 0;
    if (!options.refresh && cache_.fetched_at && age < options.cache_ttl_seconds)
        return {cache_.document, false, false, age, {}};
    try {
        auto document = fetch_jsn_resource_rows(resource_name, "bi", options.timeout_ms);
        cache_ = {document, std::time(nullptr)};
        return {std::move(document), true, false, 0, {}};
    } catch (const std::exception& error) {
        if (cache_.fetched_at)
            return {cache_.document, false, true, age, error.what()};
        throw;
    }
}

Json TenderOfferService::query(const TenderOfferQuery& input) {
    TenderOfferQuery options = input;
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = trim(options.query);
    options.status = lower_ascii(trim(options.status));
    options.from = compact_date(options.from, "from");
    options.to = compact_date(options.to, "to");
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    const std::set<std::string> statuses{
        "all", "live", "preparing", "active", "paused", "completed", "failed", "other"};
    if (!statuses.count(options.status))
        throw Error("status must be all, live, preparing, active, paused, completed, failed, or other");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if (!options.from.empty() && !options.to.empty() && options.from > options.to)
        throw Error("from must not be after to");
    if (options.offset < 0 || options.offset > 1000000 ||
        options.limit < 1 || options.limit > 5000)
        throw Error("tender-offer pagination is invalid");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400 ||
        options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("tender-offer cache or timeout value is invalid");

    const auto fetched = fetch(options);
    auto rows = normalize_tender_offer_rows(fetched.document.at("rows"), securities_);
    Json filtered = Json::array();
    const auto needle = lower_ascii(options.query);
    const auto from = iso_date(options.from), to = iso_date(options.to);
    for (const auto& row : rows.as_array()) {
        const auto& security = row.at("security");
        if (selected_market >= 0 &&
            static_cast<int>(security.at("market_id").as_number()) != selected_market) continue;
        if (!options.code.empty() && security.at("code").as_string() != options.code) continue;
        const auto category = row.at("status_category").as_string();
        if (options.status == "live") {
            if (category != "preparing" && category != "active" && category != "paused") continue;
        } else if (options.status != "all" && category != options.status) continue;
        const auto date = row.at("announcement_date").as_string();
        if (!from.empty() && date < from) continue;
        if (!to.empty() && date > to) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    sort_tender_offer_rows(filtered, options.sort, options.order);
    const auto summary = summarize(filtered);
    const auto matched = filtered.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < filtered.size() && paged.size() < static_cast<std::size_t>(options.limit);
         ++index) paged.push_back(filtered.as_array()[index]);

    Json source = jsn_source_metadata(fetched.document);
    if (fetched.stale) {
        source["stale"] = true;
        source["stale_reason"] = fetched.warning;
    }
    Json sources = Json::array();
    sources.push_back(std::move(source));
    Json warnings = Json::array();
    if (!fetched.warning.empty()) {
        Json warning = Json::object();
        warning["resource"] = resource_name;
        warning["message"] = fetched.warning;
        warnings.push_back(std::move(warning));
    }
    Json result = Json::object();
    result["schema"] = "tdx-market-tender-offers-native-v1";
    result["generated_at"] = now_text();
    result["availability"] = fetched.stale ? "stale-cache" : matched ? "live" : "empty";
    Json filters = Json::object();
    filters["market"] = options.market.empty() ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    filters["status"] = options.status;
    filters["from"] = options.from.empty() ? Json(nullptr) : Json(from);
    filters["to"] = options.to.empty() ? Json(nullptr) : Json(to);
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    result["filters"] = std::move(filters);
    result["summary"] = summary;
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size());
    counts["warnings"] = static_cast<std::uint64_t>(warnings.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(paged);
    result["sources"] = std::move(sources);
    result["upstream_health"] = jsn_sources_health(result.at("sources"));
    result["warnings"] = std::move(warnings);
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = fetched.refreshed;
    cache["age_seconds"] = fetched.age_seconds;
    result["cache"] = std::move(cache);
    Json units = Json::object();
    units["offer_price"] = "currency per share";
    units["planned_shares"] = "shares (source ngs is 10,000 shares)";
    units["planned_total_pct"] = "percentage-points";
    units["planned_funds_yuan"] = "yuan (source nzzj is 10,000 yuan)";
    units["actual_shares"] = "shares (source sjgs is 10,000 shares)";
    units["actual_total_pct"] = "percentage-points";
    units["actual_to_planned_pct"] = "percentage-points";
    result["units"] = std::move(units);
    result["semantics"] =
        "TDX func_yysg101 tender-offer master table. The typed fields and units come from the installed GBK cloud configuration: latest announcement, acquirer, transfer progress, share type, proposed price/shares/total-capital ratio/funds, currency, offer dates, actual accepted shares/ratio, transfer date, delisting flag and purpose. This is an event table, not an L2 feed. Multiple historical offers for one security are intentionally preserved. actual_to_planned_pct is derived from source share counts; empty upstream values remain null.";
    return result;
}

int command_market_tender_offers(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market tender-offers [options]\n\n"
            "Typed TDX tender-offer events and per-security history.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --market sz|sh|bj --code CODE  Optional security filter; required together\n"
            "  --status NAME           all|live|preparing|active|paused|completed|failed|other\n"
            "  --query TEXT --from YYYYMMDD --to YYYYMMDD\n"
            "  --sort NAME             announcement-date|start-date|end-date|offer-price|planned-shares|planned-pct|planned-funds|actual-shares|actual-pct|completion|code|source-rank\n"
            "  --order asc|desc --offset N --limit N\n"
            "  --refresh --cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    TenderOfferQuery query;
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.status = args.take_option("--status", "all");
    query.from = args.take_option("--from");
    query.to = args.take_option("--to");
    query.sort = args.take_option("--sort", "announcement-date");
    query.order = args.take_option("--order", "desc");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "1000"), "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    TenderOfferService service(load_blocks(root, {"industry"}).securities);
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed tender-offer query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
