#pragma once

#include "tdx/formula_calc.hpp"

#include <array>
#include <initializer_list>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::formula_calc_detail {

struct Bar {
    std::string date;
    std::string time;
    double open{};
    double high{};
    double low{};
    double close{};
    double amount{};
    double volume{};
    double formula_volume{};
};

using Values = std::map<std::string, std::vector<double>, std::less<>>;
using Parameters = std::map<std::string, int>;
using FormulaHandler = void (*)(
    const std::vector<Bar>&, const Parameters&, Values&, Parameters&);

inline constexpr double missing = std::numeric_limits<double>::quiet_NaN();

const Json* optional(const Json& object, std::string_view key);
std::vector<Bar> read_bars(const Json& document);
int parameter(const Parameters& supplied, std::string_view name,
              int fallback, int minimum = 1, int maximum = 1000);
void reject_unknown(const Parameters& supplied,
                    std::initializer_list<std::string_view> accepted);
std::vector<double> closes(const std::vector<Bar>& bars);
std::vector<double> rolling_mean(const std::vector<double>& source, int period);
std::vector<double> rolling_sum(const std::vector<double>& source, int period);
int parameter_alias(const Parameters& supplied, std::string_view primary,
                    std::string_view secondary, int fallback,
                    int minimum = 1, int maximum = 1000);
std::vector<double> ema(const std::vector<double>& source, int period);
std::vector<double> tdx_sma(const std::vector<double>& source, int period,
                            int weight, double seed);
Json parameter_json(const Parameters& parameters);

#define TDX_DECLARE_CALCULATOR(name) \
    void calculate_##name(const std::vector<Bar>& bars, \
                          const Parameters& supplied, Values& values, \
                          Parameters& actual)

TDX_DECLARE_CALCULATOR(ma);
TDX_DECLARE_CALCULATOR(macd);
TDX_DECLARE_CALCULATOR(kdj);
TDX_DECLARE_CALCULATOR(rsi);
TDX_DECLARE_CALCULATOR(boll);
TDX_DECLARE_CALCULATOR(cci);
TDX_DECLARE_CALCULATOR(wr);
TDX_DECLARE_CALCULATOR(bias);
TDX_DECLARE_CALCULATOR(dma);
TDX_DECLARE_CALCULATOR(mtm);
TDX_DECLARE_CALCULATOR(roc);
TDX_DECLARE_CALCULATOR(trix);
TDX_DECLARE_CALCULATOR(atr);
TDX_DECLARE_CALCULATOR(vol);
TDX_DECLARE_CALCULATOR(obv);
TDX_DECLARE_CALCULATOR(psy);
TDX_DECLARE_CALCULATOR(vr);
TDX_DECLARE_CALCULATOR(brar);
TDX_DECLARE_CALCULATOR(bbi);
TDX_DECLARE_CALCULATOR(expma);
TDX_DECLARE_CALCULATOR(dmi);
TDX_DECLARE_CALCULATOR(wvad);
TDX_DECLARE_CALCULATOR(emv);
TDX_DECLARE_CALCULATOR(cho);
TDX_DECLARE_CALCULATOR(adtm);
TDX_DECLARE_CALCULATOR(dkx);

#undef TDX_DECLARE_CALCULATOR

struct FormulaDefinition {
    std::string_view name;
    std::string_view code;
    std::array<std::string_view, 4> outputs;
    std::size_t output_count;
    FormulaHandler handler;
};

inline constexpr std::array<FormulaDefinition, 26> formula_catalog{{
    {"ma", "MA", {{"MA1", "MA2", "MA3", "MA4"}}, 4, calculate_ma},
    {"macd", "MACD", {{"DIF", "DEA", "MACD", ""}}, 3, calculate_macd},
    {"kdj", "KDJ", {{"K", "D", "J", ""}}, 3, calculate_kdj},
    {"rsi", "RSI", {{"RSI1", "RSI2", "RSI3", ""}}, 3, calculate_rsi},
    {"boll", "BOLL", {{"BOLL", "UB", "LB", ""}}, 3, calculate_boll},
    {"cci", "CCI", {{"CCI", "", "", ""}}, 1, calculate_cci},
    {"wr", "WR", {{"WR1", "WR2", "", ""}}, 2, calculate_wr},
    {"bias", "BIAS", {{"BIAS1", "BIAS2", "BIAS3", ""}}, 3, calculate_bias},
    {"dma", "DMA", {{"DIF", "DIFMA", "", ""}}, 2, calculate_dma},
    {"mtm", "MTM", {{"MTM", "MTMMA", "", ""}}, 2, calculate_mtm},
    {"roc", "ROC", {{"ROC", "MAROC", "", ""}}, 2, calculate_roc},
    {"trix", "TRIX", {{"TRIX", "MATRIX", "", ""}}, 2, calculate_trix},
    {"atr", "ATR", {{"MTR", "ATR", "", ""}}, 2, calculate_atr},
    {"vol", "VOL", {{"VOLUME", "MAVOL1", "MAVOL2", ""}}, 3, calculate_vol},
    {"obv", "OBV", {{"OBV", "MAOBV", "", ""}}, 2, calculate_obv},
    {"psy", "PSY", {{"PSY", "PSYMA", "", ""}}, 2, calculate_psy},
    {"vr", "VR", {{"VR", "MAVR", "", ""}}, 2, calculate_vr},
    {"brar", "BRAR", {{"BR", "AR", "", ""}}, 2, calculate_brar},
    {"bbi", "BBI", {{"BBI", "", "", ""}}, 1, calculate_bbi},
    {"expma", "EXPMA", {{"EXP1", "EXP2", "", ""}}, 2, calculate_expma},
    {"dmi", "DMI", {{"PDI", "MDI", "ADX", "ADXR"}}, 4, calculate_dmi},
    {"wvad", "WVAD", {{"WVAD", "MAWVAD", "", ""}}, 2, calculate_wvad},
    {"emv", "EMV", {{"EMV", "MAEMV", "", ""}}, 2, calculate_emv},
    {"cho", "CHO", {{"CHO", "MACHO", "", ""}}, 2, calculate_cho},
    {"adtm", "ADTM", {{"ADTM", "MAADTM", "", ""}}, 2, calculate_adtm},
    {"dkx", "DKX", {{"DKX", "MADKX", "", ""}}, 2, calculate_dkx},
}};

constexpr bool unique_formula_names() {
    for (std::size_t left = 0; left < formula_catalog.size(); ++left)
        for (std::size_t right = left + 1; right < formula_catalog.size(); ++right)
            if (formula_catalog[left].name == formula_catalog[right].name)
                return false;
    return true;
}

static_assert(unique_formula_names(), "native formula names must be unique");

const FormulaDefinition* find_formula(std::string_view name);
std::vector<std::string> ordered_outputs(
    const FormulaDefinition& definition, const Values& values);

}  // namespace tdx::formula_calc_detail
