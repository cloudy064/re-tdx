#include "bond_reference_internal.hpp"

namespace tdx {
namespace fs = std::filesystem;
using namespace bond_reference_detail;

BondReferenceService::BondReferenceService(fs::path root, fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)) {}

BondReferenceService::FetchResult BondReferenceService::fetch_source(
    const BondReferenceSource& source, const BondReferenceQuery& options) {
    const auto now = std::time(nullptr);
    const auto found = cache_.find(source.resource);
    if (!options.refresh && found != cache_.end()) {
        const int age = static_cast<int>(
            std::max<std::time_t>(0, now - found->second.fetched_at));
        if (age < options.cache_ttl_seconds)
            return {found->second.document, false, age};
    }
    Json document;
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            document = load_local_resource_rows(jsn_root_, source.resource);
        } catch (...) {
            // Missing or invalid local resources retain the existing live path.
        }
    }
    if (!document.is_object())
        document = fetch_jsn_resource_rows(
            source.resource, "bi", options.timeout_ms, root_);
    cache_[source.resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

}  // namespace tdx
