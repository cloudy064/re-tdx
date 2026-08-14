#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cstdint>
#include <set>

namespace tdx {

using namespace convertible_bond_detail;

Json ConvertibleBondService::query_pricing(
    const ConvertibleBondQuery& options, int selected_market) {
    bool refreshed = false;
    int age_seconds = 0;
    const std::string view = "pricing";
        const auto master = fetch_pricing(options, refreshed, age_seconds);
        const auto as_of = today_compact();
        const auto static_rows = normalize_convertible_bond_pricing_rows(
            master.at("rows"), Json::array(), securities_, as_of);
        std::vector<std::string> requested;
        std::set<std::string> unique;
        for (const auto& row : static_rows.as_array()) {
            if (!row.at("active").as_bool()) continue;
            const auto& bond = row.at("bond");
            const auto& underlying = row.at("underlying");
            if (!options.code.empty()) {
                bool selected = static_cast<int>(bond.at("market_id").as_number()) == selected_market &&
                                bond.at("code").as_string() == options.code;
                if (!selected && !underlying.is_null())
                    selected = static_cast<int>(underlying.at("market_id").as_number()) == selected_market &&
                               underlying.at("code").as_string() == options.code;
                if (!selected) continue;
            }
            const auto bond_id = bond.at("market").as_string() + ":" +
                bond.at("code").as_string();
            if (unique.insert(bond_id).second) requested.push_back(bond_id);
            if (!underlying.is_null()) {
                const auto stock_id = underlying.at("market").as_string() + ":" +
                    underlying.at("code").as_string();
                if (unique.insert(stock_id).second) requested.push_back(stock_id);
            }
        }
        Json quote_document = Json::object();
        quote_document["records"] = Json::array();
        Json quote_errors = Json::array();
        bool quotes_refreshed = false;
        int quote_age_seconds = 0;
        if (options.include_quotes && !requested.empty()) {
            try {
                quote_document = fetch_pricing_quotes(
                    requested, options, quotes_refreshed, quote_age_seconds);
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["source"] = "public-l1-snapshot";
                failure["message"] = error.what();
                quote_errors.push_back(std::move(failure));
            }
        }
        const auto normalized = normalize_convertible_bond_pricing_rows(
            master.at("rows"), quote_document, securities_, as_of);
        Json rows = Json::array();
        const auto folded = lower_ascii(trim(options.query));
        for (const auto& row : normalized.as_array()) {
            if (options.active_only && !row.at("active").as_bool()) continue;
            const auto& bond = row.at("bond");
            const auto& underlying = row.at("underlying");
            bool matches = options.code.empty();
            if (!options.code.empty()) {
                matches = static_cast<int>(bond.at("market_id").as_number()) == selected_market &&
                          bond.at("code").as_string() == options.code;
                if (!matches && !underlying.is_null())
                    matches = static_cast<int>(underlying.at("market_id").as_number()) == selected_market &&
                              underlying.at("code").as_string() == options.code;
            }
            if (matches && !folded.empty()) {
                const auto haystack = lower_ascii(
                    bond.at("security_id").as_string() + " " +
                    bond.at("name").as_string() + " " +
                    (underlying.is_null() ? "" :
                        underlying.at("security_id").as_string() + " " +
                        underlying.at("name").as_string()));
                matches = haystack.find(folded) != std::string::npos;
            }
            if (matches) rows.push_back(row);
        }
        sort_convertible_bond_pricing_rows(rows, options.sort, options.order);
        const auto matched = rows.size();
        while (static_cast<int>(rows.size()) > options.limit) rows.as_array().pop_back();

        std::uint64_t active = 0, complete = 0, bond_quotes = 0,
                      underlying_quotes = 0, exchangeable = 0;
        for (const auto& row : normalized.as_array()) {
            if (row.at("active").as_bool()) ++active;
            if (row.at("instrument_type").as_string() == "exchangeable-bond") ++exchangeable;
            if (row.at("quote").at("bond_available").as_bool()) ++bond_quotes;
            if (row.at("quote").at("underlying_available").as_bool()) ++underlying_quotes;
            if (row.at("valuation").at("availability").as_string() == "complete") ++complete;
        }
        Json summary = Json::object();
        summary["pricing_rows"] = static_cast<std::uint64_t>(normalized.size());
        summary["active_bonds"] = active;
        summary["exchangeable_bonds"] = exchangeable;
        summary["bond_quotes"] = bond_quotes;
        summary["underlying_quotes"] = underlying_quotes;
        summary["complete_valuations"] = complete;
        Json sources = Json::array();
        sources.push_back(master.at("source"));
        const auto health = jsn_sources_health(sources);
        Json result = Json::object();
        result["schema"] = "tdx-market-convertible-bonds-native-v1";
        result["generated_at"] = now_text();
        result["view"] = view;
        result["mode"] = options.code.empty() ? "catalog" : "security";
        result["availability"] = health.at("stale").as_bool()
            ? "stale-cache" : quote_errors.size() ? "partial" : "live";
        result["quote_availability"] = !options.include_quotes ? "disabled" :
            quote_errors.size() ? "unavailable" : "live";
        result["query"] = options.query;
        result["sort"] = options.sort.empty() ? "double-low" : lower_ascii(trim(options.sort));
        result["order"] = lower_ascii(trim(options.order.empty() ? "asc" : options.order));
        result["active_only"] = options.active_only;
        result["found"] = options.code.empty() || matched > 0;
        result["match_count"] = static_cast<std::uint64_t>(matched);
        result["returned"] = static_cast<std::uint64_t>(rows.size());
        result["bonds"] = Json::array();
        result["pricing"] = std::move(rows);
        result["pending_issues"] = Json::array();
        result["subscriptions"] = Json::array();
        result["details"] = Json::array();
        result["detail_errors"] = Json::array();
        result["master_errors"] = Json::array();
        result["quote_errors"] = std::move(quote_errors);
        result["summary"] = std::move(summary);
        result["sources"] = std::move(sources);
        result["upstream_health"] = health;
        Json cache = Json::object();
        cache["refreshed"] = refreshed;
        cache["age_seconds"] = age_seconds;
        cache["ttl_seconds"] = options.cache_ttl_seconds;
        cache["quotes_refreshed"] = quotes_refreshed;
        cache["quote_age_seconds"] = quote_age_seconds;
        cache["quote_ttl_seconds"] = options.quote_cache_ttl_seconds;
        result["cache"] = std::move(cache);
        return result;
    
}

}  // namespace tdx
