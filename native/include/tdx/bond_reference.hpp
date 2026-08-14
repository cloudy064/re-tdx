#pragma once

#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tdx {

struct BondReferenceSource {
    std::string group;
    std::string bucket;
    std::string name;
    std::string resource;
};

const std::vector<BondReferenceSource>& bond_reference_sources();

struct BondReferenceQuery {
    std::string group{"rating"};
    std::string bucket{"aa-plus"};
    std::string market;
    std::string code;
    std::string query;
    std::string sort{"maturity"};
    std::string order{"asc"};
    bool refresh{};
    bool include_projections{};
    int offset{};
    int limit{500};
    int cache_ttl_seconds{900};
    int timeout_ms{30000};
};

Json normalize_bond_reference_rows(const Json& rows,
                                   const BondReferenceSource& source);

class BondReferenceService {
public:
    explicit BondReferenceService(std::filesystem::path root = {},
                                  std::filesystem::path jsn_root = {});
    Json query(const BondReferenceQuery& options);

private:
    struct CachedDocument { Json document; std::time_t fetched_at{}; };
    struct FetchResult { Json document; bool refreshed{}; int age_seconds{}; };
    FetchResult fetch_source(const BondReferenceSource& source,
                             const BondReferenceQuery& options);
    // Reconciles a category bucket against its per-market projections and, when
    // requested, its client master.  Returns the reconciliation report or null
    // when the bucket has no projection plan, folds every extra fetch into
    // `fetched`, and appends the projection then client-master source metadata to
    // `projection_sources` in that order.
    Json audit_category_projections(const BondReferenceSource& selected,
                                    const BondReferenceQuery& options,
                                    FetchResult& fetched,
                                    Json& projection_sources);

    std::filesystem::path root_;
    std::filesystem::path jsn_root_;
    std::map<std::string, CachedDocument> cache_;
};

int command_market_bond_reference(const std::vector<std::string>& args);

}  // namespace tdx
