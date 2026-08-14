#include "research_internal.hpp"

namespace tdx {

using namespace detail::research;

Json normalize_research_master_rows(
    const Json& rows, const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& industry_names) {
    if (!rows.is_array()) throw Error("research master rows must be an array");
    const auto& spec = category_spec(category);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json record = Json::object(), entity = Json::object();
        std::string detail_id;
        if (spec.entity_type == "security") {
            const auto code = text_value(row, "$ZQDM1");
            if (!six_digits(code)) throw Error("research master security code is invalid");
            const int market_id = canonical_market_id(text_value(row, "$SC1"));
            entity = security_document(market_id, code, securities);
            detail_id = text_value(row, "$ZQDM");
        } else if (spec.entity_type == "industry") {
            const auto code = text_value(row, "$ZQDM");
            const auto raw_market = text_value(row, "$SC");
            detail_id = raw_market + code;
            const auto name = industry_names.find(code);
            entity["type"] = "industry";
            entity["market"] = raw_market.empty()
                ? "" : market_name(canonical_market_id(raw_market));
            entity["market_id"] = raw_market.empty()
                ? Json(nullptr) : Json(canonical_market_id(raw_market));
            entity["code"] = code;
            entity["id"] = detail_id;
            entity["name"] = name == industry_names.end() ? "" : name->second;
            entity["name_resolved"] = name != industry_names.end();
        } else {
            detail_id = text_value(row, "$ZQDM");
            entity["type"] = "institution";
            entity["id"] = detail_id;
            entity["code"] = detail_id;
            entity["name"] = copy_value(row, "jgmc");
            entity["name_resolved"] = !text_value(row, "jgmc").empty();
        }
        record["category"] = category;
        record["entity"] = std::move(entity);
        record["detail_id"] = detail_id;
        record["latest_date"] = first_value(row, {"date", "zxrq"});
        Json data = Json::object();
        map_fields(data, row, {
            {"industry", "hy"}, {"interaction_count_1m", "hdcs1"},
            {"interaction_count_3m", "hdcs2"}, {"research_count_1w", "yzdycs"},
            {"research_count_1m", "sydycs"}, {"research_count_3m", "bndycs"},
            {"research_count_6m", "yndycs"}, {"latest_institution_count", "cyjgsl"},
            {"institution_count_1m", "syjgsl"}, {"institution_count_3m", "bnjgsl"},
            {"institution_count_6m", "ynjgsl"}, {"return_pct_1m", "syzaf"},
            {"return_pct_3m", "bnzaf"}, {"return_pct_6m", "jnzaf"},
            {"post_st_date", "date1"}, {"latest_institution_count", "ycjgsl"},
            {"event_category", "lb"}, {"regulatory_measure", "jgcs"},
            {"subject", "sjry"}, {"subject_type", "rwlx"},
            {"permanent", "yjx"}, {"ban_years", "nx"}, {"count_10y", "cs"},
            {"institution_type", "jglx"}, {"researchers", "dbrw"},
            {"institution_research_count_1m", "dycs1"},
            {"institution_research_count_3m", "dycs2"},
            {"institution_research_count_6m", "dycs3"},
            {"institution_stock_count_1m", "dygs1"},
            {"institution_stock_count_3m", "dygs2"},
            {"institution_stock_count_6m", "dygs3"},
            {"industry_company_count", "hygss"}
        });
        if (category == "industry") {
            data["research_count_1m"] = copy_value(row, "yydycs");
            data["research_count_3m"] = copy_value(row, "sydycs");
            data["research_count_6m"] = copy_value(row, "bndycs");
            data["industry_coverage_pct_1m"] = copy_value(row, "yyjgsl");
            data["industry_coverage_pct_3m"] = copy_value(row, "syjgsl");
            data["industry_coverage_pct_6m"] = copy_value(row, "bnjgsl");
            data["return_pct_1m"] = copy_value(row, "yyzaf");
            data["return_pct_3m"] = copy_value(row, "syzaf");
            data["return_pct_6m"] = copy_value(row, "bnzaf");
        } else if (category == "post-st") {
            data["research_count_1m"] = copy_value(row, "yydycs");
            data["research_count_3m"] = copy_value(row, "sydycs");
            data["research_count_6m"] = copy_value(row, "bndycs");
            data["institution_count_1m"] = copy_value(row, "yyjgsl");
            data["institution_count_3m"] = copy_value(row, "syjgsl");
            data["institution_count_6m"] = copy_value(row, "bnjgsl");
            data["return_pct_1m"] = copy_value(row, "yyzaf");
            data["return_pct_3m"] = copy_value(row, "syzaf");
            data["return_pct_6m"] = copy_value(row, "bnzaf");
        }
        record["data"] = std::move(data);
        result.push_back(std::move(record));
    }
    return result;
}

}  // namespace tdx
