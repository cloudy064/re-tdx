#include "funds_internal.hpp"

namespace tdx::detail::funds {
namespace {
constexpr std::array<PeriodSpec, 7> period_values{{
    {"today", "今日", 1, 0},
    {"before-09:35", "9:35之前", 7, 5},
    {"before-10:30", "10:30之前", 2, 60},
    {"10:30-11:30", "10:30-11:30", 3, 60},
    {"13:00-14:00", "13:00-14:00", 4, 60},
    {"14:00-15:00", "14:00-15:00", 5, 60},
    {"last-30-minutes", "尾盘30分钟", 6, 30},
}};

template <typename Projection>
constexpr bool unique_by(Projection projection) {
    for (std::size_t left = 0; left < period_values.size(); ++left)
        for (std::size_t right = left + 1; right < period_values.size(); ++right)
            if (projection(period_values[left]) == projection(period_values[right]))
                return false;
    return true;
}
static_assert(unique_by(
    [](const PeriodSpec& value) { return std::string_view(value.key); }));
static_assert(unique_by([](const PeriodSpec& value) { return value.suffix; }));
}  // namespace

const std::array<PeriodSpec, 7>& periods() { return period_values; }

Json source_document() {
    Json source = Json::object();
    source["config"] = "T0002/cloud_cfg/sszjtj.xml";
    source["master_request_id"] = "200340";
    source["detail_request_id"] = "200341";
    source["module"] = "mod_peg.dll";
    Json items = Json::array();
    for (const auto& period : period_values) {
        Json item = Json::object();
        item["key"] = period.key;
        item["name"] = period.name;
        item["field_suffix"] = period.suffix;
        items.push_back(std::move(item));
    }
    source["periods"] = std::move(items);
    return source;
}
}  // namespace tdx::detail::funds
