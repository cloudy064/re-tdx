#include "tdx/patent_statistics.hpp"

#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = {0, "sz", "深圳", "000001", "平安银行"};
        tdx::Json row = tdx::Json::object();
        row["$ZQDM"] = "000001";
        row["$SC"] = "0";
        row["jzrq"] = "20251231";
        row["bqfm"] = "5";
        row["bqsy"] = "3";
        row["bqhj"] = "10";
        row["bqhdfm"] = "2";
        row["bqhdhj"] = "2";
        row["ljfm"] = "20";
        row["ljsy"] = "10";
        row["ljwg"] = "5";
        row["ljhj"] = "40";
        tdx::Json rows = tdx::Json::array();
        rows.push_back(std::move(row));
        const auto normalized = tdx::normalize_patent_statistic_rows(rows, securities);
        require(normalized.size() == 1, "one patent row");
        const auto& item = normalized.as_array().front();
        require(item.at("security").at("name").as_string() == "平安银行",
                "security name");
        require(item.at("period_application_total").as_number() == 10,
                "period application total");
        require(item.at("period_application_classified_total").as_number() == 8,
                "classified application total");
        require(item.at("period_application_total_delta").as_number() == 2,
                "unprojected application categories");
        require(item.at("cumulative_grant_total").as_number() == 40,
                "cumulative total");
        require(item.at("cumulative_grant_total_delta").as_number() == 5,
                "cumulative delta");
        std::cout << "patent statistics tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
