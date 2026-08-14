#include "special_situations_internal.hpp"

namespace tdx::special_situations_detail {

bool view_matches_kind(const std::string& view, const std::string& kind) {
    if (view == "all") return true;
    if (view == "legacy")
        return kind == "merger" || kind == "b-to-h" ||
               kind == "market-cap-risk";
    if (view == "mergers") return kind == "merger";
    if (view == "b-to-h") return kind == "b-to-h";
    if (view == "market-cap-risk") return kind == "market-cap-risk";
    if (view == "corporate-actions")
        return kind == "major-restructuring-plan" ||
               kind == "major-restructuring-review" ||
               kind == "major-restructuring-completed" ||
               kind == "ordinary-merger-plan";
    if (view == "neeq-transfers")
        return kind == "neeq-transfer-plan" ||
               kind == "neeq-transfer-completed";
    if (view == "neeq-regulation") return kind == "neeq-regulation";
    return false;
}

}  // namespace tdx::special_situations_detail

namespace tdx {
using namespace special_situations_detail;

Json SpecialSituationService::query(const SpecialSituationQuery& options) {
    const std::set<std::string> views{
        "all", "legacy", "mergers", "b-to-h", "market-cap-risk",
        "corporate-actions", "neeq-transfers", "neeq-regulation"};
    if (!views.count(options.view))
        throw Error("view must be all, legacy, mergers, b-to-h, "
                    "market-cap-risk, corporate-actions, neeq-transfers, "
                    "or neeq-regulation");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = parsed_market(options.market);
        if (selected_market < 0)
            throw Error("market must be sz/sh/bj or 0/1/2");
        if (!digits(options.code)) throw Error("code must contain six digits");
    }

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json rows = Json::array();
    for (const auto& row : master.at("rows").as_array()) {
        if (!view_matches_kind(options.view, text_value(row, "kind"))) continue;
        if (selected_market >= 0) {
            bool matched = false;
            for (const auto* key : {"primary_security", "related_security"}) {
                const auto* security = field(row, key);
                if (security && !security->is_null() &&
                    static_cast<int>(security->at("market_id").as_number()) == selected_market &&
                    security->at("code").as_string() == options.code) matched = true;
            }
            if (!matched) continue;
        }
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        rows.push_back(row);
    }

    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_kind = text_value(left, "kind");
            const auto right_kind = text_value(right, "kind");
            if (left_kind == "market-cap-risk" && right_kind == left_kind) {
                const auto left_breaches = number_value(left, "breach_count").value_or(0);
                const auto right_breaches = number_value(right, "breach_count").value_or(0);
                if (left_breaches != right_breaches) return left_breaches > right_breaches;
                const auto left_drop = std::min(
                    number_value(left, "twenty_day_change_pct").value_or(0),
                    number_value(left, "high_to_current_change_pct").value_or(0));
                const auto right_drop = std::min(
                    number_value(right, "twenty_day_change_pct").value_or(0),
                    number_value(right, "high_to_current_change_pct").value_or(0));
                if (left_drop != right_drop) return left_drop < right_drop;
            }
            const auto left_date = text_value(left, "date");
            const auto right_date = text_value(right, "date");
            if (left_kind == right_kind && left_date != right_date)
                return left_date > right_date;
            if (left_kind != right_kind) return left_kind < right_kind;
            const auto* left_active = field(left, "active");
            const auto* right_active = field(right, "active");
            if (left_active && right_active && left_active->is_bool() && right_active->is_bool() &&
                left_active->as_bool() != right_active->as_bool()) return left_active->as_bool();
            return text_value(left, "event_id") < text_value(right, "event_id");
        });

    std::uint64_t mergers = 0, b_to_h = 0, risks = 0, active_mergers = 0;
    std::uint64_t major_plans = 0, major_reviews = 0, major_completed = 0;
    std::uint64_t ordinary_plans = 0, neeq_plans = 0, neeq_regulation = 0;
    std::uint64_t neeq_completed = 0;
    std::uint64_t twenty_day = 0, one_year = 0, both = 0;
    std::set<std::string> risk_securities;
    std::set<std::string> all_securities;
    for (const auto& row : rows.as_array()) {
        const auto kind = text_value(row, "kind");
        if (kind == "merger") ++mergers;
        else if (kind == "b-to-h") ++b_to_h;
        else if (kind == "market-cap-risk") {
            ++risks;
            risk_securities.insert(row.at("primary_security").at("security_id").as_string());
            const bool t = row.at("twenty_day_triggered").as_bool();
            const bool y = row.at("one_year_triggered").as_bool();
            if (t && y) ++both;
            else if (t) ++twenty_day;
            else if (y) ++one_year;
        }
        else if (kind == "major-restructuring-plan") ++major_plans;
        else if (kind == "major-restructuring-review") ++major_reviews;
        else if (kind == "major-restructuring-completed") ++major_completed;
        else if (kind == "ordinary-merger-plan") ++ordinary_plans;
        else if (kind == "neeq-transfer-plan") ++neeq_plans;
        else if (kind == "neeq-regulation") ++neeq_regulation;
        else if (kind == "neeq-transfer-completed") ++neeq_completed;
        if ((kind == "merger" || kind == "b-to-h") &&
            row.at("active").as_bool()) ++active_mergers;
        all_securities.insert(row.at("primary_security").at("security_id").as_string());
        if (!row.at("related_security").is_null())
            all_securities.insert(row.at("related_security").at("security_id").as_string());
    }
    const auto matched = rows.size();
    while (static_cast<int>(rows.size()) > options.limit)
        rows.as_array().pop_back();

    Json quote_errors = Json::array();
    Json quote_source = Json(nullptr);
    bool quote_refreshed = false;
    int quote_age_seconds = 0;
    if (options.include_quotes && rows.size()) {
        std::vector<std::string> requested;
        for (const auto& row : rows.as_array()) {
            for (const auto* key : {"primary_security", "related_security"}) {
                const auto& security = row.at(key);
                if (security.is_null()) continue;
                requested.push_back(security.at("market").as_string() + ":" +
                                    security.at("code").as_string());
            }
        }
        try {
            const auto quotes = fetch_quotes(
                requested, options, quote_refreshed, quote_age_seconds);
            apply_special_situation_quotes(rows, quotes.at("records"));
            quote_source = Json::object();
            for (const auto* key : {"endpoint", "server_name", "requested", "received"})
                quote_source[key] = quotes.at(key);
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = "public-l1-snapshot";
            failure["message"] = error.what();
            quote_errors.push_back(std::move(failure));
        }
    }

    Json summary = Json::object();
    summary["mergers"] = mergers;
    summary["b_to_h"] = b_to_h;
    summary["active_merger_events"] = active_mergers;
    summary["market_cap_warnings"] = risks;
    summary["market_cap_unique_securities"] =
        static_cast<std::uint64_t>(risk_securities.size());
    summary["twenty_day_only"] = twenty_day;
    summary["one_year_only"] = one_year;
    summary["both_triggers"] = both;
    summary["major_restructuring_plans"] = major_plans;
    summary["major_restructuring_reviews"] = major_reviews;
    summary["major_restructuring_completed"] = major_completed;
    summary["ordinary_merger_plans"] = ordinary_plans;
    summary["neeq_transfer_plans"] = neeq_plans;
    summary["neeq_regulation_events"] = neeq_regulation;
    summary["neeq_transfer_completed"] = neeq_completed;
    summary["unique_related_securities"] =
        static_cast<std::uint64_t>(all_securities.size());

    Json result = Json::object();
    result["schema"] = "tdx-market-special-situations-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = options.code.empty() ? "catalog" : "security";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["records"] = std::move(rows);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["quote_source"] = std::move(quote_source);
    result["quote_errors"] = std::move(quote_errors);
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    cache["quote_refreshed"] = quote_refreshed;
    cache["quote_age_seconds"] = quote_age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
