#include "tdx/cloud_calc.hpp"

#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_interpreter_internal.hpp"
#include "cloud_calc_service_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/seal_order.hpp"
#include "tdx/session_audit.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <set>

namespace fs = std::filesystem;

namespace tdx {

using namespace cloud_calc_detail;
using namespace cloud_calc_service_detail;
int command_formulas_cloud_calc(const std::vector<std::string> &values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout
            << "Usage: tdx-tool formulas cloud-calc [options]\n\n"
               "Audit all configured calculations:\n"
               "  --root PATH             TDX root or cloud_cfg directory (auto-detected by "
               "default)\n"
               "  --cfg FILE|NAME         Restrict audit/evaluation to one CFG\n\n"
               "  --template              Generate the selected CFG's minimal editable row "
               "skeleton\n\n"
               "Evaluate source rows:\n"
               "  --row FILE              Flat JSON object, object array, or JSN table\n"
               "  --jsn FILE              Alias emphasizing a colheader/data JSN source\n"
               "  --row-index N           Zero-based row index (default 0)\n"
               "  --all-rows              Evaluate the first bounded batch instead of one row\n"
               "  --max-rows N            Batch bound for --all-rows (default/max 128)\n"
               "  --quotes                Resolve supported syscols through public L1/finance\n"
               "  --snapshot FILE         Resolve syscol fields from an offline snapshot JSON\n"
               "  --finance-snapshot FILE Offline 0x0010 finance document/record array\n"
               "  --special-limits-snapshot FILE  Offline public 0x0452 limit-price document\n"
               "  --timeout-ms N          Public L1/finance timeout (default 10000)\n"
               "  --set CODE=VALUE        Repeatable input override; numbers stay numeric\n"
               "  --as-of YYYYMMDD        Deterministic $SF_CurrDate$ value\n\n"
               "Output:\n"
               "  --output FILE           Write JSON instead of stdout\n"
               "  --compact               Compact JSON\n\n"
               "The command is pure C++, read-only and loads no DLL. Network is used only with "
               "--quotes.\n";
        return 0;
    }
    const auto root_name = args.take_option("--root");
    const auto cfg_name = args.take_option("--cfg");
    const auto row_name = args.take_option("--row");
    const auto jsn_name = args.take_option("--jsn");
    const auto row_index_name = args.take_option("--row-index");
    const auto row_index = bounded_index(row_index_name.empty() ? "0" : row_index_name);
    const bool all_rows = args.take_flag("--all-rows");
    const bool generate_template = args.take_flag("--template");
    const auto max_rows_value = bounded_index(args.take_option("--max-rows", "128"));
    if (max_rows_value < 1 || max_rows_value > 128)
        throw Error("--max-rows must be in 1..128");
    const auto max_rows = static_cast<std::size_t>(max_rows_value);
    const bool quotes = args.take_flag("--quotes");
    const auto snapshot_name = args.take_option("--snapshot");
    const auto finance_snapshot_name = args.take_option("--finance-snapshot");
    const auto special_limits_snapshot_name = args.take_option("--special-limits-snapshot");
    const int timeout_ms = parse_int(args.take_option("--timeout-ms", "10000"), -1);
    const auto assignments = args.take_options("--set");
    const auto as_of_name = args.take_option("--as-of");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (timeout_ms < 100 || timeout_ms > 600000)
        throw Error("--timeout-ms must be in 100..600000");
    if (quotes && (!snapshot_name.empty() || !finance_snapshot_name.empty() ||
                   !special_limits_snapshot_name.empty()))
        throw Error("--quotes cannot be combined with offline snapshot options");
    if (!row_name.empty() && !jsn_name.empty())
        throw Error("use only one of --row and --jsn");
    if (generate_template && cfg_name.empty())
        throw Error("--template requires --cfg");
    if (generate_template &&
        (!row_name.empty() || !jsn_name.empty() || quotes || !snapshot_name.empty() ||
         !finance_snapshot_name.empty() || !special_limits_snapshot_name.empty() ||
         !assignments.empty() || all_rows || !row_index_name.empty()))
        throw Error("--template cannot be combined with row, quote, snapshot, override or "
                    "row-selection options");
    if (all_rows && !row_index_name.empty())
        throw Error("--all-rows cannot be combined with --row-index");
    if (all_rows && row_name.empty() && jsn_name.empty())
        throw Error("--all-rows requires --row or --jsn");
    if ((!row_name.empty() || !jsn_name.empty()) && cfg_name.empty())
        throw Error("--row/--jsn requires --cfg");
    if ((quotes || !snapshot_name.empty() || !finance_snapshot_name.empty() ||
         !special_limits_snapshot_name.empty()) &&
        (row_name.empty() && jsn_name.empty()))
        throw Error("quote/snapshot options require --row or --jsn");
    fs::path root = root_name.empty() ? find_tdx_root() : fs::u8path(root_name);
    const int as_of = as_of_name.empty() ? 0 : parse_int(as_of_name, -1);
    if (!as_of_name.empty())
        validate_date(as_of);
    Json report;
    if (generate_template) {
        report = generate_cloud_calc_template_request(root, cfg_name);
    } else if (cfg_name.empty()) {
        report = audit_cloud_calc_configs(root);
    } else {
        const auto cfg = resolve_cfg(root, cfg_name);
        const auto source_name = row_name.empty() ? jsn_name : row_name;
        if (source_name.empty()) {
            report = audit_cloud_calc_configs(cfg);
        } else if (all_rows) {
            const auto source_path = fs::u8path(source_name);
            const auto source = Json::parse(decode_gbk(read_bytes(source_path)));
            auto batch_input = batch_rows_from_document(source, max_rows);
            CloudCalcEvaluationOptions options;
            options.quotes = quotes;
            options.timeout_ms = timeout_ms;
            options.as_of_yyyymmdd = as_of;
            if (!snapshot_name.empty())
                options.snapshot = Json::parse(read_text_utf8(fs::u8path(snapshot_name)));
            if (!finance_snapshot_name.empty())
                options.finance_snapshot =
                    Json::parse(read_text_utf8(fs::u8path(finance_snapshot_name)));
            if (!special_limits_snapshot_name.empty())
                options.special_limits_snapshot =
                    Json::parse(read_text_utf8(fs::u8path(special_limits_snapshot_name)));
            for (const auto &assignment : assignments) {
                const auto equal = assignment.find('=');
                if (equal == std::string::npos || equal == 0)
                    throw Error("--set must be CODE=VALUE: " + assignment);
                options.overrides[assignment.substr(0, equal)] =
                    assignment_value(assignment.substr(equal + 1));
            }
            report = evaluate_cloud_calc_batch_request(root, cfg_name, batch_input.rows, options);
            report["row_source"] = path_utf8(source_path);
            report["source_rows_available"] = static_cast<std::uint64_t>(batch_input.available);
            report["source_rows_truncated"] = batch_input.available > batch_input.rows.size();
        } else {
            auto row = read_row(fs::u8path(source_name), static_cast<std::size_t>(row_index));
            Json snapshots = Json::array();
            Json finance_document = Json::array();
            Json special_limits_document = Json::array();
            std::string snapshot_source = "none", finance_source = "none";
            if (!snapshot_name.empty()) {
                snapshots = Json::parse(read_text_utf8(fs::u8path(snapshot_name)));
                (void)snapshot_records(snapshots);
                snapshot_source = "file";
            }
            if (!finance_snapshot_name.empty()) {
                finance_document = Json::parse(read_text_utf8(fs::u8path(finance_snapshot_name)));
                (void)snapshot_records(finance_document);
                finance_source = "file";
            }
            if (!special_limits_snapshot_name.empty()) {
                special_limits_document =
                    Json::parse(read_text_utf8(fs::u8path(special_limits_snapshot_name)));
                if (!special_limits_document.is_object())
                    throw Error("--special-limits-snapshot must contain a 0x0452 document object");
            }
            const auto request = resolve_cloud_calc_host_fields(cfg, row, snapshots,
                                                                finance_document, nullptr, as_of);
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
            if (quotes) {
                std::vector<std::string> securities, finance_securities;
                for (const auto &value : request.at("requested_securities").as_array())
                    securities.push_back(value.as_string());
                for (const auto &value : request.at("requested_finance_securities").as_array())
                    finance_securities.push_back(value.as_string());
                if (!securities.empty()) {
                    snapshots =
                        request.at("requested_speed_securities").size()
                            ? fetch_market_speed_document(root, securities, timeout_ms)
                            : (request.at("requested_depth_securities").size()
                                   ? fetch_market_depth_document(root, securities, timeout_ms)
                                   : fetch_market_snapshot_document(root, securities, timeout_ms));
                }
                if (!finance_securities.empty())
                    finance_document = fetch_finance_document(
                        finance_securities, load_public_quote_endpoints(root).endpoints, timeout_ms,
                        80, false);
                if (seal_requested) {
                    try {
                        special_limits_document = fetch_special_limits_document(
                            load_public_quote_endpoints(root).endpoints, timeout_ms, 0, 10000,
                            false);
                    } catch (const std::exception &error) {
                        seal_enrichment["special_limit_error"] = error.what();
                    }
                }
                snapshot_source = request.at("requested_speed_securities").size()
                                      ? "public-l1-rise-speed-0x053e"
                                      : (request.at("requested_depth_securities").size()
                                             ? "public-l1-depth-0x0547"
                                             : "public-l1-0x054c");
                finance_source = "public-finance-0x0010";
            }
            if (seal_requested && blocks && snapshot_records(snapshots).size()) {
                auto details =
                    enrich_seal_snapshots(snapshots, *blocks, load_limit_rule_config(root),
                                          special_limits_document, as_of);
                if (const auto *error = optional(seal_enrichment, "special_limit_error"))
                    details["special_limit_error"] = *error;
                seal_enrichment = std::move(details);
            }
            auto host = resolve_cloud_calc_host_fields(cfg, row, snapshots, finance_document,
                                                       blocks ? &*blocks : nullptr, as_of);
            row = host.at("row");
            host.as_object().erase("row");
            host["snapshot_source"] = snapshot_source;
            host["finance_source"] = finance_source;
            host["industry_source"] = blocks ? "local-tdx-industry-hierarchy" : "none";
            host["seal_enrichment"] = std::move(seal_enrichment);
            Json explicit_overrides = Json::array();
            for (const auto &assignment : assignments) {
                const auto equal = assignment.find('=');
                if (equal == std::string::npos || equal == 0)
                    throw Error("--set must be CODE=VALUE: " + assignment);
                const auto code = assignment.substr(0, equal);
                auto value = assignment_value(assignment.substr(equal + 1));
                row[code] = value;
                Json item = Json::object();
                item["code"] = code;
                item["value"] = std::move(value);
                explicit_overrides.push_back(std::move(item));
            }
            report = evaluate_cloud_calc_row(cfg, row, as_of);
            report["execution_mode"] =
                quotes ? "native-cpp-public-l1-finance" : "native-cpp-offline";
            report["row_source"] = path_utf8(fs::u8path(source_name));
            report["row_index"] = row_index;
            report["override_count"] = static_cast<std::uint64_t>(assignments.size());
            report["explicit_overrides"] = std::move(explicit_overrides);
            report["host_context"] = std::move(host);
        }
    }
    const auto rendered = report.dump(compact ? -1 : 2) + '\n';
    if (output_name.empty())
        std::cout << rendered;
    else {
        const auto output = fs::u8path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "TBigData cloud-calc report -> " << path_utf8(output) << '\n';
    }
    return 0;
}

} // namespace tdx
