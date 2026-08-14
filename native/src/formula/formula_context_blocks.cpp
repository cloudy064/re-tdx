#include "formula_context_blocks_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {
namespace {

int market_id_for(std::string market) {
    market = lower_ascii(std::move(market));
    if (market == "1" || market == "sh") return 1;
    if (market == "2" || market == "bj") return 2;
    return 0;
}

std::set<std::string> block_membership_names(
    const BlockData& data, const std::string& market, const std::string& code,
    std::string_view family, bool direct_only) {
    const int market_id = market_id_for(market);
    std::set<std::string> names;
    for (const auto& member : data.members) {
        if (member.market_id != market_id || member.code != code ||
            (!family.empty() && member.family != family) ||
            (direct_only && member.membership != "direct") ||
            member.block_name.empty())
            continue;
        names.insert(member.block_name);
    }
    return names;
}

std::string joined_block_names(const std::set<std::string>& names) {
    std::string result;
    for (const auto& name : names) {
        if (!result.empty()) result.push_back(' ');
        result += name;
    }
    return result;
}

void bind_formula_text_symbol(Json& context, Json& symbols,
                              std::string_view name, std::string value,
                              std::string_view source) {
    symbols[std::string(name)] = 0.0;
    if (!context.as_object().count("formula_text_symbols"))
        context["formula_text_symbols"] = Json::object();
    if (!context.as_object().count("formula_text_symbol_sources"))
        context["formula_text_symbol_sources"] = Json::object();
    context["formula_text_symbols"][std::string(name)] = std::move(value);
    context["formula_text_symbol_sources"][std::string(name)] =
        std::string(source);
    context["formula_text_symbols_mode"] =
        "tcalc-string-pool-handles-materialized-as-utf8-metadata";
}

std::uint32_t formula_block_code_number(const std::string& value) {
    char* end = nullptr;
    const auto parsed = std::strtoul(value.c_str(), &end, 10);
    if (end == value.c_str()) return 0;
    return static_cast<std::uint32_t>(parsed);
}

}  // namespace

void bind_block_metadata(
    Json& context, Json& symbols, const BlockData& data,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies) {
    const auto bind_family = [&](std::string_view text_symbol,
                                 std::string_view count_symbol,
                                 std::string_view family) {
        if (!dependencies.count(std::string(text_symbol)) &&
            !dependencies.count(std::string(count_symbol)))
            return;
        const auto names =
            block_membership_names(data, market, code, family, true);
        if (dependencies.count(std::string(text_symbol)))
            bind_formula_text_symbol(
                context, symbols, text_symbol, joined_block_names(names),
                "tdx-command8-infoharbor-direct-membership");
        if (dependencies.count(std::string(count_symbol)))
            symbols[std::string(count_symbol)] =
                static_cast<std::uint64_t>(names.size());
    };
    bind_family("FGBLOCK", "FGBLOCKNUM", "style");
    bind_family("ZSBLOCK", "ZSBLOCKNUM", "index");
    if (dependencies.count("GNBLOCKNUM"))
        symbols["GNBLOCKNUM"] = static_cast<std::uint64_t>(
            block_membership_names(
                data, market, code, "concept", true).size());
    if (dependencies.count("INBLOCK")) {
        const auto names =
            block_membership_names(data, market, code, {}, false);
        std::string wire(1, '\x1F');
        for (const auto& name : names) wire += name + '\x1F';
        if (!context.as_object().count("formula_private_text_symbols"))
            context["formula_private_text_symbols"] = Json::object();
        context["formula_private_text_symbols"]["__INBLOCK_MEMBERSHIPS"] =
            std::move(wire);
        context["inblock_membership_count"] =
            static_cast<std::uint64_t>(names.size());
    }
    context["block_metadata_mode"] =
        "tcalc-command8-categories-concept-style-index-local-reconstruction";
}

void bind_block_code_functions(
    Json& context, const BlockData& data, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies) {
    const int market_id = market_id_for(market);
    using Entry = std::pair<std::uint32_t, std::string>;
    std::vector<Entry> concepts;
    std::vector<Entry> styles;
    for (const auto& member : data.members) {
        if (member.market_id != market_id || member.code != code ||
            member.membership != "direct" || member.block_code.empty())
            continue;
        Entry entry{formula_block_code_number(member.block_code),
                    member.block_name};
        if (member.family == "concept") concepts.push_back(std::move(entry));
        else if (member.family == "style") styles.push_back(std::move(entry));
    }
    const auto ascending = [](const Entry& left, const Entry& right) {
        return left.first < right.first;
    };
    std::stable_sort(concepts.begin(), concepts.end(), ascending);
    std::stable_sort(styles.begin(), styles.end(), ascending);
    constexpr std::size_t host_limit = 60;
    if (concepts.size() > host_limit) concepts.resize(host_limit);
    if (concepts.size() + styles.size() > host_limit)
        styles.resize(host_limit - concepts.size());

    if (!context.as_object().count("formula_private_text_symbols"))
        context["formula_private_text_symbols"] = Json::object();
    Json concept_codes = Json::array();
    Json style_codes = Json::array();
    for (std::size_t index = 0; index < concepts.size(); ++index) {
        const auto value = std::to_string(concepts[index].first);
        context["formula_private_text_symbols"]
               ["__GNBKZSCODE#" + std::to_string(index + 1)] = value;
        concept_codes.push_back(value);
    }
    for (std::size_t index = 0; index < styles.size(); ++index) {
        const auto value = std::to_string(styles[index].first);
        context["formula_private_text_symbols"]
               ["__FGBKZSCODE#" + std::to_string(index + 1)] = value;
        style_codes.push_back(value);
    }
    context["type167_block_code_mode"] =
        "tdxw-type167-direct-membership-dword-ascending-concept-then-style";
    context["type167_block_code_limit"] =
        static_cast<std::uint64_t>(host_limit);
    context["type167_concept_code_count"] =
        static_cast<std::uint64_t>(concepts.size());
    context["type167_style_code_count"] =
        static_cast<std::uint64_t>(styles.size());
    context["type167_block_code_total"] =
        static_cast<std::uint64_t>(concepts.size() + styles.size());
    context["type167_concept_codes"] = std::move(concept_codes);
    context["type167_style_codes"] = std::move(style_codes);

    if (dependencies.count("GETNAMEOFCODE")) {
        Json overrides = Json::object();
        for (const auto& block : data.blocks)
            if (!block.block_code.empty() && !block.name.empty())
                overrides["1|" + block.block_code] = block.name;
        const auto security = data.securities.find({market_id, code});
        if (security != data.securities.end() &&
            !security->second.name.empty())
            overrides[std::to_string(market_id) + "|" + code] =
                security->second.name;
        context["formula_code_name_root"] = path_utf8(root);
        context["formula_code_name_overrides"] = overrides;
        context["code_name_override_count"] =
            static_cast<std::uint64_t>(overrides.size());
        context["code_name_lookup_mode"] =
            "tdxw-type120-security-directory-name-offset31-native-tnf-cache";
        context["code_name_security_catalog_source"] =
            "T0002/hq_cache/{szs,shs,bjs}.tnf";
    }
}

}  // namespace tdx::formula_context_detail
