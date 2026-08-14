#include "tdx/technical_signals.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json rows(std::initializer_list<tdx::Json> values) {
    tdx::Json result = tdx::Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

tdx::Json document(const std::string& view, tdx::Json records,
                   const std::string& generated_at = "2026-08-06T09:00:00+0800") {
    tdx::Json result = tdx::Json::object();
    result["schema"] = "tdx-technical-signals-native-v1";
    result["view"] = view;
    result["generated_at"] = generated_at;
    result["parameters"] = tdx::Json::parse("{\"client_filters_applied\":true}");
    result["records"] = std::move(records);
    return result;
}

}  // namespace

int main() {
    try {
        tdx::BlockData blocks;
        blocks.securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        tdx::Block block;
        block.block_id = "research-industry:X1";
        block.family = "research-industry";
        block.block_code = "880668";
        block.name = "测试板块";
        blocks.blocks.push_back(block);
        tdx::BlockMember member;
        member.block_id = "research-industry:X1";
        member.block_code = "880668";
        member.market_id = 0;
        member.code = "000001";
        blocks.members.push_back(member);

        tdx::TechnicalSignalsQuery query;
        query.view = "nine-turn";
        query.direction = "down";
        const auto nine = tdx::normalize_technical_signal_rows(rows({tdx::Json::parse(
            "{\"code\":\"000001\",\"market\":\"0\",\"beta-value\":\"-0.19\","
            "\"priceGrowth5\":\"-0.27\",\"closeSel\":\"11.62\","
            "\"zdfSel\":\"-0.37\",\"rqSel\":\"20260803\",\"NTType\":\"0\"}")}),
            query, blocks);
        require(nine.size() == 1 &&
                    nine.as_array()[0].at("security").at("name").as_string() == "平安银行" &&
                    nine.as_array()[0].at("signal_date").as_string() == "2026-08-03" &&
                    std::abs(nine.as_array()[0].at("beta").as_number() + 0.19) < 1e-9,
                "nine-turn rows preserve direction, dates, beta and local security identity");

        query = {};
        query.view = "rps-block";
        const auto rps = tdx::normalize_technical_signal_rows(rows({tdx::Json::parse(
            "{\"code\":\"880668\",\"market\":\"1\",\"RPS1\":\"92.95\","
            "\"Growth1\":\"13.33\",\"RPS2\":\"98.74\",\"Growth2\":\"15.00\","
            "\"RPS3\":\"85.14\",\"Growth3\":\"-9.21\"}")}), query, blocks);
        require(rps.size() == 1 &&
                    rps.as_array()[0].at("block").at("block_id").as_string() ==
                        "research-industry:X1" &&
                    rps.as_array()[0].at("periods").as_array()[2].at("days").as_number() == 60 &&
                    std::abs(rps.as_array()[0].at("periods").as_array()[0]
                                  .at("growth_pct").as_number() - 13.33) < 1e-9,
                "block RPS resolves the local block and keeps three configured periods");

        query = {};
        query.view = "new-high";
        const auto highs = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse("{\"code\":\"000001\",\"market\":\"0\","
                             "\"break_date\":\"20260804\",\"period\":\"295\","
                             "\"new_high_low\":\"11.62\",\"retracement\":\"-3.43\","
                             "\"safeScore\":\"92.00\"}"),
            tdx::Json::parse("{\"code\":\"000002\",\"market\":\"0\","
                             "\"safeScore\":\"59\"}")}), query, blocks);
        require(highs.size() == 1 &&
                    highs.as_array()[0].at("direction").as_string() == "high" &&
                    highs.as_array()[0].at("period_days").as_number() == 295,
                "new-high normalization applies the client safety-score filter");

        query = {};
        query.view = "breakout";
        const auto breakout = tdx::normalize_technical_signal_rows(rows({tdx::Json::parse(
            "{\"code\":\"000001\",\"market\":\"0\",\"break_date\":\"20260804\","
            "\"priceGrowth\":\"1.78%\",\"closePrice\":\"12.56\","
            "\"transaction\":\"323,403,808.00\",\"safeScore\":\"100\","
            "\"maxPeriod\":\"20\",\"industry\":\"银行\"}")}), query, blocks);
        require(std::abs(breakout.as_array()[0].at("breakout_growth_pct").as_number() - 1.78) < 1e-9 &&
                    breakout.as_array()[0].at("turnover_amount").as_number() == 323403808.0,
                "breakout normalization parses percent and thousands-formatted numbers");

        query = {};
        query.view = "event-driven";
        const auto events = tdx::normalize_technical_signal_rows(rows({tdx::Json::parse(
            "{\"code\":\"000001\",\"market\":\"0\",\"zf\":\"13.1\","
            "\"kpzf\":\"1.1\",\"sjhsl\":\"17.2\",\"cje\":\"1000\","
            "\"sjltsz\":\"2000\",\"zf_3d\":\"7.6\",\"rxtime\":\"93302\"}")}),
            query, blocks);
        require(events.as_array()[0].at("selection_time").as_string() == "09:33:02",
                "event-driven selection time is normalized to HH:MM:SS");

        query = {};
        query.view = "auction-volume-spike";
        const auto auction_volume = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"code\":\"000001\",\"market\":\"0\","
                "\"yesterdayGrowth\":\"1002.00%\",\"yesterdayAmount\":\"1000000\","
                "\"openHighRate\":\"6.68%\",\"aggregateAuctionAmount\":\"25000\"}")}),
            query, blocks);
        require(std::abs(auction_volume.as_array()[0]
                             .at("previous_day_change_pct").as_number() - 10.02) < 1e-9 &&
                    std::abs(auction_volume.as_array()[0]
                             .at("auction_to_previous_amount_pct").as_number() - 2.5) < 1e-9,
                "auction-volume reproduces the XML /100 display formula and amount ratio");

        query = {};
        query.view = "limit-up-gap";
        const auto limit_gap = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"code\":\"000001\",\"market\":\"0\",\"bidGrowth\":\"7\","
                "\"bidAmount\":\"200\",\"bidYesAmountRatio\":\"4\","
                "\"strongStyle\":\"1\",\"yesterdayGrowth\":\"10\","
                "\"yesterdayAmount\":\"5000\"}"),
            tdx::Json::parse(
                "{\"code\":\"000002\",\"market\":\"0\","
                "\"strongStyle\":\"0\"}")}), query, blocks);
        require(limit_gap.size() == 1 &&
                    limit_gap.as_array()[0].at("auction_style").as_string() == "涨停高开" &&
                    limit_gap.as_array()[0].at("auction_amount").as_number() == 200,
                "limit-up-gap applies the disclosed strongStyle=1 client filter");

        query = {};
        query.view = "five-minute-volume-surge";
        const auto five_minute = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"code\":\"000001\",\"market\":\"0\","
                "\"yesterdayGrowth\":\"2.5\",\"yesterdayAmount\":\"1000\","
                "\"bidGrowth\":\"1.2\",\"bid920Amount\":\"80\","
                "\"bidAmount\":\"250\"}")}), query, blocks);
        require(five_minute.as_array()[0].at("signal_phase").as_string() ==
                    "opening-first-five-minutes" &&
                    five_minute.as_array()[0].at("match_amount_at_0920").as_number() == 80 &&
                    five_minute.as_array()[0]
                        .at("auction_to_previous_amount_pct").as_number() == 25,
                "five-minute surge retains 09:20 matching amount and derived auction ratio");

        query = {};
        query.view = "accumulation-surge";
        const auto accumulation = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"time\":\"11:07\",\"code\":\"000001\",\"name\":\"\"," 
                "\"market\":\"0\",\"falg\":\"积\",\"value\":\"11.50\","
                "\"safetyvalue\":\"67\"}"),
            tdx::Json::parse(
                "{\"time\":\"1108\",\"code\":\"000002\",\"market\":\"0\","
                "\"falg\":\"突\",\"value\":\"8.20\",\"safetyvalue\":\"55\"}")}),
            query, blocks);
        require(accumulation.size() == 1 &&
                    accumulation.as_array()[0].at("security").at("name").as_string() ==
                        "平安银行" &&
                    accumulation.as_array()[0].at("signal").at("code").as_string() ==
                        "积" &&
                    accumulation.as_array()[0].at("signal").at("polarity").as_string() ==
                        "positive" &&
                    accumulation.as_array()[0].at("selection_time").as_string() ==
                        "11:07" &&
                    accumulation.as_array()[0].at("current_quote").is_null(),
                "200662 factor signals map semantics and apply the XML safety threshold");

        query = {};
        query.view = "ma-break-reclaim";
        const auto reclaim = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"time\":\"0935\",\"code\":\"000001\",\"market\":\"0\","
                "\"falg\":\"立\",\"value\":\"11.60\",\"safetyvalue\":\"60\"}")}),
            query, blocks);
        require(reclaim.size() == 1 &&
                    reclaim.as_array()[0].at("signal").at("label").as_string() ==
                        "一阳上穿5/10/20日均线" &&
                    reclaim.as_array()[0].at("selection_time").as_string() == "09:35",
                "factor signal aliases retain the exact client rule and include score 60");

        const auto factor_before = document("accumulation-surge", rows({
            tdx::Json::parse(
                "{\"security\":{\"security_id\":\"SZ000001\"},"
                "\"signal\":{\"code\":\"积\"},\"selection_time\":\"10:10\"}"),
            tdx::Json::parse(
                "{\"security\":{\"security_id\":\"SZ000001\"},"
                "\"signal\":{\"code\":\"突\"},\"selection_time\":\"10:30\"}")}));
        const auto factor_after = factor_before;
        const auto factor_diff =
            tdx::diff_technical_signal_documents(factor_before, factor_after);
        require(factor_diff.at("counts").at("current").as_number() == 2 &&
                    factor_diff.at("counts").at("unchanged").as_number() == 2,
                "factor snapshots preserve accumulation and surge events for one security");

        query = {};
        query.view = "intraday-opportunity";
        const auto opportunities = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"code\":\"000001\",\"market\":\"0\","
                "\"rxyz\":\"趋势加速\",\"rxsj\":\"11:28\","
                "\"rxhzf\":\"2.05\",\"safety\":\"60\"}"),
            tdx::Json::parse(
                "{\"code\":\"000002\",\"market\":\"0\","
                "\"rxyz\":\"趋势加速\",\"rxsj\":\"11:29\","
                "\"rxhzf\":\"1.80\",\"safety\":\"59\"}")}), query, blocks);
        require(opportunities.size() == 1 &&
                    opportunities.as_array()[0].at("strategy_name").as_string() ==
                        "趋势加速" &&
                    opportunities.as_array()[0].at("safety_score").as_number() == 60 &&
                    std::abs(opportunities.as_array()[0]
                                 .at("since_selection_pct").as_number() - 2.05) < 1e-9,
                "200661 intraday opportunities reproduce the safety>=60 client filter");

        query = {};
        query.view = "t0-opportunity";
        const auto t0 = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse(
                "{\"code\":\"118072\",\"name\":\"鼎通转债\",\"market\":\"1\","
                "\"rxyz\":\"趋势加速\",\"rxsj\":\"09:54\","
                "\"rxhzf\":\"2.22\",\"safety\":\"-1\"}")}), query, blocks);
        require(t0.size() == 1 &&
                    t0.as_array()[0].at("safety_score").is_null() &&
                    t0.as_array()[0].at("reported_safety").as_number() == -1 &&
                    t0.as_array()[0].at("safety_not_applicable").as_bool() &&
                    t0.as_array()[0].at("security").at("name").as_string() ==
                        "鼎通转债",
                "T+0 opportunities preserve the -1 not-applicable safety sentinel");

        query = {};
        query.view = "low-turnover-chase";
        const auto model = tdx::normalize_technical_signal_rows(rows({tdx::Json::parse(
            "{\"code\":\"000001\",\"market\":\"0\",\"zf\":\"4.9485\","
            "\"kpzf\":\"5.1134\",\"sjhsl\":\"0.6210\","
            "\"cje\":\"415389088\",\"sjltsz\":\"66818875392\","
            "\"rxtime\":\"93003\"}")}), query, blocks);
        require(model.size() == 1 &&
                    model.as_array()[0].at("security").at("security_id").as_string() ==
                        "SZ000001" &&
                    std::abs(model.as_array()[0].at("change_pct").as_number() - 4.9485) < 1e-9 &&
                    model.as_array()[0].at("selection_time").as_string() == "09:30:03",
                "fixed 200250 strategies normalize common price, liquidity and time fields");

        query = {};
        query.view = "trend-up";
        const auto trends = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse("{\"code\":\"000001\",\"market\":\"0\","
                             "\"tdcxsj\":\"23\",\"zcw\":\"10.50\",\"zcw_per\":\"2.38\","
                             "\"zlw\":\"12.50\",\"zlw_per\":\"7.28\",\"xj\":\"11.62\","
                             "\"zd\":\"0.03\",\"zdf\":\"0.39\",\"tdqsrq\":\"20260703\","
                             "\"qsrqzjzf\":\"8.03\",\"aqxdf\":\"62.93\","
                             "\"jbmaqxdf\":\"92\"}"),
            tdx::Json::parse("{\"code\":\"000002\",\"market\":\"0\","
                             "\"tdcxsj\":\"20\",\"zcw\":\"1\",\"qsrqzjzf\":\"2\","
                             "\"jbmaqxdf\":\"59\"}")}), query, blocks);
        require(trends.size() == 1 &&
                    trends.as_array()[0].at("start_date").as_string() == "2026-07-03" &&
                    trends.as_array()[0].at("overall_safety_score").as_number() == 92,
                "trend-up applies the four client XML filters and maps support/resistance");

        query = {};
        query.view = "trend-down";
        const auto down = tdx::normalize_technical_signal_rows(rows({
            tdx::Json::parse("{\"code\":\"000001\",\"market\":\"0\","
                             "\"tdcxsj\":\"50\",\"aqxdf\":\"72\"}"),
            tdx::Json::parse("{\"code\":\"000002\",\"market\":\"0\","
                             "\"tdcxsj\":\"49\",\"aqxdf\":\"80\"}")}), query, blocks);
        require(down.size() == 1 && down.as_array()[0].at("duration_days").as_number() == 50,
                "trend-down reproduces the client duration threshold");

        const auto security_hits = tdx::technical_signal_hits_for_security(document(
            "nine-turn", rows({tdx::Json::parse(
                "{\"security\":{\"security_id\":\"SZ000001\"},\"direction\":\"up\"}") })),
            "sz", "000001", blocks);
        require(security_hits.size() == 1 &&
                    security_hits.as_array()[0].at("matched_via").as_string() == "security",
                "security reverse lookup matches normalized security identities");

        const auto block_hits = tdx::technical_signal_hits_for_security(document(
            "rps-block", rows({tdx::Json::parse(
                "{\"block\":{\"block_id\":\"research-industry:X1\","
                "\"code\":\"880668\"},\"periods\":[]}") })),
            "0", "000001", blocks);
        require(block_hits.size() == 1 &&
                    block_hits.as_array()[0].at("matched_via").as_string() == "block-membership",
                "security reverse lookup includes block RPS through local membership");

        const auto before = document("new-high", rows({
            tdx::Json::parse("{\"security\":{\"security_id\":\"SZ000001\"},\"score\":80}"),
            tdx::Json::parse("{\"security\":{\"security_id\":\"SH600000\"},\"score\":70}")}),
            "2026-08-05T16:00:00+0800");
        const auto after = document("new-high", rows({
            tdx::Json::parse("{\"security\":{\"security_id\":\"SZ000001\"},\"score\":90}"),
            tdx::Json::parse("{\"security\":{\"security_id\":\"SZ000002\"},\"score\":75}")}),
            "2026-08-06T16:00:00+0800");
        const auto difference = tdx::diff_technical_signal_documents(before, after);
        require(difference.at("counts").at("added").as_number() == 1 &&
                    difference.at("counts").at("removed").as_number() == 1 &&
                    difference.at("counts").at("changed").as_number() == 1,
                "snapshot diff distinguishes added, removed and changed entities");

        const auto snapshot_path = std::filesystem::temp_directory_path() /
            "tdx-technical-signals-native-test-snapshot.json";
        std::error_code cleanup_error;
        std::filesystem::remove(snapshot_path, cleanup_error);
        const auto first_snapshot = tdx::update_technical_signal_snapshot(snapshot_path, before);
        const auto second_snapshot = tdx::update_technical_signal_snapshot(snapshot_path, after);
        require(first_snapshot.at("created").as_bool() &&
                    !second_snapshot.at("created").as_bool() &&
                    second_snapshot.at("diff").at("counts").at("added").as_number() == 1,
                "snapshot update initializes, compares and atomically replaces state");
        std::filesystem::remove(snapshot_path, cleanup_error);

        std::cout << "technical signals tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
