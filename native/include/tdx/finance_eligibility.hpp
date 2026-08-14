#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace tdx {

using SecurityKey = std::pair<int, std::string>;

// TdxW builds FINANCE(48/52) from named sections in
// T0002/hq_cache/spblock.dat.  Each member is encoded as one market digit
// followed by the six-digit security code.
struct FinanceEligibilityResource {
    std::string source_path;
    std::set<SecurityKey> margin_financing;
    std::set<SecurityKey> sh_stock_connect;
    std::set<SecurityKey> sz_stock_connect;
    // TdxW's IST0CODE host callback also consumes these two named sections.
    std::set<SecurityKey> t0_funds;
    std::set<SecurityKey> beijing_convertible_bonds;
};

FinanceEligibilityResource parse_finance_eligibility_text(
    const std::string& utf8_text,
    std::string source_path = {});

FinanceEligibilityResource load_finance_eligibility_resource(
    const std::filesystem::path& tdx_root);

std::shared_ptr<const FinanceEligibilityResource> cached_finance_eligibility_resource(
    const std::filesystem::path& tdx_root);

// FINANCE(48) is the union of the 沪港通SH and 深港通SZ sections;
// FINANCE(52) is membership in the 融资融券 section.
std::map<int, int> finance_eligibility_values(
    const FinanceEligibilityResource& resource,
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids);

}  // namespace tdx
