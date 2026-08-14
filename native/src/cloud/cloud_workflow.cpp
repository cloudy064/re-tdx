#include "tdx/cloud_workflow.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct CivilDate {
    int year{};
    int month{};
    int day{};
};

bool leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int month_days(int year, int month) {
    static constexpr int lengths[] = {31, 28, 31, 30, 31, 30,
                                      31, 31, 30, 31, 30, 31};
    return month == 2 && leap_year(year) ? 29 : lengths[month - 1];
}

CivilDate parse_date(const std::string& text) {
    if (text.size() != 8 || !std::all_of(text.begin(), text.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("date must use YYYYMMDD");
    CivilDate value{std::stoi(text.substr(0, 4)), std::stoi(text.substr(4, 2)),
                    std::stoi(text.substr(6, 2))};
    if (value.year < 1900 || value.month < 1 || value.month > 12 || value.day < 1 ||
        value.day > month_days(value.year, value.month))
        throw Error("date is outside the supported calendar range");
    return value;
}

CivilDate today_local() {
    const auto now = std::time(nullptr);
    std::tm value{};
#ifdef _WIN32
    if (localtime_s(&value, &now)) throw Error("cannot read local date");
#else
    if (!localtime_r(&now, &value)) throw Error("cannot read local date");
#endif
    return CivilDate{value.tm_year + 1900, value.tm_mon + 1, value.tm_mday};
}

std::string date_text(const CivilDate& value) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << value.year
           << std::setw(2) << value.month << std::setw(2) << value.day;
    return output.str();
}

CivilDate shift_months(CivilDate value, int months) {
    const int index = value.year * 12 + value.month - 1 + months;
    value.year = index / 12;
    value.month = index % 12 + 1;
    value.day = std::min(value.day, month_days(value.year, value.month));
    return value;
}

CivilDate shift_years(CivilDate value, int years) {
    value.year += years;
    value.day = std::min(value.day, month_days(value.year, value.month));
    return value;
}

CivilDate latest_full_fund_report(const CivilDate& value) {
    if (value.month > 8 || (value.month == 8 && value.day >= 31))
        return CivilDate{value.year, 6, 30};
    return CivilDate{value.year - 1, 12, 31};
}

const CloudWorkflowSpec& workflow_spec(const std::string& name) {
    const auto& specs = cloud_workflow_specs();
    const auto found = std::find_if(specs.begin(), specs.end(),
        [&](const auto& item) { return item.name == name; });
    if (found == specs.end()) throw Error("unknown cloud workflow: " + name);
    return *found;
}

std::map<std::string, std::string> string_map(const Json& value) {
    if (!value.is_object()) throw Error("workflow defaults must be an object");
    std::map<std::string, std::string> result;
    for (const auto& [name, item] : value.as_object()) {
        if (!item.is_string()) throw Error("workflow default values must be strings");
        result[name] = item.as_string();
    }
    return result;
}

const Json& object_value(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("workflow value must be an object");
    const auto found = object.as_object().find(name);
    if (found == object.as_object().end())
        throw Error("workflow object has no " + std::string(name));
    return found->second;
}

const Json& casefold_value(const Json& row, const std::string& name) {
    if (!row.is_object()) throw Error("workflow row must be an object");
    const auto folded = lower_ascii(name);
    const auto found = std::find_if(row.as_object().begin(), row.as_object().end(),
        [&](const auto& item) { return lower_ascii(item.first) == folded; });
    if (found == row.as_object().end())
        throw Error("master result has no " + name + " column");
    return found->second;
}

std::string scalar_text(const Json& value) {
    if (value.is_string()) return value.as_string();
    if (value.is_number() || value.is_bool() || value.is_null()) return value.dump(-1);
    throw Error("workflow key and mapped fields must be scalar values");
}

std::vector<std::string> split_spaces(const std::string& value) {
    std::istringstream input(value);
    std::vector<std::string> result;
    std::string item;
    while (input >> item) result.push_back(item);
    return result;
}

std::map<std::string, std::string> child_parameters(
    const CloudWorkflowStep& step, const Json& row,
    const std::map<std::string, std::string>& parameters) {
    auto result = parameters;
    for (const auto& [placeholder, field] : step.field_map)
        result[placeholder] = scalar_text(casefold_value(row, field));
    for (const auto& [alias, parameter] : step.parameter_aliases) {
        const auto found = parameters.find(parameter);
        if (found == parameters.end())
            throw Error("workflow parameter " + parameter + " is missing");
        result[alias] = found->second;
    }
    return result;
}

Json execute_step(const fs::path& root, const CloudWorkflowStep& step,
                  const std::map<std::string, std::string>& parameters,
                  bool all_pages, int page_size, int max_pages,
                  const std::string& base_url, int timeout_ms) {
    int attempts = 0;
    auto document = detail::retry_cloud_json([&]() -> Json {
        if (step.transport == "json") {
            return execute_tqlex_config(root, step.request_id, parameters, {}, {}, {},
                step.body_contains, all_pages, all_pages ? 0 : -1,
                all_pages ? page_size : 0, max_pages, base_url, timeout_ms);
        }
        if (step.transport == "pbrpc") {
            return execute_pbrpc_config(root, step.request_id, parameters, {}, {}, {}, {},
                step.body_contains, base_url, timeout_ms);
        }
        throw Error("unsupported workflow transport: " + step.transport);
    }, detail::is_transient_cloud_error, attempts, 3, 250);
    document["transport"] = step.transport;
    document["req_id"] = step.request_id;
    document["attempts"] = attempts;
    return document;
}

const Json& response_from_step(const Json& step) {
    const auto& response = object_value(step, "response");
    if (!response.is_object()) throw Error("workflow step response must be an object");
    return response;
}

Json select_master_rows(const Json& rows, const std::string& key_field,
                        const std::vector<std::string>& selected, int limit) {
    if (!rows.is_array() || limit < 1) throw Error("workflow row selection is invalid");
    Json chosen = Json::array();
    std::set<std::string> wanted;
    for (const auto& value : selected) wanted.insert(lower_ascii(value));
    std::set<std::string> found;
    for (const auto& row : rows.as_array()) {
        const auto key = scalar_text(casefold_value(row, key_field));
        if (!wanted.empty() && !wanted.count(lower_ascii(key))) continue;
        chosen.push_back(row);
        found.insert(lower_ascii(key));
        if (static_cast<int>(chosen.size()) >= limit) break;
    }
    if (!wanted.empty()) {
        std::vector<std::string> missing;
        for (const auto& value : selected)
            if (!found.count(lower_ascii(value))) missing.push_back(value);
        if (!missing.empty()) {
            std::string detail = "selected master keys were not returned:";
            for (const auto& value : missing) detail += " " + value;
            throw Error(detail);
        }
    }
    return chosen;
}

std::map<std::string, std::string> assignments(const std::vector<std::string>& values,
                                               std::string_view option) {
    std::map<std::string, std::string> result;
    for (const auto& value : values) {
        const auto separator = value.find('=');
        if (separator == std::string::npos || !separator)
            throw Error(std::string(option) + " expects NAME=VALUE");
        result[value.substr(0, separator)] = value.substr(separator + 1);
    }
    return result;
}

int integer_option(const std::string& text, std::string_view name,
                   int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

}  // namespace

const std::vector<CloudWorkflowSpec>& cloud_workflow_specs() {
    static const std::vector<CloudWorkflowSpec> specs{
        {"fund-holdings", "基金筛选→行业持仓、股票持仓", "fund_code",
         {"json", "500050", {}, {}, {}},
         {{"json", "500051", {{"fund_code", "fund_code"}}, {}, {}},
          {"json", "500052", {{"fund_code", "fund_code"}}, {}, {}}}},
        {"fund-risk", "基金收益风险主表→每日基金/基准收益走势", "fund_code",
         {"json", "500030", {}, {}, {}},
         {{"json", "500031", {{"fund_code", "fund_code"}}, {}, {}}}},
        {"fund-volatility", "基金月度风险主表→月度基金/基准收益走势", "fund_code",
         {"json", "500032", {}, {}, {}},
         {{"json", "500033", {{"fund_code", "fund_code"}}, {}, {}}}},
        {"fund-interval-holdings", "基金区间持仓主表→行业平均持仓、报告期集中度",
         "fund_code", {"json", "500055", {}, {}, {}},
         {{"json", "500056", {{"fund_code", "fund_code"}}, {}, {}},
          {"json", "500057", {{"fund_code", "fund_code"}}, {}, {}}}},
        {"index-valuation", "指数估值概览→所选指数每日估值比走势", "code",
         {"json", "200000", {}, {}, {}},
         {{"pbrpc", "200001", {{"Code", "code"}}, {}, {}}}},
        {"lhb-details", "PBRPC 龙虎榜列表→普通 JSON 上榜原因", "Code",
         {"pbrpc", "500107", {}, {}, {"\"MarketType\":\"0\""}},
         {{"json", "500108", {{"Code", "Code"}, {"SetCode", "SetCode"}},
           {{"#2111.result", "result"}}, {"'MarketType': '0'"}}}},
    };
    return specs;
}

Json cloud_workflow_defaults(const std::string& name,
                             const std::string& today_yyyymmdd) {
    const auto today = today_yyyymmdd.empty() ? today_local() : parse_date(today_yyyymmdd);
    (void)workflow_spec(name);
    Json result = Json::object();
    if (name.rfind("fund-", 0) == 0) {
        result["style_details"] = "005001";
        result["fund_size"] = "0";
        result["fund_setup_time"] = "0";
    }
    if (name == "fund-holdings") {
        result["report_date"] = date_text(latest_full_fund_report(today));
    } else if (name == "fund-risk") {
        result["basic_code"] = "0";
        result["start_date"] = date_text(shift_months(today, -3));
        result["end_date"] = date_text(today);
        result["rate_freerisk"] = "3";
    } else if (name == "fund-volatility") {
        result["basic_code"] = "0";
        result["start_date"] = date_text(shift_years(today, -3));
        result["end_date"] = date_text(today);
    } else if (name == "fund-interval-holdings") {
        const auto report = latest_full_fund_report(today);
        result["start_date"] = date_text(shift_years(report, -2));
        result["end_date"] = date_text(report);
    } else if (name == "index-valuation") {
        result["StartDate"] = date_text(shift_years(today, -2));
        result["EndDate"] = date_text(today);
        result["IndexType"] = "1";
        result["Basics"] = "000001";
        result["Methods"] = "PETTM";
    } else if (name == "lhb-details") {
        result["result"] = "0";
    }
    return result;
}

Json cloud_workflows_document(const std::string& today_yyyymmdd) {
    Json records = Json::array();
    for (const auto& spec : cloud_workflow_specs()) {
        Json item = Json::object();
        item["name"] = spec.name;
        item["description"] = spec.description;
        item["key_field"] = spec.key_field;
        Json master = Json::object();
        master["transport"] = spec.master.transport;
        master["request_id"] = spec.master.request_id;
        item["master"] = std::move(master);
        Json details = Json::array();
        for (const auto& step : spec.details) {
            Json detail = Json::object();
            detail["transport"] = step.transport;
            detail["request_id"] = step.request_id;
            details.push_back(std::move(detail));
        }
        item["details"] = std::move(details);
        item["defaults"] = cloud_workflow_defaults(spec.name, today_yyyymmdd);
        records.push_back(std::move(item));
    }
    Json document = Json::object();
    document["schema"] = "tdx-cloud-workflows-native-v1";
    document["count"] = static_cast<std::uint64_t>(cloud_workflow_specs().size());
    document["records"] = std::move(records);
    return document;
}

Json cloud_result_rows(const Json& response, std::size_t result_set_index) {
    const auto& sets = object_value(response, "ResultSets");
    if (!sets.is_array() || result_set_index >= sets.size())
        throw Error("workflow result set does not exist");
    const auto& result_set = sets.as_array()[result_set_index];
    const auto& descriptions = object_value(result_set, "ColDes");
    const auto& content = object_value(result_set, "Content");
    if (!descriptions.is_array() || !content.is_array())
        throw Error("workflow ColDes and Content must be arrays");
    std::vector<std::string> columns;
    for (const auto& description : descriptions.as_array()) {
        const auto& name = object_value(description, "Name");
        if (!name.is_string() || name.as_string().empty())
            throw Error("workflow result column has no Name");
        columns.push_back(name.as_string());
    }
    Json rows = Json::array();
    for (const auto& raw : content.as_array()) {
        if (raw.is_object()) {
            rows.push_back(raw);
            continue;
        }
        Json::Array values;
        if (raw.is_array()) {
            values = raw.as_array();
        } else if (raw.is_string() && columns.size() > 1) {
            for (const auto& value : split_spaces(raw.as_string())) values.emplace_back(value);
        } else {
            values.push_back(raw);
        }
        if (values.size() != columns.size())
            throw Error("workflow row width does not match ColDes");
        Json row = Json::object();
        for (std::size_t index = 0; index < columns.size(); ++index)
            row[columns[index]] = std::move(values[index]);
        rows.push_back(std::move(row));
    }
    return rows;
}

Json run_cloud_workflow(
    const fs::path& root, const std::string& workflow_name,
    const std::map<std::string, std::string>& parameter_overrides,
    const std::vector<std::string>& selected, int limit,
    bool master_all_pages, int page_size, int max_pages,
    const std::string& base_url, int timeout_ms) {
    if (limit < 1 || limit > 100 || page_size < 1 || page_size > 1000000 ||
        max_pages < 1 || max_pages > 1000)
        throw Error("cloud workflow limits are invalid");
    const auto& spec = workflow_spec(workflow_name);
    auto parameters = string_map(cloud_workflow_defaults(workflow_name));
    for (const auto& [name, value] : parameter_overrides) parameters[name] = value;
    const bool fetch_all = master_all_pages || !selected.empty();
    auto master = execute_step(root, spec.master, parameters, fetch_all,
                               page_size, max_pages, base_url, timeout_ms);
    const auto rows = cloud_result_rows(response_from_step(master));
    const auto chosen = select_master_rows(rows, spec.key_field, selected, limit);
    Json details = Json::array();
    for (const auto& row : chosen.as_array()) {
        Json children = Json::array();
        for (const auto& step : spec.details) {
            const auto replacements = child_parameters(step, row, parameters);
            children.push_back(execute_step(root, step, replacements, false,
                                            page_size, max_pages, base_url, timeout_ms));
        }
        Json detail = Json::object();
        detail["key"] = scalar_text(casefold_value(row, spec.key_field));
        detail["master_row"] = row;
        detail["children"] = std::move(children);
        details.push_back(std::move(detail));
    }
    Json parameter_json = Json::object();
    for (const auto& [name, value] : parameters) parameter_json[name] = value;
    Json selection = Json::object();
    selection["key_field"] = spec.key_field;
    selection["master_rows"] = static_cast<std::uint64_t>(rows.size());
    selection["selected_rows"] = static_cast<std::uint64_t>(chosen.size());
    Json document = Json::object();
    document["schema"] = "tdx-cloud-workflow-native-v1";
    document["workflow"] = spec.name;
    document["description"] = spec.description;
    document["parameters"] = std::move(parameter_json);
    document["selection"] = std::move(selection);
    document["master"] = std::move(master);
    document["details"] = std::move(details);
    return document;
}

int command_cloud_workflow(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool cloud workflow [options]\n\n"
            "Native TQLEX/PBRPC master-detail workflows.\n\n"
            "Options:\n"
            "  --list                 List built-in workflows and defaults\n"
            "  --workflow NAME        Workflow identifier\n"
            "  --set NAME=VALUE       Repeatable default parameter override\n"
            "  --select KEY           Repeatable master key selection\n"
            "  --limit N              Maximum expanded master rows (default 1)\n"
            "  --master-all-pages     Fetch and retain every master page\n"
            "  --page-size N          Master page size (default 20)\n"
            "  --max-pages N          Master page safety limit (default 100)\n"
            "  --base-url URL         Default static.tdx.com.cn:7615/TQLEX\n"
            "  --timeout-ms N         Default 15000\n"
            "  --root PATH            TDX installation root\n"
            "  --output PATH          Default output/tdx-cloud-workflow-native.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const bool list = args.take_flag("--list");
    const auto workflow = args.take_option("--workflow");
    const auto overrides = assignments(args.take_options("--set"), "--set");
    const auto selected = args.take_options("--select");
    const int limit = integer_option(args.take_option("--limit", "1"), "--limit", 1, 100);
    const bool master_all_pages = args.take_flag("--master-all-pages");
    const int page_size = integer_option(args.take_option("--page-size", "20"),
                                         "--page-size", 1, 1000000);
    const int max_pages = integer_option(args.take_option("--max-pages", "100"),
                                         "--max-pages", 1, 1000);
    const auto base_url = args.take_option("--base-url",
        cloud_endpoints::tqlex);
    const int timeout_ms = integer_option(args.take_option("--timeout-ms", "15000"),
                                          "--timeout-ms", 100, 600000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option(
        "--output", list ? "output/tdx-cloud-workflows-native.json"
                         : "output/tdx-cloud-workflow-native.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    Json document;
    if (list) {
        document = cloud_workflows_document();
    } else {
        if (workflow.empty()) throw Error("provide --workflow or --list");
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
        document = run_cloud_workflow(root, workflow, overrides, selected, limit,
            master_all_pages, page_size, max_pages, base_url, timeout_ms);
    }
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << (list ? "listed cloud workflows" : "completed cloud workflow")
              << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
