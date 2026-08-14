#include "tdx/threshold_stocks.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        const auto history_raw = tdx::Json::parse(R"([
          {"rq":"20260805","szzs":"1.47","zzcz":"1.86","hssb":"1.24","zjs":"206","qyjs":"202","byjs":"4","zsz":"21139046176762.60","zaf":"18.63","rwjs":"10","dtjs":"0","$ZQDM":"220260805"},
          {"rq":"20260806","szzs":"0.57","zzcz":"-0.24","hssb":"-0.15","zjs":"212","qyjs":"208","byjs":"4","zsz":"21379573550148.47","zaf":"18.80","rwjs":"6","dtjs":"0","$ZQDM":"220260806"}
        ])");
        auto history = tdx::normalize_threshold_history_rows(history_raw, "high-price");
        require(history.size() == 2 && history.as_array()[0].at("date").as_string() == "20260806",
                "history must be normalized newest first even if upstream order differs");
        require(history.as_array()[0].at("hundred_to_thousand_yuan_count").as_number() == 208 &&
                    history.as_array()[0].at("thousand_yuan_or_more_count").as_number() == 4,
                "high-price bucket fields failed");

        const auto mega = tdx::normalize_threshold_history_rows(tdx::Json::parse(R"([
          {"rq":"20260806","zjs":"196","wyjs":"14","qyjs":"182","zsz":"68138638420450.35","zaf":"53.162009","rwjs":"","dtjs":"2","$ZQDM":"120260806"}
        ])"), "mega-cap");
        require(mega.as_array()[0].at("trillion_yuan_market_cap_count").as_number() == 14 &&
                    mega.as_array()[0].at("entered_count").as_number() == 0,
                "mega-cap buckets and client blank-as-zero counts must remain typed");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = {0, "SZ", "深圳", "000001", "平安银行"};
        const auto raw_members = tdx::Json::parse(R"([
          {"$ZQDM":"000001","$SC":"0","zaf":"0.18","spj":"2187.05","gjjz":"3.88","ztbs":"不变","dq":"深圳","dgd":"无"},
          {"$ZQDM":"600519","$SC":"1","zaf":"-1.0","spj":"998.00","gjjz":"-10.00","ztbs":"跌出","dq":"贵州","dgd":"贵州省国资委"},
          {"$ZQDM":"920001","$SC":"2","zaf":"2.0","spj":"1001.00","gjjz":"20.00","ztbs":"入围","dq":"北京","dgd":"无"}
        ])");
        auto members = tdx::normalize_threshold_member_rows(raw_members, "mega-cap", securities);
        require(members.size() == 3 &&
                    members.as_array()[0].at("security").at("name").as_string() == "平安银行" &&
                    members.as_array()[1].at("status").as_string() == "exited" &&
                    members.as_array()[2].at("security").at("market").as_string() == "bj",
                "member identity/status normalization failed");
        require(members.as_array()[0].at("market_cap_100m_yuan").as_number() == 2187.05 &&
                    members.as_array()[0].as_object().find("close_price_yuan") == members.as_array()[0].as_object().end(),
                "overloaded spj must use universe-specific typing");

        const auto trend = tdx::normalize_threshold_trend_rows(tdx::Json::parse(R"([
          {"date":"20260806","dbsl":"196"},{"date":"20260805","dbsl":"198"}
        ])"), "mega-cap");
        require(trend.as_array()[0].at("date").as_string() == "20260805" &&
                    trend.as_array()[1].at("count").as_number() == 196,
                "trend must normalize dbsl and sort chronologically");

        tdx::sort_threshold_rows(members, "members", "threshold-value", "desc");
        require(members.as_array()[0].at("security").at("code").as_string() == "000001",
                "member threshold sort failed");
        bool rejected = false;
        try { tdx::sort_threshold_rows(history, "history", "bogus", "desc"); }
        catch (const tdx::Error&) { rejected = true; }
        require(rejected, "invalid sort must be rejected");

        std::cout << "threshold-stocks tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "threshold-stocks test failed: " << error.what() << '\n';
        return 1;
    }
}
