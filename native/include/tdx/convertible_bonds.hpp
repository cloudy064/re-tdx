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

struct ConvertibleBondQuery {
    std::string view{"listed"};
    std::string query;
    std::string market;
    std::string code;
    std::string sort;
    std::string order{"desc"};
    bool refresh{};
    bool include_details{true};
    bool include_quotes{true};
    bool active_only{true};
    int limit{2000};
    int cache_ttl_seconds{900};
    int quote_cache_ttl_seconds{5};
    int timeout_ms{15000};
};

Json normalize_convertible_bond_documents(
    const Json& documents,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_pending_convertible_bond_document(
    const Json& document,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_convertible_bond_subscription_document(
    const Json& document,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_new_convertible_bond_projection_document(
    const Json& document,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json reconcile_new_convertible_bond_projection(
    const Json& subscription_rows, const Json& projection_rows);
void sort_pending_convertible_bond_rows(Json& rows, const std::string& sort,
                                        const std::string& order);
void sort_convertible_bond_subscription_rows(Json& rows, const std::string& sort,
                                             const std::string& order);
Json normalize_convertible_bond_pricing_rows(
    const Json& rows, const Json& quote_rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {},
    const std::string& as_of_date = {});
void sort_convertible_bond_pricing_rows(Json& rows, const std::string& sort,
                                        const std::string& order);

class ConvertibleBondService {
public:
    ConvertibleBondService(
        std::filesystem::path root,
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const ConvertibleBondQuery& options);

private:
    Json fetch_master(const ConvertibleBondQuery& options, bool& refreshed,
                      int& age_seconds);
    Json fetch_pending(const ConvertibleBondQuery& options, bool& refreshed,
                       int& age_seconds);
    Json fetch_subscriptions(const ConvertibleBondQuery& options, bool& refreshed,
                             int& age_seconds);
    Json fetch_pricing(const ConvertibleBondQuery& options, bool& refreshed,
                       int& age_seconds);
    Json fetch_pricing_quotes(const std::vector<std::string>& requested,
                              const ConvertibleBondQuery& options,
                              bool& refreshed, int& age_seconds);
    Json query_subscriptions(const ConvertibleBondQuery& options,
                             int selected_market);
    Json query_pricing(const ConvertibleBondQuery& options,
                       int selected_market);
    Json query_pending(const ConvertibleBondQuery& options,
                       int selected_market);
    Json query_listed(const ConvertibleBondQuery& options,
                      int selected_market);

    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
    Json pending_cache_;
    std::time_t pending_cache_time_{};
    Json subscription_cache_;
    std::time_t subscription_cache_time_{};
    Json pricing_cache_;
    std::time_t pricing_cache_time_{};
    std::map<std::string, CachedDocument> pricing_quote_cache_;
};

int command_market_convertible_bonds(const std::vector<std::string>& args);

}  // namespace tdx
