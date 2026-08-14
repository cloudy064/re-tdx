#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <string_view>

namespace tdx {
namespace {

enum class ConvertibleBondView { listed, pending, subscriptions, pricing };

struct ViewDefinition {
    std::string_view name;
    ConvertibleBondView view;
};

constexpr std::array<ViewDefinition, 4> kViews{{
    {"listed", ConvertibleBondView::listed},
    {"pending", ConvertibleBondView::pending},
    {"subscriptions", ConvertibleBondView::subscriptions},
    {"pricing", ConvertibleBondView::pricing},
}};

constexpr bool unique_views() {
    for (std::size_t left = 0; left < kViews.size(); ++left)
        for (std::size_t right = left + 1; right < kViews.size(); ++right)
            if (kViews[left].name == kViews[right].name) return false;
    return true;
}

static_assert(unique_views(), "convertible-bond views must be unique");

ConvertibleBondView resolve_view(std::string_view name) {
    for (const auto& definition : kViews)
        if (definition.name == name) return definition.view;
    throw Error("view must be listed, pending, subscriptions, or pricing");
}

}  // namespace

using namespace convertible_bond_detail;

Json ConvertibleBondService::query(const ConvertibleBondQuery& options) {
    if (options.limit < 1 || options.limit > 10000) throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.quote_cache_ttl_seconds < 0 || options.quote_cache_ttl_seconds > 3600)
        throw Error("quote_cache_ttl_seconds must be in 0..3600");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const auto view_name = lower_ascii(trim(options.view.empty() ? "listed" : options.view));
    const auto view = resolve_view(view_name);
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        if (!valid_code(options.code)) throw Error("code must contain six digits");
    }
    switch (view) {
        case ConvertibleBondView::subscriptions:
            return query_subscriptions(options, selected_market);
        case ConvertibleBondView::pricing:
            return query_pricing(options, selected_market);
        case ConvertibleBondView::pending:
            return query_pending(options, selected_market);
        case ConvertibleBondView::listed:
            return query_listed(options, selected_market);
    }
    throw Error("unreachable convertible-bond view");
}

}  // namespace tdx

