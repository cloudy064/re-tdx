#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::convertible_bond_detail {

const std::vector<std::string> master_resources{
    "list/kzz_kzzsy201_1.jsn", "list/func_kzz_tkjd201.jsn",
    "list/func_kzz_lltk201.jsn", "list/func_kzz_hstk201.jsn",
    "list/func_kzz_shtk201.jsn", "list/func_kzz_xztk201.jsn"};
const std::string exchangeable_resource{"list/kjhz_kjhzsy201_1.jsn"};
const std::string exchangeable_projection_resource{"list/func_kzz103_1.jsn"};
const std::string pending_resource{"list/dfkzz201_1.jsn"};
const std::vector<std::string> pending_projection_resources{
    "list/func_kzz102_1.jsn", "list/gxjty_zq_dfkzz102_1.jsn"};
const std::string subscription_resource{"list/func_kkzss101_1.jsn"};
const std::string new_bond_projection_resource{"list/gxjty_zq_xkzz102_1.jsn"};
const std::string pricing_resource{"list/gxjty_zq_kzzsy101_1.jsn"};

}  // namespace tdx::convertible_bond_detail
