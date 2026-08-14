#include "commodity_links_internal.hpp"

namespace tdx {
using namespace commodity_links_detail;

Json CommodityLinksService::query(const CommodityLinksQuery& input) {
    CommodityLinksQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.commodity_id = trim(options.commodity_id);
    options.theme_id = trim(options.theme_id);
    options.driver_id = trim(options.driver_id);
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = trim(options.query);
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    const auto* view_definition = find_view_definition(options.view);
    if (!view_definition)
        throw Error("view must be commodities, commodity, themes, theme, driver, security, or catalog");
    if (options.view == "commodity" && options.commodity_id.empty())
        throw Error("commodity view requires commodity_id");
    if ((options.view == "theme" || options.view == "driver") && options.theme_id.empty())
        throw Error(options.view + " view requires theme_id");
    if (options.view == "driver" && options.driver_id.empty())
        throw Error("driver view requires driver_id");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if (options.view == "security" && selected_market < 0)
        throw Error("security view requires market and code");
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    if (options.order.empty()) options.order = "desc";
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");
    if (options.sort.empty())
        options.sort = std::string(view_definition->default_sort);
    if (options.view == "commodities" &&
        !view_allows_sort(*view_definition, options.sort))
        throw Error("commodity sort must be quote-date, name, price, day-change, "
                    "5d, 10d, 30d, 60d, or source-rank");
    if (options.view == "themes" &&
        !view_allows_sort(*view_definition, options.sort))
        throw Error("theme sort must be latest-driver-date, trigger-date, name, "
                    "stocks, or source-rank");

    Json sources = Json::array(), records = Json::array(), summary = Json::object();
    bool refreshed = false;
    auto fetch_rows = [&](const std::string& resource) {
        bool fetched = false;
        auto document = fetch(resource, options.refresh, options.cache_ttl_seconds,
                              options.timeout_ms, fetched);
        refreshed = refreshed || fetched;
        sources.push_back(jsn_source_metadata(document));
        return document.at("rows");
    };

    if (options.view == "catalog") {
        records = catalog_rows();
        summary["views"] = static_cast<std::uint64_t>(records.size());
    } else if (options.view == "commodities" || options.view == "commodity") {
        auto commodities = normalize_commodity_rows(fetch_rows(commodity_master_resource()));
        summary["quote_rows"] = static_cast<std::uint64_t>(commodities.size());
        std::set<std::string> commodity_ids;
        for (const auto& row : commodities.as_array())
            commodity_ids.insert(json_text(row, "commodity_id"));
        summary["unique_commodity_ids"] = static_cast<std::uint64_t>(commodity_ids.size());
        if (options.view == "commodities") records = std::move(commodities);
        else {
            auto quotes = find_all_by_id(commodities, "commodity_id", options.commodity_id);
            if (!quotes.size()) throw Error("commodity_id is absent from the active commodity summary");
            Json row = Json::object();
            row["commodity_id"] = options.commodity_id;
            row["quotes"] = quotes;
            row["quote_count"] = static_cast<std::uint64_t>(quotes.size());
            auto stocks = normalize_commodity_stock_rows(
                fetch_rows("zjtc4/" + options.commodity_id + ".jsn"), securities_);
            auto related = normalize_commodity_security_rows(
                fetch_rows("zjtc5/" + options.commodity_id + ".jsn"), securities_);
            Json counts = Json::object();
            counts["stocks"] = static_cast<std::uint64_t>(stocks.size());
            counts["related_securities"] = static_cast<std::uint64_t>(related.size());
            row["counts"] = counts;
            row["stocks"] = std::move(stocks);
            row["related_securities"] = std::move(related);
            records.push_back(std::move(row));
            summary["selected_commodity_id"] = options.commodity_id;
        }
    } else {
        auto themes = normalize_price_theme_rows(fetch_rows(theme_master_resource()), securities_);
        summary["themes"] = static_cast<std::uint64_t>(themes.size());
        if (options.view == "themes") records = std::move(themes);
        else if (options.view == "theme" || options.view == "driver") {
            const auto* theme = find_by_id(themes, "theme_id", options.theme_id);
            if (!theme) throw Error("theme_id is absent from the active price-theme summary");
            auto stocks = normalize_theme_stock_rows(
                fetch_rows("zjtc1/" + options.theme_id + ".jsn"), securities_);
            auto drivers = normalize_theme_driver_rows(
                fetch_rows("zjtc2/" + options.theme_id + ".jsn"), securities_);
            if (options.view == "theme") {
                Json row = *theme;
                Json counts = Json::object();
                counts["stocks"] = static_cast<std::uint64_t>(stocks.size());
                counts["drivers"] = static_cast<std::uint64_t>(drivers.size());
                row["counts"] = counts;
                row["stocks"] = std::move(stocks);
                row["drivers"] = std::move(drivers);
                records.push_back(std::move(row));
                summary["selected_theme_id"] = options.theme_id;
            } else {
                const auto* driver = find_by_id(drivers, "driver_id", options.driver_id);
                if (!driver) throw Error("driver_id is absent from the selected theme history");
                Json row = *driver;
                row["theme"] = *theme;
                auto driver_stocks = normalize_driver_stock_rows(
                    fetch_rows("zjtc3/" + options.driver_id + ".jsn"), securities_);
                row["stocks"] = driver_stocks;
                row["stock_count"] = static_cast<std::uint64_t>(driver_stocks.size());
                records.push_back(std::move(row));
                summary["selected_theme_id"] = options.theme_id;
                summary["selected_driver_id"] = options.driver_id;
            }
        } else if (options.view == "security") {
            Json commodities;
            bool commodities_loaded = false;
            for (const auto& theme : themes.as_array()) {
                if (!security_set_contains(theme.at("stock_set"), selected_market, options.code))
                    continue;
                auto stocks = normalize_theme_stock_rows(
                    fetch_rows("zjtc1/" + json_text(theme, "theme_id") + ".jsn"), securities_);
                auto drivers = normalize_theme_driver_rows(
                    fetch_rows("zjtc2/" + json_text(theme, "theme_id") + ".jsn"), securities_);
                Json matching_stock = Json(nullptr);
                for (const auto& stock : stocks.as_array())
                    if (security_matches(stock.at("security"), selected_market, options.code)) {
                        matching_stock = stock;
                        break;
                    }
                Json matching_drivers = Json::array();
                for (const auto& driver : drivers.as_array())
                    if (security_set_contains(driver.at("stock_set"), selected_market, options.code))
                        matching_drivers.push_back(driver);
                Json association = Json::object();
                association["security"] = security_document(selected_market, options.code, securities_);
                association["theme"] = theme;
                association["theme_stock"] = std::move(matching_stock);
                association["drivers"] = std::move(matching_drivers);
                const auto commodity_id = json_text(theme, "associated_commodity_id");
                if (!commodity_id.empty()) {
                    if (!commodities_loaded) {
                        commodities = normalize_commodity_rows(fetch_rows(commodity_master_resource()));
                        commodities_loaded = true;
                    }
                    auto matches = find_all_by_id(commodities, "commodity_id", commodity_id);
                    association["associated_commodity_quotes"] = std::move(matches);
                } else association["associated_commodity_quotes"] = Json::array();
                records.push_back(std::move(association));
            }
            summary["security"] = security_document(selected_market, options.code, securities_);
            summary["theme_associations"] = static_cast<std::uint64_t>(records.size());
            summary["commodity_scan"] = "theme-associated commodities only; 221 commodity detail resources are not bulk-fetched";
        }
    }

    const auto needle = lower_ascii(options.query);
    Json filtered = Json::array();
    for (const auto& row : records.as_array())
        if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
    if (options.view == "commodities" || options.view == "themes")
        sort_rows(filtered, options.view, options.sort, options.order);
    const auto matched = filtered.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < filtered.size() && paged.size() < static_cast<std::size_t>(options.limit);
         ++index) paged.push_back(filtered.as_array()[index]);

    Json health = sources.size() ? jsn_sources_health(sources) : Json::object();
    if (!sources.size()) {
        health["stale"] = false;
        health["sources"] = 0;
    }
    Json result = Json::object();
    result["schema"] = "tdx-market-commodity-links-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["availability"] = options.view == "catalog" ? "catalog" :
        health.at("stale").as_bool() ? "stale-cache" : matched ? "live" : "empty";
    Json filters = Json::object();
    filters["commodity_id"] = options.commodity_id.empty() ? Json(nullptr) : Json(options.commodity_id);
    filters["theme_id"] = options.theme_id.empty() ? Json(nullptr) : Json(options.theme_id);
    filters["driver_id"] = options.driver_id.empty() ? Json(nullptr) : Json(options.driver_id);
    filters["market"] = options.market.empty() ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    result["filters"] = std::move(filters);
    result["summary"] = std::move(summary);
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(paged);
    result["catalog"] = catalog_rows();
    result["sources"] = std::move(sources);
    result["upstream_health"] = std::move(health);
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = refreshed;
    result["cache"] = std::move(cache);
    Json units = Json::object();
    units["prices"] = "upstream commodity unit or security price";
    units["changes"] = "percent";
    result["units"] = std::move(units);
    result["semantics"] =
        "TDX SPHQ/func_zjtc103 exposes public commodity quotes and selects zjtc4 related stocks plus zjtc5 industries/ETFs. A commodity relation ID can intentionally have multiple quote rows (for example futures and spot fuel oil), so quote_id identifies a row while commodity_id selects their shared stock relationship. TDX ZJTC/func_zjtc101 exposes price-rise themes, selects zjtc1 long-term related stocks and zjtc2 historical drivers, then selects zjtc3 stocks for one driver. Commodity returns reproduce client formulas from zxjg and price0..price4. CFJ and price1 are reference prices; current security quotes are host syscols absent from JSN, so derived security returns and live quote fields remain null instead of being fabricated. The security view cheaply reverses complete theme membership and its associated commodity, but deliberately does not bulk-fetch all 221 commodity-stock resources. No L2 entitlement is required.";
    return result;
}

}  // namespace tdx
