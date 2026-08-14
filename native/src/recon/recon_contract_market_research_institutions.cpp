#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

namespace {

using ResearchInstitutionContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_active_lhb_ranking(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool security_mode = contract_id == "active-lhb-security-live";
    const auto expected_period = security_mode ? "half-year" : "5d";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-active-lhb-native-v1"),
                  "tdx-market-active-lhb-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "mode",
                  string_is(member(document, "mode"),
                            security_mode ? "security" : "ranking"),
                  security_mode ? "security" : "ranking",
                  value_or_null(member(document, "mode")));
    add_assertion(result, "period",
                  string_is(member(document, "period"), expected_period),
                  expected_period, value_or_null(member(document, "period")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available,
                  "live or stale-cache", value_or_null(availability));

    const auto* period_summaries = member(document, "period_summaries");
    const bool three_periods = period_summaries && period_summaries->is_object() &&
        member(*period_summaries, "5d") && member(*period_summaries, "month") &&
        member(*period_summaries, "half-year");
    add_assertion(result, "three_period_summaries", three_periods,
                  "5d, month and half-year summaries", three_periods);

    const auto* rankings = member(document, "rankings");
    const Json* first = rankings && rankings->is_array() && rankings->size()
        ? &rankings->as_array()[0] : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* event_count = first ? member(*first, "event_count") : nullptr;
    const auto* buy = first ? member(*first, "buy_amount_yuan") : nullptr;
    const auto* sell = first ? member(*first, "sell_amount_yuan") : nullptr;
    const auto* net = first ? member(*first, "net_buy_amount_yuan") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    const bool typed_ranking = security && security->is_object() &&
        member(*security, "code") && member(*security, "code")->is_string() &&
        member(*security, "code")->as_string().size() == 6 && event_count &&
        event_count->is_number() && event_count->as_number() > 0 &&
        buy && buy->is_number() && sell && sell->is_number() && net &&
        net->is_number() && raw && raw->is_object();
    add_assertion(result, "typed_ranking", typed_ranking,
                  "security, event count, amount metrics and raw evidence",
                  value_or_null(first));

    if (!security_mode) {
        bool sorted = true;
        if (rankings && rankings->is_array()) {
            for (std::size_t i = 1; i < rankings->size(); ++i) {
                const auto* previous = member(rankings->as_array()[i - 1], "event_count");
                const auto* current = member(rankings->as_array()[i], "event_count");
                sorted = sorted && previous && current && previous->is_number() &&
                    current->is_number() && previous->as_number() >= current->as_number();
            }
        }
        add_assertion(result, "descending_event_count", sorted, true, sorted);
    } else {
        const auto* selected = member(document, "selected_ranking");
        const auto* selected_security = selected && selected->is_object()
            ? member(*selected, "security") : nullptr;
        const bool selected_identity = selected_security &&
            string_is(member(*selected_security, "market"), "sh") &&
            string_is(member(*selected_security, "code"), "603459") &&
            string_is(member(*selected, "unit_id"), "14001");
        add_assertion(result, "selected_security", selected_identity,
                      "SH603459 in unit 14001", value_or_null(selected));
        const auto* events = member(document, "events");
        const Json* first_event = events && events->is_array() && events->size()
            ? &events->as_array()[0] : nullptr;
        const auto* event_date = first_event ? member(*first_event, "event_date") : nullptr;
        const auto* event_type = first_event ? member(*first_event, "event_type") : nullptr;
        const auto* change = first_event ? member(*first_event, "change_pct") : nullptr;
        const auto* turnover = first_event ? member(*first_event, "turnover_rate_pct") : nullptr;
        const auto* event_raw = first_event ? member(*first_event, "raw") : nullptr;
        const bool typed_event = event_date && event_date->is_string() &&
            event_date->as_string().size() == 10 && event_type && event_type->is_string() &&
            !event_type->as_string().empty() && change &&
            (change->is_number() || change->is_null()) && turnover &&
            (turnover->is_number() || turnover->is_null()) && event_raw &&
            event_raw->is_object();
        add_assertion(result, "typed_event", typed_event,
                      "ISO date, reason, change, turnover and raw evidence",
                      value_or_null(first_event));
        bool sorted = true;
        if (events && events->is_array()) {
            for (std::size_t i = 1; i < events->size(); ++i) {
                const auto* previous = member(events->as_array()[i - 1], "event_date");
                const auto* current = member(events->as_array()[i], "event_date");
                sorted = sorted && previous && current && previous->is_string() &&
                    current->is_string() && previous->as_string() >= current->as_string();
            }
        }
        add_assertion(result, "descending_event_date", sorted, true, sorted);
    }

    const auto* sources = member(document, "sources");
    const auto expected_source_count = security_mode ? 4u : 3u;
    const bool healthy_sources = sources && sources->is_array() &&
        sources->size() == expected_source_count &&
        source_exists(document, "list/func_hylhb101_1.jsn") &&
        source_exists(document, "list/func_hylhb102_1.jsn") &&
        source_exists(document, "list/func_hylhb104_1.jsn") &&
        (!security_mode || source_exists(document, "hylhb14001/1603459.jsn"));
    add_assertion(result, "exact_sources", healthy_sources,
                  security_mode ? "three masters plus hylhb14001/1603459.jsn"
                                : "three HYLHB master resources",
                  healthy_sources);
}

void validate_state_owned_groups(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool groups_view = contract_id == "state-owned-groups-live";
    const bool group_view = contract_id == "state-owned-group-detail-live";
    const auto expected_view = groups_view ? "groups" : group_view ? "group" : "restructuring";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-state-owned-reform-native-v1"),
                  "tdx-market-state-owned-reform-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available,
                  "live or stale-cache", value_or_null(availability));
    const auto* group_count = member_path(document, {"summary", "group_count"});
    const auto* relationships = member_path(document, {"summary", "group_relationships"});
    const auto* unique = member_path(document, {"summary", "unique_grouped_securities"});
    const auto* mismatches = member_path(document, {"summary", "member_count_mismatches"});
    const auto* restructuring_count = member_path(document, {"summary", "restructuring_rows"});
    const bool coverage = group_count && group_count->is_number() &&
        group_count->as_number() >= 100 && relationships && relationships->is_number() &&
        relationships->as_number() >= 1800 && unique && unique->is_number() &&
        unique->as_number() >= 800 && mismatches && mismatches->is_number() &&
        mismatches->as_number() == 0 && restructuring_count &&
        restructuring_count->is_number() && restructuring_count->as_number() >= 40;
    add_assertion(result, "relationship_coverage", coverage,
                  "100+ groups, 1800+ relations, 800+ securities, zero count mismatches",
                  coverage);

    if (groups_view) {
        add_assertion(result, "dimension",
                      string_is(member(document, "dimension"), "integration"),
                      "integration", value_or_null(member(document, "dimension")));
        const auto* groups = member(document, "groups");
        const Json* first = groups && groups->is_array() && groups->size()
            ? &groups->as_array()[0] : nullptr;
        const auto* members = first ? member(*first, "members") : nullptr;
        const auto* count = first ? member(*first, "member_count") : nullptr;
        const bool typed = first && member(*first, "group_id") &&
            member(*first, "group_id")->is_string() && member(*first, "name") &&
            member(*first, "name")->is_string() && count && count->is_number() &&
            members && members->is_array() && members->size() ==
                static_cast<std::size_t>(count->as_number());
        add_assertion(result, "typed_group_members", typed,
                      "group identity and complete member array", value_or_null(first));
        bool sorted = true;
        if (groups && groups->is_array()) {
            for (std::size_t i = 1; i < groups->size(); ++i) {
                const auto* previous = member(groups->as_array()[i - 1], "member_count");
                const auto* current = member(groups->as_array()[i], "member_count");
                sorted = sorted && previous && current && previous->is_number() &&
                    current->is_number() && previous->as_number() >= current->as_number();
            }
        }
        add_assertion(result, "descending_member_count", sorted, true, sorted);
    } else if (group_view) {
        const auto* groups = member(document, "groups");
        const Json* selected = groups && groups->is_array() && groups->size() == 1
            ? &groups->as_array()[0] : nullptr;
        add_assertion(result, "selected_group",
                      selected && string_is(member(*selected, "group_id"), "gl197") &&
                          string_is(member(*selected, "name"), "央企整合"),
                      "gl197 央企整合", value_or_null(selected));
        const auto* details = member(document, "details");
        const Json* first = details && details->is_array() && details->size() >= 30
            ? &details->as_array()[0] : nullptr;
        const auto* security = first ? member(*first, "security") : nullptr;
        const auto* controller = first ? member(*first, "actual_controller") : nullptr;
        const auto* logic = first ? member(*first, "logic") : nullptr;
        const auto* returns = first ? member(*first, "returns_pct") : nullptr;
        const bool typed = security && security->is_object() &&
            member(*security, "code") && member(*security, "code")->is_string() &&
            member(*security, "code")->as_string().size() == 6 && controller &&
            controller->is_string() && !controller->as_string().empty() && logic &&
            logic->is_string() && !logic->as_string().empty() && returns &&
            returns->is_object();
        add_assertion(result, "typed_group_details", typed,
                      "30+ securities with controller, logic and returns", value_or_null(first));
        const auto* command = member_path(document, {"quote_source", "command"});
        const auto* received = member_path(document, {"quote_source", "received"});
        const bool quotes = string_is(command, "0x054C") && received &&
            received->is_number() && details &&
            received->as_number() == static_cast<double>(details->size());
        add_assertion(result, "quote_join", quotes,
                      "0x054C and one quote per detail", value_or_null(member(document, "quote_source")));
    } else {
        const auto* records = member(document, "restructuring");
        const Json* first = records && records->is_array() && records->size()
            ? &records->as_array()[0] : nullptr;
        const auto* security = first ? member(*first, "security") : nullptr;
        const auto* operation = first ? member(*first, "capital_operation") : nullptr;
        const auto* explanation = first ? member(*first, "explanation") : nullptr;
        const auto* raw = first ? member(*first, "raw") : nullptr;
        const bool typed = security && security->is_object() &&
            member(*security, "code") && member(*security, "code")->is_string() &&
            member(*security, "code")->as_string().size() == 6 && operation &&
            operation->is_string() && explanation && explanation->is_string() &&
            !explanation->as_string().empty() && raw && raw->is_object();
        add_assertion(result, "typed_restructuring", typed,
                      "security, capital operation, explanation and raw evidence",
                      value_or_null(first));
        bool sorted = true;
        if (records && records->is_array()) {
            for (std::size_t i = 1; i < records->size(); ++i) {
                const auto* previous = member(records->as_array()[i - 1], "as_of_date");
                const auto* current = member(records->as_array()[i], "as_of_date");
                const auto ps = previous && previous->is_string() ? previous->as_string() : "";
                const auto cs = current && current->is_string() ? current->as_string() : "";
                sorted = sorted && ps >= cs;
            }
        }
        add_assertion(result, "descending_date", sorted, true, sorted);
    }

    const auto* sources = member(document, "sources");
    const auto expected_count = group_view ? 6u : 5u;
    const bool exact_sources = sources && sources->is_array() &&
        sources->size() == expected_count &&
        source_exists(document, "list/func_gqgg101_1.jsn") &&
        source_exists(document, "list/func_gqgg102_1.jsn") &&
        source_exists(document, "list/func_gqgg103_1.jsn") &&
        source_exists(document, "list/func_gqgg104_1.jsn") &&
        source_exists(document, "list/func_gqgg106_1.jsn") &&
        (!group_view || source_exists(document, "gqgg/gl197.jsn"));
    add_assertion(result, "exact_sources", exact_sources,
                  group_view ? "five masters plus gqgg/gl197.jsn" : "five GQGG masters",
                  exact_sources);
}

struct ResearchInstitutionContract {
    std::string_view id;
    ResearchInstitutionContractValidator validate;
};

constexpr std::array<ResearchInstitutionContract, 5> research_institution_contracts{{
    {"active-lhb-ranking-live", validate_active_lhb_ranking},
    {"active-lhb-security-live", validate_active_lhb_ranking},
    {"state-owned-groups-live", validate_state_owned_groups},
    {"state-owned-group-detail-live", validate_state_owned_groups},
    {"state-owned-restructuring-live", validate_state_owned_groups},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < research_institution_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < research_institution_contracts.size(); ++right)
            if (research_institution_contracts[left].id == research_institution_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_research_institution_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : research_institution_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail