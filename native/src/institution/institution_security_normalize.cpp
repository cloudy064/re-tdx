#include "institution_internal.hpp"

#include <utility>

namespace tdx::institution_detail {

Json normalize_institution_history(const Json& rows) {
    if (!rows.is_array()) throw Error("institution history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["report_date"] = copy_value(row, "bgq");
        value["category"] = copy_value(row, "lb");
        value["institution_count"] = copy_value(row, "jgs");
        value["holding_shares"] = copy_value(row, "ccgs");
        value["free_float_share"] = copy_value(row, "cczlt");
        value["market_value"] = copy_value(row, "ccsz");
        value["change_shares"] = copy_value(row, "zjgs");
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_holders(const Json& rows, const std::string& reference_code) {
    if (!rows.is_array()) throw Error("top holder rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto reference = parse_holder_reference(text_value(row, "gdjc"), reference_code);
        Json value = Json::object();
        value["rank"] = copy_value(row, "gdpm");
        value["type"] = copy_value(row, "gdlx");
        value["name"] = copy_value(row, "gdmc");
        value["short_name"] = copy_value(row, "gdjc");
        value["holding_shares"] = copy_value(row, "cgsl");
        value["free_float_share"] = copy_value(row, "zb1");
        value["change_shares"] = copy_value(row, "zjgs");
        value["change_label"] = copy_value(row, "zl");
        value["holder_id"] = reference.at("holder_id");
        value["variant_id"] = reference.at("variant_id");
        value["reference_code"] = reference.at("reference_code");
        value["source_url"] = reference.at("source_url");
        value["queryable"] = reference.at("queryable");
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        result[key] = document.at(key);
    return result;
}

}  // namespace tdx::institution_detail
