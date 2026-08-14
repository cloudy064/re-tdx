#include "formula_context_local_internal.hpp"

#include "formula_context_support_internal.hpp"
#include "formula_nested_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {

void bind_formula_scalar(Json& context, const std::string& name,
                         const std::optional<double>& value) {
    if (!context.as_object().count("formula_scalar_bindings"))
        context["formula_scalar_bindings"] = Json::object();
    context["formula_scalar_bindings"][name] = value ? Json(*value) : Json(nullptr);
}

void bind_blocksetnum(Json& context, const std::filesystem::path& root,
                      const BlockData& data,
                      const std::vector<std::string>& bindings) {
    if (bindings.empty()) return;
    const auto industry_mode = tdx_user_industry_mode(root);
    const std::string active_industry = industry_mode == 2
        ? "research-industry" : "industry";
    const auto custom = custom_block_member_counts(root);
    const auto find_block_name = [&](std::string_view family,
                                     std::string_view name) -> const Block* {
        const auto normalized = lower_ascii(std::string(name));
        const auto found = std::find_if(
            data.blocks.begin(), data.blocks.end(), [&](const auto& block) {
                return block.family == family &&
                       lower_ascii(block.name) == normalized;
            });
        return found == data.blocks.end() ? nullptr : &*found;
    };
    const auto find_custom = [&](std::string_view name, bool named_only)
        -> const CustomBlockCountEntry* {
        const auto normalized = lower_ascii(std::string(name));
        const auto found = std::find_if(
            custom.entries.begin(), custom.entries.end(), [&](const auto& entry) {
                return (!named_only || !entry.built_in) &&
                       lower_ascii(entry.name) == normalized;
            });
        return found == custom.entries.end() ? nullptr : &*found;
    };

    Json resolutions = Json::array();
    constexpr std::string_view prefix = "BLOCKSETNUM#";
    for (const auto& binding : bindings) {
        const auto requested = binding.substr(prefix.size());
        auto lookup_name = requested;
        const auto normalized = lower_ascii(requested);
        std::string forced_family;
        bool custom_only = false;
        if (normalized.rfind("hy.", 0) == 0) {
            forced_family = active_industry;
            lookup_name = requested.substr(3);
        } else if (normalized.rfind("gn.", 0) == 0) {
            forced_family = "concept";
            lookup_name = requested.substr(3);
        } else if (normalized.rfind("my.", 0) == 0) {
            custom_only = true;
            lookup_name = requested.substr(3);
        }

        const Block* block = nullptr;
        const CustomBlockCountEntry* custom_block = nullptr;
        if (!forced_family.empty()) {
            block = find_block_name(forced_family, lookup_name);
        } else if (custom_only) {
            custom_block = find_custom(lookup_name, true);
        } else {
            for (const auto family : {active_industry.c_str(), "concept", "style", "index"}) {
                block = find_block_name(family, lookup_name);
                if (block) break;
            }
            if (!block) custom_block = find_custom(lookup_name, false);
        }

        const int count = block ? block->member_count
                                : custom_block ? custom_block->count : 0;
        bind_formula_scalar(context, binding, static_cast<double>(count));
        Json item = Json::object();
        item["binding"] = binding;
        item["requested_name"] = requested;
        item["lookup_name"] = lookup_name;
        item["found"] = block || custom_block;
        item["member_count"] = count;
        if (block) {
            item["family"] = block->family;
            item["block_code"] = block->block_code;
            item["source"] = block->source_file;
        } else if (custom_block) {
            item["family"] = "custom";
            item["block_code"] = custom_block->key;
            item["source"] = custom.catalog_source;
        } else {
            item["family"] = Json(nullptr);
            item["block_code"] = Json(nullptr);
            item["source"] = Json(nullptr);
        }
        resolutions.push_back(std::move(item));
    }
    context["blocksetnum_resolutions"] = std::move(resolutions);
    context["blocksetnum_binding_count"] =
        static_cast<std::uint64_t>(bindings.size());
    context["blocksetnum_industry_mode"] = industry_mode;
    context["blocksetnum_custom_catalog_source"] = custom.catalog_source;
    context["blocksetnum_custom_directory_count"] =
        static_cast<std::uint64_t>(custom.entries.size());
    context["blocksetnum_mode"] =
        "tcalc-opcode1244-command8-type5-offset1004-local-catalog-first-match";
}

void bind_local_security_metadata(
    Json& context, Json& symbols, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies) {
    if (dependencies.count("MAINBUSINESS")) {
        const int main_business_market = formula_market_id(market);
        if (main_business_market < 0)
            throw Error("MAINBUSINESS formula context has an invalid market");
        const auto catalog = cached_main_business_catalog(root);
        bind_formula_text_symbol(
            context, symbols, "MAINBUSINESS",
            main_business_for(*catalog, main_business_market, code),
            "tdxw-type167-specgpext-third-field");
        context["main_business_source"] = catalog->source_path;
        context["main_business_record_count"] =
            static_cast<std::uint64_t>(catalog->records.size());
        context["main_business_mode"] =
            "tdxw-type167-specgpext-enhanced-function-local-reconstruction";
    }
    if (dependencies.count("SAFESCORE") || dependencies.count("SHINESCORE")) {
        const int score_market = formula_market_id(market);
        if (score_market < 0)
            throw Error("security-score formula context has an invalid market");
        const auto catalog = cached_main_business_catalog(root);
        const auto safety = safety_score_for(*catalog, score_market, code);
        const auto shine = shine_score_for(*catalog, score_market, code);
        // TdxW type-167 offset 67 (SHINESCORE) first validates offset 51
        // (SAFESCORE).  Preserve that shared validity gate instead of treating
        // the two specgpext fields as independent values.
        const auto effective_shine =
            safety && static_cast<float>(*safety) > -0.0000099999997f
                ? shine
                : std::optional<double>{};
        if (dependencies.count("SAFESCORE"))
            bind_formula_scalar(context, "SAFESCORE", safety);
        if (dependencies.count("SHINESCORE"))
            bind_formula_scalar(context, "SHINESCORE", effective_shine);
        Json values = Json::object();
        values["record_found"] = safety.has_value();
        values["safety_score"] = safety ? Json(*safety) : Json(nullptr);
        values["shine_score"] = shine ? Json(*shine) : Json(nullptr);
        context["security_score_values"] = std::move(values);
        context["security_score_source"] = catalog->source_path;
        context["security_score_record_count"] =
            static_cast<std::uint64_t>(catalog->safety_scores.size());
        context["security_score_mode"] =
            "tdxw-type167-specgpext-fields4-5-local-reconstruction";
    }
    if (dependencies.count("ZDBLOCK") || dependencies.count("ZDBLOCKNUM")) {
        const int custom_block_market = formula_market_id(market);
        if (custom_block_market < 0)
            throw Error("custom-block formula context has an invalid market");
        const auto membership = custom_block_membership_for(
            root, custom_block_market, code);
        if (dependencies.count("ZDBLOCK"))
            bind_formula_text_symbol(
                context, symbols, "ZDBLOCK", membership.text,
                "tdxw-command8-category2-blocknew-membership-pipe-to-space");
        if (dependencies.count("ZDBLOCKNUM"))
            symbols["ZDBLOCKNUM"] = membership.count;
        context["custom_block_directory_source"] = membership.catalog_source;
        context["custom_block_directory_count"] =
            static_cast<std::uint64_t>(membership.directory_count);
        context["custom_block_membership_count"] = membership.count;
        context["custom_block_mode"] =
            "tdxw-command8-category2-zxg-tjg-blocknew-local-reconstruction";
    }
    if (dependencies.count("ZHBLOCK") || dependencies.count("ZHBLOCKNUM")) {
        const int combination_block_market = formula_market_id(market);
        if (combination_block_market < 0)
            throw Error("combination-block formula context has an invalid market");
        const auto membership = combination_block_membership_for(
            root, combination_block_market, code);
        if (dependencies.count("ZHBLOCK"))
            bind_formula_text_symbol(
                context, symbols, "ZHBLOCK", membership.text,
                "tdxw-command8-category3-lcidx-cis-membership-pipe-to-space");
        if (dependencies.count("ZHBLOCKNUM"))
            symbols["ZHBLOCKNUM"] = membership.count;
        context["combination_block_directory_source"] =
            membership.catalog_source;
        context["combination_block_directory_count"] =
            static_cast<std::uint64_t>(membership.directory_count);
        context["combination_block_readable_member_file_count"] =
            static_cast<std::uint64_t>(membership.readable_member_file_count);
        context["combination_block_membership_count"] = membership.count;
        context["combination_block_mode"] =
            "tdxw-command8-category3-lcidx-lii-cis-local-reconstruction";
    }
    if (dependencies.count("SIMIBLOCK")) {
        // TCalc requests command 8/category 0, but this TdxW build only owns
        // cache and producer branches for categories 1..5.  Cold category 0
        // therefore reaches the common empty-result sentinel "|", which
        // TCalc changes to one space before interning the string.
        bind_formula_text_symbol(
            context, symbols, "SIMIBLOCK", " ",
            "tdxw-command8-category0-no-provider-branch-pipe-to-space");
        context["similar_block_mode"] =
            "tdxw-command8-category0-empty-in-current-host-build";
    }
}

} // namespace tdx::formula_context_detail

