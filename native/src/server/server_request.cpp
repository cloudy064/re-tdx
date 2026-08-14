#include "server_core_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_identity.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::server_detail {
void attach_security_metadata(const BlockData& block_data, Json& document,
                              const std::string& market,
                              const std::string& code) {
    (void)enrich_security_metadata(
        block_data.securities, document, market, code);
}

void attach_security_metadata(const ApiState& state, Json& document,
                              const std::string& market,
                              const std::string& code) {
    attach_security_metadata(state.block_data, document, market, code);
}

FormulaHttpState formula_http_state(const ApiState& state) {
    return {state.root, state.block_data, state.formulas, state.jsn_root};
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string url_decode(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') result.push_back(' ');
        else if (value[index] == '%' && index + 2 < value.size()) {
            const int high = hex(value[index + 1]);
            const int low = hex(value[index + 2]);
            if (high < 0 || low < 0) throw Error("invalid URL percent encoding");
            result.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        } else result.push_back(value[index]);
    }
    return result;
}

RequestTarget parse_target(std::string_view raw) {
    RequestTarget result;
    const auto question = raw.find('?');
    result.path = url_decode(raw.substr(0, question));
    if (question == std::string_view::npos) return result;
    const auto query = raw.substr(question + 1);
    std::size_t start = 0;
    while (start <= query.size()) {
        const auto end = query.find('&', start);
        const auto item = query.substr(start, end == std::string_view::npos ? query.size() - start : end - start);
        if (!item.empty()) {
            const auto equals = item.find('=');
            const auto key = url_decode(item.substr(0, equals));
            const auto value = equals == std::string_view::npos ? "" : url_decode(item.substr(equals + 1));
            result.query[key] = value;
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

std::string query_value(const RequestTarget& target, const std::string& key,
                        std::string fallback) {
    const auto found = target.query.find(key);
    return found == target.query.end() ? std::move(fallback) : found->second;
}

bool query_bool(const RequestTarget& target, const std::string& key,
                bool fallback) {
    auto value = lower_ascii(trim(query_value(
        target, key, fallback ? "true" : "false")));
    if (value == "1" || value == "true" || value == "yes") return true;
    if (value == "0" || value == "false" || value == "no") return false;
    throw Error(key + " must be true/false or 1/0");
}

const Json* json_member(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string json_body_string(const Json& body, std::string_view key,
                             std::string fallback) {
    const auto* value = json_member(body, key);
    if (!value) return fallback;
    if (!value->is_string()) throw Error(std::string(key) + " must be a string");
    return value->as_string();
}

std::string formula_upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A')
                                     : static_cast<char>(ch);
    });
    return value;
}

void merge_formula_request_target(RequestTarget& target, const Json& body) {
    for (const auto key : {"market", "code", "codes", "kind", "period", "date",
                           "formula", "option_name", "expiry", "risk_free"}) {
        const auto* value = json_member(body, key);
        if (!value) continue;
        if (!value->is_string()) throw Error(std::string(key) + " must be a string");
        target.query[key] = value->as_string();
    }
    for (const auto key : {"pages", "page_size", "start", "timeout_ms", "lookback",
                           "workers", "adjust_cache_ttl_seconds"}) {
        const auto* value = json_member(body, key);
        if (!value) continue;
        if (!value->is_number() || std::floor(value->as_number()) != value->as_number())
            throw Error(std::string(key) + " must be an integer");
        target.query[key] = std::to_string(static_cast<std::int64_t>(value->as_number()));
    }
    for (const auto key : {"initial_capital", "commission_bps", "slippage_bps"}) {
        const auto* value = json_member(body, key);
        if (!value) continue;
        if (!value->is_number() || !std::isfinite(value->as_number()))
            throw Error(std::string(key) + " must be a finite number");
        std::ostringstream rendered;
        rendered.precision(17);
        rendered << value->as_number();
        target.query[key] = rendered.str();
    }
    for (const auto key : {"allow_future", "point_in_time_finance",
                           "adjust_refresh"}) {
        const auto* value = json_member(body, key);
        if (!value) continue;
        if (!value->is_bool()) throw Error(std::string(key) + " must be a boolean");
        target.query[key] = value->as_bool() ? "1" : "0";
    }
}

void merge_formula_adjustment_target(RequestTarget& target,
                                     const Json& body) {
    for (const auto key : {"adjust", "anchor_date"}) {
        const auto* value = json_member(body, key);
        if (!value) continue;
        if (!value->is_string()) throw Error(std::string(key) + " must be a string");
        target.query[key] = value->as_string();
    }
}

int parse_bounded(const std::string& text, std::string_view name,
                  int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

Json block_json(const Block& item) {
    Json value = Json::object();
    value["block_id"] = item.block_id;
    value["family"] = item.family;
    value["family_name"] = item.family_name;
    value["block_code"] = item.block_code;
    value["name"] = item.name;
    value["source_key"] = item.source_key;
    value["parent_block_id"] = item.parent_block_id;
    value["level"] = item.level;
    value["is_leaf"] = item.is_leaf;
    value["member_count"] = item.member_count;
    return value;
}

Json member_json(const BlockMember& item) {
    Json value = Json::object();
    value["block_id"] = item.block_id;
    value["block_code"] = item.block_code;
    value["block_name"] = item.block_name;
    value["family"] = item.family;
    value["security_id"] = item.security_id;
    value["market_id"] = item.market_id;
    value["market"] = item.market;
    value["code"] = item.code;
    value["security_name"] = item.security_name;
    value["membership"] = item.membership;
    return value;
}

Json health_document(const ApiState& state) {
    Json result = Json::object();
    result["ok"] = true;
    result["service"] = "tdx-tool";
    result["version"] = "0.2.0";
    result["api_version"] = "v1";
    result["native_cpp"] = true;
    result["python_runtime"] = false;
    result["tdx_root"] = path_utf8(state.root);
    result["serving_executable"] = state.serving_executable.empty()
        ? "" : path_utf8(state.serving_executable);
    result["serving_executable_sha256"] = state.serving_executable_sha256;
    result["web_root"] = path_utf8(state.web_root);
    result["web_index_sha256"] = state.web_index_sha256;
    result["securities"] = static_cast<std::uint64_t>(state.block_data.securities.size());
    result["blocks"] = static_cast<std::uint64_t>(state.block_data.blocks.size());
    result["memberships"] = static_cast<std::uint64_t>(state.block_data.members.size());
    result["formulas"] = static_cast<std::uint64_t>(state.formulas.at("formulas").size());
    result["formula_library_origin"] =
        state.formulas.at("runtime_library_origin");
    result["formula_runtime_dll_accessed"] =
        state.formulas.at("runtime_dll_accessed");
    result["formula_icon_cells"] = state.formula_icons.manifest.is_object()
        ? state.formula_icons.manifest.at("cell_count") : Json(0);
    result["formula_icon_runtime_dll_accessed"] =
        state.formula_icons.manifest.at("runtime_dll_accessed");
    result["jsn_available"] = !state.jsn_root.empty();
    result["jsn_root"] = state.jsn_root.empty() ? "" : path_utf8(state.jsn_root);
    result["jsn_resources"] = state.jsn_index
        ? static_cast<std::uint64_t>(state.jsn_index->resource_count()) : 0;
    result["jsn_security_keys"] = state.jsn_index
        ? static_cast<std::uint64_t>(state.jsn_index->security_key_count()) : 0;
    return result;
}

std::string normalize_market(std::string market);

std::pair<std::string, std::string> query_security(const RequestTarget& target) {
    auto market_value = trim(query_value(target, "market"));
    auto code = trim(query_value(target, "code"));
    // 兼容旧页面和外部调用常见的 SZ000001 / sz:000001 写法。市场参数
    // 省略时按 A 股代码规则推断，避免旧书签把“缺 market”变成整个页签失败。
    const auto folded_code = lower_ascii(code);
    if (folded_code.size() == 8 &&
        (folded_code.rfind("sz", 0) == 0 || folded_code.rfind("sh", 0) == 0 ||
         folded_code.rfind("bj", 0) == 0)) {
        if (market_value.empty()) market_value = folded_code.substr(0, 2);
        code = folded_code.substr(2);
    } else if (folded_code.size() == 9 && folded_code[2] == ':' &&
               (folded_code.rfind("sz", 0) == 0 || folded_code.rfind("sh", 0) == 0 ||
                folded_code.rfind("bj", 0) == 0)) {
        if (market_value.empty()) market_value = folded_code.substr(0, 2);
        code = folded_code.substr(3);
    }
    if (market_value.empty() && code.size() == 6 &&
        std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
        market_value = code.rfind("92", 0) == 0 || code[0] == '8' ? "bj"
                     : code[0] == '6' || code[0] == '9' ? "sh" : "sz";
    }
    const auto market = lower_ascii(normalize_market(std::move(market_value)));
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("code must contain exactly six digits");
    return {market, code};
}

std::pair<std::string, std::string> query_kline_security(const RequestTarget& target) {
    auto market = lower_ascii(trim(query_value(target, "market")));
    auto code = trim(query_value(target, "code"));
    if (market.empty()) {
        const auto colon = code.find(':');
        if (colon != std::string::npos) {
            market = lower_ascii(trim(code.substr(0, colon)));
            code = trim(code.substr(colon + 1));
        }
    }
    if (market.empty() && is_tdx_block_index_code(code))
        return {"sh", code};
    if (market.empty() || market == "sz" || market == "sh" || market == "bj" ||
        market == "0" || market == "1" || market == "2") {
        RequestTarget standard = target;
        standard.query["market"] = market;
        standard.query["code"] = code;
        return query_security(standard);
    }

    static const std::map<std::string, std::string> aliases{
        {"qz", "28"}, {"qd", "29"}, {"qs", "30"}, {"cz", "47"}, {"qg", "66"}
    };
    if (const auto found = aliases.find(market); found != aliases.end()) market = found->second;
    try {
        std::size_t used = 0;
        const auto market_id = std::stoul(market, &used);
        if (used != market.size() || market_id < 3 || market_id > 255)
            throw std::invalid_argument("range");
    } catch (...) {
        throw Error("expansion market must be qz/qd/qs/cz/qg or numeric ID 3..255");
    }
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (!valid_expansion_code(code))
        throw Error("expansion-market code must contain 1..9 ASCII letters, digits, or internal spaces");
    return {market, code};
}

}  // namespace tdx::server_detail
