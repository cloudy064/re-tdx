#include "server_market_events_internal.hpp"

#include "tdx/announcement_signals.hpp"
#include "tdx/block_rotation.hpp"
#include "tdx/capital_strength.hpp"
#include "tdx/commodity_links.hpp"
#include "tdx/common.hpp"
#include "tdx/disclosures.hpp"
#include "tdx/exchange_supervision.hpp"
#include "tdx/forecasts.hpp"
#include "tdx/institution_analysis.hpp"
#include "tdx/limit_ladder.hpp"
#include "tdx/limit_review.hpp"
#include "tdx/ownership.hpp"
#include "tdx/panorama.hpp"
#include "tdx/repurchases.hpp"
#include "tdx/reverse_repo.hpp"
#include "tdx/session_turnover.hpp"
#include "tdx/strong_stocks.hpp"
#include "tdx/tender_offers.hpp"
#include "tdx/threshold_stocks.hpp"

#include <filesystem>
#include <cmath>
#include <set>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::server_detail {

namespace {

const Json* disclosure_archive_member(const Json& body,
                                      std::string_view name) {
    const auto found = body.as_object().find(std::string(name));
    return found == body.as_object().end() ? nullptr : &found->second;
}

std::string disclosure_archive_string(const Json& body,
                                      std::string_view name) {
    const auto* value = disclosure_archive_member(body, name);
    if (!value) return {};
    if (!value->is_string())
        throw Error(std::string(name) + " must be a string");
    return trim(value->as_string());
}

bool disclosure_archive_bool(const Json& body, std::string_view name,
                             bool fallback) {
    const auto* value = disclosure_archive_member(body, name);
    if (!value) return fallback;
    if (!value->is_bool())
        throw Error(std::string(name) + " must be boolean");
    return value->as_bool();
}

int disclosure_archive_integer(const Json& body, std::string_view name,
                               int fallback, int minimum, int maximum) {
    const auto* value = disclosure_archive_member(body, name);
    if (!value) return fallback;
    if (!value->is_number() || !std::isfinite(value->as_number()) ||
        std::floor(value->as_number()) != value->as_number() ||
        value->as_number() < minimum || value->as_number() > maximum)
        throw Error(std::string(name) + " must be an integer in " +
                    std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    return static_cast<int>(value->as_number());
}

bool six_digits(std::string_view value) {
    if (value.size() != 6) return false;
    for (const auto ch : value)
        if (ch < '0' || ch > '9') return false;
    return true;
}

}  // namespace

Json query_market_panorama(const ApiState& state,
                           const RequestTarget& target) {
    if (!state.panorama_service) throw Error("panorama service is unavailable");
    PanoramaQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "200"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.panorama_service->query(query);
}

Json query_market_institution_analysis(const ApiState& state,
                                       const RequestTarget& target) {
    if (!state.institution_analysis_service)
        throw Error("institution-analysis service is unavailable");
    InstitutionAnalysisQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "200"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.institution_analysis_service->query(query);
}

Json query_market_limit_review(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.limit_review_service)
        throw Error("limit-review service is unavailable");
    LimitReviewQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.date = trim(query_value(target, "date"));
    query.query = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "200"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.limit_review_service->query(query);
}

Json query_market_session_turnover(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.session_turnover_service)
        throw Error("session-turnover service is unavailable");
    SessionTurnoverQuery query;
    query.universe = lower_ascii(trim(query_value(target, "universe", "a")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "after-hours")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.activity = lower_ascii(trim(query_value(target, "activity", "all")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "200"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.session_turnover_service->query(query);
}

Json query_market_block_rotation(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.block_rotation_service)
        throw Error("block-rotation service is unavailable");
    BlockRotationQuery query;
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.period = lower_ascii(trim(query_value(target, "period", "1w")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "last-date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.signal = lower_ascii(trim(query_value(target, "signal", "all")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.block_rotation_service->query(query);
}

Json query_market_limit_ladder(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.limit_ladder_service)
        throw Error("limit-ladder service is unavailable");
    LimitLadderQuery query;
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "total-height")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.activity = lower_ascii(trim(query_value(target, "activity", "all")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.limit_ladder_service->query(query);
}

Json query_market_threshold_stocks(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.threshold_stocks_service)
        throw Error("threshold-stocks service is unavailable");
    ThresholdStocksQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "history")));
    query.universe = lower_ascii(trim(query_value(target, "universe", "high-price")));
    query.date = trim(query_value(target, "date"));
    query.status = lower_ascii(trim(query_value(target, "status", "all")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort",
        query.view == "history" ? "date" : "threshold-value")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.threshold_stocks_service->query(query);
}

Json query_market_capital_strength(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.capital_strength_service)
        throw Error("capital-strength service is unavailable");
    CapitalStrengthQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "ranking")));
    query.period = lower_ascii(trim(query_value(target, "period", "5d")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort")));
    query.order = lower_ascii(trim(query_value(target, "order")));
    query.min_periods = parse_bounded(query_value(target, "min_periods", "2"),
                                      "min_periods", 1, 5);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.capital_strength_service->query(query);
}

Json query_market_strong_stocks(const ApiState& state,
                                const RequestTarget& target) {
    if (!state.strong_stocks_service)
        throw Error("strong-stocks service is unavailable");
    StrongStocksQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "intervals")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.interval_id = trim(query_value(target, "interval_id"));
    query.query = trim(query_value(target, "q"));
    query.from = trim(query_value(target, "from"));
    query.to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort")));
    query.order = lower_ascii(trim(query_value(target, "order")));
    query.min_trading_days = parse_bounded(
        query_value(target, "min_trading_days", "0"),
        "min_trading_days", 0, 1000);
    query.min_limit_up_days = parse_bounded(
        query_value(target, "min_limit_up_days", "0"),
        "min_limit_up_days", 0, 1000);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.strong_stocks_service->query(query);
}

Json query_market_commodity_links(const ApiState& state,
                                  const RequestTarget& target) {
    if (!state.commodity_links_service)
        throw Error("commodity-links service is unavailable");
    CommodityLinksQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "commodities")));
    query.commodity_id = trim(query_value(target, "commodity_id"));
    query.theme_id = trim(query_value(target, "theme_id"));
    query.driver_id = trim(query_value(target, "driver_id"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort")));
    query.order = lower_ascii(trim(query_value(target, "order")));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.commodity_links_service->query(query);
}

Json query_market_announcement_signals(const ApiState& state,
                                       const RequestTarget& target) {
    if (!state.announcement_signals_service)
        throw Error("announcement-signals service is unavailable");
    AnnouncementSignalsQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "selected")));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.direction = lower_ascii(trim(query_value(target, "direction", "all")));
    query.announcement_type = trim(query_value(target, "type"));
    query.from = trim(query_value(target, "from"));
    query.to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_history = query_bool(target, "include_history", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.announcement_signals_service->query(query);
}

Json query_market_reverse_repo(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.reverse_repo_service)
        throw Error("reverse-repo service is unavailable");
    ReverseRepoQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "rates")));
    query.market = lower_ascii(trim(query_value(target, "market", "all")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "rate")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.min_term_days = parse_bounded(
        query_value(target, "min_term_days", "0"), "min_term_days", 0, 1000);
    query.max_term_days = parse_bounded(
        query_value(target, "max_term_days", "0"), "max_term_days", 0, 1000);
    query.principal_yuan = parse_bounded(
        query_value(target, "principal_yuan", "100000"),
        "principal_yuan", 1000, 1000000000);
    query.include_quotes = query_bool(target, "include_quotes", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "100"),
                                "limit", 1, 1000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.quote_cache_ttl_seconds = parse_bounded(
        query_value(target, "quote_cache_ttl_seconds", "5"),
        "quote_cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.reverse_repo_service->query(query);
}

Json query_market_exchange_supervision(const ApiState& state,
                                       const RequestTarget& target) {
    if (!state.exchange_supervision_service)
        throw Error("exchange-supervision service is unavailable");
    ExchangeSupervisionQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "current")));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.from = trim(query_value(target, "from"));
    query.to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "end-date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.pdf_only = query_bool(target, "pdf_only");
    query.include_quotes = query_bool(target, "include_quotes", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.quote_cache_ttl_seconds = parse_bounded(
        query_value(target, "quote_cache_ttl_seconds", "5"),
        "quote_cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.exchange_supervision_service->query(query);
}

Json query_market_repurchases(const ApiState& state,
                              const RequestTarget& target) {
    if (!state.repurchase_service)
        throw Error("repurchase service is unavailable");
    RepurchaseQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "plans")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.segment = lower_ascii(trim(query_value(target, "segment", "a")));
    query.year = trim(query_value(target, "year"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty() || !query.year.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(
        query_value(target, "detail_limit", "2000"),
        "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.repurchase_service->query(query);
}

Json query_market_tender_offers(const ApiState& state,
                                const RequestTarget& target) {
    if (!state.tender_offer_service)
        throw Error("tender-offer service is unavailable");
    TenderOfferQuery query;
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.status = lower_ascii(trim(query_value(target, "status", "all")));
    query.from = trim(query_value(target, "from"));
    query.to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "announcement-date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.tender_offer_service->query(query);
}

Json query_market_ownership(const ApiState& state,
                            const RequestTarget& target) {
    if (!state.ownership_service)
        throw Error("ownership service is unavailable");
    OwnershipQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "changes")));
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.institution_id = trim(query_value(target, "institution_id"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty() || !query.institution_id.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(
        query_value(target, "detail_limit", "2000"),
        "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.ownership_service->query(query);
}

Json query_market_forecasts(const ApiState& state,
                            const RequestTarget& target) {
    if (!state.forecast_service)
        throw Error("forecast service is unavailable");
    ForecastQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "industries")));
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.query = trim(query_value(target, "q"));
    query.industry = trim(query_value(target, "industry"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.report_period = trim(query_value(target, "report_period"));
    query.include_details = query_bool(target, "include_details",
        !query.industry.empty() || !query.code.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(
        query_value(target, "detail_limit", "5000"),
        "detail_limit", 1, 10000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.forecast_service->query(query);
}

Json query_market_disclosures(const ApiState& state,
                              const RequestTarget& target) {
    if (target.query.count("archive") || target.query.count("archive_path"))
        throw Error("disclosure archive writes require confirmed POST "
                    "/api/v1/market/disclosures/archive with "
                    "X-TDX-Action: disclosure-archive");
    if (!state.disclosure_service)
        throw Error("disclosure service is unavailable");
    DisclosureQuery query;
    query.backfill_announcements = query_bool(target, "backfill_announcements");
    query.view = lower_ascii(trim(query_value(
        target, "view", query.backfill_announcements ? "announcement" : "schedule")));
    query.status = lower_ascii(trim(query_value(target, "status", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.report_period = trim(query_value(target, "report_period"));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.disclosure_service->query(query);
}

DisclosureArchiveRequest parse_disclosure_archive_request(
    const Json& body) {
    if (!body.is_object())
        throw Error("disclosure archive request body must be a JSON object");
    static const std::set<std::string> allowed{
        "backfill_announcements", "market", "code", "refresh",
        "cache_ttl_seconds", "timeout_ms"};
    for (const auto& [name, value] : body.as_object()) {
        (void)value;
        if (!allowed.count(name))
            throw Error("disclosure archive does not accept field: " + name);
    }

    DisclosureArchiveRequest request;
    request.backfill_announcements = disclosure_archive_bool(
        body, "backfill_announcements", false);
    request.market = lower_ascii(disclosure_archive_string(body, "market"));
    request.code = disclosure_archive_string(body, "code");
    request.refresh = disclosure_archive_bool(body, "refresh", false);
    request.cache_ttl_seconds = disclosure_archive_integer(
        body, "cache_ttl_seconds", 300, 0, 86400);
    request.timeout_ms = disclosure_archive_integer(
        body, "timeout_ms", 15000, 100, 60000);

    if (!request.backfill_announcements &&
        (!request.market.empty() || !request.code.empty()))
        throw Error("market and code are accepted only when "
                    "backfill_announcements is true");
    if (request.backfill_announcements) {
        if (request.market != "sz" && request.market != "sh" &&
            request.market != "bj" && request.market != "0" &&
            request.market != "1" && request.market != "2")
            throw Error("market must be sz/sh/bj or 0/1/2");
        if (!six_digits(request.code))
            throw Error("announcement backfill requires a six-digit code");
    }
    return request;
}

Json persist_disclosure_archive_observation(
    const fs::path& tdx_root, const Json& observation,
    std::string observed_at) {
    if (tdx_root.empty() || !fs::is_directory(tdx_root))
        throw Error("disclosure archive requires an existing TDX root");
    const auto canonical_root = fs::weakly_canonical(tdx_root);
    const auto path = default_disclosure_archive_path(canonical_root);
    Json existing;
    const Json* existing_pointer = nullptr;
    if (fs::is_regular_file(path)) {
        existing = Json::parse(read_text_utf8(path));
        existing_pointer = &existing;
    }
    const auto merged = merge_disclosure_archive_document(
        observation, existing_pointer, std::move(observed_at));
    atomic_write_text(path, merged.dump(2) + "\n");

    Json metadata = Json::object();
    metadata["path"] = path_utf8(path);
    metadata["schema"] = merged.at("schema");
    metadata["summary"] = merged.at("summary");
    metadata["fixed_root_path"] = true;
    return metadata;
}

Json query_market_disclosure_archive(const ApiState& state,
                                     const Json& body) {
    const auto request = parse_disclosure_archive_request(body);
    if (!state.disclosure_service)
        throw Error("disclosure service is unavailable");

    Json announcement_result;
    if (request.backfill_announcements) {
        DisclosureQuery backfill;
        backfill.view = "announcement";
        backfill.status = "all";
        backfill.market = request.market;
        backfill.code = request.code;
        backfill.backfill_announcements = true;
        backfill.refresh = request.refresh;
        backfill.limit = 20000;
        backfill.cache_ttl_seconds = request.cache_ttl_seconds;
        backfill.timeout_ms = request.timeout_ms;
        announcement_result = state.disclosure_service->query(backfill);
    }

    DisclosureQuery archive_query;
    archive_query.view = "all";
    archive_query.status = "all";
    archive_query.refresh = request.backfill_announcements
        ? false : request.refresh;
    archive_query.limit = 20000;
    archive_query.cache_ttl_seconds = request.cache_ttl_seconds;
    archive_query.timeout_ms = request.timeout_ms;
    auto observation = state.disclosure_service->query(archive_query);
    std::uint64_t announcement_rows = 0;
    if (request.backfill_announcements) {
        for (const auto& row : announcement_result.at("rows").as_array()) {
            if (!row.is_object() || !row.as_object().count("kind") ||
                !row.at("kind").is_string() ||
                row.at("kind").as_string() != "announcement-report")
                continue;
            observation["rows"].push_back(row);
            ++announcement_rows;
        }
    }

    auto result = Json::object();
    result["schema"] = "tdx-market-disclosure-archive-write-v1";
    result["written"] = true;
    result["archive"] = persist_disclosure_archive_observation(
        state.root, observation,
        observation.at("generated_at").as_string());
    result["master_observation_summary"] = observation.at("summary");
    result["backfill_announcements"] = request.backfill_announcements;
    result["announcement_rows_added"] = announcement_rows;
    if (request.backfill_announcements) {
        Json security = Json::object();
        security["market"] = request.market;
        security["code"] = request.code;
        result["security"] = std::move(security);
    }
    return result;
}

}  // namespace tdx::server_detail
