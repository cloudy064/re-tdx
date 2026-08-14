#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <string>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

// Eleven per-security contracts assert the same resilience shape (availability,
// cache staleness agreement, upstream health, per-source attempt metadata) and
// differ only by expected schema and mode.
struct StockResilienceContract {
    std::string_view id;
    std::string_view schema;
    std::string_view mode;
};

constexpr std::array<StockResilienceContract, 11> resilience_contracts{{
    {"stock-research-live", "tdx-market-research-native-v1", "security"},
    {"stock-consensus-live", "tdx-market-consensus-native-v1", "security"},
    {"stock-industry-profile-live", "tdx-industry-profile-native-v1", "security"},
    {"stock-ownership-live", "tdx-market-ownership-native-v1", "security"},
    {"stock-repurchases-live", "tdx-market-repurchases-native-v1", "security"},
    {"stock-institution-lhb-live", "tdx-market-institution-lhb-native-v1", "security"},
    {"stock-ratings-live", "tdx-market-ratings-native-v1", "selection"},
    {"stock-foreign-alerts-live", "tdx-market-foreign-alerts-native-v1", "security"},
    {"stock-unlocks-live", "tdx-market-unlocks-native-v1", "security"},
    {"stock-block-trades-live", "tdx-market-block-trades-native-v1", "security"},
    {"stock-lhb-live", "tdx-lhb-native-v1", "security"},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < resilience_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < resilience_contracts.size();
             ++right)
            if (resilience_contracts[left].id == resilience_contracts[right].id)
                return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_stock_resilience_contract(const std::string& contract_id,
                                        const Json& document, Json& result) {
    const StockResilienceContract* contract = nullptr;
    for (const auto& candidate : resilience_contracts)
        if (candidate.id == contract_id) { contract = &candidate; break; }
    if (!contract) return false;

    add_assertion(result, "schema",
                  string_is(member(document, "schema"), contract->schema),
                  std::string(contract->schema),
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool usable = string_is(availability, "live") ||
                        string_is(availability, "stale-cache") ||
                        string_is(availability, "partial");
    add_assertion(result, "availability", usable,
                  "live, stale-cache, or partial", value_or_null(availability));
    add_assertion(result, "mode",
                  string_is(member(document, "mode"), contract->mode),
                  std::string(contract->mode),
                  value_or_null(member(document, "mode")));

    const auto* cache_stale = member_path(document, {"cache", "stale"});
    const auto* upstream = member_path(document, {"cache", "upstream"});
    const auto* upstream_stale = upstream && upstream->is_object()
        ? member(*upstream, "stale") : nullptr;
    add_assertion(result, "cache_stale_boolean",
                  cache_stale && cache_stale->is_bool(), "boolean",
                  value_or_null(cache_stale));
    add_assertion(result, "upstream_health_object",
                  upstream && upstream->is_object(), "object", value_or_null(upstream));
    add_assertion(result, "upstream_stale_boolean",
                  upstream_stale && upstream_stale->is_bool(), "boolean",
                  value_or_null(upstream_stale));
    const bool stale_consistent = cache_stale && cache_stale->is_bool() &&
        upstream_stale && upstream_stale->is_bool() &&
        cache_stale->as_bool() == upstream_stale->as_bool();
    add_assertion(result, "stale_state_consistent", stale_consistent, true,
                  stale_consistent);
    const auto* attempts = member_path(document, {"cache", "upstream", "max_attempts"});
    const auto* errors = member_path(document, {"cache", "upstream", "upstream_errors"});
    add_assertion(result, "max_attempts_number",
                  attempts && attempts->is_number(), "number", value_or_null(attempts));
    add_assertion(result, "upstream_errors_array",
                  errors && errors->is_array(), "array", value_or_null(errors));

    const auto* sources = member(document, "sources");
    bool source_shape = sources && sources->is_array() && !sources->as_array().empty();
    if (source_shape) {
        for (const auto& source : sources->as_array()) {
            const auto* source_attempts = member(source, "attempts");
            const auto* source_stale = member(source, "stale");
            const auto* source_age = member(source, "age_seconds");
            const auto* source_error = member(source, "upstream_error");
            if (!source.is_object() ||
                !member(source, "resource") || !member(source, "endpoint") ||
                !source_attempts || !source_attempts->is_number() ||
                !source_stale || !source_stale->is_bool() ||
                !source_age || !source_age->is_number() ||
                !source_error || !(source_error->is_null() || source_error->is_string())) {
                source_shape = false;
                break;
            }
        }
    }
    add_assertion(result, "sources_resilience_shape", source_shape, true, source_shape);
    return true;
}

void validate_stock_roadshows_contract(const Json& document, const Json&,
                                       Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"), "tdx-roadshows-native-v1"),
                  "tdx-roadshows-native-v1", value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    const bool usable = string_is(availability, "live") ||
                        string_is(availability, "stale-cache");
    add_assertion(result, "availability", usable, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "mode",
                  string_is(member(document, "mode"), "security"), "security",
                  value_or_null(member(document, "mode")));
    add_assertion(result, "security_code",
                  string_is(member_path(document, {"security", "code"}), "000001"),
                  "000001", value_or_null(member_path(document, {"security", "code"})));
    const auto* returned = member_path(document, {"counts", "returned"});
    const bool nonempty = returned && returned->is_number() && returned->as_number() > 0;
    add_assertion(result, "records_returned", nonempty, "number > 0",
                  value_or_null(returned));
    add_assertion(result, "source_entry",
                  string_is(member_path(document, {"source", "entry"}),
                            "CWSearch.tzx_rcache"),
                  "CWSearch.tzx_rcache",
                  value_or_null(member_path(document, {"source", "entry"})));
    const auto* request_id = member_path(document, {"source", "request_id"});
    add_assertion(result, "request_id_null", request_id && request_id->is_null(),
                  Json(nullptr), value_or_null(request_id));
    add_assertion(result, "source_key",
                  string_is(member_path(document, {"source", "key"}), "ly:0_000001"),
                  "ly:0_000001", value_or_null(member_path(document, {"source", "key"})));
    const auto* attempts = member_path(document, {"source", "attempts"});
    add_assertion(result, "source_attempts_number", attempts && attempts->is_number(),
                  "number", value_or_null(attempts));
    const auto* stale = member_path(document, {"cache", "stale"});
    add_assertion(result, "cache_stale_boolean", stale && stale->is_bool(), "boolean",
                  value_or_null(stale));
}

}  // namespace tdx::recon_contract_detail
