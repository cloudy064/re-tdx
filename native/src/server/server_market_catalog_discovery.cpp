#include "server_market_catalog_internal.hpp"
#include "server_local_resource_paths_internal.hpp"

#include "tdx/calendar.hpp"
#include "tdx/common.hpp"
#include "tdx/economic_indicators.hpp"
#include "tdx/employees.hpp"
#include "tdx/hot_history.hpp"
#include "tdx/historical_securities.hpp"
#include "tdx/index_events.hpp"
#include "tdx/security_directory.hpp"
#include "tdx/strategic_themes.hpp"
#include "tdx/theme_library.hpp"
#include "tdx/thematic_opportunities.hpp"

namespace tdx::server_detail {

Json query_market_securities(const ApiState& state,
                             const RequestTarget& target) {
    if (!state.security_directory_service)
        throw Error("security-directory service is unavailable");
    SecurityDirectoryQuery query;
    query.market = lower_ascii(trim(query_value(target, "market", "all")));
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.query = trim(query_value(target, "q"));
    query.root = state.root;
    const auto explicit_host = trim(query_value(target, "host"));
    if (!explicit_host.empty()) query.hosts.push_back(explicit_host);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 100000);
    query.page_size = parse_bounded(query_value(target, "page_size", "1600"),
                                    "page_size", 1, 1700);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "3600"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.security_directory_service->query(query);
}

Json query_market_economic_indicators(const ApiState& state,
                                      const RequestTarget& target) {
    if (!state.economic_indicator_service)
        throw Error("economic-indicator service is unavailable");
    EconomicIndicatorQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.indicator_id = trim(query_value(target, "indicator_id"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "update-date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_history = query_bool(target, "include_history", true);
    query.include_related = query_bool(target, "include_related", true);
    query.include_quotes = query_bool(target, "include_quotes", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.history_limit = parse_bounded(
        query_value(target, "history_limit", "2000"),
        "history_limit", 1, 10000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.quote_cache_ttl_seconds = parse_bounded(
        query_value(target, "quote_cache_ttl_seconds", "5"),
        "quote_cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.economic_indicator_service->query(query);
}

Json query_market_strategic_themes(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.strategic_theme_service)
        throw Error("strategic-theme service is unavailable");
    StrategicThemeQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "categories")));
    query.category = trim(query_value(target, "category"));
    query.theme_id = trim(query_value(target, "theme_id"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "name")));
    query.order = lower_ascii(trim(query_value(target, "order", "asc")));
    query.include_detail = query_bool(target, "include_detail", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "900"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.strategic_theme_service->query(query);
}

Json query_market_theme_library(const ApiState& state,
                                const RequestTarget& target) {
    if (!state.theme_library_service)
        throw Error("theme-library service is unavailable");
    ThemeLibraryQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.source = lower_ascii(trim(query_value(target, "source", "general")));
    query.theme_id = trim(query_value(target, "theme_id"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "created")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_detail = query_bool(target, "include_detail", true);
    query.include_chart = query_bool(target, "include_chart", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "900"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.theme_library_service->query(query);
}

Json query_market_thematic_opportunities(const ApiState& state,
                                          const RequestTarget& target) {
    if (!state.thematic_opportunity_service)
        throw Error("thematic-opportunity service is unavailable");
    ThematicOpportunityQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.type = lower_ascii(trim(query_value(target, "type", "all")));
    query.group_id = trim(query_value(target, "group_id"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "name")));
    query.order = lower_ascii(trim(query_value(target, "order", "asc")));
    query.include_detail = query_bool(target, "include_detail", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "900"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.thematic_opportunity_service->query(query);
}

Json query_market_hot_history(const ApiState& state,
                              const RequestTarget& target) {
    HotHistoryQuery query;
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "start-date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    return load_local_hot_history(state.root, query, state.block_data.securities);
}

Json query_market_historical_securities(const ApiState& state,
                                        const RequestTarget& target) {
    HistoricalSecurityQuery query;
    query.market = lower_ascii(trim(query_value(target, "market", "all")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.presence = lower_ascii(trim(query_value(target, "presence", "all")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "code")));
    query.order = lower_ascii(trim(query_value(target, "order", "asc")));
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 10000);
    auto document = load_local_historical_securities(
        state.root, query, state.block_data.securities);
    project_local_catalog_resource_paths(document, state.root);
    return document;
}

Json query_market_index_events(const ApiState& state,
                               const RequestTarget& target) {
    IndexEventQuery query;
    query.benchmark = lower_ascii(trim(query_value(target, "benchmark", "all")));
    query.event_id = trim(query_value(target, "event_id"));
    query.query = trim(query_value(target, "q"));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.date_basis = lower_ascii(trim(query_value(target, "date_basis", "chart")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    auto document = load_local_index_events(state.root, query);
    project_local_catalog_resource_paths(document, state.root);
    return document;
}

Json query_market_calendar(const ApiState& state,
                           const RequestTarget& target) {
    if (!state.calendar_service) throw Error("calendar service is unavailable");
    CalendarQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.event_id = trim(query_value(target, "event"));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"), "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(query_value(target, "cache_ttl_seconds", "300"),
                                            "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.calendar_service->query(query);
}

Json query_market_employees(const ApiState& state,
                            const RequestTarget& target) {
    if (!state.employee_service) throw Error("employee service is unavailable");
    EmployeeQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "executive-compensation")));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"), "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(query_value(target, "cache_ttl_seconds", "900"),
                                            "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.employee_service->query(query);
}

}  // namespace tdx::server_detail
