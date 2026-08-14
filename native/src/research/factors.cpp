// FactorService entry point. Validation, the fresh-cache probe and view
// dispatch live here; each view family is implemented in its own unit
// (factors_query_matrix / _overview / _security / _table).
#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace tdx {

using namespace factor_detail;

FactorService::FactorService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json FactorService::query(const FactorQuery& input) {
    FactorQuery options = input;
    const auto& definition = validate_factor_query(options);

    const auto key = cache_key(options);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!options.refresh && cached != cache_.end()) {
        const auto age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        if (age < options.cache_ttl_seconds) {
            auto result = cached->second.document;
            result["cache"]["hit"] = true;
            result["cache"]["age_seconds"] = age;
            return result;
        }
    }

    // The composite views recurse through this same entry point to collect
    // their complete source tables, so they must be dispatched before the
    // single-request tabular path.
    if (options.view == "pattern-matrix" || options.view == "standard-matrix")
        return query_relation_matrix(options, definition, key, now);
    if (options.view == "overview")
        return query_overview(options, definition, key, now);
    if (options.view == "security")
        return query_security(options, definition, key, now);
    return query_table(options, definition, key, now);
}

}  // namespace tdx
