#include "tdx/shareholder_signals.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double left, double right, double epsilon = 1e-7) {
    return std::abs(left - right) <= epsilon;
}
tdx::Json one(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json row = tdx::Json::object();
    for (const auto& [key, value] : fields) row[key] = value;
    tdx::Json rows = tdx::Json::array();
    rows.push_back(std::move(row));
    return rows;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000006"}] = {0, "sz", "深圳", "000006", "深振业A"};
        const auto notable = tdx::normalize_shareholder_signal_rows(
            "list/func_cwnscg101_1.jsn",
            one({{"$SC","0"},{"$ZQDM","000006"},{"N001","20260331"},
                 {"N002","3"},{"N003","26670362"},{"N006","1349995046"},
                 {"N007","8.600"},{"N008","-1033.1399"}}), securities);
        const auto& n = notable.as_array().front();
        require(close(n.at("holding_pct").as_number(),
                      26670362.0 * 100.0 / 1349995046.0), "holding pct");
        require(close(n.at("holding_value_yuan").as_number(),
                      26670362.0 * 8.6), "holding value");

        const auto institution = tdx::normalize_shareholder_signal_rows(
            "list/func_jgxc101_1.jsn",
            one({{"$SC","0"},{"$ZQDM","000006"},{"zxbgq","20260331"},
                 {"zxcg","6536959"},{"sqcg","940014"},{"cgzz","5.95410813030444"},
                 {"zxgdrs","18757"},{"sqgdrs","33317"},{"rsjs","-0.437014136927"},
                 {"sdlt2","19.75"},{"sdgd2","61.00"}}), securities);
        const auto& i = institution.as_array().front();
        require(close(i.at("institution_holding_growth_pct").as_number(),
                      595.410813030444), "institution ratio to pct");
        require(close(i.at("shareholder_count_change").as_number(), -14560),
                "shareholder change");
        require(close(i.at("top10_float_holding_pct").as_number(), 19.75),
                "top ten direct pct");

        const auto research = tdx::normalize_shareholder_signal_rows(
            "list/func_jgzd101_1.jsn",
            one({{"$SC","0"},{"$ZQDM","000006"},{"date1","20260331"},
                 {"zxjlr","1457944.10"},{"qntqjlr","-5363191.76"},
                 {"jlrzf","127.18"},{"date","20260804"},{"bndycs","6"},
                 {"bnjgsl","42"},{"bnzaf","76.3804"}}), securities);
        const auto& g = research.as_array().front();
        require(close(g.at("net_profit_growth_pct").as_number(), 127.18),
                "profit direct pct");
        require(close(g.at("return_6m_pct").as_number(), 76.3804),
                "return direct pct");

        const auto small_cap = tdx::normalize_shareholder_signal_rows(
            "list/func_xszgp101_1.jsn",
            one({{"$SC","0"},{"$ZQDM","000006"},{"zxbgq","20260331"},
                 {"bgqmsz","5045785106.48"},{"zxcg","3775279"},
                 {"cgbd","604736"},{"ccsz","506453677.85"},
                 {"ccbl","10.38"},{"zcsz","83036970.15"},
                 {"jgsl","1.66"},{"jgbhl","4"}}), securities);
        const auto& small = small_cap.as_array().front();
        require(small.at("kind").as_string() == "small-cap-institution" &&
                    close(small.at("report_float_market_cap_yuan").as_number(),
                          5045785106.48) &&
                    close(small.at("institution_holding_change_shares").as_number(),
                          604736.0),
                "small-cap institution identity and raw yuan/share units");
        require(close(small.at("institution_holding_growth_pct").as_number(),
                      604736.0 / 3775279.0 * 100.0) &&
                    close(small.at("institution_holding_value_change_pct").as_number(),
                          83036970.15 / 506453677.85 * 100.0) &&
                    close(small.at("institution_count_change_pct").as_number(),
                          4.0 / 1.66 * 100.0) && small.at("raw").is_object(),
                "small-cap institution CFG formulas and neutral count source");

        const auto directory = tdx::normalize_notable_investor_directory_rows(
            one({{"name","徐开东"},{"gdjc","https://example.test/?gdid=GD012270"},
                 {"bqjs","35"},{"bqje","2101669085.54"},
                 {"bqcg","446352350"},{"bgq","20260728"},
                 {"$ZQDM","GD012270"}}));
        const auto& investor = directory.as_array().front();
        require(investor.at("investor_id").as_string() == "GD012270" &&
                    investor.at("holding_company_count").as_number() == 35 &&
                    investor.at("holding_shares").as_number() == 446352350,
                "notable investor directory totals");

        const auto holdings = tdx::normalize_notable_investor_holding_rows(
            "GD012270", "徐开东",
            one({{"$SC","0"},{"$ZQDM","000006"},{"bqpm","4"},{"sqpm","6"},
                 {"bqcg","7197059"},{"sqcg","6197059"},
                 {"bqje","43974030.49"},{"sqje","38288353.88"},
                 {"bqbl","2.75"},{"sqbl","2.3679"},
                 {"zxrq","20260331"},{"ly","十大流通股东"}}), securities);
        const auto& holding = holdings.as_array().front();
        require(holding.at("investor_name").as_string() == "徐开东" &&
                    holding.at("security").at("name").as_string() == "深振业A",
                "investor holding identity");
        require(close(holding.at("holding_change_shares").as_number(), 1000000) &&
                    close(holding.at("holding_value_change_yuan").as_number(),
                          5685676.61) &&
                    close(holding.at("holding_pct_change").as_number(), 0.3821),
                "investor holding CFG differences");
        std::cout << "shareholder signals tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
