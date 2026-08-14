#include "recon_contract_test_support.hpp"

namespace recon_contract_test {

void require(bool value, const char *message) {
    if (!value)
        throw tdx::Error(message);
}

tdx::Json formula_sample(int count) {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    tdx::Json bars = tdx::Json::array();
    for (int index = count - 1; index >= 0; --index) {
        const double close = 10.0 + index * 0.01;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-01-" + std::string(index % 28 + 1 < 10 ? "0" : "") +
                      std::to_string(index % 28 + 1);
        bar["time"] = std::to_string(index / 28 + 9) + ":00";
        bar["open"] = close - 0.02;
        bar["high"] = close + 0.05;
        bar["low"] = close - 0.05;
        bar["close"] = close;
        bar["amount"] = close * 10000.0;
        bar["volume"] = 10000.0;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

} // namespace recon_contract_test
