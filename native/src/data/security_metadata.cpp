#include "tdx/blocks.hpp"

#include "tdx/json.hpp"

#include <string>

namespace tdx {
namespace {

int local_market_id(std::string_view market) {
    const auto normalized = lower_ascii(trim(std::string(market)));
    if (normalized == "sz" || normalized == "0") return 0;
    if (normalized == "sh" || normalized == "1") return 1;
    if (normalized == "bj" || normalized == "2") return 2;
    return -1;
}

bool has_document_member(const Json& document, std::string_view key) {
    return document.as_object().find(key) != document.as_object().end();
}

}  // namespace

bool enrich_security_metadata(const SecurityCatalog& catalog, Json& document,
                              std::string_view market,
                              std::string_view code) {
    if (!document.is_object()) return false;
    const int market_id = local_market_id(market);
    if (market_id < 0) return false;
    const auto found = catalog.find({market_id, trim(std::string(code))});
    if (found == catalog.end()) return false;

    bool changed = false;
    const auto name = document.as_object().find("name");
    const bool name_missing = name == document.as_object().end() ||
        (name->second.is_string() && name->second.as_string().empty());
    if (name_missing && !found->second.name.empty()) {
        document["name"] = found->second.name;
        if (!has_document_member(document, "name_source"))
            document["name_source"] = "local-tnf-security-master";
        changed = true;
    }

    // Presence means explicit caller intent, even for a non-numeric/null
    // value.  Formula validation/fallback remains responsible for that value.
    if (found->second.price_precision &&
        !has_document_member(document, "price_precision")) {
        document["price_precision"] = static_cast<std::uint64_t>(
            *found->second.price_precision);
        if (!has_document_member(document, "price_precision_source"))
            document["price_precision_source"] =
                "local-tnf-security-master";
        changed = true;
    }
    return changed;
}

}  // namespace tdx
