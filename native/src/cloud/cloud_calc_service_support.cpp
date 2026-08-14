#include "tdx/cloud_calc.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/seal_order.hpp"
#include "tdx/session_audit.hpp"

#include "cloud_calc_builtins_internal.hpp"
#include "cloud_calc_config_internal.hpp"
#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_interpreter_internal.hpp"
#include "cloud_calc_service_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace cloud_calc_service_detail {

using cloud_calc_detail::builtin_catalog;
using cloud_calc_detail::builtin_registry;
using cloud_calc_detail::check_formula;
using cloud_calc_detail::Column;
using cloud_calc_detail::Config;
using cloud_calc_detail::config_paths;
using cloud_calc_detail::dependency_cycles;
using cloud_calc_detail::depth_syscol;
using cloud_calc_detail::enrich_seal_snapshots;
using cloud_calc_detail::evaluate_unit;
using cloud_calc_detail::finance_syscol;
using cloud_calc_detail::find_builtin;
using cloud_calc_detail::group_syscol;
using cloud_calc_detail::known_tbigdata_syscol;
using cloud_calc_detail::local_yyyymmdd;
using cloud_calc_detail::parse_config;
using cloud_calc_detail::parse_double;
using cloud_calc_detail::parse_int;
using cloud_calc_detail::quote_syscol;
using cloud_calc_detail::recognized_host_syscol;
using cloud_calc_detail::resolve_host_fields;
using cloud_calc_detail::seal_syscol;
using cloud_calc_detail::snapshot_records;
using cloud_calc_detail::speed_syscol;
using cloud_calc_detail::validate_date;

const Json *optional(const Json &object, std::string_view key) {
    if (!object.is_object())
        return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json audit_config(const Config &config,
                  std::map<std::string, std::uint64_t, std::less<>> &builtin_usage,
                  std::uint64_t &formula_count, std::uint64_t &expression_count,
                  std::uint64_t &builtin_count, std::uint64_t &valid_count,
                  std::uint64_t &executable_count, std::uint64_t &cycle_count) {
    Json report = Json::object();
    report["path"] = path_utf8(config.path);
    report["file"] = path_utf8(config.path.filename());
    Json unit_reports = Json::array();
    std::uint64_t file_formulas = 0;
    for (const auto &unit : config.units) {
        Json calculated = Json::array();
        std::map<std::string, std::size_t, std::less<>> derived;
        for (const auto &column : unit.columns)
            if (!column.calc.empty())
                derived.emplace(column.code, column.ordinal);
        std::map<std::string, std::set<std::string, std::less<>>, std::less<>> edges;
        for (const auto &column : unit.columns) {
            if (column.calc.empty())
                continue;
            ++formula_count;
            ++file_formulas;
            const auto check = check_formula(column);
            if (check.kind == "builtin") {
                ++builtin_count;
                ++builtin_usage[column.calc];
            } else {
                ++expression_count;
            }
            if (check.valid)
                ++valid_count;
            if (check.valid && check.executable)
                ++executable_count;
            Json item = Json::object();
            item["code"] = column.code;
            if (!column.name.empty())
                item["name"] = column.name;
            item["calc"] = column.calc;
            Json refs = Json::array();
            for (const auto &ref : column.refs) {
                refs.push_back(ref);
                if (derived.count(ref))
                    edges[column.code].insert(ref);
            }
            item["calcref"] = std::move(refs);
            item["calcflag"] = column.calcflag;
            item["calctype"] = column.calctype;
            item["kind"] = check.kind;
            item["valid"] = check.valid;
            item["executable"] = check.executable;
            if (check.builtin) {
                item["builtin_id"] = check.builtin->id;
                item["registered_argc"] = check.builtin->argc;
            }
            if (!check.reason.empty())
                item["reason"] = check.reason;
            calculated.push_back(std::move(item));
        }
        if (calculated.size() == 0)
            continue;
        Json unit_report = Json::object();
        unit_report["id"] = unit.id;
        unit_report["resource"] = unit.file;
        unit_report["calc_columns"] = static_cast<std::uint64_t>(calculated.size());
        unit_report["columns"] = std::move(calculated);
        Json dependencies = Json::object();
        for (const auto &[code, refs_set] : edges) {
            Json refs = Json::array();
            for (const auto &ref : refs_set)
                refs.push_back(ref);
            dependencies[code] = std::move(refs);
        }
        unit_report["derived_dependencies"] = std::move(dependencies);
        const auto cycles = dependency_cycles(unit);
        cycle_count += static_cast<std::uint64_t>(cycles.size());
        Json cycle_values = Json::array();
        for (const auto &cycle : cycles)
            cycle_values.push_back(cycle);
        unit_report["dependency_cycles"] = std::move(cycle_values);
        unit_reports.push_back(std::move(unit_report));
    }
    report["calc_columns"] = file_formulas;
    report["units"] = std::move(unit_reports);
    return report;
}

Json row_from_document(const Json &document, std::size_t row_index) {
    if (document.is_object()) {
        const auto *headers = optional(document, "colheader");
        const auto *data = optional(document, "data");
        if (!headers || !data)
            return document;
        if (!headers->is_array() || !data->is_array())
            throw Error("JSN colheader and data must be arrays");
        if (row_index >= data->size())
            throw Error("JSN row index is out of range");
        const auto &row = data->as_array()[row_index];
        if (!row.is_array())
            throw Error("JSN data row is not an array");
        Json result = Json::object();
        const auto count = std::min(headers->size(), row.size());
        for (std::size_t index = 0; index < count; ++index) {
            if (!headers->as_array()[index].is_string())
                continue;
            result[headers->as_array()[index].as_string()] = row.as_array()[index];
        }
        return result;
    }
    if (document.is_array()) {
        for (const auto &item : document.as_array()) {
            if (item.is_object() && optional(item, "colheader") && optional(item, "data"))
                return row_from_document(item, row_index);
        }
        if (row_index < document.size() && document.as_array()[row_index].is_object())
            return document.as_array()[row_index];
        throw Error(
            "JSON array contains neither a JSN table nor an object row at the requested index");
    }
    throw Error("row input must be a JSON object, object array, or JSN table");
}

Json read_row(const fs::path &path, std::size_t row_index) {
    return row_from_document(Json::parse(decode_gbk(read_bytes(path))), row_index);
}

BatchRowsDocument batch_rows_from_document(const Json &document, std::size_t maximum) {
    BatchRowsDocument result;
    if (document.is_object()) {
        const auto *headers = optional(document, "colheader");
        const auto *data = optional(document, "data");
        if (!headers && !data) {
            result.available = 1;
            result.rows.push_back(document);
            return result;
        }
        if (!headers || !data || !headers->is_array() || !data->is_array())
            throw Error("JSN colheader and data must be arrays");
        result.available = data->size();
        const auto count = std::min(maximum, result.available);
        for (std::size_t index = 0; index < count; ++index)
            result.rows.push_back(row_from_document(document, index));
        return result;
    }
    if (document.is_array()) {
        for (const auto &item : document.as_array())
            if (item.is_object() && optional(item, "colheader") && optional(item, "data"))
                return batch_rows_from_document(item, maximum);
        result.available = document.size();
        const auto count = std::min(maximum, result.available);
        for (std::size_t index = 0; index < count; ++index) {
            const auto &row = document.as_array()[index];
            if (!row.is_object())
                throw Error("batch JSON array entries must all be objects");
            result.rows.push_back(row);
        }
        return result;
    }
    throw Error("batch row input must be a JSON object, object array, or JSN table");
}

Json assignment_value(std::string value) {
    value = trim(std::move(value));
    if (value == "null")
        return Json();
    if (value == "true")
        return true;
    if (value == "false")
        return false;
    double number = 0.0;
    if (parse_double(value, number))
        return number;
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '[' && value.back() == ']')))
        return Json::parse(value);
    return value;
}

fs::path resolve_cfg(const fs::path &root, const std::string &name) {
    fs::path supplied = fs::u8path(name);
    if (fs::is_regular_file(supplied))
        return fs::weakly_canonical(supplied);
    fs::path directory = root;
    if (fs::is_directory(directory / "T0002" / "cloud_cfg"))
        directory /= fs::path("T0002") / "cloud_cfg";
    fs::path candidate = directory / supplied;
    if (candidate.extension().empty())
        candidate += ".cfg";
    if (!fs::is_regular_file(candidate))
        throw Error("CFG file does not exist: " + path_utf8(candidate));
    return fs::weakly_canonical(candidate);
}

void require_safe_cfg_request_name(const std::string &name) {
    if (name.empty() || name.size() > 128 || name == "." || name == ".." ||
        !std::all_of(name.begin(), name.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.';
        }))
        throw Error("cloud-calc cfg must be a 1..128 byte file name without directories");
}

void validate_cloud_calc_options(const CloudCalcEvaluationOptions &options) {
    if (options.timeout_ms < 100 || options.timeout_ms > 600000)
        throw Error("cloud-calc timeout_ms must be in 100..600000");
    if (options.as_of_yyyymmdd != 0)
        validate_date(options.as_of_yyyymmdd);
    if (!options.overrides.is_object())
        throw Error("cloud-calc overrides must be an object");
    if (options.overrides.size() > 256)
        throw Error("cloud-calc overrides exceed 256 fields");
    const bool has_snapshot = options.snapshot.size() != 0;
    const bool has_finance = options.finance_snapshot.size() != 0;
    const bool has_special_limits = options.special_limits_snapshot.size() != 0;
    if (options.quotes && (has_snapshot || has_finance || has_special_limits))
        throw Error("cloud-calc quotes cannot be combined with inline snapshots");
    if (has_snapshot)
        (void)snapshot_records(options.snapshot);
    if (has_finance)
        (void)snapshot_records(options.finance_snapshot);
    if (has_special_limits && !options.special_limits_snapshot.is_object())
        throw Error("cloud-calc special_limits_snapshot must be an object");
    for (const auto &[code, value] : options.overrides.as_object()) {
        if (code.empty() || code.size() > 128)
            throw Error("cloud-calc override code must contain 1..128 bytes");
        if (!value.is_number() && !value.is_string())
            throw Error("cloud-calc override values must be numbers or strings");
    }
}

void validate_cloud_calc_row(const Json &row) {
    if (!row.is_object())
        throw Error("cloud-calc row must be an object");
    if (row.size() > 4096)
        throw Error("cloud-calc row exceeds 4096 fields");
}

void collect_requested_securities(const Json &request, std::string_view key,
                                  std::set<std::string, std::less<>> &destination) {
    const auto *values = optional(request, key);
    if (!values || !values->is_array())
        return;
    for (const auto &value : values->as_array())
        if (value.is_string())
            destination.insert(value.as_string());
}

std::vector<std::string> security_vector(const std::set<std::string, std::less<>> &values) {
    return {values.begin(), values.end()};
}

int bounded_index(const std::string &value) {
    double number = 0.0;
    if (!parse_double(value, number) || number < 0.0 || number > 10000000.0 ||
        std::floor(number) != number)
        throw Error("--row-index must be an integer from 0 to 10000000");
    return static_cast<int>(number);
}

} // namespace cloud_calc_service_detail

using namespace cloud_calc_service_detail;

} // namespace tdx
