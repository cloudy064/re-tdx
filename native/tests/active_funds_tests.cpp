#include "tdx/active_funds.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

tdx::Json source_document(const std::string& resource, tdx::Json rows) {
    auto source = tdx::Json::object();
    source["resource"] = resource;
    source["size"] = 100;
    source["row_count"] = static_cast<std::uint64_t>(rows.size());
    source["endpoint"] = "test:7709";
    source["attempts"] = 1;
    source["rows"] = std::move(rows);
    return source;
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "601666"}] =
            tdx::Security{1, "SH", "上海", "601666", "平煤股份"};
        std::map<std::pair<int, std::string>, std::string> industries;
        industries[{1, "601666"}] = "煤炭开采";
        auto rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"601666\",\"$SC\":\"1\","
            "\"ksrq\":\"20260401\",\"jzrs\":\"20260630\","
            "\"jzrsz\":\"17655960529.35\",\"jzrzsz\":\"17655960529.35\","
            "\"zcsz\":\"191305178.35\",\"zcszbd\":\"-24697594.13\","
            "\"zcsl\":\"26755969\",\"zcslbd\":\"285041\",\"zcjs\":\"13\"}"));
        const auto normalized = tdx::normalize_active_fund_security_rows(
            rows, securities, industries);
        const auto& holding = normalized.as_array()[0];
        require(holding.at("security").at("security_id").as_string() == "SH601666" &&
                    holding.at("security").at("name").as_string() == "平煤股份" &&
                    holding.at("direction").as_string() == "decreased" &&
                    holding.at("fund_count").as_number() == 13 &&
                    std::abs(holding.at("holding_pct_float").as_number() -
                             1.083516119) < 0.000001 &&
                    holding.at("industry").as_string() == "煤炭开采",
                "stock holdings should preserve yuan/share units and resolve metadata");

        auto new_rows = tdx::Json::array();
        new_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"000001\",\"$SC\":\"0\",\"zcsz\":\"100\","
            "\"zcszbd\":\"100\",\"zcsl\":\"10\",\"zcslbd\":\"10\",\"zcjs\":\"1\"}"));
        const auto new_holding = tdx::normalize_active_fund_security_rows(
            new_rows, {}, {}).as_array()[0];
        require(new_holding.at("direction").as_string() == "new",
                "equal positive value and change should be classified as newly held");

        auto fund_rows = tdx::Json::array();
        fund_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"519185\",\"$SC\":\"33\",\"jjmc\":\"万家精选A\","
            "\"cgsz\":\"67719723.50\",\"cgsl\":\"9471290\","
            "\"zjzb\":\"3.590\",\"cgwl\":\"10\"}"));
        const auto funds = tdx::normalize_active_fund_detail_rows(fund_rows);
        const auto& fund = funds.as_array()[0];
        require(fund.at("fund").at("fund_id").as_string() == "FUND33-519185" &&
                    fund.at("fund").at("name").as_string() == "万家精选A" &&
                    fund.at("holding_market_value_yuan").as_number() == 67719723.5 &&
                    fund.at("holding_shares").as_number() == 9471290 &&
                    std::abs(fund.at("nav_pct").as_number() - 3.59) < 0.000001 &&
                    fund.at("holding_rank").as_number() == 10,
                "fund detail should preserve market value, shares, NAV ratio, and rank");

        tdx::BlockData data;
        data.securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        bool fail = false;
        int calls = 0;
        tdx::ActiveFundService service(std::move(data),
            [&](const std::string& resource, const std::string& prefix, int timeout_ms) {
                ++calls;
                require(prefix == "bi" && timeout_ms == 100,
                        "active-fund fetch arguments were not forwarded");
                if (fail) throw tdx::Error("server closed connection while receiving");
                if (resource.find("func_zdjjzczc101") != std::string::npos) {
                    auto master_rows = tdx::Json::array();
                    master_rows.push_back(tdx::Json::parse(
                        "{\"$ZQDM\":\"000001\",\"$SC\":\"0\","
                        "\"ksrq\":\"20260401\",\"jzrs\":\"20260630\","
                        "\"zcsz\":\"100\",\"zcszbd\":\"100\","
                        "\"zcsl\":\"10\",\"zcslbd\":\"10\",\"zcjs\":\"1\"}"));
                    return source_document(resource, std::move(master_rows));
                }
                auto detail_rows = tdx::Json::array();
                detail_rows.push_back(tdx::Json::parse(
                    "{\"$ZQDM\":\"519185\",\"$SC\":\"33\","
                    "\"jjmc\":\"万家精选A\",\"cgsz\":\"100\","
                    "\"cgsl\":\"10\",\"zjzb\":\"1.0\",\"cgwl\":\"1\"}"));
                return source_document(resource, std::move(detail_rows));
            });
        tdx::ActiveFundQuery query;
        query.view = "funds";
        query.market = "sz";
        query.code = "000001";
        query.timeout_ms = 100;
        const auto live = service.query(query);
        require(calls == 2 && live.at("availability").as_string() == "live" &&
                    live.at("funds").size() == 1,
                "active-fund live query did not warm master and detail caches");

        fail = true;
        query.refresh = true;
        const auto stale = service.query(query);
        require(calls == 4 && stale.at("availability").as_string() == "stale-cache" &&
                    stale.at("funds").size() == 1 &&
                    stale.at("cache").at("master_stale").as_bool() &&
                    stale.at("cache").at("detail_stale").as_bool() &&
                    stale.at("cache").at("stale").as_bool(),
                "active-fund query did not fall back to both stale caches");
        require(stale.at("cache").at("master_upstream_error").as_string().find(
                    "server closed connection") != std::string::npos &&
                    stale.at("cache").at("detail_upstream_error").as_string().find(
                    "server closed connection") != std::string::npos,
                "active-fund stale cache did not expose upstream diagnostics");

        std::cout << "active-fund tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "active-fund test failed: " << error.what() << '\n';
        return 1;
    }
}
