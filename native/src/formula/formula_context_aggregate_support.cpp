#include "formula_context_aggregate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace tdx::formula_context_detail {

const Json* aggregate_optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string aggregate_text_or(const Json& object, std::string_view key,
                              std::string fallback) {
    const auto* value = aggregate_optional(object, key);
    return value && value->is_string() ? value->as_string()
                                       : std::move(fallback);
}

int aggregate_integer_or(const Json& object, std::string_view key,
                         int fallback) {
    const auto* value = aggregate_optional(object, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : fallback;
}

std::string aggregate_compact_date(const std::string& value) {
    std::string result;
    for (const unsigned char ch : value)
        if (std::isdigit(ch)) result.push_back(static_cast<char>(ch));
    return result.size() >= 8 ? result.substr(0, 8) : std::string{};
}

bool aggregate_starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

const Json* aggregate_nested(
    const Json& value, std::initializer_list<std::string_view> path) {
    const Json* current = &value;
    for (const auto key : path) {
        if (!current->is_object()) return nullptr;
        const auto found = current->as_object().find(key);
        if (found == current->as_object().end()) return nullptr;
        current = &found->second;
    }
    return current;
}

std::string aggregate_market_name(int market_id) {
    return market_id == 0 ? std::string("sz") :
           market_id == 1 ? std::string("sh") :
           market_id == 2 ? std::string("bj") :
           std::to_string(market_id);
}

AggregateUniverseCatalog load_aggregate_universe_catalog(
    const std::filesystem::path& root) {
    AggregateUniverseCatalog result;
    result.industry_mode = tdx_user_industry_mode(root);
    result.active_industry = result.industry_mode == 2
        ? "research-industry" : "industry";
    result.custom = custom_block_member_counts(root);
    return result;
}

AggregateUniverseResolution resolve_aggregate_universe(
    const std::filesystem::path& root, const BlockData& data,
    const AggregateUniverseCatalog& catalog, const std::string& requested_name,
    AggregateMemberPolicy member_policy) {
    AggregateUniverseResolution result;
    result.lookup_name = requested_name;
    const auto normalized = lower_ascii(requested_name);
    std::string forced_family;
    bool custom_only = false;
    if (aggregate_starts_with(normalized, "hy.")) {
        forced_family = catalog.active_industry;
        result.lookup_name = requested_name.substr(3);
    } else if (aggregate_starts_with(normalized, "gn.")) {
        forced_family = "concept";
        result.lookup_name = requested_name.substr(3);
    } else if (aggregate_starts_with(normalized, "my.")) {
        custom_only = true;
        result.lookup_name = requested_name.substr(3);
    }

    const auto find_block = [&](std::string_view family) -> const Block* {
        const auto wanted = lower_ascii(result.lookup_name);
        const auto found = std::find_if(
            data.blocks.begin(), data.blocks.end(), [&](const Block& block) {
                return block.family == family &&
                       lower_ascii(block.name) == wanted;
            });
        return found == data.blocks.end() ? nullptr : &*found;
    };
    const auto find_custom = [&](bool named_only) -> const CustomBlockCountEntry* {
        const auto wanted = lower_ascii(result.lookup_name);
        const auto found = std::find_if(
            catalog.custom.entries.begin(), catalog.custom.entries.end(),
            [&](const CustomBlockCountEntry& entry) {
                return (!named_only || !entry.built_in) &&
                       lower_ascii(entry.name) == wanted;
            });
        return found == catalog.custom.entries.end() ? nullptr : &*found;
    };

    const Block* block = nullptr;
    const CustomBlockCountEntry* custom = nullptr;
    if (!forced_family.empty()) {
        block = find_block(forced_family);
    } else if (custom_only) {
        custom = find_custom(true);
    } else {
        for (const auto family :
             {catalog.active_industry.c_str(), "concept", "style", "index"}) {
            block = find_block(family);
            if (block) break;
        }
        if (!block) custom = find_custom(false);
    }

    if (block) {
        result.found = true;
        result.family = block->family;
        result.block_code = block->block_code;
        result.source = block->source_file;
        std::set<AggregateSecurityKey> seen;
        for (const auto& member : data.members) {
            const AggregateSecurityKey key{member.market_id, member.code};
            if (member.block_id != block->block_id || !seen.insert(key).second)
                continue;
            result.members.push_back(key);
        }
        if (member_policy == AggregateMemberPolicy::horcalc)
            std::sort(result.members.begin(), result.members.end());
    } else if (custom) {
        result.found = true;
        result.family = "custom";
        result.block_code = custom->key;
        result.source = catalog.custom.catalog_source;
        std::set<AggregateSecurityKey> seen;
        for (const auto& member : load_custom_block_members(root, custom->key)) {
            const AggregateSecurityKey key{member.market_id, member.code};
            if (member_policy == AggregateMemberPolicy::indicator &&
                !seen.insert(key).second)
                continue;
            result.members.push_back(key);
        }
    }
    return result;
}

AggregateDailyMap load_aggregate_daily(
    const std::filesystem::path& root,
    const std::vector<AggregateUniverseResolution>& resolutions,
    std::uint64_t& member_file_count) {
    std::set<AggregateSecurityKey> members;
    for (const auto& resolution : resolutions)
        members.insert(resolution.members.begin(), resolution.members.end());
    AggregateDailyMap result;
    member_file_count = 0;
    for (const auto& member : members) {
        if (std::filesystem::is_regular_file(
                locate_daily_file(root, member.first, member.second)))
            ++member_file_count;
        auto bars = load_daily_bars(root, member.first, member.second);
        if (!bars.empty()) result.emplace(member, std::move(bars));
    }
    return result;
}

}  // namespace tdx::formula_context_detail
