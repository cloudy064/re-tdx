#pragma once

#include "formula_engine_internal.hpp"
#include "formula_language_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_runtime_detail {

using StringSeries = std::vector<std::string>;

struct StringEnvironment {
    using Symbols = std::map<std::string, StringSeries, std::less<>>;
    Symbols symbols;
    std::map<std::pair<int, std::string>, std::string> code_name_overrides;
    std::shared_ptr<const SecurityCatalog> security_catalog;

    StringSeries& operator[](const std::string& key) { return symbols[key]; }
    auto find(std::string_view key) const { return symbols.find(key); }
    auto end() const { return symbols.end(); }
    std::size_t count(std::string_view key) const { return symbols.count(key); }
};

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
    double open_interest{std::numeric_limits<double>::quiet_NaN()};
    double hk_short_volume{std::numeric_limits<double>::quiet_NaN()};
    double auxiliary_price{std::numeric_limits<double>::quiet_NaN()};
};

formula_engine_detail::Series evaluate_node(
    const formula_language_detail::Node& node,
    formula_engine_detail::Environment& env,
    const StringEnvironment& strings,
    std::size_t size);

bool evaluate_string_node(
    const formula_language_detail::Node& node,
    formula_engine_detail::Environment& env,
    const StringEnvironment& strings,
    std::size_t size,
    StringSeries& output);

std::vector<Bar> read_bars(const Json& document);

}  // namespace tdx::formula_runtime_detail
