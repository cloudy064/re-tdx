#include "tdx/cloud_calc.hpp"

#include "cloud_calc_config_internal.hpp"
#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_interpreter_internal.hpp"
#include "cloud_calc_service_internal.hpp"
#include "tdx/common.hpp"

namespace fs = std::filesystem;

namespace tdx {

using namespace cloud_calc_detail;
using namespace cloud_calc_service_detail;
Json evaluate_cloud_calc_row(const fs::path &cfg, const Json &row, int as_of_yyyymmdd) {
    if (!row.is_object())
        throw Error("cloud-calc input row must be a JSON object");
    if (as_of_yyyymmdd != 0)
        validate_date(as_of_yyyymmdd);
    const auto config = parse_config(cfg);
    Json units = Json::array();
    std::uint64_t calculated = 0;
    std::uint64_t evaluated = 0;
    std::uint64_t unavailable = 0;
    std::uint64_t errors = 0;
    for (const auto &unit : config.units) {
        bool has_calc = false;
        for (const auto &column : unit.columns)
            if (!column.calc.empty()) {
                has_calc = true;
                break;
            }
        if (!has_calc)
            continue;
        auto report = evaluate_unit(unit, row, as_of_yyyymmdd);
        const auto &counts = report.at("counts");
        calculated += static_cast<std::uint64_t>(counts.at("calculated").as_number());
        evaluated += static_cast<std::uint64_t>(counts.at("evaluated").as_number());
        unavailable += static_cast<std::uint64_t>(counts.at("unavailable").as_number());
        errors += static_cast<std::uint64_t>(counts.at("errors").as_number());
        units.push_back(std::move(report));
    }
    Json counts = Json::object();
    counts["calculated"] = calculated;
    counts["evaluated"] = evaluated;
    counts["unavailable"] = unavailable;
    counts["errors"] = errors;
    Json result = Json::object();
    result["schema"] = evaluation_schema;
    result["execution_mode"] = "native-cpp-offline";
    result["dll_loaded"] = false;
    result["cfg"] = path_utf8(cfg);
    result["as_of"] = as_of_yyyymmdd == 0 ? local_yyyymmdd() : as_of_yyyymmdd;
    result["input_field_count"] = static_cast<std::uint64_t>(row.size());
    result["counts"] = std::move(counts);
    result["units"] = std::move(units);
    return result;
}

Json resolve_cloud_calc_host_fields(const fs::path &cfg, const Json &row,
                                    const Json &snapshot_document, int as_of_yyyymmdd) {
    return resolve_host_fields(parse_config(cfg), row, snapshot_document, Json::array(), nullptr,
                               as_of_yyyymmdd);
}

Json resolve_cloud_calc_host_fields(const fs::path &cfg, const Json &row,
                                    const Json &snapshot_document, const Json &finance_document,
                                    const BlockData *block_data, int as_of_yyyymmdd) {
    return resolve_host_fields(parse_config(cfg), row, snapshot_document, finance_document,
                               block_data, as_of_yyyymmdd);
}

Json audit_cloud_calc_request(const fs::path &root, const std::string &cfg_name) {
    if (!cfg_name.empty())
        require_safe_cfg_request_name(cfg_name);
    return cfg_name.empty() ? audit_cloud_calc_configs(root)
                            : audit_cloud_calc_configs(resolve_cfg(root, cfg_name));
}

} // namespace tdx
