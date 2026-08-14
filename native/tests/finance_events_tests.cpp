#include "tdx/finance_events.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

tdx::Json row(const char* market, const char* code, const char* date) {
    tdx::Json value = tdx::Json::object();
    value["$SC"] = market;
    value["$ZQDM"] = code;
    value["ggrq"] = date;
    return value;
}

tdx::Json source(const char* resource, std::initializer_list<tdx::Json> rows) {
    tdx::Json value = tdx::Json::object();
    value["resource"] = resource;
    value["rows"] = tdx::Json::array();
    for (const auto& item : rows) value["rows"].push_back(item);
    return value;
}

}  // namespace

int main() {
    try {
        tdx::Json documents = tdx::Json::array();
        documents.push_back(source("list/func_qxfa104_1.jsn", {
            row("0", "000001", "20260801"),
            row("0", "000001", "20260805"),
            row("0", "000001", "20260806"),
            row("1", "000001", "20260805"),
        }));
        documents.push_back(source("list/func_qxfa401_1.jsn", {
            row("0", "000001", "20260804"),
            row("0", "000002", "20260805"),
        }));

        const auto values = tdx::finance_event_values_from_documents(
            documents, "sz", "000001", {90, 91}, "20260805");
        require(values.at(90).days == 1 && values.at(90).event_date == "20260805",
                "FINANCE(90) must count today as day one and ignore future rows");
        require(values.at(91).days == 2 && values.at(91).event_date == "20260804",
                "FINANCE(91) must use inclusive natural days");

        const auto missing = tdx::finance_event_values_from_documents(
            documents, "sh", "600000", {90, 91}, "20260805");
        require(missing.at(90).days == 0 && missing.at(90).event_date.empty() &&
                missing.at(91).days == 0 && missing.at(91).event_date.empty(),
                "absent events must bind zero");

        const auto leap = tdx::finance_event_values_from_documents(
            tdx::Json::parse(
                R"([{"resource":"list/func_qxfa104_1.jsn","rows":[{"$SC":"0","$ZQDM":"000001","ggrq":"20240228"}]},{"resource":"list/func_qxfa401_1.jsn","rows":[]}])"),
            "0", "000001", {90}, "20240301");
        require(leap.at(90).days == 3, "natural-day count must cross leap day");

        tdx::TdxStatsResource stats;
        stats.source_path = "tdx://fixture/zhb.zip";
        stats.tip_info_events.emplace(
            std::make_pair(0, std::string("000001")),
            tdx::TdxTipInfoEventRow{0, "000001", "20260804", 1.25,
                                    std::nullopt, std::nullopt, std::nullopt});
        stats.tip_info_events.emplace(
            std::make_pair(0, std::string("000002")),
            tdx::TdxTipInfoEventRow{0, "000002", "20260804", -1.25,
                                    std::nullopt, std::nullopt, std::nullopt});
        const auto northbound = tdx::finance_event_values_from_stats(
            stats, "sz", "000001", {88}, "20260805");
        require(northbound.at(88).days == 2 &&
                northbound.at(88).event_date == "20260804" &&
                northbound.at(88).resource == "tdx://fixture/zhb.zip#tipinfo.dat",
                "FINANCE(88) must use positive signed tipinfo northbound dates");
        const auto decrease = tdx::finance_event_values_from_stats(
            stats, "0", "000002", {88}, "20260805");
        require(decrease.at(88).days == 0 && decrease.at(88).event_date.empty(),
                "negative tipinfo direction belongs to FINANCE(89), not FINANCE(88)");
        const auto absent = tdx::finance_event_values_from_stats(
            stats, "sh", "600000", {88}, "20260805");
        require(absent.at(88).days == 0,
                "missing tipinfo northbound event must bind zero");

        std::cout << "finance event tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "finance event tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
