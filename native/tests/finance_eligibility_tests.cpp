#include "tdx/finance_eligibility.hpp"
#include "tdx/security_status.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

}  // namespace

int main() {
    try {
        const auto resource = tdx::parse_finance_eligibility_text(
            "#融资融券\r\n"
            "0000001\r\n"
            "1688788\r\n"
            "bad-row\r\n"
            "#沪港通SH\r\n"
            "1688788\r\n"
            "#深港通SZ\r\n"
            "0000001\r\n"
            "#T+0基金\r\n"
            "1510050\r\n"
            "#北证可转债\r\n"
            "2810011\r\n"
            "#其他分组\r\n"
            "2000001\r\n",
            "fixture/spblock.dat");
        require(resource.margin_financing.size() == 2,
                "融资融券 section parsing failed");
        require(resource.sh_stock_connect.size() == 1 &&
                resource.sz_stock_connect.size() == 1,
                "沪深港通 section parsing failed");
        require(resource.t0_funds.count({1, "510050"}) == 1 &&
                    resource.beijing_convertible_bonds.count({2, "810011"}) == 1,
                "IST0CODE spblock sections parsing failed");

        const auto ping_an = tdx::finance_eligibility_values(
            resource, "sz", "000001", {48, 52});
        require(ping_an.at(48) == 1 && ping_an.at(52) == 1,
                "FINANCE(48/52) union and margin mapping failed");
        const auto sh = tdx::finance_eligibility_values(
            resource, "1", "688788", {48, 52});
        require(sh.at(48) == 1 && sh.at(52) == 1,
                "numeric Shanghai market normalization failed");
        const auto missing = tdx::finance_eligibility_values(
            resource, "bj", "000001", {48, 52});
        require(missing.at(48) == 0 && missing.at(52) == 0,
                "absent eligibility must bind zero");

        require(tdx::tcalc_is_t0_security(resource, 1, "510050") &&
                    tdx::tcalc_is_t0_security(resource, 0, "123001") &&
                    !tdx::tcalc_is_t0_security(resource, 0, "122001") &&
                    tdx::tcalc_is_t0_security(resource, 1, "113001") &&
                    !tdx::tcalc_is_t0_security(resource, 1, "112001") &&
                    tdx::tcalc_is_t0_security(resource, 2, "810011"),
                "IST0CODE exact fixed-code and named-section union failed");
        require(tdx::tcalc_is_st_security(0, "000001", "*ST样本") &&
                    tdx::tcalc_is_st_security(1, "688001", "ST样本") &&
                    !tdx::tcalc_is_st_security(1, "000001", "ST指数"),
                "ISSTCODE applicability and substring rule failed");
        const auto quit = tdx::parse_security_quit_text(
            "0|000001|0||20130327\r\n"
            "1|600001|0|20260810|20260901\r\n"
            "1|600002|0|20260808|20260901\r\n"
            "1|600003|1||\r\n",
            20260809, "fixture/infoharbor_spec.cfg");
        require(quit.parsed_record_count == 4 && quit.active.size() == 2 &&
                    tdx::tcalc_is_quit_security(quit, 0, "000001") &&
                    tdx::tcalc_is_quit_security(quit, 1, "600002") &&
                    !tdx::tcalc_is_quit_security(quit, 1, "600001"),
                "ISQUITCODE activation-date and status parsing failed");
        require(tdx::tcalc_is_futures_or_option_category(3) &&
                    tdx::tcalc_is_futures_or_option_category(12) &&
                    !tdx::tcalc_is_futures_or_option_category(5),
                "ISQHQQCODE category union failed");

        std::cout << "finance eligibility tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
