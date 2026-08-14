#include "futures_issuance_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>

namespace tdx {

using namespace futures_issuance_detail;

FuturesIssuanceService::FuturesIssuanceService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json FuturesIssuanceService::fetch_resources(
    const std::vector<std::string>& resources,
    const FuturesIssuanceQuery& options,
    Json& cache_status) {
    const auto now = std::time(nullptr);
    std::vector<std::string> missing;
    for (const auto& resource : resources) {
        const auto cached = cache_.find(resource);
        if (options.refresh || cached == cache_.end() ||
            now - cached->second.fetched_at >= options.cache_ttl_seconds)
            missing.push_back(resource);
    }
    if (!missing.empty()) {
        const auto fetched = fetch_jsn_resources_rows(missing, "bi", options.timeout_ms);
        for (const auto& document : fetched.as_array()) {
            const auto resource = text_value(document, "resource");
            cache_[resource] = CachedResource{document, std::time(nullptr)};
        }
    }
    Json result = Json::array();
    for (const auto& resource : resources) {
        const auto found = cache_.find(resource);
        if (found == cache_.end()) throw Error("resource was not cached after fetch: " + resource);
        result.push_back(found->second.document);
    }
    cache_status["requested"] = static_cast<std::uint64_t>(resources.size());
    cache_status["refreshed"] = static_cast<std::uint64_t>(missing.size());
    cache_status["ttl_seconds"] = options.cache_ttl_seconds;
    return result;
}

Json FuturesIssuanceService::query(const FuturesIssuanceQuery& options) {
    if (!contains_value(sections, options.section))
        throw Error("section must be all, futures, ipo, placements, rights, or preferred-shares");
    if (!options.contract_key.empty() && !safe_key(options.contract_key, 3, 32))
        throw Error("contract_key contains unsupported characters");
    if (!options.year.empty() && !digits(options.year, 4))
        throw Error("year must contain four digits");
    if (!options.industry_key.empty() && !safe_key(options.industry_key, 5, 32))
        throw Error("industry_key contains unsupported characters");
    if (!contains_value(placement_statuses, options.placement_status))
        throw Error("placement_status must be all, locked, unlocked, active, stopped, implemented, or registered");
    const auto security_market = normalized_market_id(options.market);
    if (!options.code.empty() && !digits(options.code, 6))
        throw Error("code must contain six digits");
    if (options.code.empty() != security_market.empty())
        throw Error("market and code must be supplied together");
    if (options.search.size() > 120)
        throw Error("search must not exceed 120 bytes");
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    if (options.section == "ipo" && !options.contract_key.empty())
        throw Error("contract_key is only valid for futures/all section");
    if (options.section == "futures" && (!options.year.empty() || !options.industry_key.empty()))
        throw Error("year and industry_key are only valid for ipo/all section");
    if (options.section != "all" && options.section != "placements" &&
        options.section != "rights" && options.section != "preferred-shares" &&
        (options.placement_status != "all" || !options.market.empty() ||
         !options.code.empty() || !options.search.empty()))
        throw Error("security filters are only valid for placements/rights/preferred-shares/all section");
    if ((options.section == "rights" || options.section == "preferred-shares") &&
        options.placement_status != "all")
        throw Error("placement_status is unavailable in rights/preferred-shares sections");
    if ((options.section == "placements" || options.section == "rights" ||
         options.section == "preferred-shares") &&
        (!options.contract_key.empty() || !options.year.empty() || !options.industry_key.empty()))
        throw Error("futures/IPO filters are unavailable in issuance-detail sections");

    std::vector<std::string> masters;
    const bool include_futures = options.section == "all" || options.section == "futures";
    const bool include_ipo = options.section == "all" || options.section == "ipo";
    const bool include_placements = options.section == "all" || options.section == "placements";
    const bool include_rights = options.section == "all" || options.section == "rights";
    const bool include_preferred = options.section == "all" ||
        options.section == "preferred-shares";
    if (include_futures) append_resource_paths(masters, futures_resources);
    if (include_ipo) append_resource_paths(masters, ipo_resources);
    if (include_placements) append_resource_paths(masters, placement_resources);
    if (include_rights) append_resource_paths(masters, rights_resources);
    if (include_preferred) append_resource_paths(masters, preferred_share_resources);
    Json cache_status = Json::object();
    const auto documents = fetch_resources(masters, options, cache_status);
    Json sources = Json::array();
    for (const auto& document : documents.as_array()) sources.push_back(source_summary(document));

    Json result = Json::object();
    result["schema"] = "tdx-futures-issuance-native-v1";
    result["generated_at"] = now_text();
    result["section"] = options.section;
    result["commodity_futures"] = Json::array();
    result["monthly_futures"] = Json::array();
    result["index_futures"] = Json::array();
    result["ipo_annual"] = Json::array();
    result["listed_bond_issuers"] = Json::array();
    result["unlisted_bond_issuers"] = Json::array();
    result["private_placements"] = Json::array();
    result["placement_summary"] = private_placement_summary(Json::array());
    result["rights_offerings"] = Json::array();
    result["rights_summary"] = rights_offering_summary(Json::array());
    result["preferred_shares"] = Json::array();
    result["preferred_share_summary"] = preferred_share_summary(Json::array());
    Json counts = Json::object();
    if (include_futures) {
        auto commodity = normalize_futures_contract_rows(
            document_for(documents, futures_resources[0].resource).at("rows"));
        auto monthly = normalize_monthly_futures(
            document_for(documents, futures_resources[1].resource).at("rows"));
        auto indices = normalize_index_futures(
            document_for(documents, futures_resources[2].resource).at("rows"));
        counts["commodity_futures"] = static_cast<std::uint64_t>(commodity.size());
        counts["monthly_futures"] = static_cast<std::uint64_t>(monthly.size());
        counts["index_futures"] = static_cast<std::uint64_t>(indices.size());
        result["commodity_futures"] = limited(std::move(commodity), options.limit);
        result["monthly_futures"] = limited(std::move(monthly), options.limit);
        result["index_futures"] = limited(std::move(indices), options.limit);
    }
    if (include_ipo) {
        auto annual = normalize_ipo_annual(
            document_for(documents, ipo_resources[0].resource).at("rows"));
        auto listed = normalize_listed_issuers(
            document_for(documents, ipo_resources[1].resource).at("rows"), securities_);
        auto unlisted = normalize_unlisted_issuers(
            document_for(documents, ipo_resources[2].resource).at("rows"));
        counts["ipo_years"] = static_cast<std::uint64_t>(annual.size());
        counts["listed_bond_issuers"] = static_cast<std::uint64_t>(listed.size());
        counts["unlisted_bond_issuers"] = static_cast<std::uint64_t>(unlisted.size());
        result["ipo_annual"] = limited(std::move(annual), options.limit);
        result["listed_bond_issuers"] = limited(std::move(listed), options.limit);
        result["unlisted_bond_issuers"] = limited(std::move(unlisted), options.limit);
    }
    if (include_placements) {
        Json placements = Json::array();
        for (const auto& source : placement_resources) {
            auto normalized = normalize_private_placement_rows(
                document_for(documents, source.resource).at("rows"),
                std::string(source.resource), std::string(source.category), securities_);
            for (auto& row : normalized.as_array()) placements.push_back(std::move(row));
        }
        Json filtered = Json::array();
        for (auto& row : placements.as_array()) {
            if (!placement_status_matches(row, options.placement_status)) continue;
            const auto& security = row.at("security");
            if (!security_market.empty() &&
                (std::to_string(static_cast<int>(security.at("market_id").as_number())) != security_market ||
                 security.at("code").as_string() != options.code)) continue;
            if (!options.search.empty()) {
                const std::string haystack = security.at("code").as_string() + " " +
                    security.at("name").as_string() + " " + text_value(row, "industry") + " " +
                    text_value(row, "stage") + " " + text_value(row, "issue_details");
                if (!contains_folded(haystack, options.search)) continue;
            }
            filtered.push_back(std::move(row));
        }
        std::sort(filtered.as_array().begin(), filtered.as_array().end(),
            [](const Json& left, const Json& right) {
                const auto left_date = text_value(left, "sort_date");
                const auto right_date = text_value(right, "sort_date");
                if (left_date != right_date) return left_date > right_date;
                return left.at("security").at("security_id").as_string() <
                       right.at("security").at("security_id").as_string();
            });
        result["placement_summary"] = private_placement_summary(filtered);
        counts["private_placements"] = static_cast<std::uint64_t>(filtered.size());
        result["private_placements"] = limited(std::move(filtered), options.limit);
    }
    if (include_rights) {
        Json offerings = Json::array();
        for (const auto& source : rights_resources) {
            auto normalized = normalize_rights_offering_rows(
                document_for(documents, source.resource).at("rows"),
                std::string(source.resource), std::string(source.category), securities_);
            for (auto& row : normalized.as_array()) offerings.push_back(std::move(row));
        }
        Json filtered = Json::array();
        for (auto& row : offerings.as_array()) {
            const auto& security = row.at("security");
            if (!security_market.empty() &&
                (std::to_string(static_cast<int>(security.at("market_id").as_number())) !=
                     security_market || security.at("code").as_string() != options.code))
                continue;
            if (!options.search.empty()) {
                const std::string haystack = security.at("code").as_string() + " " +
                    security.at("name").as_string() + " " + text_value(row, "stage") + " " +
                    text_value(row, "rights_code") + " " + text_value(row, "rights_name") + " " +
                    text_value(row, "issue_details");
                if (!contains_folded(haystack, options.search)) continue;
            }
            filtered.push_back(std::move(row));
        }
        std::sort(filtered.as_array().begin(), filtered.as_array().end(),
            [](const Json& left, const Json& right) {
                if (text_value(left, "sort_date") != text_value(right, "sort_date"))
                    return text_value(left, "sort_date") > text_value(right, "sort_date");
                return left.at("event_id").as_string() < right.at("event_id").as_string();
            });
        result["rights_summary"] = rights_offering_summary(filtered);
        counts["rights_offerings"] = static_cast<std::uint64_t>(filtered.size());
        result["rights_offerings"] = limited(std::move(filtered), options.limit);
    }
    if (include_preferred) {
        auto rows = normalize_preferred_share_rows(
            document_for(documents, preferred_share_resources.front().resource).at("rows"),
            securities_);
        Json filtered = Json::array();
        for (auto& row : rows.as_array()) {
            const auto& security = row.at("underlying_security");
            if (!security_market.empty() &&
                (std::to_string(static_cast<int>(security.at("market_id").as_number())) !=
                     security_market || security.at("code").as_string() != options.code))
                continue;
            if (!options.search.empty()) {
                const std::string haystack = security.at("code").as_string() + " " +
                    security.at("name").as_string() + " " +
                    text_value(row, "preferred_code") + " " +
                    text_value(row, "preferred_name") + " " +
                    text_value(row, "issue_method");
                if (!contains_folded(haystack, options.search)) continue;
            }
            filtered.push_back(std::move(row));
        }
        std::sort(filtered.as_array().begin(), filtered.as_array().end(),
            [](const Json& left, const Json& right) {
                if (text_value(left, "listing_date") != text_value(right, "listing_date"))
                    return text_value(left, "listing_date") > text_value(right, "listing_date");
                return text_value(left, "preferred_code") <
                    text_value(right, "preferred_code");
            });
        result["preferred_share_summary"] = preferred_share_summary(filtered);
        counts["preferred_shares"] = static_cast<std::uint64_t>(filtered.size());
        result["preferred_shares"] = limited(std::move(filtered), options.limit);
    }

    result["selected_contract"] = nullptr;
    result["related_stocks"] = Json::array();
    result["position_history"] = Json::array();
    if (!options.contract_key.empty()) {
        bool index_contract = false;
        const Json* selected = nullptr;
        for (const auto& row : result.at("commodity_futures").as_array())
            if (row.at("contract_key").as_string() == options.contract_key) selected = &row;
        for (const auto& row : result.at("index_futures").as_array())
            if (row.at("contract_key").as_string() == options.contract_key) {
                selected = &row;
                index_contract = true;
            }
        if (!selected) throw Error("contract_key is absent from the current futures master tables");
        result["selected_contract"] = *selected;
        const auto resource = std::string(index_contract ? "qhtj2/" : "qhtj1/") +
                              options.contract_key + ".jsn";
        Json detail_cache = Json::object();
        const auto detail_documents = fetch_resources({resource}, options, detail_cache);
        const auto& detail = detail_documents.as_array().front();
        sources.push_back(source_summary(detail));
        if (index_contract)
            result["position_history"] = limited(normalize_position_history(detail.at("rows")), options.limit);
        else
            result["related_stocks"] = limited(normalize_related_stocks(detail.at("rows"), securities_), options.limit);
        cache_status["detail"] = std::move(detail_cache);
    }

    result["ipo_industries"] = Json::array();
    result["ipo_monthly"] = Json::array();
    result["ipo_securities"] = Json::array();
    if (!options.year.empty()) {
        const std::vector<std::string> resources{
            "ipotj102/" + options.year + ".jsn", "ipotj104/" + options.year + ".jsn"};
        Json detail_cache = Json::object();
        const auto detail_documents = fetch_resources(resources, options, detail_cache);
        result["ipo_industries"] = limited(normalize_ipo_industries(
            document_for(detail_documents, resources[0]).at("rows")), options.limit);
        result["ipo_monthly"] = limited(normalize_ipo_monthly(
            document_for(detail_documents, resources[1]).at("rows")), options.limit);
        for (const auto& document : detail_documents.as_array()) sources.push_back(source_summary(document));
        cache_status["ipo_year_detail"] = std::move(detail_cache);
    }
    if (!options.industry_key.empty()) {
        const auto resource = "ipotj103/" + options.industry_key + ".jsn";
        Json detail_cache = Json::object();
        const auto detail_documents = fetch_resources({resource}, options, detail_cache);
        const auto& detail = detail_documents.as_array().front();
        result["ipo_securities"] = limited(
            normalize_ipo_security_rows(detail.at("rows"), securities_), options.limit);
        sources.push_back(source_summary(detail));
        cache_status["ipo_industry_detail"] = std::move(detail_cache);
    }
    counts["related_stocks"] = static_cast<std::uint64_t>(result.at("related_stocks").size());
    counts["position_history"] = static_cast<std::uint64_t>(result.at("position_history").size());
    counts["ipo_industries"] = static_cast<std::uint64_t>(result.at("ipo_industries").size());
    counts["ipo_months"] = static_cast<std::uint64_t>(result.at("ipo_monthly").size());
    counts["ipo_securities"] = static_cast<std::uint64_t>(result.at("ipo_securities").size());
    result["counts"] = std::move(counts);
    result["sources"] = std::move(sources);
    result["cache"] = std::move(cache_status);
    result["read_only"] = true;
    return result;
}


}  // namespace tdx
