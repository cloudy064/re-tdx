#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace tdx {

// Resolve the subset of TDX external formula data for which this project has a
// verified public protocol mapping (FINANCE, CAPITAL and selected DYNAINFO IDs).
Json build_formula_market_context_document(
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const Json& analysis,
    int timeout_ms = 10000,
    const BlockData* block_data = nullptr,
    const Json* kline_document = nullptr,
    bool point_in_time_finance = false,
    const Json* formula_library = nullptr,
    const std::filesystem::path& jsn_root = {},
    const Json* nested_state = nullptr,
    const std::map<std::string, double>* formula_parameters = nullptr);

// Pure series builder used after archived disclosure events have been joined
// with their report-period professional-finance records.  A report becomes
// visible only on a bar strictly after its actual disclosure date.
Json build_point_in_time_finance_series_document(
    const Json& kline_document,
    const Json& available_reports,
    const std::set<int>& finance_bindings,
    const std::set<int>& finvalue_bindings);

// Pure fixed-vector builder for TCalc DIVFACTOR(1/2).  It consumes decoded
// public 0x000F records and preserves the DLL's float32, bonus-only semantics.
Json build_divfactor_series_document(
    const Json& kline_document,
    const Json& capital_document);

}  // namespace tdx
