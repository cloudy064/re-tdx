#include "technical_signals_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>

namespace tdx {
using namespace technical_signals_detail;
Json normalize_technical_signal_rows(
    const Json& rows, const TechnicalSignalsQuery& input,
    const BlockData& blocks) {
    if (!rows.is_array()) throw Error("technical signal rows must be an array");
    TechnicalSignalsQuery query = input;
    query.view = lower_ascii(trim(query.view));
    query.direction = lower_ascii(trim(query.direction));
    if (!known_view(query.view)) throw Error("unknown technical signal view: " + query.view);
    if (query.direction != "all" && query.direction != "up" && query.direction != "down")
        throw Error("direction must be all, up, or down");
    if (query.rps1 < 0) query.rps1 = query.view == "rps-block" ? 85 : 90;
    if (query.rps2 < 0) query.rps2 = query.view == "rps-block" ? 85 : 90;
    if (query.rps3 < 0) query.rps3 = query.view == "rps-block" ? 85 : 90;

    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        if (!row.is_object() || text(row, "code").empty()) continue;
        Json value = Json::object();
        if (query.view == "rps-block") value["block"] = block_document(row, blocks);
        else value["security"] = security_document(row, blocks);

        if (query.view == "nine-turn") {
            const auto type = static_cast<int>(number(row, "NTType").value_or(-1));
            value["direction"] = type == 0 ? "down" : type == 1 ? "up" : "unknown";
            value["signal_date"] = date_text(text(row, "rqSel"));
            value["selection_close"] = number_json(number(row, "closeSel"));
            value["since_signal_pct"] = number_json(number(row, "zdfSel"));
            value["five_day_growth_pct"] = number_json(number(row, "priceGrowth5"));
            value["beta"] = number_json(number_value(prefix_field(row, "beta")));
        } else if (query.view == "rps-stock" || query.view == "rps-block") {
            Json periods = Json::array();
            add_rps_period(periods, query.duration1, row, "RPS1", "Growth1");
            add_rps_period(periods, query.duration2, row, "RPS2", "Growth2");
            add_rps_period(periods, query.duration3, row, "RPS3", "Growth3");
            value["periods"] = std::move(periods);
        } else if (query.view == "new-high" || query.view == "new-low") {
            value["direction"] = query.view == "new-high" ? "high" : "low";
            value["break_date"] = date_text(text(row, "break_date"));
            value["period_days"] = number_json(number(row, "period"));
            value["extreme_price"] = number_json(number(row, "new_high_low"));
            value["retracement_pct"] = number_json(number(row, "retracement"));
            value["safety_score"] = number_json(number(row, "safeScore"));
        } else if (query.view == "breakout") {
            value["break_date"] = date_text(text(row, "break_date"));
            value["breakout_growth_pct"] = number_json(number(row, "priceGrowth"));
            value["close_price"] = number_json(number(row, "closePrice"));
            value["turnover_amount"] = number_json(number(row, "transaction"));
            value["safety_score"] = number_json(number(row, "safeScore"));
            value["sideways_period_days"] = number_json(number(row, "maxPeriod"));
            value["industry"] = text(row, "industry");
        } else if (query.view == "strong-start") {
            value["chosen_date"] = date_text(text(row, "chosen_date"));
            value["growth_since_chosen_pct"] = number_json(number(row, "growthFromChosen"));
            value["maximum_retracement_pct"] = number_json(number(row, "maxRetracement"));
            value["exit_date"] = date_text(text(row, "shiftOut_date"));
            value["exit_profit_pct"] = number_json(number(row, "shiftOut_profit"));
            value["exit_reason"] = text(row, "shiftOut_reason");
            value["safety_score"] = number_json(number(row, "safeScore"));
        } else if (const auto* opportunity = opportunity_signal(query.view)) {
            const auto reported_safety = number(row, "safety");
            value["strategy_title"] = opportunity->title;
            value["strategy_name"] = text(row, "rxyz");
            value["selection_time"] = factor_time_text(text(row, "rxsj"));
            value["since_selection_pct"] = number_json(number(row, "rxhzf"));
            value["reported_safety"] = number_json(reported_safety);
            value["safety_score"] = opportunity->safety_filter
                ? number_json(reported_safety) : Json(nullptr);
            value["safety_not_applicable"] =
                !opportunity->safety_filter && reported_safety && *reported_safety == -1.0;
            value["current_quote"] = Json(nullptr);
        } else if (const auto* signal = factor_signal(query.view)) {
            const auto code = text(row, "falg");
            value["strategy_title"] = signal->title;
            value["signal"] = factor_signal_semantics(*signal, code);
            value["selection_time"] = factor_time_text(text(row, "time"));
            value["selection_price"] = number_json(number(row, "value"));
            value["safety_score"] = number_json(number(row, "safetyvalue"));
            value["current_quote"] = Json(nullptr);
            value["since_selection_pct"] = Json(nullptr);
        } else if (const auto* strategy = auction_strategy(query.view)) {
            const auto yesterday_amount = number(row, "yesterdayAmount");
            const auto auction_amount = number(row,
                query.view == "limit-up-gap" || query.view == "five-minute-volume-surge"
                    ? "bidAmount" : "aggregateAuctionAmount");
            auto yesterday_growth = number(row, "yesterdayGrowth");
            if (query.view == "auction-volume-spike" && yesterday_growth)
                yesterday_growth = *yesterday_growth / 100.0;
            value["strategy_title"] = strategy->title;
            value["signal_phase"] = query.view == "five-minute-volume-surge"
                ? "opening-first-five-minutes" : "pre-open-auction";
            value["previous_day_change_pct"] = number_json(yesterday_growth);
            value["previous_day_amount"] = number_json(yesterday_amount);
            value["auction_change_pct"] = number_json(number(row,
                query.view == "limit-up-gap" || query.view == "five-minute-volume-surge"
                    ? "bidGrowth" : "openHighRate"));
            value["auction_amount"] = number_json(auction_amount);
            auto ratio = number(row, "bidYesAmountRatio");
            if (!ratio && yesterday_amount && auction_amount && *yesterday_amount != 0.0)
                ratio = *auction_amount * 100.0 / *yesterday_amount;
            value["auction_to_previous_amount_pct"] = number_json(ratio);
            if (query.view == "weak-limit-reversal") {
                value["previous_limit_open_count"] = number_json(number(row, "limitUpOpen"));
                value["previous_limit_reason"] = text(row, "reason");
                value["limit_sequence"] = text(row, "MDayNBan");
            } else if (query.view == "failed-limit-reversal") {
                value["previous_limit_break_pct"] = number_json(number(row, "zhaBanRate"));
                value["previous_limit_reason"] = text(row, "reason");
                value["limit_sequence"] = text(row, "MDayNBan");
            } else if (query.view == "upper-shadow-engulf") {
                value["previous_upper_wick_pct"] = number_json(number(row, "upperWick"));
            } else if (query.view == "limit-up-gap") {
                const auto style = number(row, "strongStyle");
                value["auction_style_code"] = number_json(style);
                value["auction_style"] = style && *style == 1.0 ? "涨停高开" :
                    style && *style == 0.0 ? "跌停冲高" : "unknown";
            } else if (query.view == "five-minute-volume-surge") {
                value["match_amount_at_0920"] = number_json(number(row, "bid920Amount"));
            }
        } else if (query.view == "event-driven" || model_strategy(query.view)) {
            value["change_pct"] = number_json(number(row, "zf"));
            value["open_change_pct"] = number_json(number(row, "kpzf"));
            value["turnover_pct"] = number_json(number(row, "sjhsl"));
            value["turnover_amount"] = number_json(number(row, "cje"));
            value["float_market_cap"] = number_json(number(row, "sjltsz"));
            if (query.view == "event-driven")
                value["three_day_change_pct"] = number_json(number(row, "zf_3d"));
            value["selection_time"] = time_text(text(row, "rxtime"));
        } else {
            value["direction"] = query.view == "trend-up" ? "up" : "down";
            value["duration_days"] = number_json(number(row, "tdcxsj"));
            value["support_price"] = number_json(number(row, "zcw"));
            value["distance_to_support_pct"] = number_json(number(row, "zcw_per"));
            value["resistance_price"] = number_json(number(row, "zlw"));
            value["distance_to_resistance_pct"] = number_json(number(row, "zlw_per"));
            value["current_price"] = number_json(number(row, "xj"));
            value["price_change"] = number_json(number(row, "zd"));
            value["change_pct"] = number_json(number(row, "zdf"));
            value["start_date"] = date_text(text(row, "tdqsrq"));
            value["since_start_pct"] = number_json(number(
                row, query.view == "trend-up" ? "qsrqzjzf" : "qsrqzjdf"));
            value["channel_safety_score"] = number_json(number(row, "aqxdf"));
            value["overall_safety_score"] = number_json(
                query.view == "trend-up" ? number(row, "jbmaqxdf") : number(row, "aqxdf"));
        }
        if (passes_client_filter(value, query)) result.push_back(std::move(value));
    }
    return result;
}

Json technical_signal_hits_for_security(
    const Json& document, const std::string& market, const std::string& code,
    const BlockData& blocks) {
    if (!document.is_object() || text(document, "schema") != "tdx-technical-signals-native-v1")
        throw Error("technical signal security lookup requires a native-v1 document");
    const auto* records = field(document, "records");
    if (!records || !records->is_array())
        throw Error("technical signal document has no records array");
    const int selected_market = market_id(market);
    const auto selected_code = trim(code);
    if (selected_code.size() != 6 ||
        !std::all_of(selected_code.begin(), selected_code.end(), ::isdigit))
        throw Error("security code must contain exactly six digits");
    const auto selected_id = market_prefix(selected_market) + selected_code;

    std::set<std::string> block_ids;
    std::set<std::string> block_codes;
    for (const auto& member : blocks.members) {
        if (member.market_id != selected_market || member.code != selected_code) continue;
        if (!member.block_id.empty()) block_ids.insert(member.block_id);
        if (!member.block_code.empty()) block_codes.insert(member.block_code);
    }

    Json hits = Json::array();
    const auto view = text(document, "view");
    for (const auto& record : records->as_array()) {
        if (!record.is_object()) continue;
        bool matched = false;
        std::string basis;
        if (const auto* security = field(record, "security"); security && security->is_object()) {
            matched = text(*security, "security_id") == selected_id;
            basis = "security";
        } else if (const auto* block = field(record, "block"); block && block->is_object()) {
            const auto block_id = text(*block, "block_id");
            const auto block_code = text(*block, "code");
            matched = (!block_id.empty() && block_ids.find(block_id) != block_ids.end()) ||
                      (!block_code.empty() && block_codes.find(block_code) != block_codes.end());
            basis = "block-membership";
        }
        if (!matched) continue;
        Json hit = Json::object();
        hit["view"] = view;
        hit["matched_via"] = basis;
        hit["record"] = record;
        hits.push_back(std::move(hit));
    }
    return hits;
}

}  // namespace tdx
