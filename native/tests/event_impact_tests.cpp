#include "tdx/event_impact.hpp"

#include <iostream>
#include <stdexcept>

namespace { void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); } }

int main() {
    try {
        tdx::Json raw = tdx::Json::object(); raw["sjmc"] = "重大事件"; raw["ms"] = "事件正文TXT:http://example.test"; raw["date1"] = "20230119"; raw["date2"] = ""; raw["szzs1"] = "11001.104"; raw["szzs5"] = "10957.013"; raw["szzs2"] = "10852.270"; raw["qjzf4"] = "-0.9559"; raw["qjzf3"] = "6.0830";
        tdx::Json rows = tdx::Json::array(); rows.push_back(raw); const auto result = tdx::normalize_event_impact_rows("list/func_zdsj103_1.jsn", rows); require(result.size() == 1, "row count"); const auto& item = result.as_array().front(); require(item.at("benchmark").as_string() == "nasdaq-composite", "benchmark"); require(item.at("description").as_string() == "事件正文", "description"); require(item.at("source_url").as_string() == "http://example.test", "url"); require(item.at("impacts").at("start_day_pct").as_number() == -0.9559, "impact"); require(item.at("end_date").as_string().empty(), "empty end date"); std::cout << "event impact tests passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
