#include "tdx/common.hpp"
#include "tdx/hk_finance.hpp"

#include "hk_finance_internal.hpp"
#include "formula_context_hk_finance_internal.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-8) {
    return std::abs(left - right) <= epsilon;
}

tdx::Bytes bytes(std::string_view text) {
    return tdx::Bytes(text.begin(), text.end());
}

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

constexpr std::string_view kFixture =
    "00001|3830044500|20250630|13913000|222.4000|22.0000|113758600|"
    "55479000|85200|144.852|111001|32.5566|19721101|3830044500|"
    "12107500|24|146.7734\r\n"
    "00700|9122883125|20250930|55739500|450|2006.1776|207327200|"
    "117208000|16658200|140.260|110002|21.7985|20040616|9122883125|"
    "8843500|13|20.9248\n";

}  // namespace

int main() {
    try {
        const fs::path source = "synthetic-hkcwdata.dat";
        const auto records = tdx::hk_finance_detail::parse_decrypted_resource(
            bytes(kFixture), source);
        require(records.size() == 2, "record count");
        require(records.front().raw[0] == "00001", "first code");
        require(records.front().raw[2] == "20250630", "report date");
        require(records.front().raw[10] == "111001", "classification");

        tdx::HkFinanceQuery all;
        all.limit = 1;
        const auto document = tdx::hk_finance_detail::build_document(
            records, source, all);
        require(document.at("schema").as_string() ==
                    "tdx-market-hk-finance-native-v1", "schema");
        require(document.at("match_count").as_number() == 2, "all match count");
        require(document.at("returned").as_number() == 1, "page limit");
        require(document.at("transport").at("network_requests").as_number() == 0,
                "offline transport");
        require(document.at("native_schema").at("fields").size() == 16,
                "native field catalog");
        const auto& row = document.at("rows").as_array().front();
        require(row.at("report_date").as_string() == "20250630",
                "report projection");
        require(row.at("listing_date").as_string() == "19721101",
                "listing projection");
        require(close(row.at("native_values").at("security_152").as_number(),
                      383004.45), "native /10000 conversion");
        require(close(row.at("native_values").at("security_240").as_number(),
                      2.224), "native /100 conversion");
        require(close(row.at("finance").at("shares").at("total_10k")
                          .as_number(), 383004.45),
                "typed total shares");
        require(close(row.at("finance").at("income_statement")
                          .at("revenue_10k").as_number(), 13913000.0),
                "typed revenue");
        require(close(row.at("finance").at("per_share").at("dividend")
                          .as_number(), 2.224),
                "typed per-share dividend");
        require(close(row.at("finance").at("valuation").at("pe_ttm")
                          .as_number(), 32.5566),
                "typed TTM PE");
        require(close(row.at("finance").at("valuation").at("pe_static")
                          .as_number(), 146.7734),
                "typed static PE");
        const auto& fields = document.at("native_schema").at("fields").as_array();
        require(fields[3].at("semantic_key").as_string() ==
                    "per_share.dividend", "HK dividend semantic metadata");
        require(fields[14].at("semantic_status").as_string() ==
                    "verified-role-enum-unresolved",
                "currency adjustment enum boundary");
        require(row.at("finance").at("currency_adjustment")
                    .at("native_code").as_number() == 24.0 &&
                row.at("finance").at("currency_adjustment")
                    .at("conversion_required").as_bool(),
                "typed currency adjustment marker");
        const std::set<int> hk_selectors{
            1, 2, 3, 6, 7, 9, 10, 16, 19, 20, 30, 31,
            32, 33, 34, 37, 38, 42, 53};
        const auto formula_finance =
            tdx::formula_context_detail::project_tcalc_hk_finance_values(
                row, 31, 6, hk_selectors);
        require(formula_finance.size() == hk_selectors.size() &&
                    formula_finance.at("2").as_number() == 31.0 &&
                    formula_finance.at("3").as_number() == 6.0,
                "TCalc HK market and security-type selectors");
        require(close(formula_finance.at("6").as_number(), 3830044500.0) &&
                    close(formula_finance.at("10").as_number(),
                          1137586000000.0) &&
                    close(formula_finance.at("19").as_number(),
                          554790000000.0) &&
                    close(formula_finance.at("20").as_number(),
                          139130000000.0) &&
                    close(formula_finance.at("30").as_number(),
                          852000000.0),
                "TCalc type-105 HK scaled financial selectors");
        require(close(formula_finance.at("1").as_number(), 3830044500.0) &&
                    close(formula_finance.at("7").as_number(),
                          3830044500.0),
                "TCalc type-103 HK category-2 capital slots");
        require(close(formula_finance.at("33").as_number(), 0.22) &&
                    close(formula_finance.at("34").as_number(), 144.852) &&
                    close(formula_finance.at("38").as_number(), 0.22) &&
                    close(formula_finance.at("53").as_number(), 2.224),
                "TCalc HK per-share aliases and dividend selector");
        require(formula_finance.at("31").as_number() == 0.0 &&
                    formula_finance.at("32").as_number() == 0.0 &&
                    formula_finance.at("42").as_number() > 19000.0,
                "TCalc HK zero branches and listing days");
        require_error([&] {
            (void)tdx::formula_context_detail::
                project_tcalc_hk_finance_values(row, 71, 6, {23});
        }, "not locally recoverable");
        require_error([&] {
            (void)tdx::formula_context_detail::
                project_tcalc_hk_finance_values(row, 71, 6, {7});
        }, "not locally recoverable");
        require(close(tdx::formula_context_detail::
                          project_tcalc_hk_finance_values(row, 71, 6, {6})
                              .at("6").as_number(),
                      3830044500.0),
                "legacy HK market retains proven type-105 selectors");
        require(fields[10].at("semantic_key").as_string() ==
                    "valuation.pe_ttm", "TTM PE semantic metadata");
        require(fields[15].at("semantic_key").as_string() ==
                    "valuation.pe_static", "static PE semantic metadata");

        tdx::HkFinanceQuery selected;
        selected.code = "00700";
        const auto security = tdx::hk_finance_detail::build_document(
            records, source, selected);
        require(security.at("match_count").as_number() == 1, "code filter");
        require(security.at("rows").as_array().front().at("classification_code")
                    .as_string() == "110002", "selected classification");

        tdx::HkFinanceQuery classification;
        classification.classification = "111001";
        classification.report_from = "20250101";
        classification.report_to = "20250630";
        require(tdx::hk_finance_detail::build_document(records, source,
                    classification).at("match_count").as_number() == 1,
                "classification and date filters");

        tdx::HkFinanceQuery search;
        search.query = "9122883125";
        require(tdx::hk_finance_detail::build_document(records, source, search)
                    .at("rows").as_array().front().at("code").as_string() ==
                    "00700", "native value search");

        require_error([&] {
            tdx::hk_finance_detail::parse_decrypted_resource(
                bytes("00001|1|20250101\n"), source);
        }, "17 columns");
        require_error([&] {
            tdx::hk_finance_detail::parse_decrypted_resource(
                bytes("0001|1|20250101|1|1|1|1|1|1|1|1|1|20200101|1|1|1|1\n"),
                source);
        }, "security code");
        require_error([&] {
            tdx::hk_finance_detail::parse_decrypted_resource(
                bytes("00001|x|20250101|1|1|1|1|1|1|1|1|1|20200101|1|1|1|1\n"),
                source);
        }, "numeric column 1");
        require_error([&] {
            tdx::hk_finance_detail::parse_decrypted_resource(
                bytes(std::string(kFixture) + std::string(kFixture.substr(
                    0, kFixture.find('\n') + 1))), source);
        }, "duplicate HK security code");

        tdx::HkFinanceQuery invalid;
        invalid.code = "0700";
        require_error([&] {
            tdx::hk_finance_detail::build_document(records, source, invalid);
        }, "five digits");

        const auto missing_root = fs::temp_directory_path() /
            "tdx-hk-finance-definitely-missing";
        require_error([&] {
            tdx::load_local_hk_finance(missing_root, {});
        }, "unavailable");

        std::cout << "HK finance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
