#include "formula_scan_internal.hpp"

namespace tdx {
using namespace formula_scan_detail;

Json make_formula_scan_watch_state(const Json& scan,
                                   const std::string& configuration_id,
                                   std::uint64_t iteration) {
    const auto* matches = optional(scan, "matches");
    if (!matches || !matches->is_array())
        throw Error("formula scan watch state requires matches");
    Json state = Json::object();
    state["schema_version"] = 1;
    state["schema"] = "tdx-formula-watch-state-v1";
    state["configuration_id"] = configuration_id;
    state["iteration"] = iteration;
    for (const auto* key : {"formula", "kind", "period", "lookback",
                            "adjustment_mode", "adjustment_summary",
                            "formula_source_mode", "formula_source_md5"})
        if (const auto* value = optional(scan, key)) state[key] = *value;
    state["active_count"] = static_cast<std::uint64_t>(matches->size());
    state["active_matches"] = *matches;
    return state;
}

std::string formula_scan_block_text(const Json& scan) {
    const Json* matches = optional(scan, "matches");
    if (!matches) matches = optional(scan, "active_matches");
    if (!matches || !matches->is_array())
        throw Error("formula scan block export requires matches");
    std::set<std::string> lines;
    for (const auto& row : matches->as_array()) {
        const auto* market = optional(row, "market");
        const auto* code = optional(row, "code");
        if (!market || !market->is_string() || !code || !code->is_string())
            throw Error("formula scan block export requires market/code");
        const auto normalized_market = lower_ascii(trim(market->as_string()));
        const auto normalized_code = trim(code->as_string());
        if (normalized_code.size() != 6 ||
            !std::all_of(normalized_code.begin(), normalized_code.end(),
                         [](char ch) { return ch >= '0' && ch <= '9'; }))
            throw Error("formula scan block export requires a six-digit code");
        const char prefix = normalized_market == "sz" ? '0' :
                            normalized_market == "sh" ? '1' :
                            normalized_market == "bj" ? '2' : '\0';
        if (!prefix) throw Error("formula scan block export supports sz/sh/bj only");
        lines.insert(std::string(1, prefix) + normalized_code);
    }
    std::string result;
    for (const auto& line : lines) result += line + "\r\n";
    return result;
}

}  // namespace tdx
