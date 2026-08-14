#include "tpool_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_calc.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formulas.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::tpool_detail {

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string xml_unescape(std::string value) {
    for (const auto& [encoded, decoded] : std::vector<std::pair<std::string, std::string>>{
             {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"},
             {"&gt;", ">"}, {"&amp;", "&"}}) {
        std::size_t position = 0;
        while ((position = value.find(encoded, position)) != std::string::npos) {
            value.replace(position, encoded.size(), decoded);
            position += decoded.size();
        }
    }
    return value;
}

Attributes parse_attributes(std::string_view tag) {
    Attributes result;
    std::size_t position = 1;
    while (position < tag.size() && !std::isspace(static_cast<unsigned char>(tag[position])) &&
           tag[position] != '>' && tag[position] != '/') ++position;
    while (position < tag.size()) {
        while (position < tag.size() && std::isspace(static_cast<unsigned char>(tag[position])))
            ++position;
        if (position >= tag.size() || tag[position] == '>' || tag[position] == '/') break;
        const auto name_begin = position;
        while (position < tag.size() && (std::isalnum(static_cast<unsigned char>(tag[position])) ||
               tag[position] == '_' || tag[position] == '-')) ++position;
        if (position == name_begin) { ++position; continue; }
        auto name = lower_ascii(std::string(tag.substr(name_begin, position - name_begin)));
        while (position < tag.size() && std::isspace(static_cast<unsigned char>(tag[position])))
            ++position;
        if (position >= tag.size() || tag[position] != '=') {
            result.emplace(std::move(name), "");
            continue;
        }
        ++position;
        while (position < tag.size() && std::isspace(static_cast<unsigned char>(tag[position])))
            ++position;
        if (position >= tag.size() || (tag[position] != '\'' && tag[position] != '\"')) {
            result.emplace(std::move(name), "");
            continue;
        }
        const char quote = tag[position++];
        const auto value_begin = position;
        while (position < tag.size() && tag[position] != quote) ++position;
        result.emplace(std::move(name), xml_unescape(
            std::string(tag.substr(value_begin, position - value_begin))));
        if (position < tag.size()) ++position;
    }
    return result;
}

std::vector<Attributes> scan_elements(const std::string& xml, std::string_view element) {
    std::vector<Attributes> result;
    const auto needle = "<" + std::string(element);
    std::size_t position = 0;
    while ((position = xml.find(needle, position)) != std::string::npos) {
        const auto after = position + needle.size();
        if (after < xml.size() && !std::isspace(static_cast<unsigned char>(xml[after])) &&
            xml[after] != '>' && xml[after] != '/') {
            position = after;
            continue;
        }
        bool quoted = false;
        char quote = 0;
        std::size_t end = after;
        for (; end < xml.size(); ++end) {
            const char ch = xml[end];
            if (quoted && ch == quote) quoted = false;
            else if (!quoted && (ch == '\'' || ch == '\"')) { quoted = true; quote = ch; }
            else if (!quoted && ch == '>') break;
        }
        if (end >= xml.size()) throw Error("unterminated <" + std::string(element) + "> tag");
        result.push_back(parse_attributes(std::string_view(xml).substr(position, end - position + 1)));
        position = end + 1;
    }
    return result;
}

struct CellBlock {
    Attributes attributes;
    std::vector<Attributes> functions;
    std::vector<Attributes> stocks;
    std::vector<Attributes> action_policies;
};

std::vector<CellBlock> scan_cell_blocks(const std::string& xml) {
    std::vector<CellBlock> result;
    std::size_t position = 0;
    while ((position = xml.find("<cell", position)) != std::string::npos) {
        const auto boundary = position + 5;
        if (boundary < xml.size() && !std::isspace(static_cast<unsigned char>(xml[boundary])) &&
            xml[boundary] != '>' && xml[boundary] != '/') {
            position = boundary;
            continue;
        }
        bool quoted = false;
        char quote = 0;
        std::size_t start_end = boundary;
        for (; start_end < xml.size(); ++start_end) {
            const char ch = xml[start_end];
            if (quoted && ch == quote) quoted = false;
            else if (!quoted && (ch == '\'' || ch == '\"')) { quoted = true; quote = ch; }
            else if (!quoted && ch == '>') break;
        }
        if (start_end >= xml.size()) throw Error("unterminated <cell> tag");
        const auto closing = xml.find("</cell>", start_end + 1);
        if (closing == std::string::npos) throw Error("<cell> is missing </cell>");
        CellBlock block;
        block.attributes = parse_attributes(
            std::string_view(xml).substr(position, start_end - position + 1));
        const auto body = xml.substr(start_end + 1, closing - start_end - 1);
        block.functions = scan_elements(body, "func");
        block.stocks = scan_elements(body, "stk");
        block.action_policies = scan_elements(body, "psatt");
        result.push_back(std::move(block));
        position = closing + 7;
    }
    return result;
}

Json attributes_json(const Attributes& attributes) {
    Json result = Json::object();
    for (const auto& [name, value] : attributes) result[name] = value;
    return result;
}

std::string attribute(const Attributes& attributes, std::string_view key) {
    const auto found = attributes.find(key);
    return found == attributes.end() ? "" : found->second;
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string market_from_setcode(const std::string& value) {
    if (value == "0") return "sz";
    if (value == "1") return "sh";
    if (value == "2") return "bj";
    return "";
}

bool supported_formula(const std::string& code) {
    static const std::set<std::string, std::less<>> supported{
        "MA", "MACD", "KDJ", "RSI", "BOLL", "CCI", "WR", "BIAS",
        "DMA", "MTM", "ROC", "TRIX", "ATR", "VOL", "OBV", "PSY",
        "VR", "BRAR", "BBI", "EXPMA", "DMI", "WVAD", "EMV", "CHO",
        "ADTM", "DKX"};
    return supported.count(upper_ascii(trim(code))) != 0;
}

bool supported_operation(int operation);
std::string json_text(const Json& object, std::string_view key);
int json_integer_text(const Json& object, std::string_view key, int fallback);

const Json* library_formula(const Json* library, const std::string& code) {
    if (!library || !library->is_object()) return nullptr;
    const auto formulas = library->as_object().find("formulas");
    if (formulas == library->as_object().end() || !formulas->second.is_array()) return nullptr;
    const auto wanted = upper_ascii(trim(code)); const Json* fallback = nullptr;
    for (const auto& formula : formulas->second.as_array()) {
        const auto found = formula.as_object().find("code");
        if (found == formula.as_object().end() || !found->second.is_string() ||
            upper_ascii(trim(found->second.as_string())) != wanted) continue;
        const auto kind = formula.as_object().find("kind_key");
        if (kind != formula.as_object().end() && kind->second.is_string() &&
            kind->second.as_string() == "technical") return &formula;
        if (!fallback) fallback = &formula;
    }
    return fallback;
}

std::vector<std::string> formula_parameter_names(const Json& formula) {
    std::vector<std::string> result;
    const auto found = formula.as_object().find("parameters");
    if (found == formula.as_object().end() || !found->second.is_array()) return result;
    for (const auto& parameter : found->second.as_array()) {
        const auto name = parameter.as_object().find("name");
        if (name != parameter.as_object().end() && name->second.is_string())
            result.push_back(name->second.as_string());
    }
    return result;
}

Json formula_analysis(const Json& formula) {
    const auto source = formula.as_object().find("source_text");
    return analyze_formula_source(source != formula.as_object().end() && source->second.is_string()
                                      ? source->second.as_string() : "",
                                  formula_parameter_names(formula));
}

void upgrade_tpool_formula_compatibility(Json& inspection, const Json* library) {
    std::size_t supported = 0, builtin_supported = 0, ready = 0;
    for (auto& function : inspection["functions"].as_array()) {
        const int set = json_integer_text(function, "nset", 0);
        const auto source_kind = tpool_rule_source_kind(set);
        if (source_kind == RuleSourceKind::latest_finance ||
            source_kind == RuleSourceKind::realtime_quote) {
            const auto plan = annotate_tpool_builtin_rule(function);
            function["execution_ready"] = plan.execution_ready;
            function["blocking_reason"] = plan.blocking_reason;
            if (plan.field_recognized) ++builtin_supported;
            if (plan.execution_ready) ++ready;
            continue;
        }
        function["rule_kind"] = tpool_rule_source_name(source_kind);
        if (source_kind != RuleSourceKind::formula) {
            function["formula_supported"] = false;
            function["execution_ready"] = false;
            function["calculation_engine"] = "unavailable";
            function["blocking_reason"] = "unknown TPool nset rule source";
            continue;
        }
        const auto code = json_text(function, "formula_code");
        const auto* formula = library_formula(library, code);
        const auto operation = json_integer_text(function, "noperate", -1);
        const auto period = json_text(function, "period");
        const auto history = annotate_tpool_rule_history(function);
        bool compatible = false; Json analysis;
        if (library && formula) {
            analysis = formula_analysis(*formula);
            compatible = analysis.at("executable_with_context").as_bool();
        } else if (!library) {
            compatible = json_bool(function, "formula_supported");
        }
        const bool executable = compatible && tpool_supported_operation(set, operation) &&
                                !period.empty() && history.supported;
        function["formula_supported"] = compatible;
        function["execution_ready"] = executable;
        function["calculation_engine"] = library && compatible
            ? "tdx-source-interpreter-v1"
            : (compatible ? "tdx-native-compatible-v4" : "unavailable");
        if (formula) function["formula_analysis"] = std::move(analysis);
        if (compatible) ++supported;
        if (executable) { ++ready; function["blocking_reason"] = ""; }
        else if (!compatible) function["blocking_reason"] = formula
            ? "formula requires unsupported, future, or unavailable context data"
            : "formula is absent from the recovered formula library";
        else if (period.empty()) function["blocking_reason"] = "unsupported or unknown period";
        else if (!history.supported) function["blocking_reason"] = history.blocking_reason;
        else function["blocking_reason"] = "unsupported or unknown comparison operation";
    }
    inspection["native_formula_reference_count"] = static_cast<std::uint64_t>(supported);
    inspection["native_builtin_reference_count"] =
        static_cast<std::uint64_t>(builtin_supported);
    inspection["execution_ready_function_count"] = static_cast<std::uint64_t>(ready);
    inspection["formula_library_attached"] = library != nullptr;
    inspection["compatibility"]["formula_values"] =
        "recovered TCalc source interpreter with verified readonly market context";
    inspection["compatibility"]["builtin_values"] =
        "recovered TPool latest-finance and realtime-quote field interpreter";
    inspection["compatibility"]["safe_to_evaluate_individual_rules"] = ready > 0;
}

std::string operator_name(int operation) {
    switch (operation) {
        case 0: return "equal";
        case 1: return "greater-than";
        case 2: return "less-than";
        case 3: return "cross-above";
        case 4: return "cross-below";
        case 5: return "exact-rank";
        case 6: return "top-or-bottom-n";
        case 7: return "rank-tail";
        case 8: return "local-trough";
        case 9: return "local-peak";
        default: return "unknown";
    }
}

bool supported_operation(int operation) {
    return operation >= 0 && operation <= 9;
}

bool ranking_operation(int operation) {
    return operation >= 5 && operation <= 7;
}

int integer_text(const std::string& value, int fallback) {
    if (value.empty()) return fallback;
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed);
        return consumed == value.size() ? result : fallback;
    } catch (...) { return fallback; }
}

double float_text(const std::string& value, double fallback) {
    if (value.empty()) return fallback;
    try {
        std::size_t consumed = 0;
        const double result = std::stod(value, &consumed);
        return consumed == value.size() ? result : fallback;
    } catch (...) { return fallback; }
}

std::string time_unit_name(int unit) {
    switch (unit) {
        case 0: return "seconds";
        case 1: return "minutes";
        case 2: return "hours";
        default: return "unknown";
    }
}

double scaled_seconds(int value, int unit) {
    if (unit == 0) return value;
    if (unit == 1) return static_cast<double>(value) * 60.0;
    if (unit == 2) return static_cast<double>(value) * 3600.0;
    return std::numeric_limits<double>::quiet_NaN();
}

std::string start_type_name(int type) {
    switch (type) {
        case 0: return "immediate";
        case 1: return "pool-tick-threshold";
        case 2: return "before-market-open";
        case 3: return "after-market-open";
        case 4: return "before-market-close";
        case 5: return "after-market-close";
        case 6: return "scheduled-weekdays";
        case 7: return "scheduled-daily";
        default: return "unknown";
    }
}

std::string cycle_type_name(int type) {
    switch (type) {
        case 0: return "repeat";
        case 1: return "repeat-within-window";
        case 2: return "one-shot";
        default: return "unknown";
    }
}

std::string hhmmss_text(int raw) {
    if (raw < 0 || raw > 235959) return "";
    const int hour = raw / 10000;
    const int minute = raw / 100 % 100;
    const int second = raw % 100;
    if (hour > 23 || minute > 59 || second > 59) return "";
    const auto two = [](int value) {
        return std::string(value < 10 ? "0" : "") + std::to_string(value);
    };
    return two(hour) + ":" + two(minute) + ":" + two(second);
}

Json annotated_flow_json(const Attributes& attributes) {
    Json row = attributes_json(attributes);
    const int start_type = integer_text(attribute(attributes, "starttype"), 0);
    const int start_value = integer_text(attribute(attributes, "starttime"), 0);
    const int start_unit = integer_text(attribute(attributes, "starttimetype"), 0);
    const int cycle_type = integer_text(attribute(attributes, "cxtype"), 0);
    const int cycle_value = integer_text(attribute(attributes, "cxtime"), 0);
    const int cycle_unit = integer_text(attribute(attributes, "cxtimetype"), 0);
    row["start_type_id"] = start_type;
    row["start_type_name"] = start_type_name(start_type);
    row["start_time_unit"] = time_unit_name(start_unit);
    const auto start_seconds = scaled_seconds(start_value, start_unit);
    row["start_offset_seconds"] = std::isfinite(start_seconds) ? Json(start_seconds) : Json(nullptr);
    row["scheduled_clock"] = hhmmss_text(integer_text(attribute(attributes, "starttimehms"), -1));
    row["cycle_type_id"] = cycle_type;
    row["cycle_type_name"] = cycle_type_name(cycle_type);
    row["cycle_window_unit"] = time_unit_name(cycle_unit);
    const auto cycle_seconds = scaled_seconds(cycle_value, cycle_unit);
    row["cycle_window_seconds"] = std::isfinite(cycle_seconds) ? Json(cycle_seconds) : Json(nullptr);
    row["interval_value"] = integer_text(attribute(attributes, "jgtime"), 0);
    row["interval_seconds"] = std::max(1, integer_text(attribute(attributes, "jgtime"), 0));
    row["transfer_enabled"] = integer_text(attribute(attributes, "tran"), 0) != 0;
    row["empty_previous_set"] = integer_text(attribute(attributes, "emptyps"), 0) != 0;
    row["timing_mapping_verified"] = start_type >= 0 && start_type <= 7 &&
                                      cycle_type >= 0 && cycle_type <= 2;
    row["runtime_field_evidence"] =
        "TPool.dll sub_1001E610 and one-second TimerFunc: offsets 20..38, runtime state 42..54";
    return row;
}

Json flow_graph_document(const std::vector<Attributes>& flows,
                         const std::vector<Attributes>& cells) {
    std::set<std::string, std::less<>> nodes;
    std::set<std::string, std::less<>> duplicates;
    for (const auto& cell : cells) {
        const auto id = attribute(cell, "id");
        if (id.empty()) continue;
        if (!nodes.insert(id).second) duplicates.insert(id);
    }
    std::map<std::string, int, std::less<>> indegree;
    std::map<std::string, std::vector<std::string>, std::less<>> adjacency;
    for (const auto& node : nodes) { indegree[node] = 0; adjacency[node] = {}; }
    Json dangling = Json::array();
    std::size_t enabled_edges = 0;
    for (std::size_t index = 0; index < flows.size(); ++index) {
        const auto start = attribute(flows[index], "startid");
        const auto end = attribute(flows[index], "endid");
        const bool endpoints_valid = nodes.count(start) && nodes.count(end);
        if (!endpoints_valid) {
            Json item = Json::object();
            item["flow_index"] = static_cast<std::uint64_t>(index);
            item["startid"] = start;
            item["endid"] = end;
            item["missing_start"] = !nodes.count(start);
            item["missing_end"] = !nodes.count(end);
            dangling.push_back(std::move(item));
        }
        if (integer_text(attribute(flows[index], "tran"), 0) == 0 || !endpoints_valid)
            continue;
        ++enabled_edges;
        adjacency[start].push_back(end);
        ++indegree[end];
    }
    Json roots = Json::array();
    Json sinks = Json::array();
    std::vector<std::string> queue;
    auto remaining = indegree;
    for (const auto& node : nodes) {
        if (indegree[node] == 0) { roots.push_back(node); queue.push_back(node); }
        if (adjacency[node].empty()) sinks.push_back(node);
    }
    std::size_t cursor = 0;
    std::size_t visited = 0;
    while (cursor < queue.size()) {
        const auto node = queue[cursor++];
        ++visited;
        for (const auto& next : adjacency[node]) if (--remaining[next] == 0) queue.push_back(next);
    }
    Json duplicate_rows = Json::array();
    for (const auto& id : duplicates) duplicate_rows.push_back(id);
    Json graph = Json::object();
    graph["node_count"] = static_cast<std::uint64_t>(nodes.size());
    graph["edge_count"] = static_cast<std::uint64_t>(flows.size());
    graph["enabled_edge_count"] = static_cast<std::uint64_t>(enabled_edges);
    graph["roots"] = std::move(roots);
    graph["sinks"] = std::move(sinks);
    graph["duplicate_cell_ids"] = std::move(duplicate_rows);
    graph["dangling_edges"] = std::move(dangling);
    graph["acyclic"] = visited == nodes.size();
    graph["references_valid"] = duplicates.empty() && graph.at("dangling_edges").size() == 0;
    graph["stateful_execution_supported"] = false;
    graph["native_state_machine_supported"] =
        duplicates.empty() && graph.at("dangling_edges").size() == 0;
    graph["native_state_machine_supports_cycles"] = true;
    graph["evidence"] = "TPool.dll flow parser and runtime worker; static interpretation only";
    return graph;
}

std::vector<fs::path> pool_directories(const fs::path& root) {
    return {root / "tpool", root / "T0002" / "tpool", root / "userdata" / "tpool"};
}

std::string decode_xml_file(const fs::path& path) {
    auto bytes = read_bytes(path);
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        return std::string(bytes.begin() + 3, bytes.end());
    const std::string raw(bytes.begin(), bytes.end());
    auto declaration = lower_ascii(raw.substr(0, std::min<std::size_t>(raw.size(), 256)));
    if (declaration.find("utf-8") != std::string::npos ||
        declaration.find("utf8") != std::string::npos) return raw;
    return decode_gbk(bytes);
}

}  // namespace tdx::tpool_detail

namespace tdx {

using namespace tpool_detail;

Json parse_tpool_xml_document(const std::string& xml, const std::string& source_name) {
    if (xml.size() > 64ULL * 1024ULL * 1024ULL) throw Error("TPool XML exceeds 64 MiB safety limit");
    const auto flows = scan_elements(xml, "flow");
    const auto cell_blocks = scan_cell_blocks(xml);
    std::vector<Attributes> cells;
    std::vector<std::pair<Attributes, std::string>> functions;
    std::vector<std::pair<Attributes, std::string>> stocks;
    std::vector<std::pair<Attributes, std::string>> action_policies;
    for (const auto& block : cell_blocks) {
        cells.push_back(block.attributes);
        const auto cell_id = attribute(block.attributes, "id");
        for (const auto& item : block.functions) functions.emplace_back(item, cell_id);
        for (const auto& item : block.stocks) stocks.emplace_back(item, cell_id);
        for (const auto& item : block.action_policies)
            action_policies.emplace_back(item, cell_id);
    }
    if (cell_blocks.empty()) {
        cells = scan_elements(xml, "cell");
        for (const auto& item : scan_elements(xml, "func")) functions.emplace_back(item, "");
        for (const auto& item : scan_elements(xml, "stk")) stocks.emplace_back(item, "");
        for (const auto& item : scan_elements(xml, "psatt"))
            action_policies.emplace_back(item, "");
    }

    Json flow_rows = Json::array();
    for (const auto& item : flows) flow_rows.push_back(annotated_flow_json(item));

    std::size_t supported = 0, builtin_supported = 0;
    std::set<std::string, std::less<>> referenced_formulas;
    Json function_rows = Json::array();
    for (const auto& scoped : functions) {
        const auto& item = scoped.first;
        Json row = attributes_json(item);
        row["cell_id"] = scoped.second;
        const int set = integer_text(attribute(item, "nset"), 0);
        const auto source_kind = tpool_rule_source_kind(set);
        const auto formula = upper_ascii(trim(attribute(item, "accode")));
        if (source_kind == RuleSourceKind::formula && !formula.empty())
            referenced_formulas.insert(formula);
        const bool formula_supported = supported_formula(formula);
        const int operation = integer_text(attribute(item, "noperate"), -1);
        const auto period = period_name(integer_text(attribute(item, "nperiod"), -1));
        row["formula_code"] = formula;
        row["exclude_st"] = integer_text(attribute(item, "bnost"), 0) != 0;
        row["exclude_suspended"] = integer_text(attribute(item, "bnotp"), 0) != 0;
        row["exclude_delisted"] = integer_text(attribute(item, "bnotq"), 0) != 0;
        row["filter_mapping"] = "bnost=ST, bnotp=suspension, bnotq=delisting";
        row["filter_mapping_source"] = "TPool.dll field analysis; runtime checks use directory/name and latest daily bar heuristics";
        if (source_kind == RuleSourceKind::latest_finance ||
            source_kind == RuleSourceKind::realtime_quote) {
            const auto plan = annotate_tpool_builtin_rule(row);
            row["execution_ready"] = plan.execution_ready;
            row["blocking_reason"] = plan.blocking_reason;
            if (plan.field_recognized) ++builtin_supported;
        } else if (source_kind == RuleSourceKind::formula) {
            const auto history = annotate_tpool_rule_history(row);
            const bool execution_ready = formula_supported &&
                tpool_supported_operation(set, operation) &&
                !period.empty() && history.supported;
            if (formula_supported) ++supported;
            row["rule_kind"] = tpool_rule_source_name(source_kind);
            row["formula_supported"] = formula_supported;
            row["operator"] = tpool_operator_name(set, operation);
            row["operator_mapping_verified"] =
                tpool_supported_operation(set, operation);
            row["period"] = period;
            row["calculation_engine"] = formula_supported
                ? "tdx-native-compatible-v4" : "unavailable";
            row["execution_ready"] = execution_ready;
            if (execution_ready) row["blocking_reason"] = "";
            else if (!formula_supported) row["blocking_reason"] = "formula is outside the native calculation subset";
            else if (period.empty()) row["blocking_reason"] = "unsupported or unknown period";
            else if (!history.supported) row["blocking_reason"] = history.blocking_reason;
            else row["blocking_reason"] = "unsupported or unknown comparison operation";
        } else {
            row["rule_kind"] = "unknown";
            row["formula_supported"] = false;
            row["operator"] = "unknown";
            row["operator_mapping_verified"] = false;
            row["period"] = "";
            row["calculation_engine"] = "unavailable";
            row["execution_ready"] = false;
            row["blocking_reason"] = "unknown TPool nset rule source";
        }
        function_rows.push_back(std::move(row));
    }

    std::set<std::string, std::less<>> security_ids;
    Json stock_rows = Json::array();
    for (const auto& scoped : stocks) {
        const auto& item = scoped.first;
        Json row = attributes_json(item);
        row["cell_id"] = scoped.second;
        const auto market = market_from_setcode(attribute(item, "setcode"));
        const auto code = attribute(item, "code");
        row["market"] = market;
        row["security_id"] = market.empty() ? code : market + code;
        if (!code.empty()) security_ids.insert((market.empty() ? "?:" : market) + code);
        stock_rows.push_back(std::move(row));
    }

    Json formula_codes = Json::array();
    for (const auto& item : referenced_formulas) formula_codes.push_back(item);
    Json action_policy_rows = Json::array();
    std::size_t configured_actions = 0;
    for (const auto& scoped : action_policies) {
        auto policy = tpool_action_policy_document(
            attributes_json(scoped.first), scoped.second);
        configured_actions += static_cast<std::size_t>(
            policy.at("configured_action_count").as_number());
        action_policy_rows.push_back(std::move(policy));
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-inspection-v1";
    result["source"] = source_name;
    result["read_only"] = true;
    result["dll_loaded"] = false;
    result["worker_started"] = false;
    result["flow_count"] = static_cast<std::uint64_t>(flows.size());
    result["function_count"] = static_cast<std::uint64_t>(functions.size());
    result["stock_count"] = static_cast<std::uint64_t>(stocks.size());
    result["security_count"] = static_cast<std::uint64_t>(security_ids.size());
    result["attribute_count"] = static_cast<std::uint64_t>(action_policies.size());
    result["action_policy_count"] = static_cast<std::uint64_t>(action_policies.size());
    result["configured_action_count"] = static_cast<std::uint64_t>(configured_actions);
    auto host_callback_contracts = tpool_host_callback_contracts_document();
    result["host_callback_count"] = host_callback_contracts.at("callback_count");
    result["host_callback_contracts"] = std::move(host_callback_contracts);
    result["cell_count"] = static_cast<std::uint64_t>(cells.size());
    result["referenced_formula_count"] = static_cast<std::uint64_t>(referenced_formulas.size());
    result["referenced_formulas"] = std::move(formula_codes);
    result["native_formula_reference_count"] = static_cast<std::uint64_t>(supported);
    result["native_builtin_reference_count"] =
        static_cast<std::uint64_t>(builtin_supported);
    const auto ready = std::count_if(function_rows.as_array().begin(), function_rows.as_array().end(),
        [](const Json& row) { return row.at("execution_ready").as_bool(); });
    result["execution_ready_function_count"] = static_cast<std::uint64_t>(ready);
    result["flows"] = std::move(flow_rows);
    result["functions"] = std::move(function_rows);
    result["stocks"] = std::move(stock_rows);
    result["action_policies"] = std::move(action_policy_rows);
    Json cell_rows = Json::array();
    for (const auto& cell : cells) {
        Json row = attributes_json(cell);
        const auto id = attribute(cell, "id");
        row["function_count"] = static_cast<std::uint64_t>(std::count_if(
            functions.begin(), functions.end(), [&](const auto& item) { return item.second == id; }));
        row["stock_count"] = static_cast<std::uint64_t>(std::count_if(
            stocks.begin(), stocks.end(), [&](const auto& item) { return item.second == id; }));
        const auto action_count = std::count_if(
            action_policies.begin(), action_policies.end(),
            [&](const auto& item) { return item.second == id; });
        row["action_policy_count"] = static_cast<std::uint64_t>(action_count);
        row["has_action_policy"] = action_count > 0;
        cell_rows.push_back(std::move(row));
    }
    result["cells"] = std::move(cell_rows);
    result["flow_graph"] = flow_graph_document(flows, cells);
    Json compatibility = Json::object();
    compatibility["formula_values"] = "partially available through tdx-native-compatible-v4";
    compatibility["builtin_values"] =
        "latest-finance nset=3 and realtime-quote nset=4 field catalogs are mapped";
    compatibility["operator_mapping"] = "verified for 0..7, 8 and 9 from TPool comparison helpers";
    compatibility["host_callbacks"] =
        "three registered callback contracts are described; none is invoked";
    compatibility["host_actions"] =
        "psatt policies are parsed and emitted as planned-only actions";
    compatibility["safe_to_inspect"] = true;
    compatibility["safe_to_evaluate_individual_rules"] = ready > 0;
    compatibility["safe_to_execute"] = false;
    compatibility["safe_to_advance_native_state"] =
        result.at("flow_graph").at("references_valid");
    compatibility["native_state_file"] =
        "optional standalone JSON; atomically written by pool watch --state-file";
    compatibility["execution_boundary"] =
        "compatible rules, read-only flow/state projection and recovered psatt action plans; original host callbacks, UI/file actions and TDX pool mutation are not executed";
    result["compatibility"] = std::move(compatibility);
    return result;
}

Json inspect_tpool_file_document(const fs::path& path) {
    if (!fs::is_regular_file(path)) throw Error("TPool XML does not exist: " + path_utf8(path));
    auto result = parse_tpool_xml_document(decode_xml_file(path), path_utf8(path));
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["sha256"] = lower_ascii(sha256_file(path));
    return result;
}

Json inspect_tpool_root_document(const fs::path& root) {
    Json directories = Json::array();
    Json pools = Json::array();
    for (const auto& directory : pool_directories(root)) {
        Json info = Json::object();
        info["path"] = path_utf8(directory);
        info["exists"] = fs::is_directory(directory);
        directories.push_back(std::move(info));
        if (!fs::is_directory(directory)) continue;
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!entry.is_regular_file() || lower_ascii(entry.path().extension().string()) != ".xml")
                continue;
            pools.push_back(inspect_tpool_file_document(entry.path()));
        }
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-catalog-v1";
    result["root"] = path_utf8(root);
    result["read_only"] = true;
    result["pool_count"] = static_cast<std::uint64_t>(pools.size());
    result["directories"] = std::move(directories);
    result["pools"] = std::move(pools);
    return result;
}

}  // namespace tdx
