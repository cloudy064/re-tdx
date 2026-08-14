#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void require(bool condition, const std::string &message) {
    if (!condition)
        throw tdx::Error(message);
}

tdx::Json sample(int count) {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["name"] = "平安银行";
    tdx::Json bars = tdx::Json::array();
    for (int i = count - 1; i >= 0; --i) {
        const double close = 10.0 + i;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-01-" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
        bar["time"] = "15:00";
        bar["open"] = close - 0.2;
        bar["high"] = close + 1.0;
        bar["low"] = close - 1.0;
        bar["close"] = close;
        bar["amount"] = close * 10000.0;
        bar["volume"] = 10000;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

tdx::Json chip_sample() {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["period"] = "day";
    tdx::Json bar = tdx::Json::object();
    bar["date"] = "2026-01-05";
    bar["time"] = "15:00";
    bar["open"] = 10.02;
    bar["high"] = 10.06;
    bar["low"] = 10.00;
    bar["close"] = 10.03;
    bar["amount"] = 100300.0;
    bar["volume"] = 10000.0;
    document["bars"] = tdx::Json::array();
    document["bars"].push_back(std::move(bar));
    return document;
}

tdx::Json expansion_sample() {
    tdx::Json document = tdx::Json::object();
    document["market"] = "47";
    document["code"] = "IFL9";
    document["period"] = "day";
    document["expansion_market"] = true;
    tdx::Json bars = tdx::Json::array();
    for (int i = 1; i >= 0; --i) {
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-08-0" + std::to_string(i + 1);
        bar["time"] = "15:00";
        bar["open"] = 3500.0 + i;
        bar["high"] = 3520.0 + i;
        bar["low"] = 3490.0 + i;
        bar["close"] = 3510.0 + i;
        bar["amount"] = nullptr;
        bar["volume"] = 7000 + i;
        bar["open_interest"] = 123456 + i;
        bar["hk_short_volume"] = 20000.0 + i;
        bar["auxiliary_price"] = 3508.0 + i;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

tdx::Json time_average_sample() {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["period"] = "time";
    document["expansion_market"] = false;
    document["bars"] = tdx::Json::array();
    for (const auto &row : std::vector<std::tuple<std::string, double, double, double>>{
             {"09:31", 10.0, 1000.0, 100.0},
             {"09:32", 12.0, 2400.0, 200.0},
             {"09:33", 15.0, 1500.0, 100.0}}) {
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-08-07";
        bar["time"] = std::get<0>(row);
        bar["open"] = std::get<1>(row);
        bar["high"] = std::get<1>(row);
        bar["low"] = std::get<1>(row);
        bar["close"] = std::get<1>(row);
        bar["amount"] = std::get<2>(row);
        bar["volume"] = std::get<3>(row);
        document["bars"].push_back(std::move(bar));
    }
    return document;
}

const tdx::Json &point_value(const tdx::Json &result, std::size_t index, const std::string &name) {
    const auto &values = result.at("points").as_array().at(index).at("values");
    const auto found = values.as_object().find(name);
    if (found == values.as_object().end())
        throw tdx::Error("fixture output is missing: " + name);
    return found->second;
}

const tdx::Json &formula(const tdx::Json &library, const std::string &code) {
    for (const auto &item : library.at("formulas").as_array())
        if (item.at("code").as_string() == code)
            return item;
    throw tdx::Error("fixture formula not found: " + code);
}

} // namespace formula_engine_test
