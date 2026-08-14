#include "tdx/blowfish.hpp"
#include "tdx/common.hpp"
#include "tdx/investment.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kInvestmentKey = "359=dm5h3q543;6jd;aeda";

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-10) {
    return std::abs(left - right) <= epsilon;
}

std::string hex(const tdx::Bytes& data) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(data.size() * 2);
    for (const auto byte : data) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0FU]);
    }
    return result;
}

void write_i32_le(tdx::Bytes& target, std::size_t offset, std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    for (std::size_t index = 0; index < 4; ++index)
        target[offset + index] = static_cast<std::uint8_t>(bits >> (index * 8U));
}

void write_f64_le(tdx::Bytes& target, std::size_t offset, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::size_t index = 0; index < 8; ++index)
        target[offset + index] = static_cast<std::uint8_t>(bits >> (index * 8U));
}

void write_field(tdx::Bytes& target, std::size_t offset, std::size_t width,
                 std::string_view utf8) {
    const auto encoded = tdx::encode_gbk(utf8);
    require(encoded.size() < width, "fixture text exceeds field");
    std::copy(encoded.begin(), encoded.end(),
              target.begin() + static_cast<std::ptrdiff_t>(offset));
    target[offset + encoded.size()] = 0;
}

tdx::Bytes fee_rule(int market_kind, std::string_view prefix,
                    std::string_view name, double commission_rate,
                    double minimum_commission, double stamp_tax_rate,
                    double transfer_fee_rate, double minimum_transfer_fee,
                    double fixed_fee) {
    tdx::Bytes result(79, 0);
    write_i32_le(result, 0, market_kind);
    write_field(result, 4, 7, prefix);
    write_field(result, 11, 20, name);
    const double values[] = {
        commission_rate, minimum_commission, stamp_tax_rate,
        transfer_fee_rate, minimum_transfer_fee, fixed_fee,
    };
    for (std::size_t index = 0; index < 6; ++index)
        write_f64_le(result, 31 + index * 8, values[index]);
    return result;
}

struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("tdx-investment-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    fs::path private_directory = root / "T0002" / "private-invest";
    fs::path quote_snapshot = root / "quotes.json";
    fs::path empty_quote_snapshot = root / "empty-quotes.json";

    Fixture() {
        fs::create_directories(private_directory);
        tdx::atomic_write_text(
            root / "T0002" / "user.ini",
            "[OTHER]\r\nINVESTPATH=private-invest\r\n");

        tdx::Bytes pinfo(192, 0x5A);
        write_field(pinfo, 0, 21, "测试组合");
        write_field(pinfo, 21, 21, "p@ss");
        tdx::atomic_write_bytes(
            private_directory / "pinfo.dat",
            tdx::blowfish_ecb_encrypt(
                std::move(pinfo), kInvestmentKey,
                tdx::BlowfishWordOrder::little_endian));

        tdx::Bytes detail(1200, 0);
        write_field(detail, 0, 21, "p@ss");
        write_i32_le(detail, 200, 0);
        write_i32_le(detail, 204, 20260812);
        write_field(detail, 208, 101, "首笔买入");
        write_i32_le(detail, 309, -1);
        write_field(detail, 313, 10, "000001");
        write_i32_le(detail, 323, 0);
        write_i32_le(detail, 347, 1000);
        write_f64_le(detail, 351, 10.5);
        write_f64_le(detail, 359, 5.2);
        write_f64_le(detail, 367, 10505.2);

        write_i32_le(detail, 400, 0);
        write_i32_le(detail, 404, 20260813);
        write_field(detail, 408, 101, "现金调整");
        write_i32_le(detail, 509, 1);
        write_field(detail, 513, 10, "");
        write_i32_le(detail, 523, 10);
        write_f64_le(detail, 567, 2500.0);

        write_i32_le(detail, 600, 0);
        write_i32_le(detail, 604, 20260814);
        write_field(detail, 608, 101, "现金分红");
        write_i32_le(detail, 709, 1);
        write_field(detail, 713, 10, "000001");
        write_i32_le(detail, 723, 2);
        write_i32_le(detail, 747, 1000);
        write_f64_le(detail, 751, 0.1);
        write_f64_le(detail, 767, 100.0);

        write_i32_le(detail, 800, 0);
        write_i32_le(detail, 804, 20260815);
        write_field(detail, 808, 101, "十送一");
        write_i32_le(detail, 909, 0);
        write_field(detail, 913, 10, "000001");
        write_i32_le(detail, 923, 3);
        write_i32_le(detail, 947, 100);

        write_i32_le(detail, 1000, 0);
        write_i32_le(detail, 1004, 20260816);
        write_field(detail, 1008, 101, "部分卖出");
        write_i32_le(detail, 1109, 1);
        write_field(detail, 1113, 10, "000001");
        write_i32_le(detail, 1123, 1);
        write_i32_le(detail, 1147, 200);
        write_f64_le(detail, 1151, 12.0);
        write_f64_le(detail, 1159, 5.0);
        write_f64_le(detail, 1167, 2395.0);
#ifdef _WIN32
        const fs::path detail_path = private_directory /
            fs::path(tdx::utf8_to_wide("测试组合.da0"));
#else
        const fs::path detail_path = private_directory / "测试组合.da0";
#endif
        tdx::atomic_write_bytes(
            detail_path,
            tdx::blowfish_ecb_encrypt(
                std::move(detail), kInvestmentKey,
                tdx::BlowfishWordOrder::little_endian));

        auto broad = fee_rule(0, "0", "深A", 0.001, 1.0, 0.0005,
                              0.00002, 0.1, 0.0);
        auto exact = fee_rule(0, "000", "深A精选", 0.0003, 5.0, 0.0005,
                              0.00002, 0.1, 1.0);
        auto sh = fee_rule(1, "6", "沪A", 0.0003, 5.0, 0.0005,
                           0.00002, 0.1, 0.0);
        broad.insert(broad.end(), exact.begin(), exact.end());
        broad.insert(broad.end(), sh.begin(), sh.end());
        tdx::atomic_write_bytes(root / "T0002" / "trdpara.dat", broad);

        tdx::Json snapshot = tdx::Json::object();
        snapshot["schema"] = "tdx-market-snapshot-native-v1";
        snapshot["generated_at"] = "2026-08-16T15:00:00";
        snapshot["command"] = "0x054C";
        tdx::Json quote_rows = tdx::Json::array();
        tdx::Json quote = tdx::Json::object();
        quote["market_id"] = 0;
        quote["code"] = "000001";
        quote["name"] = "平安银行";
        quote["last_price"] = 12.0;
        quote["pre_close_price"] = 11.5;
        quote["change_pct"] = (12.0 / 11.5 - 1.0) * 100.0;
        quote["time_raw"] = 150000;
        quote_rows.push_back(std::move(quote));
        snapshot["records"] = std::move(quote_rows);
        tdx::atomic_write_text(quote_snapshot, snapshot.dump(2) + "\n");
        snapshot["records"] = tdx::Json::array();
        tdx::atomic_write_text(
            empty_quote_snapshot, snapshot.dump(2) + "\n");
    }

    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
};

template <typename Callback>
void require_error(Callback&& callback, const char* needle) {
    try {
        callback();
    } catch (const std::exception& error) {
        require(std::string(error.what()).find(needle) != std::string::npos,
                "unexpected error text");
        return;
    }
    throw std::runtime_error("expected error");
}

}  // namespace

int main() {
    try {
        const tdx::Bytes zeros(8, 0);
        const std::string zero_key(8, '\0');
        const auto standard_cipher = tdx::blowfish_ecb_encrypt(
            zeros, zero_key, tdx::BlowfishWordOrder::big_endian);
        require(hex(standard_cipher) == "4EF997456198DD78",
                "standard Blowfish vector");
        require(tdx::blowfish_ecb_decrypt(
                    standard_cipher, zero_key,
                    tdx::BlowfishWordOrder::big_endian) == zeros,
                "standard Blowfish round trip");

        Fixture fixture;
        tdx::InvestmentQuery summary_query;
        const auto summary = tdx::load_local_investment(
            fixture.root, summary_query);
        require(summary.at("schema").as_string() ==
                    "tdx-investment-portfolio-v1", "schema");
        require(summary.at("network_requests").as_number() == 0,
                "offline source");
        require(summary.at("summary").at("portfolio_count").as_number() == 1,
                "portfolio count");
        require(summary.at("summary").at(
                    "transaction_record_count").as_number() == 5,
                "transaction count excludes header");
        require(summary.at("summary").at("fee_rule_count").as_number() == 3,
                "fee rule count");
        require(summary.at("sources").at("private_directory").at(
                    "source").as_string() == "user.ini:[OTHER]/INVESTPATH",
                "configured private directory");
        const auto& portfolio = summary.at("portfolios").as_array().front();
        require(portfolio.at("name").as_string() == "测试组合",
                "GBK portfolio name");
        require(portfolio.at("password_protected").as_bool(),
                "password presence only");
        require(portfolio.at("header_password_matches").as_bool(),
                "detail header password");
        require(portfolio.as_object().find("password") ==
                    portfolio.as_object().end(), "password value is absent");
        require(!summary.at("privacy").at(
                    "password_values_emitted").as_bool(), "privacy contract");

        tdx::InvestmentQuery transaction_page_query;
        transaction_page_query.view = "transactions";
        transaction_page_query.portfolio_name = "测试组合";
        transaction_page_query.limit = 1;
        const auto transaction_page = tdx::load_local_investment(
            fixture.root, transaction_page_query);
        require(transaction_page.at("pagination").at("total").as_number() == 5,
                "transaction pagination total");
        require(transaction_page.at("pagination").at("returned").as_number() == 1,
                "transaction pagination returned");
        require(transaction_page.at("pagination").at("has_more").as_bool(),
                "transaction pagination has more");
        const auto& buy = transaction_page.at("transactions").as_array().front();
        require(buy.at("date").as_string() == "2026-08-12",
                "transaction date");
        require(buy.at("type").as_string() == "security-buy",
                "transaction type");
        require(buy.at("type_label").as_string() == "买入股票",
                "transaction resource label");
        require(buy.at("cash_direction_name").as_string() == "outflow",
                "buy cash direction");
        require(buy.at("security").at("code").as_string() == "000001",
                "transaction security code");
        require(close(buy.at("recorded_total").as_number(), 10505.2),
                "transaction native total");
        require(buy.as_object().find("note") == buy.as_object().end(),
                "transaction notes default private");

        tdx::InvestmentQuery all_transactions_query;
        all_transactions_query.view = "transactions";
        all_transactions_query.portfolio_name = "测试组合";
        all_transactions_query.include_notes = true;
        const auto all_transactions = tdx::load_local_investment(
            fixture.root, all_transactions_query);
        require(all_transactions.at("transactions").as_array().size() == 5,
                "all transaction rows");
        require(all_transactions.at("transactions").as_array().front().at(
                    "note").as_string() == "首笔买入",
                "opt-in transaction note");
        const auto& cash = all_transactions.at("transactions").as_array()[1];
        require(cash.at("type").as_string() == "cash-blue-adjustment",
                "cash adjustment type");
        require(cash.at("security").is_null(),
                "cash transaction has no security");
        require(close(cash.at("recorded_total").as_number(), 2500.0),
                "cash adjustment amount");
        require(all_transactions.at("privacy").at(
                    "transaction_notes_emitted").as_bool(),
                "transaction notes opt-in contract");

        tdx::InvestmentQuery holdings_query;
        holdings_query.view = "holdings";
        holdings_query.portfolio_name = "测试组合";
        const auto holdings = tdx::load_local_investment(
            fixture.root, holdings_query);
        require(holdings.at("ledger_summary").at(
                    "transaction_count").as_number() == 5,
                "holding replay transaction count");
        require(holdings.at("ledger_summary").at(
                    "active_holding_count").as_number() == 1,
                "active holding count");
        require(holdings.at("holdings").as_array().size() == 1,
                "holding row count");
        const auto& holding = holdings.at("holdings").as_array().front();
        require(holding.at("quantity").as_number() == 900,
                "holding quantity after bonus and sale");
        require(close(holding.at("average_cost").as_number(),
                      9.459272727272727, 1e-9),
                "native running average cost");
        require(close(holding.at("realized_profit").as_number(),
                      503.14545, 1e-3),
                "native realized profit");
        require(close(holding.at("cash_dividends").as_number(), 100.0),
                "holding cash dividends");
        require(close(holdings.at("cash_ledger").at("inflows").as_number(),
                      4995.0), "ledger inflows");
        require(close(holdings.at("cash_ledger").at("outflows").as_number(),
                      10505.2), "ledger outflows");
        require(close(holdings.at("cash_ledger").at("net_flow").as_number(),
                      -5510.2), "ledger net flow");
        require(!holdings.at("privacy").at(
                    "transaction_notes_emitted").as_bool(),
                "holding replay never emits notes");

        tdx::InvestmentQuery valuation_query;
        valuation_query.view = "valuation";
        valuation_query.portfolio_name = "测试组合";
        valuation_query.quote_snapshot_path = fixture.quote_snapshot;
        const auto valuation = tdx::load_local_investment(
            fixture.root, valuation_query);
        require(valuation.at("network_requests").as_number() == 0,
                "offline valuation does not use network");
        require(valuation.at("valuation").at("quote_source").at(
                    "mode").as_string() == "offline-snapshot",
                "offline valuation provenance");
        require(valuation.at("valuation").at("summary").at(
                    "complete").as_bool(), "complete valuation coverage");
        require(valuation.at("valuation").at("summary").at(
                    "valued_holding_count").as_number() == 1,
                "valued holding count");
        const auto& valued = valuation.at("valuation").at(
            "positions").as_array().front();
        require(valued.at("security").at("name").as_string() == "平安银行",
                "quote security name");
        require(close(valued.at("valuation").at(
                    "market_value").as_number(), 10800.0),
                "position market value");
        require(close(valued.at("valuation").at(
                    "unrealized_profit").as_number(), 2286.65454545, 1e-6),
                "position unrealized profit");
        require(close(valued.at("valuation").at("estimated_exit_fee").at(
                    "total_fee").as_number(), 11.616, 1e-9),
                "estimated sell fee");
        require(close(valued.at("valuation").at(
                    "estimated_break_even_price").as_number(),
                      9.47086424, 1e-6),
                "fee-aware break-even price");
        require(close(valuation.at("valuation").at("summary").at(
                    "total_assets").as_number(), 5289.8, 1e-6),
                "reconstructed cash plus market value");
        require(close(valuation.at("valuation").at("summary").at(
                    "total_profit").as_number(), 2789.8, 1e-3),
                "realized plus unrealized portfolio profit");

        valuation_query.quote_snapshot_path = fixture.empty_quote_snapshot;
        const auto partial_valuation = tdx::load_local_investment(
            fixture.root, valuation_query);
        require(!partial_valuation.at("valuation").at("summary").at(
                    "complete").as_bool(), "missing quote marks partial valuation");
        require(partial_valuation.at("valuation").at("summary").at(
                    "missing_quote_count").as_number() == 1,
                "missing quote count");
        require(partial_valuation.at("valuation").at("summary").at(
                    "total_assets").is_null() &&
                partial_valuation.at("valuation").at("summary").at(
                    "total_profit").is_null(),
                "partial coverage never claims total portfolio values");

        tdx::InvestmentQuery missing_portfolio_query;
        missing_portfolio_query.view = "transactions";
        missing_portfolio_query.portfolio_name = "不存在";
        require_error([&] {
            tdx::load_local_investment(fixture.root, missing_portfolio_query);
        }, "not found");

        tdx::InvestmentQuery quote_query;
        quote_query.view = "fee-rules";
        quote_query.market = "sz";
        quote_query.code = "000001";
        quote_query.side = "sell";
        quote_query.price = 10.0;
        quote_query.quantity = 1000;
        const auto quote = tdx::load_local_investment(fixture.root, quote_query);
        require(quote.at("selection").at("rule").at(
                    "prefix").as_string() == "000",
                "longest matching security prefix");
        require(close(quote.at("fee_quote").at("notional").as_number(),
                      10000.0), "notional");
        require(close(quote.at("fee_quote").at("commission").as_number(),
                      5.0), "minimum commission");
        require(close(quote.at("fee_quote").at("stamp_tax").as_number(),
                      5.0), "sell stamp tax");
        require(close(quote.at("fee_quote").at("transfer_fee").as_number(),
                      0.2), "transfer fee");
        require(close(quote.at("fee_quote").at("total_fee").as_number(),
                      11.2), "total fee");
        require(quote.as_object().find("portfolios") == quote.as_object().end(),
                "fee-only view skips private index");

        tdx::atomic_write_bytes(fixture.private_directory / "pinfo.dat", {0x01});
        tdx::InvestmentQuery portfolios_query;
        portfolios_query.view = "portfolios";
        require_error([&] {
            tdx::load_local_investment(fixture.root, portfolios_query);
        }, "invalid size");

        const auto empty_root = fixture.root / "empty-install";
        fs::create_directories(empty_root / "T0002");
        const auto empty = tdx::load_local_investment(
            empty_root, tdx::InvestmentQuery{});
        require(empty.at("summary").at("portfolio_count").as_number() == 0,
                "absent pinfo is a clean empty state");
        require(empty.at("summary").at("fee_rule_count").as_number() == 0,
                "absent fee rules are a clean empty state");

        std::cout << "Investment tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
