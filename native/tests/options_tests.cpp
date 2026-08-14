#include "tdx/options.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        const auto commodity = tdx::parse_option_instrument(
            5, "A 8X06SH", "A2609-C-4400");
        require(commodity && commodity->underlying_market_id == 29 &&
                    commodity->underlying_code == "A2609" && commodity->call &&
                    commodity->american && commodity->futures_model &&
                    commodity->strike == 4400.0,
                "DCE option metadata follows the TdxW host mapping");

        const auto fixture_root = std::filesystem::temp_directory_path() /
                                  "tdx-options-expiry-fixture";
        std::filesystem::create_directories(fixture_root / "T0002" / "hq_cache");
        std::ofstream(fixture_root / "TdxW.exe", std::ios::binary).put('\0');
        std::ofstream(fixture_root / "T0002" / "hq_cache" / "code2name_qq.ini",
                      std::ios::binary)
            << "A,name,OD,1,2603,20260225,10,0.5,6,,0.5,u,u,104,12,0,rule\n";
        std::ofstream(fixture_root / "T0002" / "hq_cache" / "neednote.dat",
                      std::ios::binary)
            << "[Data]\nRecentHSHoliday=20260101,20260216,20260217,20260218,"
               "20260219,20260220,20260223,\n";
        const auto cutoff_expiry = tdx::resolve_option_expiry_document(
            fixture_root, *commodity);
        require(cutoff_expiry.at("expiry").as_string() == "2026-08-18" &&
                    cutoff_expiry.at("rule_code").as_number() == 104,
                "TDX DCE product rule resolves the public delivery month to its expiry month");
        const auto cutoff_contract = tdx::parse_option_instrument(
            5, "TEST2603", "A2603-C-4400");
        const auto exact_cutoff = tdx::resolve_option_expiry_document(
            fixture_root, *cutoff_contract);
        require(exact_cutoff.at("expiry").as_string() == "2026-02-25" &&
                    exact_cutoff.at("status").as_string() == "exact_cutoff_override",
                "TDX rule transition contract uses its explicit resource expiry");

        const auto index = tdx::parse_option_instrument(
            7, "IO8XTEST", "IO2609-P-3900");
        require(index && index->underlying_market_id == 47 &&
                    index->underlying_code == "IF300" && !index->call &&
                    !index->american && !index->futures_model,
                "CFFEX index option metadata maps to the TDX spot index");

        auto instruments = tdx::Json::array();
        instruments.push_back(tdx::Json::parse(
            R"({"market_id":5,"code":"A 8X06SH","name":"A2609-C-4400"})"));
        instruments.push_back(tdx::Json::parse(
            R"({"market_id":5,"code":"A 8X06SI","name":"A2609-P-4400"})"));
        instruments.push_back(tdx::Json::parse(
            R"({"market_id":29,"code":"A2609","name":"豆一2609"})"));
        auto directory = tdx::Json::object();
        directory["schema"] = "tdx-expansion-instruments-v1";
        directory["total"] = 3;
        directory["instruments"] = std::move(instruments);
        const auto puts = tdx::normalize_option_catalog_document(
            directory, 5, "A2609", {}, "put", {}, 100);
        require(puts.at("matched").as_number() == 1 &&
                    puts.at("options").as_array()[0].at("type").as_string() == "put",
                "option catalog normalizes and filters call/put contracts");
        const auto cached_puts = tdx::normalize_option_catalog_document(
            puts, 5, "A2609", {}, "put", {}, 100);
        require(cached_puts.at("matched").as_number() == 1,
                "normalized in-process option catalog can be filtered again");

        require(tdx::tdx_normal_cdf(0.0) == 0.0,
                "TQQCalc zero-boundary CDF behavior is preserved");
        require(std::abs(tdx::tdx_normal_cdf(1.0) - 0.8413447) < 0.00001,
                "TQQCalc normal CDF approximation is reproduced");
        const auto historical = tdx::tdx_historical_volatility({100.0, 110.0, 100.0});
        require(std::abs(historical - std::log(1.1) * std::sqrt(250.0)) < 1e-12,
                "TdxW selector-35 close-to-close population volatility is reproduced");

        const double european_price = tdx::tdx_option_price(
            false, false, true, 1.0, 105.0, 100.0, 0.5, 0.25, 0.02);
        const double european_iv = tdx::tdx_implied_volatility(
            false, false, true, 1.0, 105.0, 100.0, 0.5, 0.2, 0.02,
            european_price);
        require(std::abs(european_iv - 0.25) < 0.00011,
                "European TQQCalc implied volatility inverts its pricing model");
        const auto european_analytics = tdx::tdx_option_analytics(
            false, false, true, 1.0, 105.0, 100.0, 0.5, 0.25, 0.02);
        require(european_analytics.price > 10.0 &&
                    european_analytics.delta > 0.6 && european_analytics.delta < 0.8 &&
                    european_analytics.gamma > 0.0 && european_analytics.vega > 0.0 &&
                    european_analytics.rho > 0.0 && european_analytics.theta < 0.0,
                "European TQQCalc_Index Greeks have the expected signs and scale");

        const double american_price = tdx::tdx_option_price(
            true, true, false, 1.0, 4700.0, 4800.0, 0.1, 0.30, 0.0187);
        const double american_iv = tdx::tdx_implied_volatility(
            true, true, false, 1.0, 4700.0, 4800.0, 0.1, 0.25, 0.0187,
            american_price);
        require(std::abs(american_iv - 0.30) < 0.00011,
                "American 20-node TQQCalc implied volatility inverts its model");
        const auto american_analytics = tdx::tdx_option_analytics(
            true, true, false, 1.0, 4700.0, 4800.0, 0.1, 0.30, 0.0187);
        require(american_analytics.price > 0.0 && american_analytics.delta < 0.0 &&
                    american_analytics.gamma > 0.0 && american_analytics.vega > 0.0,
                "American TQQCalc_Index tree Greeks are reconstructed");

        tdx::Json chain_catalog = tdx::Json::object();
        chain_catalog["options"] = tdx::Json::array();
        tdx::Json chain_quotes = tdx::Json::object();
        chain_quotes["quotes"] = tdx::Json::array();
        chain_quotes["endpoint"] = "fixture:7727";
        const struct ChainFixture {
            const char* code; const char* name; bool call; double strike;
            std::uint64_t volume; std::uint64_t open_interest;
        } chain_fixture[]{
            {"C95", "IO2602-C-95", true, 95.0, 10, 100},
            {"P95", "IO2602-P-95", false, 95.0, 30, 150},
            {"C105", "IO2602-C-105", true, 105.0, 20, 200},
            {"P105", "IO2602-P-105", false, 105.0, 60, 450},
        };
        for (const auto& fixture : chain_fixture) {
            const auto parsed = tdx::parse_option_instrument(7, fixture.code, fixture.name);
            require(parsed.has_value(), "option-chain fixture parses");
            chain_catalog["options"].push_back(tdx::option_instrument_document(*parsed));
            tdx::Json quote = tdx::Json::object();
            quote["market_id"] = 7; quote["code"] = fixture.code;
            quote["price"] = tdx::tdx_option_price(
                false, false, fixture.call, 1.0, 100.0, fixture.strike,
                32.0 / 365.0, 0.25, 0.02);
            quote["pre_close"] = quote.at("price");
            quote["volume"] = fixture.volume;
            quote["open_interest"] = fixture.open_interest;
            chain_quotes["quotes"].push_back(std::move(quote));
        }
        const auto chain = tdx::analyze_option_chain_document(
            chain_catalog, chain_quotes, 100.0, "2026-01-01", "2026-02-01", 0.20, 0.02);
        require(chain.at("summary").at("strike_count").as_number() == 2.0 &&
                    chain.at("summary").at("calculated_iv_count").as_number() == 4.0,
                "option-chain analysis pairs call/put legs and calculates all IVs");
        require(std::abs(chain.at("summary").at("put_call_volume_ratio").as_number() - 3.0) < 1e-12 &&
                    std::abs(chain.at("summary").at("put_call_open_interest_ratio").as_number() - 2.0) < 1e-12,
                "option-chain volume and open-interest Put/Call ratios are exact");
        require(chain.at("summary").at("max_pain").at("strike").as_number() == 105.0,
                "option-chain maximum-pain strike minimizes aggregate open-interest payout");
        require(std::abs(chain.at("strikes").as_array().front().at("call")
                             .at("implied_volatility").as_number() - 0.25) < 0.00011,
                "option-chain IV uses the reconstructed TQQCalc solver");

        std::cout << "option tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "option test failed: " << error.what() << '\n';
        return 1;
    }
}
