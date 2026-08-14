#include "tdx/cloud_calc.hpp"

#include "cloud_calc_builtins_internal.hpp"
#include "cloud_calc_config_internal.hpp"
#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_service_internal.hpp"
#include "tdx/common.hpp"

#include <map>
#include <set>

namespace fs = std::filesystem;

namespace tdx {

using namespace cloud_calc_detail;
using namespace cloud_calc_service_detail;
Json audit_cloud_calc_configs(const fs::path &root_or_cfg) {
    const auto paths = config_paths(root_or_cfg);
    std::map<std::string, std::uint64_t, std::less<>> usage;
    std::map<std::string, std::uint64_t, std::less<>> syscol_usage;
    std::map<std::string, std::uint64_t, std::less<>> implicit_syscol_usage;
    std::map<std::string, std::uint64_t, std::less<>> external_inputs;
    std::uint64_t formulas = 0;
    std::uint64_t expressions = 0;
    std::uint64_t builtin_formulas = 0;
    std::uint64_t valid = 0;
    std::uint64_t executable = 0;
    std::uint64_t cycles = 0;
    std::uint64_t cfg_with_calc = 0;
    std::uint64_t syscol_columns = 0, syscol_with_ref_security = 0, implicit_host_columns = 0;
    Json files = Json::array();
    Json parse_errors = Json::array();
    for (const auto &path : paths) {
        try {
            const auto config = parse_config(path);
            for (const auto &unit : config.units) {
                std::set<std::string, std::less<>> codes;
                for (const auto &column : unit.columns)
                    codes.insert(column.code);
                for (const auto &column : unit.columns) {
                    if (!column.syscol.empty()) {
                        ++syscol_usage[column.syscol];
                        ++syscol_columns;
                        if (!column.refzqdm.empty() || !unit.refunit.empty())
                            ++syscol_with_ref_security;
                    } else if (known_tbigdata_syscol(column.code)) {
                        ++implicit_syscol_usage[column.code];
                        ++implicit_host_columns;
                    }
                    if (column.calc.empty())
                        continue;
                    for (const auto &ref : column.refs) {
                        double literal = 0.0;
                        if (!codes.count(ref) && !parse_double(ref, literal))
                            ++external_inputs[ref];
                    }
                }
            }
            auto report = audit_config(config, usage, formulas, expressions, builtin_formulas,
                                       valid, executable, cycles);
            if (report.at("calc_columns").as_number() > 0.0) {
                ++cfg_with_calc;
                files.push_back(std::move(report));
            }
        } catch (const std::exception &error) {
            Json item = Json::object();
            item["path"] = path_utf8(path);
            item["error"] = error.what();
            parse_errors.push_back(std::move(item));
        }
    }
    std::uint64_t used_implemented = 0;
    Json used = Json::array();
    for (const auto &[name, count] : usage) {
        const auto *builtin = find_builtin(name);
        Json item = Json::object();
        item["name"] = name;
        item["count"] = count;
        item["id"] = builtin ? builtin->id : -1;
        item["implemented"] = builtin && builtin->implemented;
        if (builtin && builtin->implemented)
            ++used_implemented;
        used.push_back(std::move(item));
    }
    std::uint64_t registered_implemented = 0;
    for (const auto &builtin : builtin_registry())
        if (builtin.implemented)
            ++registered_implemented;
    Json summary = Json::object();
    summary["cfg_files"] = static_cast<std::uint64_t>(paths.size());
    summary["cfg_with_calc"] = cfg_with_calc;
    summary["calc_columns"] = formulas;
    summary["expressions"] = expressions;
    summary["builtin_formulas"] = builtin_formulas;
    summary["valid"] = valid;
    summary["invalid"] = formulas - valid;
    summary["current_config_executable"] = executable;
    summary["current_config_unimplemented"] = formulas - executable;
    summary["registered_builtin_count"] = static_cast<std::uint64_t>(builtin_registry().size());
    summary["registered_builtin_implemented"] = registered_implemented;
    summary["used_builtin_count"] = static_cast<std::uint64_t>(usage.size());
    summary["implemented_used_builtin_count"] = used_implemented;
    summary["parse_errors"] = static_cast<std::uint64_t>(parse_errors.size());
    summary["dependency_cycles"] = cycles;
    summary["syscol_columns"] = syscol_columns;
    summary["syscol_with_ref_security"] = syscol_with_ref_security;
    summary["implicit_host_columns"] = implicit_host_columns;
    std::uint64_t external_count = 0;
    for (const auto &[name, count] : external_inputs)
        external_count += count;
    summary["external_formula_input_occurrences"] = external_count;
    summary["external_formula_input_count"] = static_cast<std::uint64_t>(external_inputs.size());
    Json host_catalog = Json::array();
    std::uint64_t auto_resolvable_host_columns = 0;
    std::set<std::string, std::less<>> host_names;
    for (const auto &[name, count] : syscol_usage) {
        (void)count;
        host_names.insert(name);
    }
    for (const auto &[name, count] : implicit_syscol_usage) {
        (void)count;
        host_names.insert(name);
    }
    for (const auto &syscol : host_names) {
        const auto explicit_found = syscol_usage.find(syscol);
        const auto implicit_found = implicit_syscol_usage.find(syscol);
        const auto explicit_count =
            explicit_found == syscol_usage.end() ? 0 : explicit_found->second;
        const auto implicit_count =
            implicit_found == implicit_syscol_usage.end() ? 0 : implicit_found->second;
        Json item = Json::object();
        item["system_column"] = syscol;
        item["count"] = explicit_count + implicit_count;
        item["explicit_syscol_count"] = explicit_count;
        item["implicit_code_count"] = implicit_count;
        if (syscol == "$JJJZ")
            item["resolver"] = "tdx-quote-core-etf-iopv";
        else if (seal_syscol(syscol))
            item["resolver"] = "tdxw-type163-public-depth-local-rules-0x0452";
        else if (speed_syscol(syscol))
            item["resolver"] = "public-l1-rise-speed-0x053e";
        else if (depth_syscol(syscol))
            item["resolver"] = "public-l1-depth-0x0547";
        else if (quote_syscol(syscol))
            item["resolver"] = "public-l1-0x054c";
        else if (syscol == "$PE")
            item["resolver"] = "tdxw-dynainfo39-public-finance-0x0010-plus-l1";
        else if (finance_syscol(syscol))
            item["resolver"] = "public-finance-0x0010-plus-l1";
        else if (group_syscol(syscol))
            item["resolver"] =
                syscol == "$S_NUM"
                    ? "tbigdata-native-s-zqdm-member-count"
                    : (syscol == "$S_JQZF"
                           ? "tbigdata-native-block-aggregation-public-l1-total-shares"
                           : "tbigdata-native-block-aggregation-public-l1");
        else if (syscol == "$TDXHY" || syscol == "$TDXHYCODE")
            item["resolver"] = "local-tdx-industry-hierarchy";
        else if (syscol == "$BONDAI")
            item["resolver"] = "row-bond-actual-365";
        else
            item["resolver"] = "unresolved";
        const bool auto_resolved = quote_syscol(syscol) || finance_syscol(syscol) ||
                                   group_syscol(syscol) || syscol == "$TDXHY" ||
                                   syscol == "$TDXHYCODE" || syscol == "$BONDAI";
        item["auto_resolved"] = auto_resolved;
        if (auto_resolved)
            auto_resolvable_host_columns += explicit_count + implicit_count;
        host_catalog.push_back(std::move(item));
    }
    summary["effective_host_columns"] = syscol_columns + implicit_host_columns;
    summary["auto_resolvable_host_columns"] = auto_resolvable_host_columns;
    summary["unresolved_host_columns"] =
        syscol_columns + implicit_host_columns - auto_resolvable_host_columns;
    Json external_catalog = Json::array();
    for (const auto &[name, count] : external_inputs) {
        Json item = Json::object();
        item["name"] = name;
        item["count"] = count;
        if (name == "DQLL2") {
            item["resolver"] = "row-current-coupon-rate";
        } else if (name == "price2") {
            // The only external price2 references in the installed CFG set
            // are two hidden calculations in func_qxfa201.  That resource is
            // the still-locked private-placement list: date2 is in the future,
            // so the close preceding the unlock day does not exist yet.  Once
            // the event matures it moves to func_qxfa202, whose schema and JSN
            // both carry price2 explicitly.
            item["resolver"] = "upstream-lifecycle-conditional";
            item["availability"] = "undefined-before-private-placement-unlock";
            item["active_resource"] = "func_qxfa201_1.jsn";
            item["matured_resource"] = "func_qxfa202_1.jsn";
            item["manual_input_recommended"] = false;
        } else {
            item["resolver"] = "explicit-input-required";
        }
        external_catalog.push_back(std::move(item));
    }
    Json report = Json::object();
    report["schema"] = audit_schema;
    report["execution_mode"] = "native-cpp-offline";
    report["dll_loaded"] = false;
    report["source_contract"] = "TBigData.dll 1.0.2.40 static table and cloud_cfg calc/calcref";
    report["summary"] = std::move(summary);
    report["used_builtins"] = std::move(used);
    report["builtin_catalog"] = builtin_catalog(usage);
    report["host_system_columns"] = std::move(host_catalog);
    report["external_formula_inputs"] = std::move(external_catalog);
    report["parse_errors"] = std::move(parse_errors);
    report["files"] = std::move(files);
    return report;
}

} // namespace tdx
