#include "options_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tdx::option_detail {

double tdx_round4(double value) {
    return static_cast<long long>(value * 10000.0 + 0.503000020980835) * 0.0001;
}

double european_price(bool futures_model, bool call, double divisor,
                      double underlying, double strike, double years,
                      double volatility, double risk_free) {
    const double discount = std::exp(-years * risk_free);
    const double sigma_t = std::sqrt(years) * volatility;
    double d1 = std::log(underlying / strike);
    d1 += futures_model ? volatility * volatility * 0.5 * years
                        : (volatility * volatility * 0.5 + risk_free) * years;
    d1 /= sigma_t;
    const double d2 = d1 - sigma_t;
    if (futures_model) {
        if (call)
            return discount * (underlying * tdx_normal_cdf(d1) - strike * tdx_normal_cdf(d2)) / divisor;
        return discount * (strike * tdx_normal_cdf(-d2) - underlying * tdx_normal_cdf(-d1)) / divisor;
    }
    if (call)
        return (underlying * tdx_normal_cdf(d1) -
                discount * strike * tdx_normal_cdf(d2)) / divisor;
    return (discount * strike * tdx_normal_cdf(-d2) -
            underlying * tdx_normal_cdf(-d1)) / divisor;
}

double normal_density(double value) {
    return std::exp(-0.5 * value * value) / std::sqrt(6.2831853071795864769);
}

double american_price(bool futures_model, bool call, double divisor,
                      double underlying, double strike, double years,
                      double volatility, double risk_free);

OptionAnalytics european_analytics(bool futures_model, bool call, double divisor,
                                   double underlying, double strike, double years,
                                   double volatility, double risk_free) {
    OptionAnalytics result;
    result.price = european_price(futures_model, call, divisor, underlying, strike,
                                  years, volatility, risk_free);
    const double root_years = std::sqrt(years);
    const double sigma_t = root_years * volatility;
    const double discount = std::exp(-years * risk_free);
    double d1 = std::log(underlying / strike);
    d1 += futures_model ? volatility * volatility * 0.5 * years
                        : (volatility * volatility * 0.5 + risk_free) * years;
    d1 /= sigma_t;
    const double d2 = d1 - sigma_t;
    const double density = normal_density(d1);

    if (!futures_model) {
        result.delta = call ? tdx_normal_cdf(d1) : tdx_normal_cdf(d1) - 1.0;
        result.gamma = density / (underlying * volatility * root_years);
        const double diffusion = density * underlying * volatility / (2.0 * root_years);
        if (call) {
            result.theta = -(diffusion + tdx_normal_cdf(d2) * discount * strike * risk_free) /
                           divisor;
            result.rho = tdx_normal_cdf(d2) * discount * strike * years / divisor;
        } else {
            result.theta = -(diffusion - tdx_normal_cdf(-d2) * discount * strike * risk_free) /
                           divisor;
            result.rho = -tdx_normal_cdf(-d2) * discount * strike * years / divisor;
        }
        result.vega = density * underlying * root_years / divisor;
    } else {
        result.delta = (call ? tdx_normal_cdf(d1) : tdx_normal_cdf(d1) - 1.0) * discount;
        result.gamma = density * discount /
                       (underlying * volatility * root_years);
        const double diffusion = density * discount * underlying * volatility /
                                 (2.0 * root_years);
        const double discounted_value = call
            ? (tdx_normal_cdf(d1) * underlying - tdx_normal_cdf(d2) * strike) * discount
            : (tdx_normal_cdf(-d2) * strike - tdx_normal_cdf(-d1) * underlying) * discount;
        result.theta = -(diffusion - discounted_value * risk_free) / divisor;
        result.vega = discount * underlying * density * root_years / divisor;
        result.rho = (call ? -discounted_value : -discounted_value) * years / divisor;
    }
    return result;
}

OptionAnalytics american_analytics(bool futures_model, bool call, double divisor,
                                   double underlying, double strike, double years,
                                   double volatility, double risk_free) {
    const double dt = years / kBinomialNodes;
    const double u = std::exp(volatility * std::sqrt(dt));
    const double d = std::exp(-volatility * std::sqrt(dt));
    const double growth = futures_model ? 1.0 : std::exp(risk_free * dt);
    const double probability = (growth - d) / (u - d);
    const double inverse_probability = 1.0 - probability;
    const double discount = std::exp(-risk_free * dt);
    std::vector<std::vector<double>> nodes(kBinomialNodes);
    std::vector<std::vector<double>> values(kBinomialNodes);
    for (int layer = 0; layer < kBinomialNodes; ++layer) {
        nodes[static_cast<std::size_t>(layer)].resize(static_cast<std::size_t>(layer + 1));
        values[static_cast<std::size_t>(layer)].resize(static_cast<std::size_t>(layer + 1));
        for (int i = 0; i <= layer; ++i)
            nodes[static_cast<std::size_t>(layer)][static_cast<std::size_t>(i)] =
                underlying * std::pow(d, layer - i) * std::pow(u, i);
    }
    const int terminal = kBinomialNodes - 1;
    for (int i = 0; i <= terminal; ++i) {
        const double node = nodes[static_cast<std::size_t>(terminal)][static_cast<std::size_t>(i)];
        values[static_cast<std::size_t>(terminal)][static_cast<std::size_t>(i)] =
            call ? std::max(node - strike, 0.0) : std::max(strike - node, 0.0);
    }
    for (int layer = terminal - 1; layer >= 0; --layer) {
        for (int i = 0; i <= layer; ++i) {
            const double node = nodes[static_cast<std::size_t>(layer)][static_cast<std::size_t>(i)];
            const double exercise = call ? std::max(node - strike, 0.0)
                                         : std::max(strike - node, 0.0);
            const double continuation = discount *
                (probability * values[static_cast<std::size_t>(layer + 1)][static_cast<std::size_t>(i + 1)] +
                 inverse_probability * values[static_cast<std::size_t>(layer + 1)][static_cast<std::size_t>(i)]);
            values[static_cast<std::size_t>(layer)][static_cast<std::size_t>(i)] =
                std::max(exercise, continuation);
        }
    }

    OptionAnalytics result;
    result.price = values[0][0] / divisor;
    result.delta = (values[1][1] - values[1][0]) / (nodes[1][1] - nodes[1][0]);
    const double lower_delta = (values[2][1] - values[2][0]) /
                               (nodes[2][1] - nodes[2][0]);
    const double upper_delta = (values[2][2] - values[2][1]) /
                               (nodes[2][2] - nodes[2][1]);
    result.gamma = (upper_delta - lower_delta) /
                   ((nodes[2][2] - nodes[2][0]) * 0.5);
    result.theta = (values[2][1] - values[0][0]) / (2.0 * dt) / divisor;
    result.vega = (american_price(futures_model, call, divisor, underlying, strike,
                                  years, volatility + 0.01, risk_free) - result.price) * 100.0;
    result.rho = (american_price(futures_model, call, divisor, underlying, strike,
                                 years, volatility, risk_free + 0.01) - result.price) * 100.0;
    return result;
}

double american_price(bool futures_model, bool call, double divisor,
                      double underlying, double strike, double years,
                      double volatility, double risk_free) {
    const double dt = years / kBinomialNodes;
    const double u = std::exp(volatility * std::sqrt(dt));
    const double d = std::exp(-volatility * std::sqrt(dt));
    const double growth = futures_model ? 1.0 : std::exp(risk_free * dt);
    const double probability = (growth - d) / (u - d);
    const double inverse_probability = 1.0 - probability;
    const double discount = std::exp(-risk_free * dt);
    std::vector<double> values(kBinomialNodes);
    for (int i = 0; i < kBinomialNodes; ++i) {
        const double node = underlying * std::pow(d, kBinomialNodes - i - 1) * std::pow(u, i);
        values[static_cast<std::size_t>(i)] = call ? std::max(node - strike, 0.0)
                                                   : std::max(strike - node, 0.0);
    }
    for (int layer = kBinomialNodes - 2; layer >= 0; --layer) {
        for (int i = 0; i <= layer; ++i) {
            const double node = underlying * std::pow(d, layer - i) * std::pow(u, i);
            const double exercise = call ? std::max(node - strike, 0.0)
                                         : std::max(strike - node, 0.0);
            const double continuation = discount *
                (probability * values[static_cast<std::size_t>(i + 1)] +
                 inverse_probability * values[static_cast<std::size_t>(i)]);
            values[static_cast<std::size_t>(i)] = std::max(exercise, continuation);
        }
    }
    return values.front() / divisor;
}

}  // namespace tdx::option_detail

namespace tdx {

using namespace option_detail;

double tdx_normal_cdf(double value) {
    const double density = std::exp(-0.5 * value * value) / std::sqrt(6.28318);
    if (value == 0.0) return 0.0;  // Preserved TQQCalc.dll boundary behavior.
    const double factor = 1.0 / (1.0 + std::abs(value) * 0.2316419);
    const double tail = density * (((((factor * 1.330274429 - 1.821255978) * factor +
        1.781477937) * factor - 0.356563782) * factor + 0.31938153) * factor);
    return value > 0.0 ? 1.0 - tail : tail;
}

double tdx_historical_volatility(const std::vector<double>& closes) {
    if (closes.size() < 2) return 0.0;
    std::vector<double> returns(closes.size() - 1, 0.0);
    for (std::size_t i = 1; i < closes.size(); ++i) {
        if (closes[i] > 0.0001 && closes[i - 1] > 0.0001)
            returns[i - 1] = std::log(closes[i] / closes[i - 1]);
    }
    double mean = 0.0;
    for (const double value : returns) mean += value;
    mean /= static_cast<double>(returns.size());
    double variance = 0.0;
    for (const double value : returns) {
        const double delta = value - mean;
        variance += delta * delta;
    }
    variance /= static_cast<double>(returns.size());
    return std::sqrt(variance) * std::sqrt(250.0);
}

double tdx_option_price(bool futures_model, bool american, bool call,
                        double divisor, double underlying, double strike,
                        double years, double volatility, double risk_free) {
    if (divisor < 0.000001 || underlying < 0.000001 || strike < 0.000001 ||
        years < 0.000001 || volatility < 0.000001)
        return 0.0;
    return american
        ? american_price(futures_model, call, divisor, underlying, strike,
                         years, volatility, risk_free)
        : european_price(futures_model, call, divisor, underlying, strike,
                         years, volatility, risk_free);
}

OptionAnalytics tdx_option_analytics(
    bool futures_model, bool american, bool call, double divisor,
    double underlying, double strike, double years, double volatility,
    double risk_free) {
    if (divisor < 0.000001 || underlying < 0.000001 || strike < 0.000001 ||
        years < 0.000001 || volatility < 0.000001)
        return {};
    auto result = american
        ? american_analytics(futures_model, call, divisor, underlying, strike,
                             years, volatility, risk_free)
        : european_analytics(futures_model, call, divisor, underlying, strike,
                             years, volatility, risk_free);
    result.price = tdx_round4(result.price);
    result.delta = tdx_round4(result.delta);
    result.gamma = tdx_round4(result.gamma);
    result.theta = tdx_round4(result.theta);
    result.vega = tdx_round4(result.vega);
    result.rho = tdx_round4(result.rho);
    return result;
}

double tdx_implied_volatility(bool futures_model, bool american, bool call,
                              double divisor, double underlying, double strike,
                              double years, double initial_volatility,
                              double risk_free, double option_price) {
    if (divisor < 0.000001 || underlying < 0.000001 || strike < 0.000001 ||
        years < 0.000001 || initial_volatility < 0.000001 || option_price < 0.000001)
        return 0.0;
    double volatility = initial_volatility;
    double candidate = volatility;
    const int iterations = american ? 10 : 5000;
    for (int i = 0; i < iterations; ++i) {
        const double current = tdx_option_price(futures_model, american, call, divisor,
                                                underlying, strike, years, volatility, risk_free);
        const double bumped_volatility = volatility + 0.0001;
        const double bumped = tdx_option_price(futures_model, american, call, divisor,
                                               underlying, strike, years,
                                               bumped_volatility, risk_free);
        const double step = std::abs(bumped - current) >= 0.0001
            ? (current - option_price) / (bumped - current) * 0.0001
            : current - option_price;
        candidate = volatility - step;
        if (candidate < 0.0 || candidate < 0.000001 ||
            std::abs(candidate - volatility) < 0.000001)
            break;
        volatility = candidate;
    }
    if (candidate < 0.0001) candidate = 0.0001;
    return tdx_round4(candidate);
}

}  // namespace tdx
