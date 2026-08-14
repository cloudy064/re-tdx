#include "funds_internal.hpp"

#include <algorithm>

namespace tdx {
Json IntradayFundsService::query(const IntradayFundsQuery& options) {
    using namespace detail::funds;
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 3600)
        throw Error("cache_ttl_seconds must be in 0..3600");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool has_security = !options.market.empty() || !options.code.empty();
    const int modes = (has_security ? 1 : 0) + (!options.industry.empty() ? 1 : 0) +
                      (options.all_industries ? 1 : 0);
    if (modes > 1) throw Error("choose security, industry, or all-industries mode");
    auto master = fetch_master(options);
    if (options.all_industries) return build_all(master, options);
    if (!options.industry.empty()) {
        if (options.industry.size() != 6 || options.industry.rfind("881", 0) != 0)
            throw Error("industry must be a six-digit 881xxx code");
        auto detail = fetch_detail("1", options.industry, options);
        return build_industry(options.industry, master, detail, options);
    }
    if (has_security) {
        if (options.market.empty() || options.code.empty())
            throw Error("security mode requires both market and code");
        const int market_id = market_id_from_text(options.market);
        if (options.code.size() != 6 || !std::all_of(
                options.code.begin(), options.code.end(),
                [](char ch) { return ch >= '0' && ch <= '9'; }))
            throw Error("code must contain six digits");
        const auto* industry = industry_for_security(market_id, options.code);
        if (!industry)
            throw Error("security has no local level-1 research-industry assignment");
        auto detail = fetch_detail(industry->market, industry->code, options);
        return build_security(market_id, options.code, *industry, master, detail, options);
    }
    return build_master(master, options);
}
}  // namespace tdx
