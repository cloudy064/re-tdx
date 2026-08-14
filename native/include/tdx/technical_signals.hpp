#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct TechnicalSignalsQuery {
    std::string view{"nine-turn"};
    std::string direction{"all"};
    std::string board{"main"};
    int duration1{10};
    int rps1{-1};
    int duration2{20};
    int rps2{-1};
    int duration3{60};
    int rps3{-1};
    int index_period{2};
    int history_period{120};
    int retracement{5};
    int sideways_period{20};
    int amplitude{10};
    int breakout_period{2};
    bool apply_client_filters{true};
    bool enrich_quotes{};
    bool refresh{};
    int limit{5000};
    int cache_ttl_seconds{15};
    int timeout_ms{15000};
};

struct TechnicalSignalSecurityQuery {
    std::string market;
    std::string code;
    bool apply_client_filters{true};
    bool refresh{};
    int cache_ttl_seconds{15};
    int timeout_ms{15000};
};

Json normalize_technical_signal_rows(
    const Json& rows, const TechnicalSignalsQuery& query,
    const BlockData& blocks = {});
Json technical_signal_hits_for_security(
    const Json& document, const std::string& market, const std::string& code,
    const BlockData& blocks = {});
Json diff_technical_signal_documents(const Json& previous, const Json& current);
Json update_technical_signal_snapshot(
    const std::filesystem::path& path, const Json& current);

class TechnicalSignalsService {
public:
    TechnicalSignalsService(std::filesystem::path root, BlockData blocks = {});
    Json query(const TechnicalSignalsQuery& options);
    Json query_security(const TechnicalSignalSecurityQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_technical_signals(const std::vector<std::string>& args);

}  // namespace tdx
