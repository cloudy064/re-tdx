#pragma once

#include "tdx/json.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct FormulaExplicitContextCaptureImportRequest {
    Json context_template;
    Json capture;
    bool allow_partial{false};
};

struct FormulaFutureReplayRequest {
    Json kline_document;
    std::string source;
    std::map<std::string, double> parameters;
    std::string formula_code{"CUSTOM"};
    Json context;
    bool context_provided{false};
    std::size_t max_observations{20};
    std::size_t max_events{1000};
};

// Analyze one recovered TDX formula without executing it.  The result describes
// syntax support, market-data dependencies, look-ahead functions and outputs.
Json analyze_formula_source(const std::string& source,
                            const std::vector<std::string>& parameters = {});

// Build the same formula-row shape used by the extracted TCalc library for a
// caller-supplied source string.  Scan/backtest/evaluate can therefore share
// one implementation and one set of semantic safety checks.
Json make_formula_source_definition(
    const std::string& source,
    const std::string& code = "CUSTOM",
    const std::string& kind = "technical",
    const std::map<std::string, double>& parameters = {});

// Add per-formula analysis to an extract_formulas_document() result and produce
// aggregate coverage counters.
Json analyze_formula_library_document(Json library);

// Resolve double-quoted `INDICATOR.OUTPUT` dependencies across a recovered
// formula library.  The report is structural: it identifies missing targets,
// missing outputs, runtime-ineligible targets and cyclic technical formulas
// without fetching market data or executing any formula.
Json analyze_formula_reference_graph(const Json& library);

// Build a caller-fillable context document for formulas whose broker-private
// or authorized Level2 inputs cannot be obtained from public OHLCV/L1 data.
Json make_formula_explicit_context_template_document(
    Json library,
    const std::vector<std::string>& formula_codes = {},
    const std::vector<std::string>& stamps = {});

// Extract exact DATE|TIME keys from a normal K-line document so explicit
// caller-owned series can be prepared for an entire evaluation window.
std::vector<std::string> formula_context_stamps_from_kline(const Json& kline_document);

// Validate a caller-owned capture against an exact context template and
// materialize the existing tdx-formula-explicit-context-v1 document consumed
// by evaluate/audit.  The importer is pure: it neither obtains restricted
// data nor invokes a broker, SDK, subscription, account, or order operation.
Json import_formula_explicit_context_capture(
    const FormulaExplicitContextCaptureImportRequest& request);

// Re-evaluate an explicitly allowed future-function formula over successively
// revealed K-line prefixes.  Only changes to already-existing historical
// output points are retained, making repaint behavior observable without
// enabling scan, backtest, account, order, SDK, or network operations.
Json replay_formula_future_document(const FormulaFutureReplayRequest& request);

// Report whether every caller-owned binding required by an analyzed formula is
// structurally ready for execution.  Ordinary series still need at least one
// numeric value; a fully materialized TCalc type-31 context may represent a
// complete native-missing window with explicit null values.
bool formula_explicit_context_ready(const Json& analysis, const Json* context);

// Execute recovered TDX source against a normal fetch_kline_document() result.
// Parameter names are ASCII case-insensitive.
Json evaluate_formula_source_document(
    Json kline_document,
    const std::string& source,
    const std::map<std::string, double>& parameters = {},
    const std::string& formula_code = "CUSTOM",
    const Json* context = nullptr);

// Execute a formula row from extract_formulas_document(); metadata defaults are
// merged with the supplied overrides.
Json evaluate_formula_document(
    Json kline_document,
    const Json& formula,
    const std::map<std::string, double>& parameters = {},
    const Json* context = nullptr);

// Execute every statically executable formula without retaining the large
// point arrays.  This turns syntax coverage into a runtime compatibility
// report and identifies formulas whose outputs remain empty.
Json audit_formula_library_document(Json kline_document, Json library,
                                    const Json* context = nullptr,
                                    bool allow_future = false);

// Evaluate a formula over a collection of K-line documents and retain securities
// with a true output during the last `lookback` bars.
Json scan_formula_documents(
    const std::vector<Json>& kline_documents,
    const Json& formula,
    const std::map<std::string, double>& parameters = {},
    int lookback = 1);

// Compare two scan snapshots (or a persisted watch state and a scan snapshot)
// by security_id.  Besides enter/exit, updates retain a continuously matched
// security whose trigger bar or signal payload changed.
Json diff_formula_scan_results(const Json& previous, const Json& current);

// Build the compact, source-free state persisted by `formulas watch`.
Json make_formula_scan_watch_state(const Json& scan,
                                   const std::string& configuration_id,
                                   std::uint64_t iteration);

// Render the active scan matches as a TDX blocknew .blk membership file.
std::string formula_scan_block_text(const Json& scan);

// Long-only, signal-at-close / execute-next-open backtest for expert formulas.
Json backtest_formula_document(
    Json kline_document,
    const Json& formula,
    const std::map<std::string, double>& parameters = {},
    double initial_capital = 100000.0,
    double commission_bps = 2.5,
    double slippage_bps = 1.0,
    const Json* context = nullptr);

int command_formulas_analyze(const std::vector<std::string>& args);
int command_formulas_context_template(const std::vector<std::string>& args);
int command_formulas_context_import(const std::vector<std::string>& args);
int command_formulas_audit(const std::vector<std::string>& args);
int command_formulas_evaluate(const std::vector<std::string>& args);
int command_formulas_scan(const std::vector<std::string>& args);
int command_formulas_watch(const std::vector<std::string>& args);
int command_formulas_backtest(const std::vector<std::string>& args);

}  // namespace tdx
