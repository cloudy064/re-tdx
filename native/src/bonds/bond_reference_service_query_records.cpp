#include "bond_reference_internal.hpp"

namespace tdx::bond_reference_detail {

const BondReferenceSource& resolve_query(BondReferenceQuery& options) {
    options.group = lower_ascii(trim(options.group));
    options.bucket = lower_ascii(trim(options.bucket));
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = lower_ascii(trim(options.query));
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (options.group != "rating" && options.group != "rate" &&
        options.group != "category")
        throw Error("bond-reference group must be rating, rate, or category");
    const BondReferenceSource* selected_source = nullptr;
    for (const auto& source : bond_reference_sources())
        if (source.group == options.group && source.bucket == options.bucket) {
            selected_source = &source;
            break;
        }
    if (!selected_source)
        throw Error("unknown bond-reference bucket for selected group: " + options.bucket);
    if (!std::set<std::string>{"name", "code", "maturity", "rate", "remaining"}
             .count(options.sort))
        throw Error("bond-reference sort must be name, code, maturity, rate, or remaining");
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");
    if (options.offset < 0 || options.offset > 1000000 ||
        options.limit < 1 || options.limit > 5000)
        throw Error("bond-reference paging is outside the supported range");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400 ||
        options.timeout_ms < 100 || options.timeout_ms > 120000)
        throw Error("bond-reference cache/timeout is outside the supported range");
    return *selected_source;
}

Json collect_matching_rows(const Json& raw_rows,
                           const BondReferenceSource& source,
                           const BondReferenceQuery& options,
                           RowFacets& facets) {
    Json filtered = Json::array();
    for (std::size_t index = 0; index < raw_rows.size(); ++index) {
        auto row = normalize_row(raw_rows.as_array()[index], source, false, index);
        const auto& security = row.at("security");
        if (!options.market.empty() &&
            lower_ascii(security.at("market").as_string()) != options.market &&
            std::to_string(static_cast<int>(security.at("market_id").as_number())) !=
                options.market)
            continue;
        if (!options.code.empty() && security.at("code").as_string() != options.code)
            continue;
        if (!contains_text(row, options.query)) continue;
        facets.matched_security_ids.insert(security.at("security_id").as_string());
        const auto credit = row.at("bond_credit_rating").as_string();
        const auto rate = row.at("rate_type").as_string();
        const auto type = row.at("bond_type").as_string();
        if (!credit.empty()) ++facets.credit_counts[credit];
        if (!rate.empty()) ++facets.rate_counts[rate];
        if (!type.empty()) ++facets.type_counts[type];
        const auto maturity = row.at("maturity_date").as_string();
        if (!maturity.empty()) {
            if (facets.earliest_maturity.empty() || maturity < facets.earliest_maturity)
                facets.earliest_maturity = maturity;
            if (facets.latest_maturity.empty() || maturity > facets.latest_maturity)
                facets.latest_maturity = maturity;
        }
        filtered.push_back(std::move(row));
    }
    return filtered;
}

void sort_matching_rows(Json& matched_rows, const BondReferenceQuery& options) {
    const bool desc = options.order == "desc";
    auto text_key = [&](const Json& row) -> std::string {
        if (options.sort == "maturity") return row.at("maturity_date").as_string();
        if (options.sort == "name") return row.at("security").at("name").as_string();
        return row.at("security").at("code").as_string();
    };
    auto numeric_key = [&](const Json& row) -> double {
        const auto& value = row.at(options.sort == "rate"
            ? "current_coupon_rate_pct" : "remaining_years");
        return value.is_number() ? value.as_number() :
            (desc ? -std::numeric_limits<double>::infinity()
                  : std::numeric_limits<double>::infinity());
    };
    std::stable_sort(matched_rows.as_array().begin(), matched_rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (options.sort == "rate" || options.sort == "remaining") {
                const auto a = numeric_key(left), b = numeric_key(right);
                if (a != b) return desc ? a > b : a < b;
            } else {
                const auto a = text_key(left), b = text_key(right);
                if (a != b) return desc ? a > b : a < b;
            }
            return left.at("security").at("security_id").as_string() <
                   right.at("security").at("security_id").as_string();
        });
}

Json paginate_rows(const Json& matched_rows, const Json& raw_rows,
                   const BondReferenceSource& source,
                   const BondReferenceQuery& options) {
    Json records = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < matched_rows.size() &&
             records.size() < static_cast<std::size_t>(options.limit);
         ++index) {
        const auto raw_index = static_cast<std::size_t>(
            matched_rows.as_array()[index].at("_raw_index").as_number());
        records.push_back(normalize_row(raw_rows.as_array()[raw_index],
                                        source, true, raw_index));
    }
    return records;
}

}  // namespace tdx::bond_reference_detail
