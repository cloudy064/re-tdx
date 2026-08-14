#include "funds_internal.hpp"

#include <algorithm>

namespace tdx {
Json IntradayFundsService::build_security(int market_id, const std::string& code,
    const ResearchIndustry& industry_ref, const FetchResult& master,
    const FetchResult& detail, const IntradayFundsQuery& options) {
    using namespace detail::funds; auto industry = build_industry(industry_ref.code, master, detail, options);
    const auto& components = industry.at("industry").at("components").as_array(); Json funds = nullptr;
    for (const auto& component : components) if (component.at("market_id").as_number() == market_id && component.at("code").as_string() == code) { funds = component; break; }
    Json result = Json::object(); result["schema"] = "tdx-intraday-funds-native-v1"; result["generated_at"] = now_text();
    result["mode"] = "security"; result["source"] = source_document(); result["cache"] = industry.at("cache");
    result["availability"] = industry.at("availability"); result["security"] = security_reference(market_id, code, securities_);
    Json selected = Json::object(); selected["market"] = industry_ref.market; selected["code"] = industry_ref.code;
    selected["name"] = industry_ref.name; selected["summary"] = industry.at("industry").at("summary");
    selected["component_count"] = industry.at("industry").at("component_count"); result["industry"] = std::move(selected);
    result["found"] = !funds.is_null(); result["funds"] = std::move(funds); return result;
}
Json IntradayFundsService::build_all(const FetchResult& master, const IntradayFundsQuery& options) {
    using namespace detail::funds; const auto master_rows = response_rows(master.document); Json industries = Json::array();
    std::map<std::pair<int, std::string>, Json> reverse; std::uint64_t component_count = 0;
    bool any_refreshed = master.refreshed, any_stale = master.stale; int oldest_age = master.age_seconds; Json errors = Json::array();
    if (master.stale) { Json error = Json::object(); error["scope"] = "master"; error["message"] = master.upstream_error; errors.push_back(std::move(error)); }
    for (const auto& [market, code] : industry_keys(master_rows)) {
        const auto detail = fetch_detail(market, code, options); any_refreshed = any_refreshed || detail.refreshed;
        any_stale = any_stale || detail.stale; oldest_age = std::max(oldest_age, detail.age_seconds);
        if (detail.stale) { Json error = Json::object(); error["scope"] = "detail"; error["market"] = market;
            error["code"] = code; error["message"] = detail.upstream_error; errors.push_back(std::move(error)); }
        auto document = build_industry(code, master, detail, options); auto industry = document.at("industry");
        const auto industry_name = industry.at("name").as_string();
        for (std::size_t index = 0; index < industry.at("components").size(); ++index) {
            const auto& component = industry.at("components").as_array()[index];
            const int member_market = static_cast<int>(component.at("market_id").as_number()); const auto member_code = component.at("code").as_string();
            auto [found, inserted] = reverse.emplace(std::make_pair(member_market, member_code), Json::object());
            if (inserted) { found->second = security_reference(member_market, member_code, securities_); found->second["matches"] = Json::array(); }
            Json match = Json::object(); match["industry_market"] = market; match["industry_code"] = code;
            match["industry_name"] = industry_name; match["component_index"] = static_cast<std::uint64_t>(index); found->second["matches"].push_back(std::move(match));
        }
        component_count += static_cast<std::uint64_t>(industry.at("component_count").as_number()); industries.push_back(std::move(industry));
    }
    Json securities = Json::array(); std::uint64_t multi = 0;
    for (auto& [key, security] : reverse) { if (security.at("matches").size() > 1) ++multi; securities.push_back(std::move(security)); }
    Json counts = Json::object(); counts["market_rows"] = static_cast<std::uint64_t>(master_rows.size());
    counts["available_industries"] = static_cast<std::uint64_t>(industry_keys(master_rows).size()); counts["expanded_industries"] = static_cast<std::uint64_t>(industries.size());
    counts["component_records"] = component_count; counts["securities"] = static_cast<std::uint64_t>(securities.size()); counts["multi_industry_securities"] = multi;
    Json cache = Json::object(); cache["ttl_seconds"] = options.cache_ttl_seconds; cache["hit"] = !any_refreshed || any_stale;
    cache["stale"] = any_stale; cache["any_refreshed"] = any_refreshed; cache["oldest_age_seconds"] = oldest_age; cache["upstream_errors"] = std::move(errors);
    Json result = Json::object(); result["schema"] = "tdx-intraday-funds-native-v1"; result["generated_at"] = now_text(); result["mode"] = "all-industries";
    result["source"] = source_document(); result["cache"] = std::move(cache); result["availability"] = any_stale ? "stale-cache" : "live";
    result["counts"] = std::move(counts); result["market_rows"] = normalized_rows(master_rows, securities_, false, {});
    result["industries"] = std::move(industries); result["securities"] = std::move(securities); return result;
}
}  // namespace tdx
