#include "ratings_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

using namespace ratings_detail;

std::string classify_rating_stance(const std::string& rating) {
    const auto value = trim(rating);
    if (value.empty() || value == "未知") return "unknown";
    const std::vector<std::string> negative{
        "卖出", "沽售", "减持", "跑输", "逊于", "低配", "回避"};
    for (const auto& marker : negative)
        if (value.find(marker) != std::string::npos) return "negative";
    const std::vector<std::string> neutral{
        "中性", "持有", "同步", "与大市一致", "市场表现"};
    for (const auto& marker : neutral)
        if (value.find(marker) != std::string::npos) return "neutral";
    const std::vector<std::string> positive{
        "买入", "增持", "推荐", "跑赢", "优于", "超配", "收集", "强于"};
    for (const auto& marker : positive)
        if (value.find(marker) != std::string::npos) return "positive";
    const auto english = lower_ascii(value);
    const std::vector<std::string> english_negative{
        "sell", "negative", "underperform", "underweight", "reduce"};
    for (const auto& marker : english_negative)
        if (english.find(marker) != std::string::npos) return "negative";
    const std::vector<std::string> english_neutral{
        "neutral", "hold", "market perform", "equal-weight", "sector weight"};
    for (const auto& marker : english_neutral)
        if (english.find(marker) != std::string::npos) return "neutral";
    const std::vector<std::string> english_positive{
        "strong buy", "buy", "positive", "outperform", "overweight", "accumulate"};
    for (const auto& marker : english_positive)
        if (english.find(marker) != std::string::npos) return "positive";
    return "unknown";
}

Json normalize_hong_kong_rating_rows(
    const Json& rows, const std::map<std::string, std::string>& names) {
    if (!rows.is_array()) throw Error("Hong Kong rating master rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 5) || text_value(row, "$SC") != "31") continue;
        const auto rating = text_value(row, "ZXPJ");
        Json item = Json::object();
        item["security"] = hong_kong_security_document(code, names);
        item["latest_report_date"] = text_value(row, "ZXRQ");
        item["latest_institution"] = text_value(row, "YJJG");
        item["latest_rating"] = rating;
        item["stance"] = classify_rating_stance(rating);
        item["latest_target_price_hkd"] = number_or_null(number_value(row, "ZXMBJ"));
        item["six_month_report_count"] = number_value(row, "JGSL").value_or(0.0);
        item["six_month_average_target_price_hkd"] =
            number_or_null(number_value(row, "MBJ"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_date = text_value(left, "latest_report_date");
            const auto right_date = text_value(right, "latest_report_date");
            if (left_date != right_date) return left_date > right_date;
            return number_value(left, "six_month_report_count").value_or(0.0) >
                number_value(right, "six_month_report_count").value_or(0.0);
        });
    return result;
}

Json normalize_united_states_rating_rows(const Json& rows) {
    if (!rows.is_array())
        throw Error("United States rating master rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = upper_ascii(text_value(row, "$ZQDM"));
        if (!united_states_symbol(code) || text_value(row, "$SC") != "74") continue;
        const auto rating = text_value(row, "ZXPJ");
        Json item = Json::object();
        item["security"] = united_states_security_document(
            code, text_value(row, "ZQJC"));
        item["latest_report_date"] = text_value(row, "YJRQ");
        item["latest_institution"] = text_value(row, "PJJG");
        item["latest_rating"] = rating;
        item["stance"] = classify_rating_stance(rating);
        item["latest_target_price_usd"] =
            number_or_null(number_value(row, "ZXMBJ"));
        item["institution_count"] = number_value(row, "JGS").value_or(0.0);
        item["industry"] = text_value(row, "hy");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_date = text_value(left, "latest_report_date");
            const auto right_date = text_value(right, "latest_report_date");
            if (left_date != right_date) return left_date > right_date;
            return number_value(left, "institution_count").value_or(0.0) >
                number_value(right, "institution_count").value_or(0.0);
        });
    return result;
}

Json normalize_industry_rating_rows(
    const Json& rows, const std::map<std::string, std::string>& names) {
    if (!rows.is_array()) throw Error("industry rating master rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6) || text_value(row, "$SC") != "1") continue;
        const auto rating = text_value(row, "ZXPJ");
        const double total = number_value(row, "JGSL").value_or(0.0);
        const double bullish = number_value(row, "KDS").value_or(0.0);
        const double bearish = number_value(row, "KKS").value_or(0.0);
        Json item = Json::object();
        item["industry"] = industry_document(code, names);
        item["latest_report_date"] = text_value(row, "ZXRQ");
        item["latest_institution"] = text_value(row, "YJJG");
        item["latest_rating"] = rating;
        item["latest_rating_change"] = text_value(row, "PJBH");
        item["stance"] = classify_rating_stance(rating);
        item["six_month_report_count"] = total;
        item["six_month_bullish_count"] = bullish;
        item["six_month_bearish_count"] = bearish;
        item["six_month_unclassified_count"] = std::max(0.0, total - bullish - bearish);
        item["six_month_bullish_ratio"] = total > 0.0
            ? Json(bullish / total) : Json(nullptr);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "six_month_report_count").value_or(0.0) >
                number_value(right, "six_month_report_count").value_or(0.0);
        });
    return result;
}

Json normalize_rating_report_rows(const Json& rows, const std::string& view) {
    if (!rows.is_array()) throw Error("rating detail rows must be an array");
    if (view != "hong-kong" && view != "us" && view != "industries")
        throw Error("rating report view must be hong-kong, us, or industries");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto rating = text_value(row, "ZXPJ");
        Json item = Json::object();
        item["report_date"] = text_value(row, view == "us" ? "YJRQ" : "BGRQ");
        item["institution"] = text_value(row, view == "us" ? "PJJG" : "YJJG");
        item["rating"] = rating;
        item["previous_rating"] = text_value(row, "SCPJ");
        item["rating_change"] = view == "industries" ? text_value(row, "PJBH")
            : view == "us" ? text_value(row, "TZLX") : std::string{};
        item["rating_changed"] = !item.at("previous_rating").as_string().empty() &&
            rating != item.at("previous_rating").as_string();
        item["stance"] = classify_rating_stance(rating);
        item["target_price_hkd"] = view == "hong-kong"
            ? number_or_null(number_value(row, "MBJ")) : Json(nullptr);
        item["target_price_usd"] = view == "us"
            ? number_or_null(number_value(row, "ZXMBJ")) : Json(nullptr);
        item["initial_price_usd"] = view == "us"
            ? number_or_null(number_value(row, "QCJ")) : Json(nullptr);
        item["reason"] = view == "us" ? std::string{} : text_value(row, "yy");
        item["related_code"] = text_value(row, "$ZQDM");
        if (view == "hong-kong") item["security_code"] = text_value(row, "$ZQDM1");
        if (view == "us") item["security_code"] = upper_ascii(text_value(row, "$ZQDM"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "report_date") > text_value(right, "report_date");
        });
    return result;
}

}  // namespace tdx
