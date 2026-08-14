#include "research_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>

namespace tdx {
namespace {

struct CategoryDefinition {
    std::string_view id;
    std::string_view label;
    std::string_view resource;
    std::string_view entity_type;
    std::array<std::string_view, 2> namespaces;
    std::size_t namespace_count;
};

constexpr std::array<CategoryDefinition, 9> category_definitions{{
    {"institution-research", "最新机构调研", "list/func_tzzhd109_1.jsn", "security", {"tzzhd", {}}, 1},
    {"interaction", "最新互动问答", "list/func_tzzhd103_1.jsn", "security", {"tzzhd", {}}, 1},
    {"featured-interaction", "精选互动问答", "list/func_tzzhd190_1.jsn", "security", {"tzzhd", {}}, 1},
    {"industry", "行业调研热度", "list/func_tzzhd110_1.jsn", "industry", {"tzzhd2", {}}, 1},
    {"post-st", "摘帽后被调研", "list/func_tzzhd112_1.jsn", "security", {"tzzhd", {}}, 1},
    {"exchange-inquiry", "交易所问询", "list/func_tzzhd105_1.jsn", "security", {"tzzhd", "cfjg"}, 2},
    {"regulatory", "交易所监管", "list/func_tzzhd114_1.jsn", "security", {"cfjg", {}}, 1},
    {"market-ban", "市场禁入", "list/func_tzzhd115_1.jsn", "security", {"cfjg", {}}, 1},
    {"notable-institution", "知名调研机构", "list/func_tzzhd117_1.jsn", "institution", {"zmjg", {}}, 1},
}};

struct DetailDefinition {
    std::string_view name_space;
    detail::research::DetailKind kind;
};

constexpr std::array<DetailDefinition, 4> detail_definitions{{
    {"tzzhd", detail::research::DetailKind::activity},
    {"cfjg", detail::research::DetailKind::regulatory},
    {"tzzhd2", detail::research::DetailKind::related_security},
    {"zmjg", detail::research::DetailKind::related_security},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(category_definitions,
    [](const CategoryDefinition& value) { return value.id; }));
static_assert(unique_by(category_definitions,
    [](const CategoryDefinition& value) { return value.resource; }));
static_assert(unique_by(detail_definitions,
    [](const DetailDefinition& value) { return value.name_space; }));

}  // namespace

const std::vector<ResearchCategorySpec>& research_categories() {
    static const std::vector<ResearchCategorySpec> categories = [] {
        std::vector<ResearchCategorySpec> result;
        result.reserve(category_definitions.size());
        for (const auto& definition : category_definitions) {
            ResearchCategorySpec item;
            item.id = definition.id;
            item.label = definition.label;
            item.resource = definition.resource;
            item.entity_type = definition.entity_type;
            for (std::size_t index = 0; index < definition.namespace_count; ++index)
                item.detail_namespaces.emplace_back(definition.namespaces[index]);
            result.push_back(std::move(item));
        }
        return result;
    }();
    return categories;
}

namespace detail::research {

const ResearchCategorySpec& category_spec(const std::string& id) {
    const auto& categories = research_categories();
    const auto found = std::find_if(categories.begin(), categories.end(),
        [&](const ResearchCategorySpec& item) { return item.id == id; });
    if (found == categories.end())
        throw Error("category must be institution-research, interaction, featured-interaction, "
                    "industry, post-st, exchange-inquiry, regulatory, market-ban, or "
                    "notable-institution");
    return *found;
}

DetailKind detail_kind(std::string_view name_space) {
    const auto found = std::find_if(detail_definitions.begin(), detail_definitions.end(),
        [&](const DetailDefinition& item) { return item.name_space == name_space; });
    if (found == detail_definitions.end())
        throw Error("unknown research detail namespace: " + std::string(name_space));
    return found->kind;
}

std::string derived_detail_id(const std::string& category, const std::string& code) {
    if (category == "institution-research" || category == "post-st") return "j" + code;
    if (category == "interaction" || category == "featured-interaction") return "h" + code;
    if (category == "exchange-inquiry") return "1" + code;
    if (category == "regulatory") return "2" + code;
    if (category == "market-ban") return "3" + code;
    return {};
}

}  // namespace detail::research
}  // namespace tdx
