#include "tdx/employees.hpp"
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
#include <limits>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kEmployeeMaster = "list/func_ygxc101_1.jsn";
constexpr const char* kSharePlans = "list/func_qxfa501_1.jsn";

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
    const auto text = text_value(row, name);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto value = std::stod(text, &used);
        return used == text.size() && std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
    } catch (...) { return std::nullopt; }
}
Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}
int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}
std::string market_name(int id) { return id == 0 ? "sz" : id == 1 ? "sh" : "bj"; }
bool valid_code(const std::string& code) {
    return code.size() == 6 && std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
}
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    Json security = Json::object();
    security["market_id"] = id;
    security["market"] = market_name(id);
    security["code"] = code;
    security["security_id"] = market_name(id) + code;
    const auto found = securities.find({id, code});
    security["name"] = found == securities.end() ? "" : found->second.name;
    security["name_resolved"] = found != securities.end();
    return security;
}
const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing employee resource: " + std::string(resource));
}
Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"}) result[name] = document.at(name);
    return result;
}
fs::path native_path(const std::string& value);
Json load_local_resource_rows(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local employee resource is unavailable: " + path_utf8(path));
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
int bounded(const std::string& text, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high) throw std::invalid_argument("range");
        return value;
    } catch (...) { throw Error(std::string(name) + " must be in " + std::to_string(low) + ".." + std::to_string(high)); }
}

}  // namespace

Json normalize_employee_share_plan_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("employee share-plan rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        const auto code = text_value(raw, "$ZQDM");
        if (!valid_code(code)) continue;
        const auto purchase_10k = number_value(raw, "gmgs");
        const auto average_price = number_value(raw, "price1");
        const auto planned_tranches = number_value(raw, "jhzqs");
        const auto implemented_tranches = number_value(raw, "ssqs");
        const auto implementation_start = text_value(raw, "date1");
        const auto duration_start = text_value(raw, "date3");
        const auto lock_start = text_value(raw, "date5");
        const auto sort_date = !implementation_start.empty() ? implementation_start
            : !duration_start.empty() ? duration_start : lock_start;
        Json item = Json::object();
        item["security"] = security_document(id, code, securities);
        item["event_id"] = std::string(kSharePlans) + ":" + std::to_string(id) +
            ":" + code + ":" + sort_date + ":" + duration_start;
        item["status"] = text_value(raw, "zt");
        item["active"] = text_value(raw, "zt") == "股票购买中";
        item["industry"] = text_value(raw, "hy");
        item["sort_date"] = sort_date;
        Json dates = Json::object();
        dates["implementation_start"] = implementation_start;
        dates["implementation_end"] = text_value(raw, "date2");
        dates["duration_start"] = duration_start;
        dates["duration_end"] = text_value(raw, "date4");
        dates["lock_start"] = lock_start;
        dates["lock_end"] = text_value(raw, "date6");
        item["dates"] = std::move(dates);
        item["purchase_average_price"] = number(raw, "price1");
        item["purchase_shares_10k"] = number(raw, "gmgs");
        item["purchase_shares"] = purchase_10k ? Json(*purchase_10k * 10000.0) : Json(nullptr);
        item["purchase_amount_yuan"] = purchase_10k && average_price
            ? Json(*purchase_10k * 10000.0 * *average_price) : Json(nullptr);
        item["share_capital_pct"] = number(raw, "zb");
        item["planned_tranches"] = number(raw, "jhzqs");
        item["implemented_tranches"] = number(raw, "ssqs");
        item["tranche_completion_pct"] = planned_tranches && implemented_tranches &&
            std::abs(*planned_tranches) > 0.000001
            ? Json(*implemented_tranches * 100.0 / *planned_tranches) : Json(nullptr);
        item["details"] = text_value(raw, "xqsm");
        item["source_resource"] = kSharePlans;
        item["raw"] = raw;
        result.push_back(std::move(item));
    }
    return result;
}

EmployeeService::EmployeeService(
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : securities_(std::move(securities)), jsn_root_(std::move(jsn_root)) {}

Json EmployeeService::fetch_master(const EmployeeQuery& options, bool& refreshed,
                                   int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_ ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    const bool need_share_plans = options.view != "catalog";
    const auto* cached_share_plans = field(cache_, "share_plans");
    const auto* cached_sources = field(cache_, "sources");
    const bool cache_covers_view = !need_share_plans ||
        (cached_share_plans && cached_share_plans->is_array() && cached_sources &&
         cached_sources->is_array() && cached_sources->size() > 1);
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds &&
        cache_covers_view) {
        refreshed = false;
        return cache_;
    }
    std::vector<std::string> resources{kEmployeeMaster};
    if (need_share_plans) resources.push_back(kSharePlans);
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : resources)
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    const auto& source = document_for(documents, kEmployeeMaster);
    Json rows = Json::array();
    for (const auto& raw : source.at("rows").as_array()) {
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        const auto code = text_value(raw, "$ZQDM");
        if (!valid_code(code)) continue;
        auto security = security_document(id, code, securities_);
        const auto employees = number_value(raw, "ygzs");
        const auto previous = number_value(raw, "sqyg");
        Json item = Json::object();
        item["security"] = std::move(security);
        item["net_profit_yuan"] = number(raw, "jlr");
        item["liabilities_yuan"] = number(raw, "fz");
        item["employees"] = number(raw, "ygzs");
        item["previous_employees"] = number(raw, "sqyg");
        item["employee_change"] = employees && previous ? Json(*employees - *previous) : Json(nullptr);
        item["employee_change_pct"] = employees && previous && std::abs(*previous) > 0.000001
            ? Json((*employees - *previous) * 100.0 / *previous) : Json(nullptr);
        item["employee_compensation_yuan"] = number(raw, "yfzgxc");
        item["rd_expense_yuan"] = number(raw, "yffy");
        item["rd_revenue_pct"] = number(raw, "yffyzb");
        item["executive_compensation_yuan"] = number(raw, "xcze");
        item["executive_compensation_liability_pct"] = number(raw, "zfzb");
        item["executive_compensation_employee_pct"] = number(raw, "zgxcb");
        item["compensation_per_employee_yuan"] = number(raw, "rjxc");
        item["profit_per_employee_yuan"] = number(raw, "rjjlr");
        item["masters_or_above"] = number(raw, "shjys");
        item["bachelors_or_above"] = number(raw, "bkjyx");
        rows.push_back(std::move(item));
    }
    Json document = Json::object();
    document["rows"] = std::move(rows);
    document["share_plans"] = Json::array();
    document["source"] = source_summary(source);
    Json sources = Json::array();
    sources.push_back(source_summary(source));
    if (need_share_plans) {
        const auto& share_plan_source = document_for(documents, kSharePlans);
        document["share_plans"] = normalize_employee_share_plan_rows(
            share_plan_source.at("rows"), securities_);
        sources.push_back(source_summary(share_plan_source));
    }
    document["sources"] = std::move(sources);
    cache_ = document;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return document;
}

Json EmployeeService::query(const EmployeeQuery& options) {
    const std::set<std::string> views{"catalog", "share-plans", "all"};
    if (!views.count(options.view))
        throw Error("view must be catalog, share-plans, or all");
    const std::set<std::string> sorts{"employees", "employee-change", "compensation", "executive-compensation", "rd-expense", "profit-per-employee"};
    if (!sorts.count(options.sort)) throw Error("unknown employee sort: " + options.sort);
    if (options.limit < 1 || options.limit > 10000) throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty()) throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        if (!valid_code(options.code)) throw Error("code must contain six digits");
    }
    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto folded = lower_ascii(trim(options.query));
    const bool include_companies = options.view == "catalog" || options.view == "all";
    const bool include_share_plans = options.view == "share-plans" || options.view == "all";
    Json rows = Json::array();
    if (include_companies) for (const auto& row : master.at("rows").as_array()) {
        const auto& security = row.at("security");
        if (!options.code.empty() && (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
            security.at("code").as_string() != options.code)) continue;
        if (!folded.empty()) {
            const auto haystack = lower_ascii(security.at("security_id").as_string() + " " + security.at("name").as_string());
            if (haystack.find(folded) == std::string::npos) continue;
        }
        rows.push_back(row);
    }
    const std::map<std::string, std::string> sort_fields{
        {"employees", "employees"}, {"employee-change", "employee_change"},
        {"compensation", "employee_compensation_yuan"},
        {"executive-compensation", "executive_compensation_yuan"},
        {"rd-expense", "rd_expense_yuan"}, {"profit-per-employee", "profit_per_employee_yuan"}};
    const auto sort_field = sort_fields.at(options.sort);
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(), [&](const Json& a, const Json& b) {
        return number_value(a, sort_field).value_or(-std::numeric_limits<double>::infinity()) >
               number_value(b, sort_field).value_or(-std::numeric_limits<double>::infinity());
    });
    const auto matched = rows.size();
    while (static_cast<int>(rows.size()) > options.limit) rows.as_array().pop_back();
    Json share_plans = Json::array();
    if (include_share_plans) for (const auto& row : master.at("share_plans").as_array()) {
        const auto& security = row.at("security");
        if (!options.code.empty() &&
            (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
             security.at("code").as_string() != options.code)) continue;
        if (!folded.empty()) {
            const auto haystack = lower_ascii(
                security.at("security_id").as_string() + " " +
                security.at("name").as_string() + " " + text_value(row, "industry") + " " +
                text_value(row, "status") + " " + text_value(row, "details"));
            if (haystack.find(folded) == std::string::npos) continue;
        }
        share_plans.push_back(row);
    }
    std::stable_sort(share_plans.as_array().begin(), share_plans.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_date = text_value(left, "sort_date");
            const auto right_date = text_value(right, "sort_date");
            if (left_date != right_date) return left_date > right_date;
            return left.at("event_id").as_string() < right.at("event_id").as_string();
        });
    const auto share_plan_matched = share_plans.size();
    double share_plan_shares = 0.0;
    double share_plan_amount = 0.0;
    std::uint64_t share_plan_active = 0;
    std::set<std::string> share_plan_securities;
    for (const auto& row : share_plans.as_array()) {
        share_plan_shares += number_value(row, "purchase_shares").value_or(0.0);
        share_plan_amount += number_value(row, "purchase_amount_yuan").value_or(0.0);
        if (const auto* active = field(row, "active"); active && active->is_bool() && active->as_bool())
            ++share_plan_active;
        share_plan_securities.insert(row.at("security").at("security_id").as_string());
    }
    while (static_cast<int>(share_plans.size()) > options.limit)
        share_plans.as_array().pop_back();
    Json executives = Json::array();
    Json detail_errors = Json::array();
    if (!options.code.empty() && include_companies) {
        const auto resource = "ggxc/" + std::to_string(selected_market) + options.code + ".jsn";
        try {
            const auto detail = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
            for (const auto& raw : detail.at("rows").as_array()) {
                if (text_value(raw, "xm").empty()) continue;
                Json item = Json::object();
                item["name"] = text_value(raw, "xm");
                item["gender_age_education"] = text_value(raw, "xb_nl_xl");
                item["position"] = text_value(raw, "zw");
                item["annual_compensation_yuan"] = number(raw, "ndbc");
                item["cutoff_date"] = text_value(raw, "jzrq");
                executives.push_back(std::move(item));
            }
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = resource;
            failure["message"] = error.what();
            detail_errors.push_back(std::move(failure));
        }
    }
    double total_employees = 0, total_compensation = 0, total_rd = 0;
    std::uint64_t growing = 0, shrinking = 0;
    for (const auto& row : master.at("rows").as_array()) {
        total_employees += number_value(row, "employees").value_or(0);
        total_compensation += number_value(row, "employee_compensation_yuan").value_or(0);
        total_rd += number_value(row, "rd_expense_yuan").value_or(0);
        const auto change = number_value(row, "employee_change").value_or(0);
        if (change > 0) ++growing; else if (change < 0) ++shrinking;
    }
    Json summary = Json::object();
    summary["companies"] = static_cast<std::uint64_t>(master.at("rows").size());
    summary["employees"] = total_employees;
    summary["employee_compensation_yuan"] = total_compensation;
    summary["rd_expense_yuan"] = total_rd;
    summary["employee_growing_companies"] = growing;
    summary["employee_shrinking_companies"] = shrinking;
    Json result = Json::object();
    result["schema"] = "tdx-market-employees-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = options.code.empty() ? "catalog" : "security";
    result["view"] = options.view;
    result["sort"] = options.sort;
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["companies"] = std::move(rows);
    result["selected"] = !options.code.empty() && result.at("companies").size() ? result.at("companies").as_array().front() : Json(nullptr);
    result["executives"] = std::move(executives);
    result["share_plans"] = std::move(share_plans);
    result["share_plan_match_count"] = static_cast<std::uint64_t>(share_plan_matched);
    result["share_plan_returned"] = static_cast<std::uint64_t>(result.at("share_plans").size());
    Json share_plan_summary = Json::object();
    share_plan_summary["plans"] = static_cast<std::uint64_t>(share_plan_matched);
    share_plan_summary["unique_securities"] = static_cast<std::uint64_t>(share_plan_securities.size());
    share_plan_summary["active_plans"] = share_plan_active;
    share_plan_summary["completed_plans"] = static_cast<std::uint64_t>(share_plan_matched) - share_plan_active;
    share_plan_summary["purchase_shares"] = share_plan_shares;
    share_plan_summary["purchase_amount_yuan"] = share_plan_amount;
    result["share_plan_summary"] = std::move(share_plan_summary);
    result["detail_errors"] = std::move(detail_errors);
    result["summary"] = std::move(summary);
    result["source"] = master.at("source");
    result["sources"] = master.at("sources");
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_employees(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market employees [--view catalog|share-plans|all] [--query TEXT] [--market MARKET --code CODE] "
                     "[--sort employees|employee-change|compensation|executive-compensation|rd-expense|profit-per-employee] "
                     "[--root PATH] [--output PATH] [--compact]\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    EmployeeQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "catalog")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.sort = lower_ascii(trim(args.take_option("--sort", "executive-compensation")));
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option("--output", "output/tdx-market-employees-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    EmployeeService service(load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number() << " employee rows and "
              << document.at("share_plan_returned").as_number() << " share-plan rows -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
