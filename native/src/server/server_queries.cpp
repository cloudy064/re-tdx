#include "server_core_internal.hpp"
#include "server_market_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/blocks_quote.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/institution.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_identity.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx::server_detail {
Json query_cloud_workflow_api(const ApiState& state, const RequestTarget& target) {
    const auto name = trim(query_value(target, "name"));
    if (name.empty() || name.size() > 64) throw Error("workflow name is required");
    std::vector<std::string> selected;
    const auto selection = query_value(target, "select");
    std::size_t offset = 0;
    while (offset <= selection.size() && !selection.empty()) {
        const auto end = selection.find(',', offset);
        const auto value = trim(selection.substr(offset,
            end == std::string::npos ? std::string::npos : end - offset));
        if (!value.empty()) selected.push_back(value);
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    if (selected.size() > 10) throw Error("select accepts at most 10 keys");
    const int limit = parse_bounded(query_value(target, "limit", "1"),
                                    "limit", 1, 10);
    const int page_size = parse_bounded(query_value(target, "page_size", "50"),
                                        "page_size", 1, 500);
    const int max_pages = parse_bounded(query_value(target, "max_pages", "100"),
                                        "max_pages", 1, 100);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                      "timeout_ms", 100, 30000);
    return run_cloud_workflow(state.root, name,
        query_json_string_map(target, "set"), selected, limit,
        query_bool(target, "master_all_pages"), page_size, max_pages,
        cloud_endpoints::tqlex, timeout);
}

Json apply_requested_kline_adjustment(const fs::path& root,
                                      const RequestTarget& target,
                                      const std::string& market,
                                      const std::string& code,
                                      const std::string& kind,
                                      int timeout,
                                      Json document) {
    const int cache_ttl = parse_bounded(
        query_value(target, "adjust_cache_ttl_seconds", "900"),
        "adjust_cache_ttl_seconds", 0, 86400);
    return adjust_security_kline_document(
        std::move(document), market, code, kind,
        query_value(target, "adjust", "none"),
        query_value(target, "anchor_date"), root, {}, timeout,
        cache_ttl, query_bool(target, "adjust_refresh"));
}

Json apply_requested_kline_adjustment(const ApiState& state,
                                      const RequestTarget& target,
                                      const std::string& market,
                                      const std::string& code,
                                      const std::string& kind,
                                      int timeout,
                                      Json document) {
    return apply_requested_kline_adjustment(
        state.root, target, market, code, kind, timeout,
        std::move(document));
}

Json apply_requested_local_kline_adjustment(
    const ApiState& state, const RequestTarget& target,
    const std::string& market, const std::string& code,
    const std::string& kind, Json document) {
    const auto mode = normalize_kline_adjustment_mode(
        query_value(target, "adjust", "none"));
    if (mode == "none") {
        document["adjustment_mode"] = "none";
        return document;
    }
    const auto normalized_kind = lower_ascii(trim(kind));
    if (normalized_kind == "index" ||
        (normalized_kind == "auto" && market == "sh" &&
         is_tdx_block_index_code(code)))
        throw Error("corporate-action adjustment is only available for securities");
    const auto daily = load_local_kline_document(
        state.root, market, code, "stock", "day", 20, 800, 0, "all");
    const auto capital = load_local_capital_changes_document(
        state.root, {market + ":" + code});
    return apply_kline_adjustment(
        std::move(document), daily, capital, mode,
        query_value(target, "anchor_date"));
}

Json query_minute(const ApiState& state, const RequestTarget& target) {
    const auto block_query = trim(query_value(target, "block"));
    std::optional<BlockQuoteTarget> block_target;
    std::string market;
    std::string code;
    if (!block_query.empty()) {
        if (!trim(query_value(target, "market")).empty() ||
            !trim(query_value(target, "code")).empty())
            throw Error("block is mutually exclusive with market/code");
        block_target = resolve_block_quote_target(state.block_data, block_query);
        market = block_target->market;
        code = block_target->code;
    } else {
        std::tie(market, code) = query_kline_security(target);
    }
    const auto source = lower_ascii(trim(query_value(target, "source", "online")));
    auto kind = lower_ascii(trim(query_value(target, "kind", "auto")));
    if (block_target) {
        if (kind == "stock") throw Error("kind=stock is invalid with block");
        if (kind == "auto") kind = "index";
    }
    const auto period = lower_ascii(trim(query_value(target, "period", "time")));
    // 分时图保持单日语义；分钟 K 线默认返回已下载的完整历史页。
    const auto default_date = period == "time" || period == "timeline" ? "latest" : "all";
    const auto date = trim(query_value(target, "date", default_date));
    const int pages = parse_bounded(query_value(target, "pages", "1"), "pages", 1, 5);
    const int page_size = parse_bounded(query_value(target, "page_size", "800"),
                                        "page_size", 1, 800);
    const int start = parse_bounded(query_value(target, "start", "0"), "start", 0, 65535);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    if (source == "local") {
        if (market != "sz" && market != "sh" && market != "bj")
            throw Error("local K-line source is unavailable for expansion markets");
        auto document = load_local_kline_document(
            state.root, market, code, kind, period,
            pages, page_size, start, date);
        if (block_target) {
            document["block"] = block_json(block_target->block);
            document["name"] = block_target->block.name;
            document["name_source"] = "local-block-catalog";
            document["security_type"] = "block-index";
        } else {
            attach_security_metadata(state, document, market, code);
        }
        return apply_requested_local_kline_adjustment(
            state, target, market, code, kind, std::move(document));
    }
    if (source != "online") throw Error("source must be online or local");
    auto document = fetch_kline_document(market, code, kind, period, pages, page_size,
                                         start, date, timeout, state.root);
    if (block_target) {
        document["block"] = block_json(block_target->block);
        document["name"] = block_target->block.name;
        document["name_source"] = "local-block-catalog";
        document["security_type"] = "block-index";
    } else {
        attach_security_metadata(state, document, market, code);
    }
    return apply_requested_kline_adjustment(
        state, target, market, code, kind, timeout, std::move(document));
}

Json query_jsn_security(const ApiState& state, const RequestTarget& target) {
    if (state.jsn_root.empty()) throw Error("JSN data directory is unavailable; pass --jsn-root");
    const auto [market, code] = query_security(target);
    const int limit = parse_bounded(query_value(target, "limit", "500"), "limit", 1, 10000);
    if (!state.jsn_index) throw Error("JSN index is unavailable");
    return state.jsn_index->query_security(market, code, limit);
}

Json query_security_profile(const ApiState& state, const RequestTarget& target) {
    if (!state.institution_service) throw Error("institution service is unavailable");
    const auto [market, code] = query_security(target);
    InstitutionQuery query;
    query.market = market;
    query.code = code;
    query.refresh = query_bool(target, "refresh");
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                     "timeout_ms", 100, 60000);
    return state.institution_service->query_security(query);
}

Json query_industry_tree(const ApiState& state, const RequestTarget& target) {
    const auto family = lower_ascii(trim(query_value(target, "family", "industry")));
    Json blocks = Json::array();
    for (const auto& block : state.block_data.blocks)
        if (family == "all" || block.family == family) blocks.push_back(block_json(block));
    Json result = Json::object();
    result["family"] = family;
    result["count"] = static_cast<std::uint64_t>(blocks.size());
    result["blocks"] = std::move(blocks);
    return result;
}

Json query_jsn_resource(const ApiState& state, const RequestTarget& target, bool allow_write) {
    const auto resource = trim(query_value(target, "resource"));
    const auto prefix = trim(query_value(target, "prefix", "bi"));
    const auto action = lower_ascii(trim(query_value(target, "action", "probe")));
    if (resource.empty()) throw Error("resource is required");
    if (action != "probe" && action != "download")
        throw Error("action must be probe or download");
    if (action == "download" && !allow_write)
        throw Error("JSN download requires a same-origin POST with X-TDX-Action");
    if (action == "download" && state.jsn_root.empty())
        throw Error("JSN data directory is unavailable; pass --jsn-root");
    const auto output = state.jsn_root.empty() ? fs::path("output") / "tdx-jsn" : state.jsn_root;
    auto result = transfer_jsn_resource(
        resource, prefix, output, action == "download", 10000, state.root);
    if (action == "download" && state.jsn_index) {
        *state.jsn_index = JsnIndex(state.jsn_root);
        if (state.jsn_discovery_mutex) {
            std::lock_guard<std::mutex> lock(*state.jsn_discovery_mutex);
            state.jsn_discovery_cache.reset();
            state.jsn_candidate_cache.clear();
        }
    }
    return result;
}

Json query_blocks(const ApiState& state, const RequestTarget& target) {
    const auto query = trim(query_value(target, "q"));
    const auto family = lower_ascii(trim(query_value(target, "family")));
    const int limit = parse_bounded(query_value(target, "limit", "200"), "limit", 1, 2000);
    if (query.empty()) throw Error("query parameter q is required");
    const auto folded = lower_ascii(query);
    std::vector<const Block*> exact, partial;
    for (const auto& block : state.block_data.blocks) {
        if (!family.empty() && block.family != family) continue;
        const auto id = lower_ascii(block.block_id);
        const auto code = lower_ascii(block.block_code);
        const auto name = lower_ascii(block.name);
        if (id == folded || code == folded || name == folded) exact.push_back(&block);
        else if (id.find(folded) != std::string::npos ||
                 code.find(folded) != std::string::npos ||
                 name.find(folded) != std::string::npos) partial.push_back(&block);
    }
    const auto& matched = exact.empty() ? partial : exact;
    std::set<std::string> ids;
    Json blocks = Json::array();
    for (const auto* block : matched) {
        ids.insert(block->block_id);
        blocks.push_back(block_json(*block));
    }
    Json members = Json::array();
    std::size_t total_members = 0;
    for (const auto& member : state.block_data.members) {
        if (!ids.count(member.block_id)) continue;
        ++total_members;
        if (members.size() < static_cast<std::size_t>(limit)) members.push_back(member_json(member));
    }
    Json result = Json::object();
    result["query"] = query;
    result["family"] = family;
    result["block_count"] = static_cast<std::uint64_t>(blocks.size());
    result["membership_count"] = static_cast<std::uint64_t>(total_members);
    result["returned_memberships"] = static_cast<std::uint64_t>(members.size());
    result["truncated"] = total_members > members.size();
    result["blocks"] = std::move(blocks);
    result["members"] = std::move(members);
    return result;
}

std::string normalize_market(std::string market) {
    market = lower_ascii(trim(std::move(market)));
    // 老版页面曾把完整 security_id 放进 market；接口在 code 仍单独提供时
    // 也应能恢复出市场，而不是返回 400。
    for (const char delimiter : {':', '/', '-'}) {
        const auto position = market.find(delimiter);
        if (position != std::string::npos) {
            market = market.substr(0, position);
            break;
        }
    }
    if (market.size() == 8 &&
        std::all_of(market.begin() + 2, market.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) market.resize(2);
    if (market == "0" || market == "sz") return "SZ";
    if (market == "1" || market == "sh") return "SH";
    if (market == "2" || market == "bj") return "BJ";
    if (market == "szse" || market == "shenzhen") return "SZ";
    if (market == "sse" || market == "shanghai") return "SH";
    if (market == "bse" || market == "beijing") return "BJ";
    throw Error("market must identify Shenzhen, Shanghai, or Beijing");
}

Json query_security_blocks(const ApiState& state, const RequestTarget& target) {
    const auto market = normalize_market(query_value(target, "market"));
    const auto code = trim(query_value(target, "code"));
    const auto family = lower_ascii(trim(query_value(target, "family")));
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("code must contain exactly six digits");
    std::set<std::string> ids;
    Json memberships = Json::array();
    std::string security_name;
    for (const auto& member : state.block_data.members) {
        if (member.market != market || member.code != code ||
            (!family.empty() && member.family != family)) continue;
        ids.insert(member.block_id);
        memberships.push_back(member_json(member));
        if (security_name.empty()) security_name = member.security_name;
    }
    Json blocks = Json::array();
    for (const auto& block : state.block_data.blocks)
        if (ids.count(block.block_id)) blocks.push_back(block_json(block));
    Json result = Json::object();
    result["security_id"] = market + code;
    result["market"] = market;
    result["code"] = code;
    result["name"] = security_name;
    result["block_count"] = static_cast<std::uint64_t>(blocks.size());
    result["blocks"] = std::move(blocks);
    result["memberships"] = std::move(memberships);
    return result;
}

Json query_securities(const ApiState& state, const RequestTarget& target) {
    const auto query = trim(query_value(target, "q"));
    const auto folded = lower_ascii(query);
    const int limit = parse_bounded(query_value(target, "limit", "20"),
                                    "limit", 1, 100);
    if (query.empty() || query.size() > 64)
        throw Error("query parameter q is required and must not exceed 64 bytes");
    std::vector<const Security*> exact, partial;
    for (const auto& [key, security] : state.block_data.securities) {
        (void)key;
        const auto code = lower_ascii(security.code);
        const auto name = lower_ascii(security.name);
        const auto id = lower_ascii(security.market + security.code);
        if (code == folded || name == folded || id == folded)
            exact.push_back(&security);
        else if (code.find(folded) != std::string::npos ||
                 name.find(folded) != std::string::npos ||
                 id.find(folded) != std::string::npos)
            partial.push_back(&security);
    }
    const auto& matches = exact.empty() ? partial : exact;
    Json records = Json::array();
    for (const auto* security : matches) {
        if (static_cast<int>(records.size()) >= limit) break;
        Json row = Json::object();
        row["security_id"] = security->market + security->code;
        row["market"] = lower_ascii(security->market);
        row["market_id"] = security->market_id;
        row["code"] = security->code;
        row["name"] = security->name;
        records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["query"] = query;
    result["match_count"] = static_cast<std::uint64_t>(matches.size());
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["truncated"] = matches.size() > records.size();
    result["securities"] = std::move(records);
    return result;
}

}  // namespace tdx::server_detail
