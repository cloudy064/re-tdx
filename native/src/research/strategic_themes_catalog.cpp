#include "strategic_themes_internal.hpp"

#include <algorithm>
#include <array>

namespace tdx::detail::strategic_themes {
namespace {

struct CategorySpec {
    const char* block_id;
    const char* name;
    const char* config_name;
    const char* resource;
    const char* name_field;
    const char* id_prefix;
    const char* description_field;
};

constexpr std::array<CategorySpec, 26> category_specs{{
    {"Z01", "国防军工", "func_gfjg101", "list/func_gfjg101_1.jsn", "gname", "", ""},
    {"Z02", "军工公司系", "func_gfjg102", "list/func_gfjg102_1.jsn", "gsx", "", "MS"},
    {"Z03", "健康中国", "func_jkzg101", "list/func_jkzg101_1.jsn", "gname", "", ""},
    {"Z04", "美丽中国", "func_mlzg101", "list/func_mlzg101_1.jsn", "gname", "", ""},
    {"Z10", "交通强国", "func_jtqg101", "list/func_jtqg101_1.jsn", "gname", "", ""},
    {"Z05", "工业4.0", "func_gy101", "list/func_gy101_1.jsn", "gname", "", ""},
    {"Z06", "汽车产业", "func_qccy101", "list/func_qccy101_1.jsn", "gname", "", ""},
    {"Z07", "人工智能", "func_rgzn101", "list/func_rgzn101_1.jsn", "gname", "", ""},
    {"Z08", "5G6G", "func_5G101", "list/func_5G101_1.jsn", "gname", "", ""},
    {"Z09", "新基建", "func_xjj101", "list/func_xjj101_1.jsn", "gname", "", ""},
    {"Z11", "大金融", "func_djr101", "list/func_djr101_1.jsn", "gname", "", ""},
    {"Z12", "非接触经济", "func_fjcjj101", "list/func_fjcjj101_1.jsn", "gname", "", ""},
    {"Z13", "半导体", "func_bdt101", "list/func_bdt101_1.jsn", "gname", "", ""},
    {"Z14", "碳中和", "func_tzh_1", "list/func_tzh_1.jsn", "gname", "", ""},
    {"Z16", "数字经济", "func_szjj101", "list/func_szjj101_1.jsn", "gname", "", ""},
    {"Z17", "算力产业", "func_slcy101", "list/func_slcy101_1.jsn", "gname", "", ""},
    {"Z18", "机器人", "func_jqr101", "list/func_jqr101_1.jsn", "gname", "", ""},
    {"Z19", "大消费", "func_dxf101", "list/func_dxf101_1.jsn", "gname", "", ""},
    {"Z20", "风光锂储", "func_fglc101", "list/func_fglc101_1.jsn", "gname", "", ""},
    {"Z23", "新材料", "func_xcl101", "list/func_xcl101_1.jsn", "gname", "", ""},
    {"Z24", "新型电力", "func_xxdl101", "list/func_xxdl101_1.jsn", "gname", "", ""},
    {"Z25", "国产软件", "func_gcrj101", "list/func_gcr101_1.jsn", "gname", "", ""},
    {"Z27", "大周期", "func_dzq101", "list/func_dzq101_1.jsn", "gname", "", ""},
    {"Z30", "农业安全", "func_nyaq101", "list/func_nyaq101_1.jsn", "gname", "", ""},
    {"Z31", "地产链", "func_dcl101", "list/func_dcl101_1.jsn", "gname", "", ""},
    {"HLW", "互联网+", "func_hlw101", "list/func_hlw101_1.jsn", "gname", "HLW:", ""},
}};

constexpr std::array<ViewSpec, 5> views{{
    {"catalog", ViewKind::catalog, false, false},
    {"categories", ViewKind::categories, false, false},
    {"themes", ViewKind::themes, false, false},
    {"theme", ViewKind::theme, true, false},
    {"security", ViewKind::security, false, true},
}};

constexpr std::array<SortSpec, 3> sorts{{
    {"name", SortKind::name},
    {"members", SortKind::members},
    {"id", SortKind::id},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(category_specs,
    [](const CategorySpec& value) { return std::string_view(value.block_id); }));
static_assert(unique_by(category_specs,
    [](const CategorySpec& value) { return std::string_view(value.config_name); }));
static_assert(unique_by(category_specs,
    [](const CategorySpec& value) { return std::string_view(value.resource); }));
static_assert(unique_by(views,
    [](const ViewSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(sorts,
    [](const SortSpec& value) { return std::string_view(value.id); }));

}  // namespace

const ViewSpec& view_spec(std::string_view value) {
    const auto normalized = lower_ascii(trim(std::string(value)));
    const auto found = std::find_if(views.begin(), views.end(),
        [&](const ViewSpec& entry) { return entry.id == normalized; });
    if (found == views.end())
        throw Error("view must be catalog, categories, themes, theme, or security");
    return *found;
}

const SortSpec& sort_spec(std::string_view value) {
    const auto normalized = lower_ascii(trim(std::string(value)));
    const auto found = std::find_if(sorts.begin(), sorts.end(),
        [&](const SortSpec& entry) { return entry.id == normalized; });
    if (found == sorts.end())
        throw Error("strategic theme sort must be name, members, or id");
    return *found;
}

void sort_themes(Json& rows, const SortSpec& sort, std::string_view order_value) {
    const auto order = lower_ascii(trim(std::string(order_value)));
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort.kind == SortKind::members) {
                const auto a = left.at("master_member_count").as_number();
                const auto b = right.at("master_member_count").as_number();
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto key = sort.kind == SortKind::name ? "name" : "theme_id";
                const auto& a = left.at(key).as_string();
                const auto& b = right.at(key).as_string();
                if (a != b) return descending ? a > b : a < b;
            }
            return left.at("theme_id").as_string() <
                   right.at("theme_id").as_string();
        });
}

}  // namespace tdx::detail::strategic_themes

namespace tdx {

const std::vector<StrategicThemeCategory>& strategic_theme_categories() {
    using namespace detail::strategic_themes;
    static const std::vector<StrategicThemeCategory> categories = [] {
        std::vector<StrategicThemeCategory> result;
        result.reserve(category_specs.size());
        for (const auto& value : category_specs)
            result.emplace_back(value.block_id, value.name, value.config_name,
                value.resource, value.name_field, value.id_prefix,
                value.description_field);
        return result;
    }();
    return categories;
}

}  // namespace tdx
