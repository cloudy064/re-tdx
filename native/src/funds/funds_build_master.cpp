#include "funds_internal.hpp"

namespace tdx {
Json IntradayFundsService::build_master(const FetchResult& master,
                                        const IntradayFundsQuery& options) {
    using namespace detail::funds;
    const auto raw_rows = response_rows(master.document);
    std::map<std::string, std::string> names;
    for (const auto& [code, industry] : industries_) names[code] = industry.name;
    auto rows = normalized_rows(raw_rows, securities_, false, names);
    Json industries = Json::array();
    for (const auto& row : rows.as_array())
        if (row.at("code").as_string().rfind("881", 0) == 0) industries.push_back(row);
    Json counts = Json::object(); counts["market_rows"] = static_cast<std::uint64_t>(rows.size());
    counts["available_industries"] = static_cast<std::uint64_t>(industries.size());
    Json result = Json::object(); result["schema"] = "tdx-intraday-funds-native-v1";
    result["generated_at"] = now_text(); result["mode"] = "master"; result["source"] = source_document();
    result["cache"] = intraday_funds_cache_document(master, nullptr, options.cache_ttl_seconds);
    result["availability"] = master.stale ? "stale-cache" : "live"; result["counts"] = std::move(counts);
    result["market_rows"] = std::move(rows); result["industries"] = std::move(industries); return result;
}
Json IntradayFundsService::build_industry(const std::string& code,
    const FetchResult& master, const FetchResult& detail, const IntradayFundsQuery& options) {
    using namespace detail::funds; const auto raw_master = response_rows(master.document);
    const auto* master_row = find_row(raw_master, "1", code);
    if (!master_row) throw Error("industry is not returned by ReqId 200340: " + code);
    std::map<std::string, std::string> names;
    for (const auto& [item_code, industry] : industries_) names[item_code] = industry.name;
    auto summary = normalize_intraday_fund_record(*master_row, securities_, false, names);
    auto components = normalized_rows(response_rows(detail.document), securities_, true, names);
    Json industry = Json::object(); industry["market"] = "1"; industry["code"] = code;
    industry["name"] = summary.at("name"); industry["summary"] = std::move(summary);
    industry["component_count"] = static_cast<std::uint64_t>(components.size()); industry["components"] = std::move(components);
    Json counts = Json::object(); counts["available_industries"] = static_cast<std::uint64_t>(industry_keys(raw_master).size());
    counts["component_records"] = industry.at("component_count"); Json result = Json::object();
    result["schema"] = "tdx-intraday-funds-native-v1"; result["generated_at"] = now_text(); result["mode"] = "industry";
    result["source"] = source_document(); result["cache"] = intraday_funds_cache_document(master, &detail, options.cache_ttl_seconds);
    result["availability"] = master.stale || detail.stale ? "stale-cache" : "live";
    result["counts"] = std::move(counts); result["industry"] = std::move(industry); return result;
}
}  // namespace tdx
