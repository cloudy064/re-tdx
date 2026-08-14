#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct FactorQuery {
    std::string view{"catalog"};
    std::string factor_id;
    std::string query;
    std::string market;
    std::string code;
    bool include_patterns{};
    bool all_pages{true};
    bool enrich_quotes{};
    bool refresh{};
    int limit{10000};
    int max_pages{100};
    int cache_ttl_seconds{60};
    int timeout_ms{15000};
};

Json normalize_factor_rows(const Json& rows, const std::string& view,
                           const BlockData& blocks = {});
Json factor_hits_for_security(const Json& catalog_records,
                              const Json& dashboard_records,
                              const std::string& market,
                              const std::string& code);
Json pattern_hits_for_security(const Json& pattern_matrix_records,
                               const std::string& market,
                               const std::string& code);
Json build_factor_breadth_document(const Json& catalog_records,
                                   const Json& dashboard_records);
Json reconcile_factor_breadth_document(Json breadth,
                                       const Json& dashboard_records,
                                       const Json& standard_matrix_records);
Json diff_factor_documents(const Json& previous, const Json& current);
Json update_factor_snapshot(const std::filesystem::path& path,
                            const Json& current);

namespace factor_detail {
struct ViewDefinition;
}  // namespace factor_detail

class FactorService {
public:
    FactorService(std::filesystem::path root, BlockData blocks = {});
    Json query(const FactorQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };

    // One implementation unit per view family. Each returns the finished
    // document and is responsible for storing it in cache_ under key.
    Json query_relation_matrix(const FactorQuery& options,
                               const factor_detail::ViewDefinition& definition,
                               const std::string& key, std::time_t now);
    Json query_overview(const FactorQuery& options,
                        const factor_detail::ViewDefinition& definition,
                        const std::string& key, std::time_t now);
    Json query_security(const FactorQuery& options,
                        const factor_detail::ViewDefinition& definition,
                        const std::string& key, std::time_t now);
    Json query_table(const FactorQuery& options,
                     const factor_detail::ViewDefinition& definition,
                     const std::string& key, std::time_t now);

    std::filesystem::path root_;
    BlockData blocks_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_factors(const std::vector<std::string>& args);

}  // namespace tdx
