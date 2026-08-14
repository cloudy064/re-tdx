#include "formula_context_relations_detail.hpp"
#include "formula_context_relations_catalog.hpp"

#include "formula_context_support_internal.hpp"
#include "formula_nested_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/daily.hpp"
#include "tdx/hyzt.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::formula_context_detail {

const Block* find_block(const BlockData& data, const std::string& block_id) {
    const auto found = std::find_if(data.blocks.begin(), data.blocks.end(),
        [&](const auto& block) { return block.block_id == block_id; });
    return found == data.blocks.end() ? nullptr : &*found;
}

const Block* industry_block_for(const BlockData& data, std::string market,
                                const std::string& code) {
    market = lower_ascii(std::move(market));
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2") market = "bj";
    const int market_id = market == "sh" ? 1 : market == "bj" ? 2 : 0;

    if (market_id == 1 && (starts_with(code, "880") || starts_with(code, "881"))) {
        const auto found = std::find_if(data.blocks.begin(), data.blocks.end(),
            [&](const auto& block) { return block.block_code == code; });
        if (found != data.blocks.end()) return &*found;
    }

    // TdxW type 120 chooses 880xxx for the normal industry mode and 881xxx
    // for the research-industry mode.  The desktop default is the normal
    // hierarchy, so prefer its direct (leaf) assignment and retain the
    // research hierarchy as a fallback for securities without one.
    for (const auto family : {"industry", "research-industry"}) {
        for (const auto& member : data.members) {
            if (member.market_id != market_id || member.code != code ||
                member.family != family || member.membership != "direct") continue;
            if (const auto* block = find_block(data, member.block_id)) return block;
        }
    }
    return nullptr;
}

const Block* direct_industry_block_for(const BlockData& data,
                                       const std::string& market,
                                       const std::string& code,
                                       std::string_view family) {
    const int market_id = market_id_for(market);
    for (const auto& member : data.members) {
        if (member.market_id != market_id || member.code != code ||
            member.family != family || member.membership != "direct")
            continue;
        if (const auto* block = find_block(data, member.block_id)) return block;
    }
    return nullptr;
}

void bind_industry_valuation_functions(
    Json& context, const std::filesystem::path& root,
    const std::filesystem::path& jsn_root, const std::string& market,
    const std::string& code,
    const std::set<std::string>& dependencies, const BlockData& block_data,
    int security_type_value) {
    const bool needs_pe = dependencies.count("HYSYL") != 0;
    const bool needs_pb = dependencies.count("HYSJL") != 0;
    if (!needs_pe && !needs_pb) return;
    if (!context.as_object().count("symbols")) context["symbols"] = Json::object();

    const auto normalized = normalized_market(market);
    const int mode = tdx_user_industry_mode(root);
    const std::string configured_family =
        mode == 2 ? "research-industry" : "industry";
    const Block* selected_block = nullptr;
    std::string selected_code;
    std::string selected_name;
    std::string selection;
    if (normalized == "sh" &&
        (starts_with(code, "880") || starts_with(code, "881"))) {
        selected_code = code;
        selection = "current-tdx-industry-index";
        const auto found = std::find_if(
            block_data.blocks.begin(), block_data.blocks.end(),
            [&](const auto& block) { return block.block_code == code; });
        if (found != block_data.blocks.end()) selected_block = &*found;
    } else if (security_type_value == 0) {
        selected_code = code;
        selection = "current-index";
    } else {
        selected_block = direct_industry_block_for(
            block_data, market, code, configured_family);
        if (selected_block) selected_code = selected_block->block_code;
        selection = "configured-direct-industry";
    }
    if (selected_block) selected_name = selected_block->name;
    const std::string configured_selected_code = selected_code;
    const std::string configured_selected_name = selected_name;
    const std::string configured_selected_family = selected_block
        ? selected_block->family : std::string{};

    std::shared_ptr<const IndustryValuationCatalog> catalog;
    const IndustryValuationRecord* record = nullptr;
    std::string source_error;
    bool normal_industry_fallback = false;
    try {
        catalog = load_industry_valuation_catalog(jsn_root);
        if (!selected_code.empty()) {
            const auto found = catalog->records.find(selected_code);
            if (found != catalog->records.end()) record = &found->second;
        }
        // The only audited public PE/PB resource uses normal 880xxx leaf
        // industries.  When the desktop is configured for 881xxx research
        // industries but that host-only valuation record is absent, retain a
        // useful, explicit fallback to the security's normal industry.  This
        // is never applied to a directly selected industry or broad index.
        if (!record && mode == 2 && selection == "configured-direct-industry") {
            const auto* fallback = direct_industry_block_for(
                block_data, market, code, "industry");
            if (fallback && !fallback->block_code.empty()) {
                const auto found = catalog->records.find(fallback->block_code);
                if (found != catalog->records.end()) {
                    selected_block = fallback;
                    selected_code = fallback->block_code;
                    selected_name = fallback->name;
                    record = &found->second;
                    normal_industry_fallback = true;
                    selection = "configured-research-industry-public-normal-fallback";
                }
            }
        }
    } catch (const std::exception& error) {
        source_error = error.what();
    }

    const auto narrowed = [](const std::optional<double>& value) {
        return value ? static_cast<double>(static_cast<float>(*value)) : 0.0;
    };
    const double pe = record ? narrowed(record->pe) : 0.0;
    const double pb = record ? narrowed(record->pb_mrq) : 0.0;
    if (needs_pe) context["symbols"]["HYSYL"] = pe;
    if (needs_pb) context["symbols"]["HYSJL"] = pb;

    Json metadata = Json::object();
    metadata["schema"] = "tdx-formula-industry-valuation-v1";
    metadata["requested_security"] = normalized + ":" + code;
    metadata["user_ini_key"] = "Other/UseTdxL3HY";
    metadata["configured_mode"] = mode;
    metadata["configured_family"] = configured_family;
    metadata["configured_selected_code"] = configured_selected_code.empty()
        ? Json(nullptr) : Json(configured_selected_code);
    metadata["configured_selected_name"] = configured_selected_name.empty()
        ? Json(nullptr) : Json(configured_selected_name);
    metadata["configured_selected_family"] = configured_selected_family.empty()
        ? Json(nullptr) : Json(configured_selected_family);
    metadata["selection"] = selection;
    metadata["normal_industry_fallback"] = normal_industry_fallback;
    metadata["selected_code"] = selected_code.empty() ? Json(nullptr) : Json(selected_code);
    metadata["selected_name"] = selected_name.empty() ? Json(nullptr) : Json(selected_name);
    metadata["selected_family"] = selected_block
        ? Json(selected_block->family) : Json(nullptr);
    metadata["record_available"] = record != nullptr;
    metadata["availability"] = normal_industry_fallback
        ? "public-hyzt-normal-industry-fallback"
        : record ? "public-hyzt-record"
        : selected_code.empty() ? "industry-assignment-missing"
        : source_error.empty() ? "public-hyzt-record-missing"
                               : "public-hyzt-resource-error";
    metadata["source_resource"] =
        "list/func_gx_hyzt101_1.jsn";
    metadata["source_path"] = catalog
        ? Json(path_utf8(catalog->source_path)) : Json(nullptr);
    metadata["source_size"] = catalog
        ? Json(catalog->source_size) : Json(nullptr);
    metadata["source_row_count"] = catalog
        ? Json(catalog->row_count) : Json(nullptr);
    metadata["source_industry_count"] = catalog
        ? Json(static_cast<std::uint64_t>(catalog->records.size())) : Json(nullptr);
    metadata["source_error"] = source_error.empty() ? Json(nullptr) : Json(source_error);
    metadata["member_count"] = record
        ? Json(record->member_count) : Json(nullptr);
    Json bindings = Json::object();
    if (needs_pe) {
        Json item = Json::object();
        item["value"] = pe;
        item["available"] = record && record->pe.has_value();
        item["source_field"] = "hyPE";
        item["tcalc_opcode"] = 1328;
        item["tdxw_callback_type"] = 120;
        item["tdxw_return_offset"] = 60;
        bindings["HYSYL"] = std::move(item);
    }
    if (needs_pb) {
        Json item = Json::object();
        item["value"] = pb;
        item["available"] = record && record->pb_mrq.has_value();
        item["source_field"] = "hyPB";
        item["tcalc_opcode"] = 1344;
        item["tdxw_callback_type"] = 163;
        item["tdxw_return_offset"] = 380;
        bindings["HYSJL"] = std::move(item);
    }
    metadata["bindings"] = std::move(bindings);
    metadata["numeric_mode"] =
        "tcalc-float32-broadcast-zero-when-host-record-unavailable";
    metadata["native_host_family_exact"] = !normal_industry_fallback;
    metadata["boundary"] =
        "public HYZT covers normal 880xxx leaf industries; configured 881xxx securities use an explicit normal-industry fallback, while directly selected research and broad indexes return zero when absent";
    context["industry_valuation"] = std::move(metadata);
}

const Block* level1_research_industry_block_for(
    const BlockData& data, const std::string& market, const std::string& code) {
    const int market_id = market_id_for(market);
    for (const auto& member : data.members) {
        if (member.market_id != market_id || member.code != code ||
            member.family != "research-industry")
            continue;
        const auto* block = find_block(data, member.block_id);
        if (block && block->level == 1) return block;
    }
    return nullptr;
}

Json industry_index_kline(const Json& target, const std::string& code, int timeout_ms) {
    const auto period = text_or(target, "period", "day");
    const int count = std::max(1, integer_or(target, "count", 800));
    const int page_size = std::min(800, std::max(
        1, integer_or(target, "page_size", std::min(800, count))));
    const int downloaded = std::max(count, integer_or(target, "downloaded", count));
    const int pages = std::min(20, std::max(1, (downloaded + page_size - 1) / page_size));
    const int start = std::max(0, integer_or(target, "start", 0));
    const auto cache_key = code + ":" + period + ":" + std::to_string(pages) + ":" +
                           std::to_string(page_size) + ":" + std::to_string(start);
    static std::mutex cache_mutex;
    static std::map<std::string, Json> cache;
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto found = cache.find(cache_key);
    if (found != cache.end()) return found->second;
    auto result = fetch_kline_document("sh", code, "index", period, pages,
                                       page_size, start, "all", timeout_ms);
    cache.emplace(cache_key, result);
    return result;
}

void bind_industry_series(Json& context, const std::filesystem::path& root,
                          const Json& target, const std::string& market,
                          const std::string& code, const std::set<std::string>& dependencies,
                          int timeout_ms, const BlockData& block_data) {
    if (!needs_industry_context(dependencies)) return;
    const bool needs_series = std::any_of(
        industry_series_bindings.begin(), industry_series_bindings.end(),
        [&](const auto& binding) {
            return dependencies.count(std::string(binding.symbol)) != 0;
        });
    const bool needs_default_industry = needs_series || dependencies.count("HYBLOCK") ||
                                        dependencies.count("HYZSCODE");
    const auto* block = needs_default_industry
        ? industry_block_for(block_data, market, code) : nullptr;
    if (needs_default_industry && (!block || block->block_code.empty()))
        throw Error("formula industry context found no direct industry assignment");

    if (dependencies.count("LEVEL1HYBLOCK")) {
        if (!context.as_object().count("symbols"))
            context["symbols"] = Json::object();
        const auto* level1 = level1_research_industry_block_for(
            block_data, market, code);
        bind_formula_text_symbol(
            context, context["symbols"], "LEVEL1HYBLOCK",
            level1 ? level1->name : std::string{},
            "tdxw-type167-research-industry-code-prefix3-local-hierarchy");
        Json metadata = Json::object();
        metadata["family"] = "research-industry";
        metadata["level"] = 1;
        metadata["block_code"] = level1 ? Json(level1->block_code) : Json(nullptr);
        metadata["block_name"] = level1 ? Json(level1->name) : Json(nullptr);
        metadata["source_key"] = level1 ? Json(level1->source_key) : Json(nullptr);
        context["level1_research_industry"] = std::move(metadata);
    }
    if (dependencies.count("MOREHYBLOCK")) {
        if (!context.as_object().count("symbols"))
            context["symbols"] = Json::object();
        const int mode = tdx_user_industry_mode(root);
        const std::string family = mode == 2 ? "research-industry" : "industry";
        const auto* selected = direct_industry_block_for(
            block_data, market, code, family);
        bind_formula_text_symbol(
            context, context["symbols"], "MOREHYBLOCK",
            selected ? selected->name : std::string{},
            "tdxw-type167-user-ini-industry-mode-leaf-name");
        Json metadata = Json::object();
        metadata["user_ini_key"] = "Other/UseTdxL3HY";
        metadata["configured_mode"] = mode;
        metadata["type167_mode_byte"] = mode == 2 ? 1 : 0;
        metadata["selected_family"] = family;
        metadata["block_code"] = selected ? Json(selected->block_code) : Json(nullptr);
        metadata["block_name"] = selected ? Json(selected->name) : Json(nullptr);
        metadata["source_key"] = selected ? Json(selected->source_key) : Json(nullptr);
        context["more_industry"] = std::move(metadata);
    }
    if (!needs_default_industry) return;
    if (needs_series) {
        const auto document = industry_index_kline(target, block->block_code, timeout_ms);
        const auto* rows = optional(document, "bars");
        if (!rows || !rows->is_array())
            throw Error("industry-index K-line context returned no bars");
        if (!context.as_object().count("series")) context["series"] = Json::object();
        std::map<std::string, double> previous;
        for (const auto& binding : industry_series_bindings) {
            const std::string symbol(binding.symbol);
            if (!dependencies.count(symbol)) continue;
            Json points = Json::object();
            for (const auto& row : rows->as_array()) {
                const auto date = text_or(row, "date"), time = text_or(row, "time");
                const auto* value = optional(row, binding.field);
                if (date.empty() || !value || !value->is_number()) continue;
                double number = value->as_number();
                // The native HY_INDEX OHLCV evaluators carry the previous value
                // forward when the downloaded industry bar contains a near-zero
                // field.  Breadth counts and amount are kept as reported.
                if (binding.carry_forward_near_zero) {
                    if (number < 0.00001 && previous.count(symbol)) number = previous.at(symbol);
                    if (number >= 0.00001) previous[symbol] = number;
                }
                points[date + "|" + time] = number;
            }
            context["series"][symbol] = std::move(points);
        }
    }

    // The numeric interpreter ignores drawing side effects.  Bind a harmless
    // numeric surrogate so DRAWTEXT_FIX can be evaluated, and preserve the
    // actual string for renderers and API clients.
    if (dependencies.count("HYBLOCK") || dependencies.count("HYZSCODE"))
        if (!context.as_object().count("symbols")) context["symbols"] = Json::object();
    if (dependencies.count("HYBLOCK")) {
        bind_formula_text_symbol(context, context["symbols"], "HYBLOCK", block->name,
                                 "tdxw-type120-industry-code-local-hierarchy");
    }
    if (dependencies.count("HYZSCODE"))
        bind_formula_text_symbol(context, context["symbols"], "HYZSCODE",
                                 block->block_code,
                                 "tdxw-type120-industry-index-code-local-hierarchy");
    Json metadata = Json::object();
    metadata["family"] = block->family;
    metadata["family_name"] = block->family_name;
    metadata["block_code"] = block->block_code;
    metadata["block_name"] = block->name;
    metadata["security"] = "sh:" + block->block_code;
    context["industry_index"] = std::move(metadata);
    if (needs_series)
        context["industry_index_series_mode"] =
            "tcalc-type120-880-default-881-fallback-date-time-aligned";
}


} // namespace tdx::formula_context_detail
