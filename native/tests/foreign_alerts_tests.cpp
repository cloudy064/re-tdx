#include "tdx/foreign_alerts.hpp"

#include <iostream>
#include <map>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        require(tdx::foreign_alert_status_key("强制减仓") == "forced-reduction" &&
                    tdx::foreign_alert_status_key("逼近预警") == "approaching",
                "foreign alert status mapping failed");
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "600885"}] =
            tdx::Security{1, "SH", "上海", "600885", "宏发股份"};
        auto master = tdx::Json::array();
        master.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"600885\",\"$SC\":\"1\",\"date\":\"20260803\"," 
            "\"wzcgs\":\"41863.957\",\"zzgb\":\"27.05\",\"yjzt\":\"预警\"," 
            "\"cgbd\":\"-4910720.000\",\"bdbl\":\"-1.16\"}"));
        const auto normalized = tdx::normalize_foreign_alert_master_rows(
            master, securities);
        const auto& item = normalized.as_array()[0];
        require(item.at("security").at("name").as_string() == "宏发股份",
                "foreign alert security name was not resolved");
        require(item.at("foreign_holding_shares").as_number() == 418639570.0,
                "master ten-thousand-share unit was not converted");
        require(item.at("daily_change_shares").as_number() == -4910720.0,
                "daily share change was not preserved");

        auto history = tdx::Json::array();
        history.push_back(tdx::Json::parse(
            "{\"date\":\"20260803\",\"wzcgs\":\"418639570.00\"," 
            "\"zzgb\":\"27.05\",\"yjzt\":\"预警\",\"cgbd\":\"-4910720.00\"," 
            "\"bdbl\":\"-1.16\"}"));
        const auto detail = tdx::normalize_foreign_alert_history_rows(history);
        require(detail.as_array()[0].at("foreign_holding_shares").as_number() ==
                    418639570.0,
                "history share unit should already be shares");
        std::cout << "foreign-alert tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "foreign-alert test failed: " << error.what() << '\n';
        return 1;
    }
}
