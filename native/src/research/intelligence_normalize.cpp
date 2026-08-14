#include "tdx/intelligence.hpp"

#include "intelligence_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace intelligence_detail;

Json normalize_attention_rows(const Json &rows,
                              const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("attention rows must be an array");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6))
            continue;
        int id = 0;
        try {
            id = market_id(text_value(row, "$SC"));
        } catch (...) {
            continue;
        }
        const auto intraday = number_value(row, "tdx2");
        const auto after_hours = number_value(row, "tdx3");
        const auto pro_intraday = number_value(row, "zytdx1");
        const auto pro_after_hours = number_value(row, "zytdx2");
        const auto resonance_intraday = number_value(row, "tdx5");
        const auto resonance_after_hours = number_value(row, "tdx6");
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["date"] = text_value(row, "date");
        value["listing_status"] = text_value(row, "sszt");
        value["rank"] = number_json(number_value(row, "tdx7"));
        value["rank_change"] = number_json(number_value(row, "tdx8"));
        value["attention_total"] = sum_json(intraday, after_hours);
        value["attention_intraday"] = number_json(intraday);
        value["attention_after_hours"] = number_json(after_hours);
        value["professional_attention_total"] = sum_json(pro_intraday, pro_after_hours);
        value["professional_attention_intraday"] = number_json(pro_intraday);
        value["professional_attention_after_hours"] = number_json(pro_after_hours);
        value["resonance_total"] = sum_json(resonance_intraday, resonance_after_hours);
        value["resonance_intraday"] = number_json(resonance_intraday);
        value["resonance_after_hours"] = number_json(resonance_after_hours);
        value["resonance_change"] = number_json(number_value(row, "tdx16"));
        value["sentiment_heat"] = number_json(number_value(row, "xq1"));
        value["sentiment_weekly_new"] = number_json(number_value(row, "xq4"));
        value["bullish_votes_month"] = number_json(number_value(row, "kds"));
        value["bearish_votes_month"] = number_json(number_value(row, "kks"));
        value["bullish_ratio_pct"] = number_json(number_value(row, "kdzb"));
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
                     [](const Json &left, const Json &right) {
                         return number_value(left, "rank").value_or(1e12) <
                                number_value(right, "rank").value_or(1e12);
                     });
    return result;
}

Json normalize_value_attention_categories(
    const Json &rows, const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("value-attention category rows must be an array");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto category_id = text_value(row, "$ZQDM");
        if (!digit_identifier(category_id))
            continue;
        const auto keys = member_keys(text_value(row, "$S_ZQDM"));
        Json members = Json::array();
        for (const auto &[market, code] : keys)
            members.push_back(security_json(market, code, securities));
        Json value = Json::object();
        value["category_id"] = category_id;
        value["category_name"] = text_value(row, "flname");
        value["reported_member_count"] = number_json(number_value(row, "scount"));
        value["inline_member_count"] = static_cast<std::uint64_t>(members.size());
        value["members"] = std::move(members);
        value["member_source"] = "inline-master";
        value["source_resource"] = value_attention_resource;
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_value_attention_detail_rows(
    const Json &rows, const std::string &category_id,
    const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("value-attention detail rows must be an array");
    if (!digit_identifier(category_id))
        throw Error("value-attention category id must be numeric");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6))
            continue;
        int market = 0;
        try {
            market = market_id(text_value(row, "$SC"));
        } catch (...) {
            continue;
        }
        Json value = Json::object();
        value["category_id"] = category_id;
        value["security"] = security_json(market, code, securities);
        value["anchor_price_yuan"] = number_json(number_value(row, "aqjg"));
        value["adjusted_anchor_price_yuan"] = number_json(number_value(row, "fqaqjg"));
        value["three_month_adjusted_close_yuan"] = number_json(number_value(row, "price1"));
        value["breach_depth_pct"] = Json(nullptr);
        value["source_resource"] = "jzgz1/" + category_id + ".jsn";
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_risk_rows(const Json &rows, const std::string &category,
                         const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("risk rows must be an array");
    if (category != "observation" && category != "potential" && category != "discredited")
        throw Error("normalized risk category must be observation, potential, or discredited");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const bool discredited = category == "discredited";
        const auto code = text_value(row, discredited ? "$ZQDM1" : "$ZQDM");
        if (!digits(code, 6))
            continue;
        int id = 0;
        try {
            id = market_id(text_value(row, discredited ? "$SC1" : "$SC"));
        } catch (...) {
            continue;
        }
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["category"] = category;
        value["category_label"] = category == "observation" ? "风险观察"
                                  : category == "potential" ? "潜在爆雷"
                                                            : "失信被执行";
        value["risk_type"] = discredited ? text_value(row, "rwlx") : text_value(row, "bllx");
        value["detail"] = discredited ? text_value(row, "sjry") : text_value(row, "blxq");
        value["safety_score"] =
            discredited ? Json(nullptr) : number_json(number_value(row, "zxfs"));
        value["announcement_date"] = discredited ? text_value(row, "date") : "";
        value["involved_subject"] = discredited ? text_value(row, "sjry") : "";
        value["object_type"] = discredited ? text_value(row, "rwlx") : "";
        value["occurrences_past_year"] =
            discredited ? number_json(number_value(row, "cs")) : Json(nullptr);
        value["record_id"] = category + ":" + market_prefix(id) + code + ":" +
                             (discredited ? text_value(row, "date") + ":" + text_value(row, "$ZQDM")
                                          : text_value(row, "bllx"));
        value["source_resource"] = discredited                 ? discredited_resource
                                   : category == "observation" ? observation_resource
                                                               : potential_resource;
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
                     [category](const Json &left, const Json &right) {
                         if (category == "discredited")
                             return text_value(left, "announcement_date") >
                                    text_value(right, "announcement_date");
                         return number_value(left, "safety_score").value_or(1e12) <
                                number_value(right, "safety_score").value_or(1e12);
                     });
    return result;
}

Json normalize_highlight_rows(const Json &rows,
                              const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("highlight rows must be an array");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6))
            continue;
        int id = 0;
        try {
            id = market_id(text_value(row, "$SC"));
        } catch (...) {
            continue;
        }
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["highlight_count"] = number_json(number_value(row, "lds"));
        value["primary_highlight_type"] = text_value(row, "ldlx");
        value["highlight_detail"] = text_value(row, "ldxq");
        value["safety_score"] = number_json(number_value(row, "zxfs"));
        value["highlight_score"] = number_json(number_value(row, "ldfz"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

void sort_highlight_rows(Json &rows, const std::string &sort_value,
                         const std::string &order_value) {
    if (!rows.is_array())
        throw Error("highlight sort requires an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    if (!std::set<std::string>{"highlight-count", "safety-score", "highlight-score", "code"}.count(
            sort))
        throw Error(
            "highlight sort must be highlight-count, safety-score, highlight-score, or code");
    if (order != "asc" && order != "desc")
        throw Error("highlight order must be asc or desc");
    const bool descending = order == "desc";
    auto metric = [&](const Json &row) -> std::optional<double> {
        if (sort == "highlight-count")
            return number_value(row, "highlight_count");
        if (sort == "safety-score")
            return number_value(row, "safety_score");
        if (sort == "highlight-score")
            return number_value(row, "highlight_score");
        return std::nullopt;
    };
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
                     [&](const Json &left, const Json &right) {
                         const auto &left_security = left.at("security");
                         const auto &right_security = right.at("security");
                         if (sort == "code") {
                             const auto a = text_value(left_security, "security_id");
                             const auto b = text_value(right_security, "security_id");
                             if (a != b)
                                 return descending ? a > b : a < b;
                         } else {
                             const auto a = metric(left);
                             const auto b = metric(right);
                             if (a.has_value() != b.has_value())
                                 return a.has_value();
                             if (a && b && *a != *b)
                                 return descending ? *a > *b : *a < *b;
                         }
                         return text_value(left_security, "security_id") <
                                text_value(right_security, "security_id");
                     });
}

Json intelligence_detail::highlight_summary(const Json &rows) {
    std::map<std::string, std::uint64_t> types;
    std::map<std::string, std::uint64_t> markets;
    std::uint64_t resolved = 0, safety_count = 0;
    double safety_sum = 0.0;
    std::optional<double> minimum_safety, maximum_safety;
    for (const auto &row : rows.as_array()) {
        const auto type = text_value(row, "primary_highlight_type");
        if (!type.empty())
            ++types[type];
        const auto &security = row.at("security");
        ++markets[text_value(security, "market")];
        const auto *name_resolved = ptr(security, "name_resolved");
        if (name_resolved && name_resolved->is_bool() && name_resolved->as_bool())
            ++resolved;
        const auto safety = number_value(row, "safety_score");
        if (safety) {
            ++safety_count;
            safety_sum += *safety;
            minimum_safety = minimum_safety ? std::min(*minimum_safety, *safety) : *safety;
            maximum_safety = maximum_safety ? std::max(*maximum_safety, *safety) : *safety;
        }
    }
    Json by_type = Json::array();
    for (const auto &[type, count] : types) {
        Json item = Json::object();
        item["type"] = type;
        item["count"] = count;
        by_type.push_back(std::move(item));
    }
    std::stable_sort(by_type.as_array().begin(), by_type.as_array().end(),
                     [](const Json &left, const Json &right) {
                         const auto a = left.at("count").as_number();
                         const auto b = right.at("count").as_number();
                         if (a != b)
                             return a > b;
                         return left.at("type").as_string() < right.at("type").as_string();
                     });
    Json by_market = Json::object();
    for (const auto &[market, count] : markets)
        by_market[market] = count;
    Json result = Json::object();
    result["securities"] = static_cast<std::uint64_t>(rows.size());
    result["names_resolved"] = resolved;
    result["type_count"] = static_cast<std::uint64_t>(types.size());
    result["by_type"] = std::move(by_type);
    result["by_market"] = std::move(by_market);
    result["minimum_safety_score"] = number_json(minimum_safety);
    result["maximum_safety_score"] = number_json(maximum_safety);
    result["average_safety_score"] =
        safety_count ? Json(safety_sum / static_cast<double>(safety_count)) : Json(nullptr);
    return result;
}

Json normalize_intelligence_event_rows(
    const Json &rows, const std::string &source,
    const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("intelligence event rows must be an array");
    if (source != "events" && source != "ministries" && source != "hotspots")
        throw Error("intelligence event source is invalid");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto raw_id = text_value(row, "$ZQDM");
        const auto title =
            source == "hotspots" ? text_value(row, "zqmc") : text_value(row, "title");
        if (raw_id.empty() || title.empty())
            continue;
        Json members = Json::array();
        for (const auto &[id, code] : member_keys(text_value(row, "$S_ZQDM")))
            members.push_back(security_json(id, code, securities));
        Json value = Json::object();
        value["event_id"] = source + ":" + raw_id;
        value["raw_id"] = raw_id;
        value["source"] = source;
        value["source_label"] = source == "events"       ? "事件驱动"
                                : source == "ministries" ? "部委要闻"
                                                         : "热点映射";
        value["date"] = text_value(row, "date");
        value["title"] = title;
        value["type"] = source == "hotspots" ? "热点" : text_value(row, "type");
        value["organization"] = text_value(row, "bw");
        value["content"] =
            source == "hotspots" ? text_value(row, "yy") : text_value(row, "Contents");
        value["importance"] = number_json(number_value(row, "zycd"));
        value["stock_count_reported"] = number_json(number_value(row, "sl"));
        value["days"] = number_json(number_value(row, "ts"));
        value["member_count"] = static_cast<std::uint64_t>(members.size());
        value["members"] = std::move(members);
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    sort_events(result);
    return result;
}

Json normalize_intelligence_topic_rows(const Json &rows) {
    if (!rows.is_array())
        throw Error("intelligence topic rows must be an array");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto id = text_value(row, "$ZQDM");
        const auto name = text_value(row, "MC");
        if (!digit_identifier(id) || name.empty())
            continue;
        Json value = Json::object();
        value["topic_id"] = id;
        value["created_date"] = text_value(row, "DATE");
        value["name"] = name;
        value["category"] = text_value(row, "LX");
        value["description"] = text_value(row, "MS");
        value["updated_date"] = text_value(row, "gxdate");
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
                     [](const Json &left, const Json &right) {
                         return text_value(left, "updated_date") >
                                text_value(right, "updated_date");
                     });
    return result;
}

Json normalize_intelligence_timeline_rows(const Json &rows, const std::string &kind) {
    if (!rows.is_array())
        throw Error("intelligence timeline rows must be an array");
    if (kind != "topic" && kind != "news")
        throw Error("intelligence timeline kind must be topic or news");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto rich = parse_rich_text(text_value(row, "title"));
        if (rich.headline.empty())
            continue;
        Json value = Json::object();
        value["kind"] = kind;
        value["date"] = text_value(row, "date");
        value["headline"] = rich.headline;
        value["content"] = rich.content;
        value["source_url"] = rich.source_url.empty() ? Json(nullptr) : Json(rich.source_url);
        value["has_embedded_content"] = !rich.content.empty();
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
                     [](const Json &left, const Json &right) {
                         return text_value(left, "date") > text_value(right, "date");
                     });
    return result;
}

Json normalize_market_anomaly_rows(const Json &rows) {
    if (!rows.is_array())
        throw Error("market anomaly rows must be an array");
    Json result = Json::array();
    for (const auto &row : rows.as_array()) {
        const auto previous = number_value(row, "szzs1");
        const auto close = number_value(row, "szzs2");
        const auto week_close = number_value(row, "szzs9");
        const auto sh_volume = number_value(row, "CJL3");
        const auto sz_volume = number_value(row, "CJL1");
        const auto sh_turnover = number_value(row, "szzs6");
        const auto sz_turnover = number_value(row, "szzs12");
        Json value = Json::object();
        value["date"] = text_value(row, "fsrq");
        value["anomaly_type"] = text_value(row, "sblx");
        value["previous_close"] = number_json(previous);
        value["close"] = number_json(close);
        value["change_points"] = previous && close ? Json(*close - *previous) : Json(nullptr);
        value["same_day_change_pct"] = previous && close && *previous != 0.0
                                           ? Json((*close - *previous) * 100.0 / *previous)
                                           : Json(nullptr);
        value["previous_day_change_pct"] = number_json(number_value(row, "szzs7"));
        value["next_day_change_pct"] = number_json(number_value(row, "szzs8"));
        value["week_later_close"] = number_json(week_close);
        value["next_week_change_pct"] = close && week_close && *close != 0.0
                                            ? Json((*week_close - *close) * 100.0 / *close)
                                            : Json(nullptr);
        value["limit_up_count"] = number_json(number_value(row, "ztjs"));
        value["limit_down_count"] = number_json(number_value(row, "dtjs"));
        value["sh_volume"] = number_json(sh_volume);
        value["sz_volume"] = number_json(sz_volume);
        value["market_volume"] = sum_json(sh_volume, sz_volume);
        value["sh_turnover_yuan"] = number_json(sh_turnover);
        value["sz_turnover_yuan"] = number_json(sz_turnover);
        value["market_turnover_yuan"] = sum_json(sh_turnover, sz_turnover);
        value["reason"] = text_value(row, "ydyy");
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
                     [](const Json &left, const Json &right) {
                         return text_value(left, "date") > text_value(right, "date");
                     });
    return result;
}

Json normalize_intelligence_event_member_rows(
    const Json &rows, const std::map<std::pair<int, std::string>, Security> &securities) {
    if (!rows.is_array())
        throw Error("intelligence event member rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto &row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6))
            continue;
        try {
            const int id = market_id(text_value(row, "$SC"));
            if (seen.insert({id, code}).second)
                result.push_back(security_json(id, code, securities));
        } catch (...) {
        }
    }
    return result;
}

} // namespace tdx
