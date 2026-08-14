#include "tdx/capital_strength.hpp"
#include "tdx/common.hpp"

#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::Json fixture(const char* code, const char* ddx, const char* total,
                  const char* main, const char* period_suffix) {
    tdx::Json row = tdx::Json::object();
    row["$SC"] = code[0] == '6' ? "1" : "0";
    row["$ZQDM"] = code;
    row["ltgb"] = "100000000";
    row[std::string("zf") + period_suffix] = "12.50";
    row[std::string("zjlr") + period_suffix] = total;
    row[std::string("zljlr") + period_suffix] = main;
    row["ddx"] = ddx;
    row["date"] = "20260806";
    return row;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = {0, "SZ", "深圳", "000001", "平安银行"};
        securities[{1, "600000"}] = {1, "SH", "上海", "600000", "浦发银行"};

        tdx::Json raw = tdx::Json::array();
        raw.push_back(fixture("000001", "3.25", "-100", "200", "5"));
        raw.push_back(fixture("600000", "2.50", "50", "150", "5"));
        auto rows = tdx::normalize_capital_strength_rows(raw, "5d", securities);
        require(rows.size() == 2, "normalize row count");
        const auto& first = rows.as_array().front();
        require(first.at("period").as_string() == "5d", "period normalized");
        require(first.at("security").at("name").as_string() == "平安银行",
                "security name resolved");
        require(first.at("source_rank").as_number() == 1, "source rank retained");
        require(first.at("float_shares").as_number() == 100000000,
                "float shares normalized");
        require(first.at("total_net_inflow_yuan").as_number() == -100,
                "negative total net inflow retained");
        require(first.at("main_net_inflow_yuan").as_number() == 200,
                "main net inflow retained");
        require(first.at("ddx_float_share_pct").as_number() == 3.25,
                "DDX retained without scaling");
        require(first.at("raw").is_object(), "raw row retained");

        tdx::sort_capital_strength_rows(rows, "ranking", "total-net", "desc");
        require(rows.as_array().front().at("security").at("code").as_string() ==
                    "600000", "ranking sort by total net");

        tdx::Json groups = tdx::Json::array();
        for (const auto& period : {"5d", "10d", "20d", "30d", "3m"}) {
            tdx::Json group_raw = tdx::Json::array();
            const auto suffix = std::string(period) == "3m" ? "3y" :
                std::string(period).substr(0, std::string(period).size() - 1);
            group_raw.push_back(fixture("000001", "3.00", "-10", "20",
                                        suffix.c_str()));
            if (std::string(period) == "5d")
                group_raw.push_back(fixture("600000", "2.00", "30", "40", "5"));
            tdx::Json group = tdx::Json::object();
            group["period"] = period;
            group["rows"] = tdx::normalize_capital_strength_rows(
                group_raw, period, securities);
            groups.push_back(std::move(group));
        }
        auto confluence = tdx::compose_capital_strength_confluence(groups);
        require(confluence.at("summary").at("source_rows").as_number() == 6,
                "confluence source rows");
        require(confluence.at("summary").at("unique_securities").as_number() == 2,
                "confluence unique securities");
        require(confluence.at("summary").at("all_five_periods").as_number() == 1,
                "all-five count");
        auto combined = confluence.at("records");
        tdx::sort_capital_strength_rows(
            combined, "confluence", "period-count", "desc");
        require(combined.as_array().front().at("period_count").as_number() == 5,
                "confluence primary sort");
        require(combined.as_array().front().at("observations").size() == 5,
                "confluence observations retained");

        bool rejected = false;
        try { (void)tdx::normalize_capital_strength_rows(raw, "bad", securities); }
        catch (const tdx::Error&) { rejected = true; }
        require(rejected, "invalid period rejected");

        std::cout << "capital-strength tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "capital-strength test failed: " << error.what() << '\n';
        return 1;
    }
}
