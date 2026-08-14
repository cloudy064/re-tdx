#include "tdx/economic_indicators.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}
}

int main() {
    try {
        auto raw = tdx::Json::object();
        raw["date"] = "20260806";
        raw["NAME"] = "阴极铜:期货结算价";
        raw["lb"] = "价格";
        raw["sz"] = "106530.00";
        raw["dw"] = "元/吨";
        raw["hb"] = "0.74";
        raw["tb"] = "34.56";
        raw["pl"] = "日";
        raw["bgq"] = "20260806";
        raw["$ZQDM"] = "M2800000005";
        auto rows = tdx::Json::array();
        rows.push_back(raw);
        const auto indicators = tdx::normalize_economic_indicator_rows(rows);
        const auto& indicator = indicators.as_array()[0];
        require(indicator.at("indicator_id").as_string() == "M2800000005" &&
                    indicator.at("update_date").as_string() == "2026-08-06" &&
                    std::abs(indicator.at("current_value").as_number() - 106530.0) < 1e-8 &&
                    indicator.at("history_resource").as_string() ==
                        "jjzb1/M2800000005.jsn" &&
                    indicator.at("related_resource").as_string() ==
                        "jjzb2/M2800000005.jsn",
                "master normalizer must preserve value, dates and dynamic keys");

        auto history_rows = tdx::Json::array();
        history_rows.push_back(tdx::Json::parse(R"({"date":"20260806","data":"106530"})"));
        history_rows.push_back(tdx::Json::parse(R"({"date":"20260805","data":"105550"})"));
        history_rows.push_back(tdx::Json::parse(R"({"date":"","data":""})"));
        const auto history = tdx::normalize_economic_indicator_history(history_rows);
        require(history.size() == 2 &&
                    history.as_array()[0].at("date").as_string() == "2026-08-05" &&
                    history.as_array()[1].at("value").as_number() == 106530.0,
                "history normalizer must drop blanks and sort ascending");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "601600"}] = {1, "SH", "上海", "601600", "中国铝业"};
        auto relation_rows = tdx::Json::array();
        relation_rows.push_back(tdx::Json::parse(
            R"({"$ZQDM":"601600","$SC":"1","hy":"工业金属"})"));
        relation_rows.push_back(tdx::Json::parse(
            R"({"$ZQDM":"601600","$SC":"1","hy":"重复"})"));
        auto quote_rows = tdx::Json::array();
        quote_rows.push_back(tdx::Json::parse(
            R"({"market_id":1,"code":"601600","last_price":9.81,"change_pct":2.4,"amount":123456789})"));
        const auto relations = tdx::normalize_economic_indicator_relations(
            relation_rows, quote_rows, securities);
        const auto& relation = relations.as_array()[0];
        require(relations.size() == 1 &&
                    relation.at("security").at("name").as_string() == "中国铝业" &&
                    relation.at("industry").as_string() == "工业金属" &&
                    relation.at("quote_available").as_bool() &&
                    std::abs(relation.at("last_price").as_number() - 9.81) < 1e-8,
                "relation normalizer must deduplicate and attach public quotes");

        auto sortable = indicators;
        auto older = indicator;
        older["indicator_id"] = "M2100040000";
        older["update_date"] = "2026-07-29";
        older["year_on_year_pct"] = 60.0;
        sortable.push_back(older);
        tdx::sort_economic_indicator_rows(sortable, "yoy", "desc");
        require(sortable.as_array()[0].at("indicator_id").as_string() ==
                    "M2100040000", "numeric sort");

        std::cout << "economic indicator tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "economic indicator test failed: " << error.what() << '\n';
        return 1;
    }
}
