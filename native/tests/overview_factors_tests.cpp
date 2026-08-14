#include "tdx/overview_factors.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        tdx::Json rows = tdx::Json::array();
        for (const auto& [id, label] : std::vector<std::pair<const char*, const char*>>{
                {"zb0", "利空"}, {"zb1", "中性"}, {"zb2", "利好"}, {"zb3", ""}}) {
            tdx::Json row = tdx::Json::object();
            row["$ZQDM"] = id;
            row["zbname"] = std::string("factor-") + id;
            row["ms"] = "source description";
            row["lx"] = label;
            row["pname"] = "chart";
            rows.push_back(std::move(row));
        }
        const auto normalized = tdx::normalize_overview_factor_rows(rows);
        require(normalized.size() == 4, "four overview factors");
        require(normalized.as_array()[0].at("signal").as_string() == "negative",
                "negative signal");
        require(normalized.as_array()[1].at("signal").as_string() == "neutral",
                "neutral signal");
        require(normalized.as_array()[2].at("signal").as_string() == "positive",
                "positive signal");
        require(normalized.as_array()[3].at("signal").as_string() == "unrated",
                "unrated signal");
        std::cout << "overview factors tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
