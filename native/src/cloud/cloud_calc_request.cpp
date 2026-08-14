#include "tdx/cloud_calc.hpp"

#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_service_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/seal_order.hpp"
#include "tdx/session_audit.hpp"

#include <optional>
#include <set>

namespace fs = std::filesystem;

namespace tdx {

using namespace cloud_calc_detail;
using namespace cloud_calc_service_detail;
Json evaluate_cloud_calc_request(const fs::path &root, const std::string &cfg_name,
                                 const Json &source_row,
                                 const CloudCalcEvaluationOptions &options) {
    if (cfg_name.empty())
        throw Error("cloud-calc cfg is required");
    require_safe_cfg_request_name(cfg_name);
    validate_cloud_calc_row(source_row);
    validate_cloud_calc_options(options);
    const bool has_snapshot = options.snapshot.size() != 0;
    const bool has_finance = options.finance_snapshot.size() != 0;

    const auto cfg = resolve_cfg(root, cfg_name);
    auto row = source_row;
    auto snapshots = options.snapshot;
    auto finance_document = options.finance_snapshot;
    auto special_limits_document = options.special_limits_snapshot;
    std::string snapshot_source = has_snapshot ? "inline-request" : "none";
    std::string finance_source = has_finance ? "inline-request" : "none";
    const auto request = resolve_cloud_calc_host_fields(cfg, row, snapshots, finance_document,
                                                        nullptr, options.as_of_yyyymmdd);
    std::optional<BlockData> blocks;
    std::set<std::string> block_families;
    if (request.at("requested_industry_securities").size()) {
        block_families.insert("industry");
        block_families.insert("research-industry");
    }
    const bool seal_requested = request.at("requested_seal_securities").size() != 0;
    if (seal_requested || !block_families.empty())
        blocks = load_blocks(root, block_families);
    Json seal_enrichment = Json::object();
    seal_enrichment["requested"] = seal_requested;
    if (options.quotes) {
        std::vector<std::string> securities, finance_securities;
        for (const auto &value : request.at("requested_securities").as_array())
            securities.push_back(value.as_string());
        for (const auto &value : request.at("requested_finance_securities").as_array())
            finance_securities.push_back(value.as_string());
        if (!securities.empty()) {
            snapshots =
                request.at("requested_speed_securities").size()
                    ? fetch_market_speed_document(root, securities, options.timeout_ms)
                    : (request.at("requested_depth_securities").size()
                           ? fetch_market_depth_document(root, securities, options.timeout_ms)
                           : fetch_market_snapshot_document(root, securities, options.timeout_ms));
        }
        if (!finance_securities.empty())
            finance_document = fetch_finance_document(finance_securities,
                                                      load_public_quote_endpoints(root).endpoints,
                                                      options.timeout_ms, 80, false);
        if (seal_requested) {
            try {
                special_limits_document =
                    fetch_special_limits_document(load_public_quote_endpoints(root).endpoints,
                                                  options.timeout_ms, 0, 10000, false);
            } catch (const std::exception &error) {
                seal_enrichment["special_limit_error"] = error.what();
            }
        }
        snapshot_source =
            request.at("requested_speed_securities").size()
                ? "public-l1-rise-speed-0x053e"
                : (request.at("requested_depth_securities").size() ? "public-l1-depth-0x0547"
                                                                   : "public-l1-0x054c");
        finance_source = "public-finance-0x0010";
    }
    if (seal_requested && blocks && snapshot_records(snapshots).size()) {
        auto details = enrich_seal_snapshots(snapshots, *blocks, load_limit_rule_config(root),
                                             special_limits_document, options.as_of_yyyymmdd);
        if (const auto *error = optional(seal_enrichment, "special_limit_error"))
            details["special_limit_error"] = *error;
        seal_enrichment = std::move(details);
    }
    auto host = resolve_cloud_calc_host_fields(cfg, row, snapshots, finance_document,
                                               blocks ? &*blocks : nullptr, options.as_of_yyyymmdd);
    row = host.at("row");
    host.as_object().erase("row");
    host["snapshot_source"] = snapshot_source;
    host["finance_source"] = finance_source;
    host["industry_source"] = blocks ? "local-tdx-industry-hierarchy" : "none";
    host["seal_enrichment"] = std::move(seal_enrichment);
    Json explicit_overrides = Json::array();
    for (const auto &[code, value] : options.overrides.as_object()) {
        row[code] = value;
        Json item = Json::object();
        item["code"] = code;
        item["value"] = value;
        explicit_overrides.push_back(std::move(item));
    }
    auto report = evaluate_cloud_calc_row(cfg, row, options.as_of_yyyymmdd);
    report["execution_mode"] =
        options.quotes ? "native-cpp-public-l1-finance" : "native-cpp-offline";
    report["row_source"] = "inline-request";
    report.as_object().erase("cfg");
    report["cfg_name"] = cfg.filename().u8string();
    report["override_count"] = static_cast<std::uint64_t>(options.overrides.size());
    report["explicit_overrides"] = std::move(explicit_overrides);
    report["host_context"] = std::move(host);
    return report;
}

} // namespace tdx
