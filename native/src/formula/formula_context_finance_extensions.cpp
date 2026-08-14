#include "formula_context_finance_extensions_internal.hpp"

#include "formula_context_finance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/finance_eligibility.hpp"
#include "tdx/finance_events.hpp"
#include "tdx/professional_data.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace tdx::formula_context_detail {

void bind_formula_finance_extensions(
    Json& context, Json& finance_values, Json& professional_finance_values,
    const std::filesystem::path& root, const std::string& market,
    const std::string& code, const Json* kline_document,
    const FormulaContextPlan& plan, bool point_in_time_finance,
    int type, int timeout_ms) {
    const auto& finance_bindings = plan.finance_bindings;
    const auto& professional_finance_bindings =
        plan.professional_finance_bindings;
    if (includes_any(finance_bindings, {48, 52})) {
        std::set<int> requested;
        for (const int id : {48, 52}) if (finance_bindings.count(id)) requested.insert(id);
        const auto resource = cached_finance_eligibility_resource(root);
        const auto values = finance_eligibility_values(*resource, market, code, requested);
        Json metadata = Json::object();
        for (const auto& [id, value] : values) {
            finance_values[std::to_string(id)] = value;
            metadata[std::to_string(id)] = value;
        }
        context["finance_eligibility_mode"] =
            "tdx-spblock-finance48-connect-union-finance52-margin-membership";
        context["finance_eligibility_source"] = resource->source_path;
        context["finance_eligibility_values"] = std::move(metadata);
        context["finance_eligibility_margin_count"] =
            static_cast<std::uint64_t>(resource->margin_financing.size());
        context["finance_eligibility_stock_connect_count"] =
            static_cast<std::uint64_t>(resource->sh_stock_connect.size() +
                                       resource->sz_stock_connect.size());
    }
    if (includes_any(finance_bindings, {88, 90, 91})) {
        std::set<int> requested;
        for (const int id : {88, 90, 91}) if (finance_bindings.count(id)) requested.insert(id);
        const auto today = finance_event_today();
        const auto events = fetch_finance_event_values(market, code, requested, timeout_ms);
        Json metadata = Json::object();
        for (const auto& [id, event] : events) {
            finance_values[std::to_string(id)] = event.days;
            Json item = Json::object();
            item["days"] = event.days;
            item["event_date"] = event.event_date;
            item["resource"] = event.resource;
            metadata[std::to_string(id)] = std::move(item);
        }
        context["finance_event_today"] = today;
        context["finance_event_mode"] =
            "tdx-tipinfo-northbound-and-plan-announcement-inclusive-natural-day-zero-when-absent";
        context["finance_event_values"] = std::move(metadata);
    }
    const bool finance_growth_context = includes_any(finance_bindings, {43, 44});
    if (point_in_time_finance && (finance_growth_context || !professional_finance_bindings.empty())) {
        if (type == 0)
            throw Error("point-in-time professional finance is unavailable for index securities");
        formula_context_detail::bind_point_in_time_finance_context(
            context, root, market, code, *kline_document, finance_bindings,
            professional_finance_bindings, timeout_ms);
    } else if ((finance_growth_context || !professional_finance_bindings.empty()) && type != 0) {
        const int market_id = lower_ascii(market) == "sh" || market == "1" ? 1 :
                              lower_ascii(market) == "bj" || market == "2" ? 2 : 0;
        const auto data = fetch_latest_professional_finance_data_for_security(
            market_id, code, {}, timeout_ms, false);
        const auto found = data.records.find({market_id, code});
        if (found == data.records.end()) throw Error("professional finance context returned no security");
        if (finance_growth_context) {
            Json growth_values = Json::object();
            for (const int id : {43, 44}) if (finance_bindings.count(id)) {
                if (const auto value = professional_finance_growth_value(found->second, id)) {
                    finance_values[std::to_string(id)] = *value;
                    growth_values[std::to_string(id)] = *value;
                }
            }
            context["finance_growth_report_date"] = static_cast<std::uint64_t>(data.report_date);
            context["finance_growth_mode"] =
                "tdx-professional-fn183-fn184-latest-report-constant";
            context["finance_growth_values"] = std::move(growth_values);
        }
        for (const int id : professional_finance_bindings) {
            if (point_in_time_finance) continue;
            if (id == 0) professional_finance_values["0"] = static_cast<std::uint64_t>(data.report_date);
            else if (id > 0 && static_cast<std::size_t>(id) <= found->second.fields.size() &&
                     found->second.fields[static_cast<std::size_t>(id - 1)])
                professional_finance_values[std::to_string(id)] =
                    *found->second.fields[static_cast<std::size_t>(id - 1)];
        }
        if (!professional_finance_bindings.empty()) {
            context["professional_finance_report_date"] = static_cast<std::uint64_t>(data.report_date);
            context["professional_finance_mode"] = "latest-available-report-constant";
        }
    }
}

} // namespace tdx::formula_context_detail

