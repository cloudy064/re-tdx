#include "formula_environment_internal.hpp"

#include "formula_engine_support_internal.hpp"
#include "formula_operator_semantics_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/security_status.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_runtime_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;

namespace {

bool chip_security_supported(std::string market, const std::string& code) {
    market = lower_ascii(std::move(market));
    int numeric = -1;
    try {
        std::size_t used = 0;
        numeric = std::stoi(code, &used);
        if (used != code.size()) numeric = -1;
    } catch (...) { numeric = -1; }
    if (market == "sz" || market == "0") return code.rfind("39", 0) != 0;
    if (market == "sh" || market == "1")
        return !code.empty() && code.front() != '8' && numeric > 999 && numeric < 990000;
    if (market == "bj" || market == "2") return numeric < 899000 || numeric > 899999;
    return true;
}

double chip_capital_divisor(std::string market, const std::string& code) {
    market = lower_ascii(std::move(market));
    if ((market == "sz" || market == "0") && code.size() >= 2) {
        if ((code[0] == '0' || code[0] == '2') && (code[1] == '3' || code[1] == '8'))
            return 100.0;
        if (code[0] == '3' && code[1] == '8') return 100.0;
    }
    if ((market == "sh" || market == "1") && code.size() >= 3 &&
        code[0] == '5' && code[1] == '8' && (code[2] == '0' || code[2] == '2'))
        return 100.0;
    return 1.0;
}

// Reads a string field off the K-line document, or "" when it is absent or not
// a string.  The chip/ST helpers all tolerate empty selectors.
std::string document_text(const Json& document, std::string_view field) {
    const auto* value = optional(document, field);
    return value && value->is_string() ? value->as_string() : std::string{};
}

}  // namespace

void FormulaEnvironmentBuilder::bind_price_fields(Environment& env) const {
    const auto field = [&](const std::string& name, auto getter) {
        Series values;
        values.reserve(bars_.size());
        for (const auto& bar : bars_) values.push_back(getter(bar));
        env[name] = std::move(values);
    };
    field("OPEN", [](const Bar& b) { return native_float(b.open); }); env["O"] = env["OPEN"];
    field("HIGH", [](const Bar& b) { return native_float(b.high); }); env["H"] = env["HIGH"];
    field("LOW", [](const Bar& b) { return native_float(b.low); }); env["L"] = env["LOW"];
    field("CLOSE", [](const Bar& b) { return native_float(b.close); }); env["C"] = env["CLOSE"];
    field("AMOUNT", [](const Bar& b) { return native_float(b.amount); }); env["AMO"] = env["AMOUNT"];
    field("VOL", [](const Bar& b) { return native_float(b.formula_volume); });
    env["V"] = env["VOL"]; env["VOLUME"] = env["VOL"];
    field("__RAW_VOLUME", [](const Bar& b) { return native_float(b.volume); });
    field("VOLINSTK", [](const Bar& b) { return native_float(b.open_interest); });
    env["CCL"] = env["VOLINSTK"];
    field("HKSHORTVOL", [](const Bar& b) { return native_float(b.hk_short_volume); });
    field("ZSTJJ", [](const Bar& b) { return native_float(b.auxiliary_price); });
    env["QHJSJ"] = env["ZSTJJ"];
}

void FormulaEnvironmentBuilder::bind_adjustment_flag(Environment& env) const {
    int adjustment_flag = 0;
    if (const auto* mode = optional(kline_document_, "adjustment_mode")) {
        if (mode->is_number()) {
            const auto value = mode->as_number();
            if (!std::isfinite(value) || std::floor(value) != value ||
                value < 0.0 || value > 2.0)
                throw Error("K-line adjustment_mode must be 0, 1, or 2");
            adjustment_flag = static_cast<int>(value);
        } else if (mode->is_string()) {
            const auto value = lower_ascii(trim(mode->as_string()));
            if (value.empty() || value == "none" || value == "raw")
                adjustment_flag = 0;
            else if (value == "qfq" || value == "front" || value == "forward" ||
                     value == "fixed_qfq")
                adjustment_flag = 1;
            else if (value == "hfq" || value == "back" || value == "backward" ||
                     value == "fixed_hfq")
                adjustment_flag = 2;
            else
                throw Error("unknown K-line adjustment_mode: " + value);
        } else {
            throw Error("K-line adjustment_mode must be a string or integer");
        }
    }
    env["TQFLAG"] = constant(static_cast<double>(adjustment_flag), bars_.size());
}

void FormulaEnvironmentBuilder::bind_directional_bars(Environment& env) const {
    const auto directional = tcalc_directional_bars(env, bars_.size());
    env["DOPEN"] = directional.open;
    env["DHIGH"] = directional.high;
    env["DLOW"] = directional.low;
    env["DCLOSE"] = directional.close;
    env["DVOL"] = directional.volume;
}

void FormulaEnvironmentBuilder::bind_security_identity(
    Environment& env, StringEnvironment& string_env) const {
    const auto chip_market = document_text(kline_document_, "market");
    const auto chip_code = document_text(kline_document_, "code");
    const auto chip_name = document_text(kline_document_, "name");
    string_env["CODE"] = StringSeries(bars_.size(), chip_code);
    string_env["STKNAME"] = StringSeries(bars_.size(), chip_name);
    env["__MCST_VOLUME_SCALE"] = constant(
        tcalc_mcst_volume_scale(chip_market, chip_code), bars_.size());
    env["__ZXNH_TYPICAL_PRICE"] = constant(
        tcalc_zxnh_uses_typical_price(chip_market, chip_code) ? 1.0 : 0.0,
        bars_.size());
    env["__CHIP_SUPPORTED"] = constant(
        chip_security_supported(chip_market, chip_code) ? 1.0 : 0.0, bars_.size());
    env["__LFS_SECURITY_SUPPORTED"] = constant(
        tcalc_lfs_security_supported(chip_market, chip_code) ? 1.0 : 0.0,
        bars_.size());
    env["__CHIP_CAPITAL_DIVISOR"] = constant(
        chip_capital_divisor(chip_market, chip_code), bars_.size());
    const int chip_market_id = tcalc_market_id(chip_market);
    const bool is_st_security = tdx::tcalc_is_st_security(
        chip_market_id, chip_code, chip_name);
    env["__IS_ST_SECURITY"] = constant(is_st_security ? 1.0 : 0.0, bars_.size());
}

void FormulaEnvironmentBuilder::bind_external_indicators(Environment& env) const {
    if (program_.symbols.count("EXTERNAL#KDJ.J")) {
        const auto n = constant(9.0, bars_.size());
        const auto smoothing = constant(3.0, bars_.size());
        const auto one = constant(1.0, bars_.size());
        const auto lowest = evaluate_call("LLV", {env.at("LOW"), n}, env, bars_.size());
        const auto highest = evaluate_call("HHV", {env.at("HIGH"), n}, env, bars_.size());
        Series rsv(bars_.size(), missing);
        double previous_ratio = missing;
        for (std::size_t i = 0; i < bars_.size(); ++i) {
            const auto range = highest[i] - lowest[i];
            const auto ratio = formula_operator_semantics_detail::tcalc_divide(
                env.at("CLOSE")[i] - lowest[i], range, previous_ratio);
            if (std::isfinite(ratio))
                rsv[i] = formula_operator_semantics_detail::tcalc_multiply(
                    ratio, 100.0);
            previous_ratio = ratio;
        }
        const auto k = evaluate_call("SMA", {rsv, smoothing, one}, env, bars_.size());
        const auto d = evaluate_call("SMA", {k, smoothing, one}, env, bars_.size());
        Series j(bars_.size(), missing);
        for (std::size_t i = 0; i < bars_.size(); ++i) {
            const auto three_k =
                formula_operator_semantics_detail::tcalc_multiply(3.0, k[i]);
            const auto two_d =
                formula_operator_semantics_detail::tcalc_multiply(2.0, d[i]);
            j[i] = formula_operator_semantics_detail::tcalc_subtract(
                three_k, two_d);
        }
        env["EXTERNAL#KDJ.J"] = std::move(j);
    }
    if (program_.symbols.count("SAR.SAR")) {
        env["SAR.SAR"] = evaluate_call("TDXSAR", {constant(4.0, bars_.size()),
            constant(2.0, bars_.size()), constant(2.0, bars_.size()),
            constant(20.0, bars_.size())}, env, bars_.size());
    }
}

Json FormulaEnvironmentBuilder::bind_parameters(Environment& env) const {
    Json actual = Json::object();
    for (const auto& [name, value] : parameters_) {
        const auto key = upper_ascii(name);
        // TCalc formula records store the active parameter value in a dword
        // float slot.  Use that same value both for execution and the
        // effective-parameter report returned to callers.
        const auto effective = native_float(value);
        if (!std::isfinite(effective))
            throw Error("formula parameter " + name +
                        " is outside the native float range");
        env[key] = constant(effective, bars_.size());
        actual[key] = effective;
    }
    return actual;
}

}  // namespace tdx::formula_runtime_detail
