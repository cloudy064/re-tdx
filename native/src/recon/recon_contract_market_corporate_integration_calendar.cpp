#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tdx::recon_contract_detail {

void validate_calendar_expanded_contract(const Json& document, const Json&,
                                         Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-calendar-native-v1"),
                  "tdx-market-calendar-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_mode",
                  string_is(member(document, "view"), "all") &&
                      string_is(member(document, "mode"), "security") &&
                      string_is(member(document, "availability"), "live"),
                  "all/security/live", value_or_null(member(document, "mode")));
    const auto* macro = member_path(document, {"summary", "macro"});
    const auto* meetings = member_path(document, {"summary", "meeting"});
    const auto* company = member_path(document, {"summary", "company"});
    const auto* listings = member_path(document, {"summary", "listing"});
    const auto* major = member_path(document, {"summary", "major-event"});
    const auto* announcements = member_path(
        document, {"summary", "ipo-announcement"});
    const auto* recent = member_path(document, {"summary", "recent-ipo"});
    const auto* star = member_path(document, {"summary", "star-news"});
    const auto* chinext = member_path(document, {"summary", "chinext-news"});
    const auto* neeq = member_path(document, {"summary", "neeq-news"});
    const bool population = macro && macro->is_number() && macro->as_number() > 0 &&
        meetings && meetings->is_number() && meetings->as_number() > 0 &&
        company && company->is_number() && company->as_number() > 0 &&
        listings && listings->is_number() && listings->as_number() > 0 &&
        major && major->is_number() && major->as_number() > 0 &&
        announcements && announcements->is_number() && announcements->as_number() > 0 &&
        recent && recent->is_number() && recent->as_number() > 0 &&
        star && star->is_number() && star->as_number() > 0 &&
        chinext && chinext->is_number() && chinext->as_number() > 0 &&
        neeq && neeq->is_number() && neeq->as_number() > 0;
    add_assertion(result, "ten_family_population", population,
                  "all ten rolling calendar source families remain populated",
                  population);

    bool every_row_selected = true, auditable_rows = true;
    bool display_safe = true, optional_units = true;
    const auto* rows = member(document, "rows");
    if (rows && rows->is_array()) {
        for (const auto& row : rows->as_array()) {
            bool selected = false;
            const auto* security = member(row, "security");
            if (security && security->is_object() &&
                string_is(member(*security, "market"), "sh") &&
                string_is(member(*security, "code"), "688783")) selected = true;
            const auto* related = member(row, "related_securities");
            if (related && related->is_array())
                for (const auto& item : related->as_array())
                    selected = selected || (item.is_object() &&
                        string_is(member(item, "market"), "sh") &&
                        string_is(member(item, "code"), "688783"));
            every_row_selected = every_row_selected && selected;
            const auto* raw = member(row, "raw");
            auditable_rows = auditable_rows && nonempty_string(member(row, "kind")) &&
                raw && raw->is_object();
            const auto* content = member(row, "content");
            if (content && content->is_string())
                display_safe = display_safe &&
                    content->as_string().find('<') == std::string::npos;
            if (string_is(member(row, "kind"), "recent-ipo")) {
                const auto normalized = numeric_value(member(row, "raised_yuan"));
                const auto raw_raised = raw && raw->is_object()
                    ? numeric_value(member(*raw, "mjzj")) : std::nullopt;
                optional_units = optional_units && selected && normalized && raw_raised &&
                    std::abs(*normalized - *raw_raised) < 0.01 &&
                    *normalized >= 100000000.0 &&
                    member(row, "overfunding_yuan") &&
                    member(row, "overfunding_yuan")->is_number() &&
                    member(row, "snapshot_date") &&
                    member(row, "snapshot_date")->is_string();
            }
        }
    } else every_row_selected = false;
    const bool security_projection = rows && rows->is_array() &&
        !rows->as_array().empty() && every_row_selected && auditable_rows &&
        display_safe && optional_units;
    add_assertion(result, "security_projection",
                  security_projection,
                  "every returned row matches SH688783, remains auditable, sanitized and unit-safe",
                  security_projection);

    std::vector<std::string> required_sources{
        "list/func_cjrl101_1.jsn", "list/func_cjrl105_1.jsn",
        "list/func_ggrl101_1.jsn", "list/func_gsrl206_1.jsn",
        "list/func_dsjtx101_1.jsn", "list/func_xgrl101_1.jsn",
        "list/func_xgrl102_1.jsn", "list/func_xgrl103_1.jsn",
        "list/func_xgrl104_1.jsn", "list/func_xgrl105_1.jsn"};
    if (member_path(document, {"summary", "company_events"})) {
        required_sources.insert(required_sources.end(), {
            "list/func_gsrl201_1.jsn", "list/func_gsrl202_1.jsn",
            "list/func_gsrl203_1.jsn", "list/func_gsrl204_1.jsn",
            "list/func_gsrl205_1.jsn", "list/func_gsrl207_1.jsn",
            "list/func_gsrl208_1.jsn",
            "list/func_gsrl209_1.jsn"});
    }
    if (member_path(document, {"summary", "futures-calendar"}))
        required_sources.push_back("list/func_qhrl400_1.jsn");
    if (member_path(document, {"summary", "ipo-subscription"}))
        required_sources.push_back("list/func_zdgz_kcbsq103_1.jsn");
    if (member_path(document, {"summary", "ipo-review"}))
        required_sources.push_back("list/func_zdgz_kcbsq101_1.jsn");
    if (member_path(document, {"summary", "ipo-guidance"}))
        required_sources.push_back("list/func_zdgz_qzkcbd101_1.jsn");
    if (member_path(document, {"summary", "ipo-subscription-detail"}))
        required_sources.push_back("list/func_sbxg101_1.jsn");
    if (member_path(document, {"summary", "ipo-companion-news"}))
        required_sources.push_back("list/func_sbxg102_1.jsn");
    if (member_path(document, {"summary", "us-ipo-application"}))
        required_sources.push_back("list/func_mgrl101_1.jsn");
    if (member_path(document, {"summary", "us-ipo-calendar"}))
        required_sources.push_back("list/func_mgrl102_1.jsn");
    if (member_path(document, {"summary", "us-ipo-listed"}))
        required_sources.push_back("list/func_mgxg101_1.jsn");
    if (member_path(document, {"summary", "us-ipo-pending"}))
        required_sources.push_back("list/func_mgxg102_1.jsn");
    bool exact_sources = false, local_first = true;
    const auto* sources = member(document, "sources");
    if (sources && sources->is_array()) {
        exact_sources = sources->size() == required_sources.size();
        for (const auto& source : required_sources)
            exact_sources = exact_sources && source_exists(document, source);
        for (const auto& source : sources->as_array()) {
            const auto* endpoint = member(source, "endpoint");
            local_first = local_first && endpoint && endpoint->is_string() &&
                endpoint->as_string().rfind("local-jsn:", 0) == 0;
        }
    } else local_first = false;
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
    add_assertion(result, "local_first_boundary", local_first,
                  "normal API query reads the mirrored JSN files", local_first);
}

}  // namespace tdx::recon_contract_detail
