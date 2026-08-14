#include "institution_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

using namespace institution_detail;

Json parse_holder_reference(const std::string& url,
                            const std::string& fallback_reference_code) {
    std::map<std::string, std::string> params;
    const auto question = url.find('?');
    if (question != std::string::npos) {
        std::size_t offset = question + 1;
        while (offset <= url.size()) {
            const auto end = url.find('&', offset);
            const auto item = std::string_view(url).substr(offset,
                end == std::string::npos ? url.size() - offset : end - offset);
            if (!item.empty()) {
                const auto equals = item.find('=');
                const auto name = lower_ascii(percent_decode(item.substr(0, equals)));
                const auto value = equals == std::string_view::npos
                    ? std::string{} : percent_decode(item.substr(equals + 1));
                if (!name.empty() && !params.count(name)) params[name] = value;
            }
            if (end == std::string::npos) break;
            offset = end + 1;
        }
    }
    const auto field = [&](const std::string& name) {
        const auto found = params.find(name);
        return found == params.end() ? std::string{} : trim(found->second);
    };
    const auto holder_id = field("gdid");
    const auto variant_id = field("tdxid");
    auto reference_code = field("gp");
    if (!six_digits(reference_code)) reference_code = fallback_reference_code;
    Json result = Json::object();
    result["holder_id"] = holder_id;
    result["variant_id"] = variant_id;
    result["holder_name"] = field("gdname");
    result["reference_code"] = reference_code;
    result["source_url"] = url;
    result["queryable"] = !holder_id.empty() && six_digits(reference_code);
    return result;
}

Json tqlex_result_rows(const Json& response) {
    if (!response.is_object()) throw Error("holder TQLEX response must be an object");
    const auto sets_found = response.as_object().find("ResultSets");
    if (sets_found == response.as_object().end() || !sets_found->second.is_array() ||
        sets_found->second.as_array().empty()) return Json::array();
    const auto& set = sets_found->second.as_array().front();
    if (!set.is_object()) throw Error("holder TQLEX result set must be an object");
    const auto columns_found = set.as_object().find("ColName");
    const auto content_found = set.as_object().find("Content");
    if (columns_found == set.as_object().end() || !columns_found->second.is_array())
        throw Error("holder TQLEX ColName is missing");
    if (content_found == set.as_object().end()) return Json::array();
    if (!content_found->second.is_array()) throw Error("holder TQLEX Content must be an array");
    std::vector<std::string> columns;
    for (const auto& value : columns_found->second.as_array()) columns.push_back(scalar_text(value));
    Json result = Json::array();
    for (const auto& row : content_found->second.as_array()) {
        if (row.is_object()) {
            result.push_back(row);
            continue;
        }
        if (!row.is_array()) throw Error("holder TQLEX row must be an array");
        Json value = Json::object();
        for (std::size_t index = 0; index < columns.size(); ++index)
            value[columns[index]] = index < row.as_array().size()
                ? row.as_array()[index] : Json(nullptr);
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_holder_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("holder history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = text_value(row, "sc");
        int id = -1;
        try { id = market_id(market); } catch (...) {}
        const auto code = text_value(row, "zqdm");
        Json value = Json::object();
        value["row_id"] = copy_value(row, "T001");
        value["currently_held"] = text_value(row, "bh") == "1";
        value["stock_record_count"] = copy_value(row, "cnt");
        value["report_date"] = copy_value(row, "rq");
        value["market"] = id < 0 ? market : market_name(id);
        value["market_id"] = id < 0 ? Json(nullptr) : Json(id);
        value["code"] = code;
        value["security_id"] = id < 0 || !six_digits(code)
            ? code : market_prefix(id) + code;
        value["name"] = copy_value(row, "zqjc");
        value["holding_shares"] = copy_value(row, "T006");
        value["share_percent"] = copy_value(row, "T007");
        value["shareholder_list_type"] = copy_value(row, "stype");
        value["share_nature"] = copy_value(row, "T012");
        value["change_shares"] = copy_value(row, "T008");
        value["change_type"] = copy_value(row, "T009");
        value["change_label"] = change_label(text_value(row, "T009"));
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_holder_stock_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("holder stock rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["report_date"] = copy_value(row, "rq");
        value["holding_shares"] = copy_value(row, "T006");
        value["share_percent"] = copy_value(row, "T007");
        value["share_nature"] = copy_value(row, "T012");
        value["change_type"] = copy_value(row, "T009");
        value["change_label"] = change_label(text_value(row, "T009"));
        value["change_shares"] = copy_value(row, "T008");
        value["shareholder_list_type"] = copy_value(row, "stype");
        result.push_back(std::move(value));
    }
    return result;
}

}  // namespace tdx
