#include "tdx/cloud_calc.hpp"

#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_interpreter_internal.hpp"
#include "cloud_calc_service_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/seal_order.hpp"
#include "tdx/session_audit.hpp"

#include <optional>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace cloud_calc_detail;
using namespace cloud_calc_service_detail;
Json evaluate_cloud_calc_batch_request(const fs::path &root, const std::string &cfg_name,
                                       const Json &source_rows,
                                       const CloudCalcEvaluationOptions &options) {
    if (cfg_name.empty())
        throw Error("cloud-calc batch cfg is required");
    require_safe_cfg_request_name(cfg_name);
    validate_cloud_calc_options(options);
    if (!source_rows.is_array())
        throw Error("cloud-calc batch rows must be an array");
    if (source_rows.size() == 0 || source_rows.size() > 128)
        throw Error("cloud-calc batch rows must contain 1..128 objects");
    std::uint64_t input_fields = 0;
    for (const auto &row : source_rows.as_array()) {
        validate_cloud_calc_row(row);
        input_fields += static_cast<std::uint64_t>(row.size());
        if (input_fields > 65536)
            throw Error("cloud-calc batch exceeds 65536 input fields");
    }

    const auto cfg = resolve_cfg(root, cfg_name);
    std::set<std::string, std::less<>> securities;
    std::set<std::string, std::less<>> finance_securities;
    std::set<std::string, std::less<>> speed_securities;
    std::set<std::string, std::less<>> depth_securities;
    std::set<std::string, std::less<>> industry_securities;
    std::set<std::string, std::less<>> seal_securities;
    std::vector<std::string> preflight_errors(source_rows.size());
    for (std::size_t index = 0; index < source_rows.size(); ++index) {
        try {
            const auto request = resolve_cloud_calc_host_fields(
                cfg, source_rows.as_array()[index], options.snapshot, options.finance_snapshot,
                nullptr, options.as_of_yyyymmdd);
            collect_requested_securities(request, "requested_securities", securities);
            collect_requested_securities(request, "requested_finance_securities",
                                         finance_securities);
            collect_requested_securities(request, "requested_speed_securities", speed_securities);
            collect_requested_securities(request, "requested_depth_securities", depth_securities);
            collect_requested_securities(request, "requested_industry_securities",
                                         industry_securities);
            collect_requested_securities(request, "requested_seal_securities", seal_securities);
        } catch (const std::exception &error) {
            preflight_errors[index] = error.what();
        }
    }

    Json snapshots = options.snapshot;
    Json finance_document = options.finance_snapshot;
    Json special_limits_document = options.special_limits_snapshot;
    std::string snapshot_source = options.snapshot.size() ? "inline-request" : "none";
    std::string finance_source = options.finance_snapshot.size() ? "inline-request" : "none";
    std::string quote_mode = "none";
    std::string special_limit_error;
    std::uint64_t quote_document_fetches = 0;
    std::uint64_t finance_document_fetches = 0;
    std::uint64_t special_limit_document_fetches = 0;
    if (options.quotes) {
        const auto quote_securities = security_vector(securities);
        if (!quote_securities.empty()) {
            if (!speed_securities.empty()) {
                snapshots = fetch_market_speed_document(root, quote_securities, options.timeout_ms);
                snapshot_source = "public-l1-rise-speed-0x053e";
                quote_mode = "speed-0x053e";
            } else if (!depth_securities.empty()) {
                snapshots = fetch_market_depth_document(root, quote_securities, options.timeout_ms);
                snapshot_source = "public-l1-depth-0x0547";
                quote_mode = "depth-0x0547";
            } else {
                snapshots =
                    fetch_market_snapshot_document(root, quote_securities, options.timeout_ms);
                snapshot_source = "public-l1-0x054c";
                quote_mode = "snapshot-0x054c";
            }
            quote_document_fetches = 1;
        }
        if (!finance_securities.empty()) {
            finance_document = fetch_finance_document(security_vector(finance_securities),
                                                      load_public_quote_endpoints(root).endpoints,
                                                      options.timeout_ms, 80, false);
            finance_document_fetches = 1;
        }
        finance_source = "public-finance-0x0010";
        if (!seal_securities.empty()) {
            try {
                special_limits_document =
                    fetch_special_limits_document(load_public_quote_endpoints(root).endpoints,
                                                  options.timeout_ms, 0, 10000, false);
                special_limit_document_fetches = 1;
            } catch (const std::exception &error) {
                special_limit_error = error.what();
            }
        }
    }

    CloudCalcEvaluationOptions shared_options = options;
    shared_options.quotes = false;
    shared_options.snapshot = snapshots;
    shared_options.finance_snapshot = finance_document;
    shared_options.special_limits_snapshot = special_limits_document;
    Json row_results = Json::array();
    std::uint64_t succeeded = 0, failed = 0;
    std::uint64_t calculated = 0, evaluated = 0, unavailable = 0, errors = 0;
    for (std::size_t index = 0; index < source_rows.size(); ++index) {
        Json entry = Json::object();
        entry["row_index"] = static_cast<std::uint64_t>(index);
        try {
            if (!preflight_errors[index].empty())
                throw Error(preflight_errors[index]);
            auto result = evaluate_cloud_calc_request(root, cfg_name, source_rows.as_array()[index],
                                                      shared_options);
            if (options.quotes) {
                result["execution_mode"] = "native-cpp-public-l1-finance";
                result["host_context"]["snapshot_source"] = snapshot_source;
                result["host_context"]["finance_source"] = finance_source;
                if (!special_limit_error.empty())
                    result["host_context"]["seal_enrichment"]["special_limit_error"] =
                        special_limit_error;
            }
            const auto &counts = result.at("counts");
            calculated += static_cast<std::uint64_t>(counts.at("calculated").as_number());
            evaluated += static_cast<std::uint64_t>(counts.at("evaluated").as_number());
            unavailable += static_cast<std::uint64_t>(counts.at("unavailable").as_number());
            errors += static_cast<std::uint64_t>(counts.at("errors").as_number());
            entry["status"] = "ok";
            entry["result"] = std::move(result);
            ++succeeded;
        } catch (const std::exception &error) {
            entry["status"] = "error";
            entry["error"] = error.what();
            ++failed;
        }
        row_results.push_back(std::move(entry));
    }

    Json counts = Json::object();
    counts["rows"] = static_cast<std::uint64_t>(source_rows.size());
    counts["succeeded"] = succeeded;
    counts["failed"] = failed;
    counts["calculated"] = calculated;
    counts["evaluated"] = evaluated;
    counts["unavailable"] = unavailable;
    counts["errors"] = errors;
    Json fetch_plan = Json::object();
    fetch_plan["quote_mode"] = quote_mode;
    fetch_plan["unique_quote_securities"] = static_cast<std::uint64_t>(securities.size());
    fetch_plan["unique_finance_securities"] = static_cast<std::uint64_t>(finance_securities.size());
    fetch_plan["unique_industry_securities"] =
        static_cast<std::uint64_t>(industry_securities.size());
    fetch_plan["unique_seal_securities"] = static_cast<std::uint64_t>(seal_securities.size());
    fetch_plan["quote_document_fetches"] = quote_document_fetches;
    fetch_plan["finance_document_fetches"] = finance_document_fetches;
    fetch_plan["special_limit_document_fetches"] = special_limit_document_fetches;
    Json report = Json::object();
    report["schema"] = batch_schema;
    report["execution_mode"] =
        options.quotes ? "native-cpp-public-l1-finance" : "native-cpp-offline";
    report["dll_loaded"] = false;
    report["cfg_name"] = cfg.filename().u8string();
    report["as_of"] = options.as_of_yyyymmdd == 0 ? local_yyyymmdd() : options.as_of_yyyymmdd;
    report["row_source"] = "inline-request-batch";
    report["request_body_retained"] = false;
    report["input_field_count"] = input_fields;
    report["counts"] = std::move(counts);
    report["fetch_plan"] = std::move(fetch_plan);
    report["rows"] = std::move(row_results);
    return report;
}

} // namespace tdx
