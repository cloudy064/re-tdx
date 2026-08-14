#pragma once

#include "tdx/json.hpp"
#include "tdx/blocks.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

// Audit every TBigData calc/calcref column below a TDX cloud_cfg directory,
// or a single CFG file. This is a read-only parser and never loads TBigData.dll.
Json audit_cloud_calc_configs(const std::filesystem::path& root_or_cfg);

// Evaluate the calculated columns in one CFG against a flat input row. The row
// may contain numbers, numeric strings and comma-separated cash-flow vectors.
// as_of_yyyymmdd==0 uses the local calendar date for $SF_CurrDate$.
Json evaluate_cloud_calc_row(const std::filesystem::path& cfg,
                             const Json& row,
                             int as_of_yyyymmdd = 0);

// Reconstruct the subset of TdxW host fields that can be proven from a public
// 0x054C/0x0547 snapshot, 0x053E rise-speed quote, and the source row itself.
// snapshot_document may be either a
// market snapshot document ({"records":[...]}) or a raw record array. No
// network request is made here. The returned document contains the enriched
// row, per-field provenance and explicit unresolved host fields.
Json resolve_cloud_calc_host_fields(const std::filesystem::path& cfg,
                                    const Json& row,
                                    const Json& snapshot_document = Json::array(),
                                    int as_of_yyyymmdd = 0);

// Extended deterministic source form used when the caller also has a public
// 0x0010 finance document and/or the locally parsed TDX industry hierarchy.
Json resolve_cloud_calc_host_fields(const std::filesystem::path& cfg,
                                    const Json& row,
                                    const Json& snapshot_document,
                                    const Json& finance_document,
                                    const BlockData* block_data,
                                    int as_of_yyyymmdd = 0);

// Safe reusable orchestration for localhost/API callers. `cfg_name` is resolved
// below root/T0002/cloud_cfg and cannot escape that directory. Inline snapshot
// documents provide the same deterministic mode as the CLI file options;
// `quotes=true` instead fetches only the public L1/finance fields requested by
// the selected CFG.
struct CloudCalcEvaluationOptions {
    bool quotes{false};
    Json snapshot{Json::array()};
    Json finance_snapshot{Json::array()};
    Json special_limits_snapshot{Json::array()};
    Json overrides{Json::object()};
    int as_of_yyyymmdd{0};
    int timeout_ms{10000};
};

Json audit_cloud_calc_request(const std::filesystem::path& root,
                              const std::string& cfg_name = {});

// Derive the smallest conservative editable row skeleton needed by a CFG.
// Calculated columns and proven host fields are separated from caller-supplied
// inputs; no JSN row or server path is read from the request.
Json generate_cloud_calc_template_request(const std::filesystem::path& root,
                                          const std::string& cfg_name);

Json evaluate_cloud_calc_request(const std::filesystem::path& root,
                                 const std::string& cfg_name,
                                 const Json& row,
                                 const CloudCalcEvaluationOptions& options = {});

// Evaluate 1..128 inline rows while sharing one public quote/finance fetch plan.
// Results remain associated by zero-based row_index; request rows are never
// retained or echoed in the returned document.
Json evaluate_cloud_calc_batch_request(const std::filesystem::path& root,
                                       const std::string& cfg_name,
                                       const Json& rows,
                                       const CloudCalcEvaluationOptions& options = {});

int command_formulas_cloud_calc(const std::vector<std::string>& args);

}  // namespace tdx
