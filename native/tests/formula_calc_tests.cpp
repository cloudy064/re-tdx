#include "tdx/formula_calc.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json sample(int count, bool increasing = false) {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["period"] = "day";
    tdx::Json bars = tdx::Json::array();
    for (int i = count - 1; i >= 0; --i) {
        const double price = increasing ? 10.0 + i : 10.0;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-01-" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
        bar["time"] = "15:00";
        bar["open"] = price;
        bar["high"] = price;
        bar["low"] = price;
        bar["close"] = price;
        bar["amount"] = price * 100.0;
        bar["volume"] = 1000;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

tdx::Json oscillating_sample(int count) {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["period"] = "day";
    document["index_mode"] = false;
    tdx::Json bars = tdx::Json::array();
    for (int i = count; i >= 1; --i) {
        const double price = i % 2 ? 11.0 : 10.0;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-02-" + std::string(i < 10 ? "0" : "") + std::to_string(i);
        bar["time"] = "15:00";
        bar["open"] = price;
        bar["high"] = price + 1.0;
        bar["low"] = price - 1.0;
        bar["close"] = price;
        bar["amount"] = price * 1000.0;
        bar["volume"] = 1000;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

tdx::Json directional_sample(int count) {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["period"] = "day";
    document["index_mode"] = false;
    tdx::Json bars = tdx::Json::array();
    for (int i = count - 1; i >= 0; --i) {
        const double close = 10.0 + i;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-03-" + std::string(i + 1 < 10 ? "0" : "") +
                      std::to_string(i + 1);
        bar["time"] = "15:00";
        bar["open"] = close - 0.5;
        bar["high"] = close + 1.0;
        bar["low"] = close - 2.0;
        bar["close"] = close;
        bar["amount"] = close * 1000.0;
        bar["volume"] = 1000;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

const tdx::Json& value(const tdx::Json& result, int index, const std::string& key) {
    return result.at("points").as_array().at(static_cast<std::size_t>(index))
        .at("values").at(key);
}

}  // namespace

int main() {
    try {
        const auto ma = tdx::calculate_formula_document(sample(20), "MA", {{"n1", 5}});
        require(ma.at("count").as_number() == 20, "MA count");
        require(value(ma, 3, "MA1").is_null(), "MA warm-up must be null");
        require(std::abs(value(ma, 4, "MA1").as_number() - 10.0) < 1e-9, "MA value");
        require(ma.at("points").as_array().front().at("date").as_string() == "2026-01-01",
                "bars must be sorted chronologically");

        const auto macd = tdx::calculate_formula_document(sample(30), "macd");
        require(std::abs(value(macd, 29, "DIF").as_number()) < 1e-9, "constant MACD DIF");
        require(std::abs(value(macd, 29, "DEA").as_number()) < 1e-9, "constant MACD DEA");
        require(std::abs(value(macd, 29, "MACD").as_number()) < 1e-9, "constant histogram");

        const auto kdj = tdx::calculate_formula_document(sample(12), "KDJ");
        require(std::abs(value(kdj, 11, "K").as_number() - 50.0) < 1e-9, "flat KDJ K");
        require(std::abs(value(kdj, 11, "D").as_number() - 50.0) < 1e-9, "flat KDJ D");
        require(std::abs(value(kdj, 11, "J").as_number() - 50.0) < 1e-9, "flat KDJ J");

        const auto rsi = tdx::calculate_formula_document(sample(25, true), "RSI");
        require(value(rsi, 0, "RSI1").is_null(), "RSI first point");
        require(std::abs(value(rsi, 24, "RSI1").as_number() - 100.0) < 1e-9, "rising RSI");

        const auto boll = tdx::calculate_formula_document(sample(20), "BOLL");
        require(value(boll, 18, "BOLL").is_null(), "BOLL warm-up");
        require(std::abs(value(boll, 19, "BOLL").as_number() - 10.0) < 1e-9, "BOLL middle");
        require(std::abs(value(boll, 19, "UB").as_number() - 10.0) < 1e-9, "BOLL upper");
        require(std::abs(value(boll, 19, "LB").as_number() - 10.0) < 1e-9, "BOLL lower");

        const auto rising = sample(60, true);
        const auto cci = tdx::calculate_formula_document(rising, "CCI");
        require(value(cci, 59, "CCI").as_number() > 0.0, "rising CCI");
        const auto bias = tdx::calculate_formula_document(rising, "BIAS");
        require(value(bias, 59, "BIAS1").as_number() > 0.0, "rising BIAS");
        const auto dma = tdx::calculate_formula_document(rising, "DMA");
        require(value(dma, 57, "DIFMA").is_null(), "DMA second-stage warm-up");
        require(value(dma, 59, "DIFMA").as_number() > 0.0, "DMA value");
        const auto mtm = tdx::calculate_formula_document(rising, "MTM");
        require(std::abs(value(mtm, 59, "MTM").as_number() - 12.0) < 1e-9, "MTM value");
        const auto roc = tdx::calculate_formula_document(rising, "ROC");
        require(value(roc, 59, "ROC").as_number() > 0.0, "ROC value");
        const auto trix = tdx::calculate_formula_document(rising, "TRIX");
        require(value(trix, 59, "TRIX").as_number() > 0.0, "TRIX value");
        const auto atr = tdx::calculate_formula_document(rising, "ATR");
        require(std::abs(value(atr, 59, "ATR").as_number() - 1.0) < 1e-9, "ATR gap range");
        const auto wr = tdx::calculate_formula_document(rising, "WR");
        require(std::abs(value(wr, 59, "WR1").as_number()) < 1e-9, "WR at window high");

        const auto vol = tdx::calculate_formula_document(rising, "VOL");
        require(std::abs(value(vol, 59, "VOLUME").as_number() - 10.0) < 1e-9,
                "stock VOL must convert shares to hands");
        require(std::abs(value(vol, 59, "MAVOL1").as_number() - 10.0) < 1e-9,
                "MAVOL1 value");
        require(vol.at("formula_volume_unit").as_string() == "hand" &&
                vol.at("formula_volume_divisor").as_number() == 100,
                "stock formula volume metadata");

        const auto obv = tdx::calculate_formula_document(rising, "OBV");
        require(std::abs(value(obv, 59, "OBV").as_number() - 590.0) < 1e-9,
                "rising OBV");
        require(std::abs(value(obv, 59, "MAOBV").as_number() - 445.0) < 1e-9,
                "MAOBV value");

        const auto psy = tdx::calculate_formula_document(rising, "PSY");
        require(value(psy, 11, "PSY").is_null(), "PSY comparison warm-up");
        require(std::abs(value(psy, 59, "PSY").as_number() - 100.0) < 1e-9,
                "rising PSY");
        require(std::abs(value(psy, 59, "PSYMA").as_number() - 100.0) < 1e-9,
                "PSYMA value");

        const auto alternating = oscillating_sample(60);
        const auto vr = tdx::calculate_formula_document(alternating, "VR");
        require(std::abs(value(vr, 59, "VR").as_number() - 100.0) < 1e-9,
                "balanced VR");
        require(std::abs(value(vr, 59, "MAVR").as_number() - 100.0) < 1e-9,
                "balanced MAVR");
        const auto brar = tdx::calculate_formula_document(alternating, "BRAR");
        require(std::abs(value(brar, 59, "BR").as_number() - 100.0) < 1e-9,
                "balanced BR");
        require(std::abs(value(brar, 59, "AR").as_number() - 100.0) < 1e-9,
                "balanced AR");

        const auto bbi = tdx::calculate_formula_document(rising, "BBI");
        require(std::abs(value(bbi, 59, "BBI").as_number() - 63.875) < 1e-9,
                "BBI value");
        const auto expma = tdx::calculate_formula_document(sample(60), "EXPMA");
        require(std::abs(value(expma, 59, "EXP1").as_number() - 10.0) < 1e-9,
                "constant EXP1");
        require(std::abs(value(expma, 59, "EXP2").as_number() - 10.0) < 1e-9,
                "constant EXP2");

        const auto directional = directional_sample(60);
        const auto dmi = tdx::calculate_formula_document(directional, "DMI");
        require(value(dmi, 13, "PDI").is_null(), "DMI comparison warm-up");
        require(std::abs(value(dmi, 59, "PDI").as_number() - 100.0 / 3.0) < 1e-9,
                "DMI PDI from recovered SUM formula");
        require(std::abs(value(dmi, 59, "MDI").as_number()) < 1e-9,
                "DMI MDI in rising series");
        require(std::abs(value(dmi, 59, "ADX").as_number() - 100.0) < 1e-9 &&
                    std::abs(value(dmi, 59, "ADXR").as_number() - 100.0) < 1e-9,
                "DMI ADX and ADXR");

        const auto wvad = tdx::calculate_formula_document(directional, "WVAD");
        require(value(wvad, 22, "WVAD").is_null(), "WVAD warm-up");
        require(std::abs(value(wvad, 59, "WVAD").as_number() - 0.004) < 1e-12,
                "WVAD recovered /10000 scaling");
        require(std::abs(value(wvad, 59, "MAWVAD").as_number() - 0.004) < 1e-12,
                "MAWVAD value");

        const auto emv = tdx::calculate_formula_document(directional, "EMV");
        require(value(emv, 25, "EMV").is_null(), "EMV nested MA warm-up");
        require(value(emv, 59, "EMV").as_number() > 0.0 &&
                    value(emv, 59, "MAEMV").as_number() > 0.0,
                "EMV positive directional movement");

        const auto cho = tdx::calculate_formula_document(directional, "CHO");
        require(value(cho, 18, "CHO").is_null(), "CHO long-average warm-up");
        require(value(cho, 59, "CHO").as_number() > 0.0 &&
                    value(cho, 59, "MACHO").as_number() > 0.0,
                "CHO cumulative-volume oscillator");

        const auto adtm = tdx::calculate_formula_document(directional, "ADTM");
        require(value(adtm, 22, "ADTM").is_null(), "ADTM comparison warm-up");
        require(std::abs(value(adtm, 59, "ADTM").as_number() - 1.0) < 1e-9 &&
                    std::abs(value(adtm, 59, "MAADTM").as_number() - 1.0) < 1e-9,
                "ADTM rising-open branch");

        const auto dkx = tdx::calculate_formula_document(directional, "DKX");
        require(value(dkx, 19, "DKX").is_null(), "DKX offset-20 warm-up");
        require(value(dkx, 59, "DKX").as_number() >
                    value(dkx, 59, "MADKX").as_number(),
                "rising DKX and MADKX");

        const auto catalog = tdx::formula_calculation_catalog_document();
        require(catalog.at("count").as_number() == 26, "calculator catalog count");
        require(catalog.at("engine").as_string() == "tdx-native-compatible-v4",
                "calculator engine version");
        std::cout << "formula calculation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
