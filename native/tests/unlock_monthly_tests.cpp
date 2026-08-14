#include "tdx/unlocks.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double left, double right, double epsilon = 0.01) {
    return std::abs(left - right) <= epsilon;
}
}

int main() {
    try {
        tdx::Json row = tdx::Json::object();
        row["$ZQDM"] = "202707";
        row["jjsl"] = "298.399989";
        row["jjsz"] = "12965.84995603";
        row["J_ZSZ"] = "114290477483346.61";
        row["J_LTSZ"] = "99760304001739.25";
        row["jjgps"] = "119";
        row["jjts"] = "125";
        tdx::Json rows = tdx::Json::array();
        rows.push_back(std::move(row));
        const auto normalized = tdx::normalize_monthly_unlock_pressure_rows(rows);
        require(normalized.size() == 1, "one monthly pressure row");
        const auto& item = normalized.as_array().front();
        require(item.at("month").as_string() == "202707", "month");
        require(close(item.at("unlock_shares").as_number(), 29839998900.0),
                "hundred-million shares conversion");
        require(close(item.at("unlock_market_value_yuan").as_number(),
                      1296584995603.0), "hundred-million yuan conversion");
        require(item.at("security_count").as_number() == 119, "security count");
        require(item.at("unlock_to_total_market_cap_pct").as_number() > 1.13,
                "total market-cap ratio");
        std::cout << "monthly unlock pressure tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
