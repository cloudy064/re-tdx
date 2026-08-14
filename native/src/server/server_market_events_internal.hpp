#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

#include <filesystem>
#include <string>

namespace tdx::server_detail {

Json query_market_panorama(const ApiState&, const RequestTarget&);
Json query_market_institution_analysis(const ApiState&, const RequestTarget&);
Json query_market_limit_review(const ApiState&, const RequestTarget&);
Json query_market_session_turnover(const ApiState&, const RequestTarget&);
Json query_market_block_rotation(const ApiState&, const RequestTarget&);
Json query_market_limit_ladder(const ApiState&, const RequestTarget&);
Json query_market_threshold_stocks(const ApiState&, const RequestTarget&);
Json query_market_capital_strength(const ApiState&, const RequestTarget&);
Json query_market_strong_stocks(const ApiState&, const RequestTarget&);
Json query_market_commodity_links(const ApiState&, const RequestTarget&);
Json query_market_announcement_signals(const ApiState&, const RequestTarget&);
Json query_market_reverse_repo(const ApiState&, const RequestTarget&);
Json query_market_exchange_supervision(const ApiState&, const RequestTarget&);
Json query_market_repurchases(const ApiState&, const RequestTarget&);
Json query_market_tender_offers(const ApiState&, const RequestTarget&);
Json query_market_ownership(const ApiState&, const RequestTarget&);
Json query_market_forecasts(const ApiState&, const RequestTarget&);
Json query_market_disclosures(const ApiState&, const RequestTarget&);

struct DisclosureArchiveRequest {
    bool backfill_announcements{};
    std::string market;
    std::string code;
    bool refresh{};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

DisclosureArchiveRequest parse_disclosure_archive_request(const Json&);
Json persist_disclosure_archive_observation(
    const std::filesystem::path& tdx_root, const Json& observation,
    std::string observed_at = {});
Json query_market_disclosure_archive(const ApiState&, const Json&);

}  // namespace tdx::server_detail
