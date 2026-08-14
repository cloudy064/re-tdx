#include "disclosures_internal.hpp"

namespace tdx::disclosure_detail {
namespace {

Json normalize_mainland_schedule(const Json& rows,
                                 const SecurityDirectory& securities) {
    return normalize_disclosure_schedule_rows(rows, false, securities);
}

Json normalize_mainland_express(const Json& rows,
                                const SecurityDirectory& securities) {
    return normalize_disclosure_express_rows(rows, securities);
}

Json normalize_mainland_recent(const Json& rows,
                               const SecurityDirectory& securities) {
    return normalize_recent_disclosure_rows(rows, false, securities);
}

Json normalize_hong_kong_schedule(const Json& rows,
                                  const SecurityDirectory& securities) {
    return normalize_disclosure_schedule_rows(rows, true, securities);
}

Json normalize_hong_kong_recent(const Json& rows,
                                const SecurityDirectory& securities) {
    return normalize_recent_disclosure_rows(rows, true, securities);
}

constexpr std::array<DisclosureResourceDefinition, 5> resources{{
    {"list/func_cbpl103_1.jsn", "schedule", false, normalize_mainland_schedule},
    {"list/func_cbpl102_1.jsn", "express", false, normalize_mainland_express},
    {"list/func_cbpl104_1.jsn", "recent", false, normalize_mainland_recent},
    {"list/func_ggplsj101_1.jsn", "schedule", true, normalize_hong_kong_schedule},
    {"list/func_ggyjpl101_1.jsn", "recent", true, normalize_hong_kong_recent},
}};

constexpr bool unique_resources() {
    for (std::size_t left = 0; left < resources.size(); ++left)
        for (std::size_t right = left + 1; right < resources.size(); ++right)
            if (std::string_view(resources[left].resource) == resources[right].resource)
                return false;
    return true;
}

static_assert(unique_resources());

}  // namespace

const std::array<DisclosureResourceDefinition, 5>& disclosure_resources() {
    return resources;
}

const std::vector<std::string>& disclosure_resource_names() {
    static const auto names = [] {
        std::vector<std::string> result;
        result.reserve(resources.size());
        for (const auto& resource : resources) result.emplace_back(resource.resource);
        return result;
    }();
    return names;
}

}  // namespace tdx::disclosure_detail
