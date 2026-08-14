#include "tdx/factors.hpp"

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

}  // namespace

int main() {
    try {
        const auto catalog = tdx::normalize_factor_rows(rows({tdx::Json::parse(
            "{\"ID\":\"24\",\"name\":\"净利润同比增长\",\"type\":\"财务型\","
            "\"BullBear\":\"0\",\"description\":\"利润增长\",\"formulaName\":\"FIN\","
            "\"pos\":\"1\",\"near2dayZDF\":\"1.25\",\"near5dayZDF\":\"-0.50\","
            "\"A1dZdf\":\"0.1\",\"A3dZdf\":\"0.3\",\"A5dZdf\":\"0.5\","
            "\"A10dZdf\":\"1.0\",\"A20dZdf\":\"2.0\",\"A60dZdf\":\"6.0\","
            "\"maxRetracement\":\"-30.2\",\"sharpRatio\":\"0.88\"}")}), "catalog");
        const auto& factor = catalog.as_array()[0];
        require(catalog.size() == 1 && factor.at("factor_id").as_string() == "24" &&
                    factor.at("direction").as_string() == "bullish" &&
                    std::abs(factor.at("live_performance").at("today_return_pct").as_number() - 1.25) < 1e-12 &&
                    factor.at("backtest").at("window_days").as_number() == 120 &&
                    factor.at("backtest").at("forward_returns_pct").at("60").as_number() == 6.0,
                "standard catalog preserves identity, direction, percentage units and backtest horizons");

        const auto members = tdx::normalize_factor_rows(rows({tdx::Json::parse(
            "{\"code\":\"000001\",\"name\":\"平安银行\",\"market\":\"0\","
            "\"zdf%\":\"2.35\",\"price\":\"12.34\"}")}), "members");
        require(members.size() == 1 &&
                    members.as_array()[0].at("security").at("security_id").as_string() == "SZ000001" &&
                    members.as_array()[0].at("factor_kind").as_string() == "standard" &&
                    std::abs(members.as_array()[0].at("change_pct").as_number() - 2.35) < 1e-12,
                "factor member rows normalize exchange identity and prices");

        const auto pattern_members = tdx::normalize_factor_rows(rows({tdx::Json::parse(
            "{\"code\":\"600000\",\"name\":\"浦发银行\",\"market\":\"1\","
            "\"zdf%\":\"-0.2\",\"price\":\"10\",\"10rzdf%\":\"3.2\"}")}),
            "pattern-members");
        require(pattern_members.as_array()[0].at("security").at("security_id").as_string() == "SH600000" &&
                    pattern_members.as_array()[0].at("ten_day_change_pct").as_number() == 3.2,
                "pattern members retain ten-day performance");

        const auto dashboard = tdx::normalize_factor_rows(rows({tdx::Json::parse(
            "{\"code\":\"688002\",\"name\":\"睿创微纳\",\"market\":\"1\","
            "\"zdf%\":\"2.5\",\"price\":\"155.63\",\"Score\":\"3\","
            "\"rxyz\":\"当日多头、MACD金叉、高价股\",\"near10zdf%\":\"1\",\"safety\":\"100\"}")}),
            "dashboard");
        require(dashboard.as_array()[0].at("signal_count").as_number() == 3 &&
                    dashboard.as_array()[0].at("selected_factors").size() == 3 &&
                    dashboard.as_array()[0].at("safety_score").as_number() == 100,
                "dashboard expands factor labels and score");

        const auto reverse = tdx::factor_hits_for_security(
            catalog, dashboard, "sh", "688002");
        require(reverse.at("found").as_bool() &&
                    reverse.at("factors").size() == 0 &&
                    reverse.at("unresolved_factor_names").size() == 3,
                "reverse lookup preserves dashboard labels missing from a partial catalog");
        const auto reverse_match = tdx::factor_hits_for_security(
            catalog,
            tdx::normalize_factor_rows(rows({tdx::Json::parse(
                "{\"code\":\"000001\",\"name\":\"平安银行\",\"market\":\"0\","
                "\"Score\":\"1\",\"rxyz\":\"净利润同比增长\",\"safety\":\"90\"}")}),
                "dashboard"),
            "0", "000001");
        require(reverse_match.at("found").as_bool() &&
                    reverse_match.at("factors").size() == 1 &&
                    reverse_match.at("factors").as_array()[0].at("factor_id").as_string() == "24" &&
                    reverse_match.at("unresolved_factor_names").size() == 0,
                "reverse lookup joins exact security and factor name to catalog metadata");
        const auto reverse_absent = tdx::factor_hits_for_security(
            catalog, dashboard, "sz", "000001");
        require(!reverse_absent.at("found").as_bool() &&
                    reverse_absent.at("factors").size() == 0,
                "a security absent from the dashboard is an explicit empty relation");

        tdx::Json pattern_matrix = tdx::Json::array();
        tdx::Json pattern_item = tdx::Json::object();
        pattern_item["factor"] = tdx::Json::parse(
            "{\"factor_id\":\"49\",\"name\":\"十字星\",\"factor_kind\":\"pattern\"}");
        pattern_item["members"] = pattern_members;
        pattern_matrix.push_back(pattern_item);
        const auto pattern_reverse = tdx::pattern_hits_for_security(
            pattern_matrix, "sh", "600000");
        require(pattern_reverse.at("factors").size() == 1 &&
                    pattern_reverse.at("factors").as_array()[0].at("factor_id").as_string() == "49" &&
                    pattern_reverse.at("evidence").as_array()[0].at("member_record")
                        .at("ten_day_change_pct").as_number() == 3.2,
                "pattern reverse index retains factor metadata and forward-list evidence");

        const auto second_catalog = tdx::normalize_factor_rows(rows({
            tdx::Json::parse(
                "{\"ID\":\"24\",\"name\":\"净利润同比增长\",\"type\":\"财务型\","
                "\"BullBear\":\"0\",\"description\":\"利润增长\",\"formulaName\":\"FIN\",\"pos\":\"1\"}"),
            tdx::Json::parse(
                "{\"ID\":\"25\",\"name\":\"营收同比增长\",\"type\":\"财务型\","
                "\"BullBear\":\"0\",\"description\":\"营收增长\",\"formulaName\":\"FIN\",\"pos\":\"2\"}")}),
            "catalog");
        const auto breadth_dashboard = tdx::normalize_factor_rows(rows({
            tdx::Json::parse(
                "{\"code\":\"000001\",\"name\":\"平安银行\",\"market\":\"0\","
                "\"zdf%\":\"2\",\"Score\":\"2\",\"rxyz\":\"净利润同比增长、营收同比增长\","
                "\"near10zdf%\":\"5\",\"safety\":\"90\"}"),
            tdx::Json::parse(
                "{\"code\":\"600000\",\"name\":\"浦发银行\",\"market\":\"1\","
                "\"zdf%\":\"-1\",\"Score\":\"1\",\"rxyz\":\"净利润同比增长\","
                "\"near10zdf%\":\"-2\",\"safety\":\"70\"}")}),
            "dashboard");
        const auto breadth = tdx::build_factor_breadth_document(
            second_catalog, breadth_dashboard);
        require(breadth.at("counts").at("dashboard_securities").as_number() == 2 &&
                    breadth.at("records").as_array()[0].at("member_count").as_number() == 2 &&
                    std::abs(breadth.at("records").as_array()[0].at("coverage_pct").as_number() - 100.0) < 1e-12 &&
                    breadth.at("cooccurrence").size() == 1 &&
                    breadth.at("cooccurrence").as_array()[0].at("member_count").as_number() == 1 &&
                    breadth.at("safety_distribution").at("60_to_79").as_number() == 1,
                "factor breadth computes coverage, co-occurrence and safety buckets");
        tdx::Json standard_matrix = tdx::Json::array();
        for (std::size_t index = 0; index < second_catalog.size(); ++index) {
            tdx::Json item = tdx::Json::object();
            item["factor"] = second_catalog.as_array()[index];
            item["status"] = "live";
            item["members"] = members;
            standard_matrix.push_back(std::move(item));
        }
        const auto reconciled = tdx::reconcile_factor_breadth_document(
            breadth, breadth_dashboard, standard_matrix);
        require(reconciled.at("counts").at("membership_consistent_factors").as_number() == 1 &&
                    reconciled.at("counts").at("membership_divergent_factors").as_number() == 1 &&
                    reconciled.at("records").as_array()[0].at("dashboard_member_count").as_number() == 2 &&
                    reconciled.at("records").as_array()[0].at("direct_member_count").as_number() == 1 &&
                    reconciled.at("records").as_array()[0].at("dashboard_only_count").as_number() == 1 &&
                    !reconciled.at("records").as_array()[0].at("membership_consistent").as_bool(),
                "breadth reconciliation preserves dashboard/direct membership divergence");

        auto snapshot_document = [](const tdx::Json& snapshot_rows,
                                    const std::string& generated) {
            tdx::Json parameters = tdx::Json::object();
            parameters["view"] = "members";
            parameters["factor_id"] = "24";
            parameters["all_pages"] = true;
            tdx::Json counts = tdx::Json::object();
            counts["truncated"] = false;
            tdx::Json document = tdx::Json::object();
            document["schema"] = "tdx-factors-native-v1";
            document["availability"] = "live";
            document["generated_at"] = generated;
            document["view"] = "members";
            document["parameters"] = parameters;
            document["counts"] = counts;
            document["records"] = snapshot_rows;
            return document;
        };
        const auto before_snapshot = snapshot_document(members, "2026-08-06T10:00:00+0800");
        tdx::Json after_rows = members;
        after_rows.push_back(pattern_members.as_array()[0]);
        const auto after_snapshot = snapshot_document(after_rows, "2026-08-06T10:01:00+0800");
        const auto difference = tdx::diff_factor_documents(before_snapshot, after_snapshot);
        require(difference.at("counts").at("added").as_number() == 1 &&
                    difference.at("counts").at("removed").as_number() == 0 &&
                    difference.at("counts").at("unchanged").as_number() == 1,
                "factor snapshot diff uses stable security identities");
        const auto snapshot_path = std::filesystem::temp_directory_path() /
            "tdx-factors-native-test-snapshot.json";
        std::error_code cleanup_error;
        std::filesystem::remove(snapshot_path, cleanup_error);
        const auto first_update = tdx::update_factor_snapshot(snapshot_path, before_snapshot);
        const auto second_update = tdx::update_factor_snapshot(snapshot_path, after_snapshot);
        require(first_update.at("created").as_bool() &&
                    !second_update.at("created").as_bool() &&
                    second_update.at("diff").at("counts").at("added").as_number() == 1,
                "factor snapshot update creates, compares and atomically replaces state");
        std::filesystem::remove(snapshot_path, cleanup_error);
        bool truncated_rejected = false;
        try {
            auto truncated = before_snapshot;
            truncated["counts"]["truncated"] = true;
            (void)tdx::diff_factor_documents(truncated, after_snapshot);
        } catch (const tdx::Error&) { truncated_rejected = true; }
        require(truncated_rejected,
                "factor snapshots reject limit-truncated membership results");
        bool partial_rejected = false;
        try {
            auto partial = before_snapshot;
            partial["availability"] = "partial";
            (void)tdx::update_factor_snapshot(snapshot_path, partial);
        } catch (const tdx::Error&) { partial_rejected = true; }
        require(partial_rejected,
                "factor snapshots reject partial multi-factor matrices");

        const auto radar = tdx::normalize_factor_rows(rows({tdx::Json::parse(
            "{\"code\":\"605100\",\"name\":\"华丰股份\",\"market\":\"1\","
            "\"rxyz\":\"量价齐升\",\"rxsj\":\"11:29\",\"rxhzf\":\"0.67\",\"safety\":\"89\"}")}),
            "intraday-radar");
        require(radar.as_array()[0].at("selected_factor").as_string() == "量价齐升" &&
                    radar.as_array()[0].at("selection_time").as_string() == "11:29" &&
                    radar.as_array()[0].at("safety_score").as_number() == 89,
                "intraday radar preserves factor, time, return and safety score");

        std::cout << "factor tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
