#include "formula_context_dynamic_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/market_cage.hpp"
#include "tdx/minute.hpp"
#include "tdx/session_audit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>

namespace tdx::formula_context_detail {
namespace {

struct DynamicAliasDefinition {
    std::string_view name;
    int selector;
    int tcalc_opcode;
};

constexpr std::array<DynamicAliasDefinition, 4> kDynamicAliases{{
    {"DYNA_NOW", 7, 1380},
    {"DYNA_ZAF", 14, 1381},
    {"DYNA_LB", 17, 1382},
    {"DYNA_ZAS", 24, 1383},
}};

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

const Json* nested(const Json& value,
                   std::initializer_list<std::string_view> path) {
    const Json* current = &value;
    for (const auto key : path) {
        current = optional(*current, key);
        if (!current) return nullptr;
    }
    return current;
}

std::string text_or(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string() : std::string{};
}

int integer_or(const Json& object, std::string_view key, int fallback) {
    const auto* value = optional(object, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : fallback;
}

bool includes_any(const std::set<int>& values,
                  std::initializer_list<int> wanted) {
    return std::any_of(wanted.begin(), wanted.end(),
                       [&](int id) { return values.count(id) != 0; });
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

const Json* matching_close_bar(const Json& document, double close) {
    const auto* bars = optional(document, "bars");
    if (!bars || !bars->is_array()) return nullptr;
    const double tolerance = std::max(0.005, std::abs(close) * 1e-6);
    for (auto it = bars->as_array().rbegin();
         it != bars->as_array().rend(); ++it) {
        const auto* value = optional(*it, "close");
        if (value && value->is_number() &&
            std::abs(value->as_number() - close) <= tolerance)
            return &*it;
    }
    return nullptr;
}

double previous_amount(const Json& daily, double previous_close) {
    const auto* bar = matching_close_bar(daily, previous_close);
    const auto* amount = bar ? optional(*bar, "amount") : nullptr;
    return amount && amount->is_number()
        ? amount->as_number()
        : std::numeric_limits<double>::quiet_NaN();
}

double previous_average_volume_hand(const Json& daily,
                                    double previous_close,
                                    int days = 5) {
    const auto* bars = optional(daily, "bars");
    if (!bars || !bars->is_array() || bars->as_array().empty())
        return std::numeric_limits<double>::quiet_NaN();
    const double tolerance = std::max(0.005, std::abs(previous_close) * 1e-6);
    std::size_t end = bars->as_array().size();
    for (std::size_t i = bars->as_array().size(); i-- > 0;) {
        const auto* close = optional(bars->as_array()[i], "close");
        if (close && close->is_number() &&
            std::abs(close->as_number() - previous_close) <= tolerance) {
            end = i + 1;
            break;
        }
    }
    const auto* last_close = optional(bars->as_array().back(), "close");
    if (end == bars->as_array().size() &&
        (!last_close || !last_close->is_number() ||
         std::abs(last_close->as_number() - previous_close) > tolerance))
        return std::numeric_limits<double>::quiet_NaN();
    const std::size_t begin = end > static_cast<std::size_t>(days)
        ? end - static_cast<std::size_t>(days) : 0;
    double total = 0.0;
    int count = 0;
    for (std::size_t i = begin; i < end; ++i) {
        const auto* volume = optional(bars->as_array()[i], "volume");
        if (volume && volume->is_number() && volume->as_number() >= 0) {
            total += volume->as_number() / 100.0;
            ++count;
        }
    }
    return count ? total / count
                 : std::numeric_limits<double>::quiet_NaN();
}

double trading_day_fraction() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const int minute = local.tm_hour * 60 + local.tm_min;
    int elapsed = 0;
    if (minute < 9 * 60 + 30) elapsed = 1;
    else if (minute < 11 * 60 + 30) elapsed = minute - (9 * 60 + 30) + 1;
    else if (minute < 13 * 60) elapsed = 120;
    else if (minute < 15 * 60) elapsed = 120 + minute - 13 * 60 + 1;
    else elapsed = 240;
    return std::clamp(elapsed / 240.0, 1.0 / 240.0, 1.0);
}

double price_minutes_ago(const Json& minute, int bars_ago) {
    const auto* bars = optional(minute, "bars");
    if (!bars || !bars->is_array() || bars->as_array().empty())
        return std::numeric_limits<double>::quiet_NaN();
    const auto index = bars->as_array().size() >
            static_cast<std::size_t>(bars_ago)
        ? bars->as_array().size() - 1 - static_cast<std::size_t>(bars_ago)
        : 0;
    const auto* close = optional(bars->as_array()[index], "close");
    return close && close->is_number()
        ? close->as_number()
        : std::numeric_limits<double>::quiet_NaN();
}

double standard_limit_rate(int type) {
    if (type == 2) return 0.30;
    if (type == 3 || type == 4) return 0.20;
    return 0.10;
}

double rounded_limit(double previous_close, double rate, bool upper) {
    return std::round(previous_close * (1.0 + (upper ? rate : -rate)) *
                      100.0) / 100.0;
}

struct CachedSpecialLimit {
    std::optional<SpecialLimitPrice> price;
    std::string endpoint;
    std::uint64_t record_count{};
};

CachedSpecialLimit cached_special_limit_price(
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    int timeout_ms) {
    static std::mutex cache_mutex;
    static Json cache;
    static int cache_day = -1;
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const int day = (local.tm_year + 1900) * 1000 + local.tm_yday;
    if (!cache.is_object() || cache_day != day) {
        try {
            cache = fetch_special_limits_document(
                load_public_quote_endpoints(root).endpoints,
                timeout_ms, 0, 10000, false);
            cache_day = day;
        } catch (...) {
            if (!cache.is_object()) throw;
        }
    }
    CachedSpecialLimit result;
    result.price = find_special_limit_price(cache, market, code);
    result.endpoint = text_or(cache, "endpoint");
    result.record_count = static_cast<std::uint64_t>(
        std::max(0, integer_or(cache, "count", 0)));
    return result;
}

class DynamicQuoteContextBuilder {
public:
    DynamicQuoteContextBuilder(
        Json& context,
        Json& symbols,
        const std::filesystem::path& root,
        const std::string& market,
        const std::string& code,
        const Json* kline_document,
        const DynamicQuoteRequirements& requirements,
        const Json* finance_record,
        int security_type,
        int timeout_ms,
        const BlockData* block_data)
        : context_(context), symbols_(symbols), root_(root), market_(market),
          code_(code), kline_document_(kline_document),
          requirements_(requirements), finance_record_(finance_record),
          security_type_(security_type), timeout_ms_(timeout_ms),
          block_data_(block_data) {}

    void build() {
        bind_speed();
        bind_quote();
        bind_aliases();
        context_["dynainfo"] = std::move(values_);
    }

private:
    void bind_speed() {
        if (!requirements_.selectors.count(24)) return;
        const auto speed = fetch_market_speed_document(
            root_, {market_ + ":" + code_}, timeout_ms_, block_data_);
        if (!speed.at("records").size())
            throw Error("rise-speed context returned no selected security");
        const auto& row = speed.at("records").as_array().front();
        const auto* value = nested(row, {"rise_speed_pct"});
        if (!value || !value->is_number())
            throw Error("rise-speed context returned no numeric rise speed");
        values_["24"] = static_cast<double>(static_cast<float>(
            value->as_number() / 100.0));
        context_["dynamic_speed_command"] = "0x053E";
    }

    void bind_quote() {
        const bool needs_quote = std::any_of(
            requirements_.selectors.begin(), requirements_.selectors.end(),
            [](int selector) { return selector != 24; });
        if (!needs_quote && !requirements_.buy_volume &&
            !requirements_.sell_volume)
            return;
        const bool needs_depth = includes_any(
            requirements_.selectors, {20, 21, 28, 29, 58, 59, 88});
        const auto quote = needs_depth
            ? fetch_market_depth_document(
                  root_, {market_ + ":" + code_}, timeout_ms_, block_data_)
            : fetch_market_snapshot_document(
                  root_, {market_ + ":" + code_}, timeout_ms_, block_data_);
        if (!quote.at("records").size())
            throw Error(needs_depth
                ? "depth context returned no selected security"
                : "snapshot context returned no selected security");
        const auto& row = quote.at("records").as_array().front();
        context_["dynamic_quote_command"] = needs_depth ? "0x0547" : "0x054C";
        bind_basic_quote(row);
    }

    void bind_basic_quote(const Json& row) {
        const auto bind = [&](int id, std::string_view key) {
            if (const auto* value = nested(row, {key});
                value && value->is_number())
                values_[std::to_string(id)] = *value;
        };
        bind(3, "pre_close_price");
        bind(4, "open_price");
        bind(5, "high_price");
        bind(6, "low_price");
        bind(7, "last_price");
        bind(8, "total_hand");
        bind(9, "current_hand");
        bind(10, "amount");
        bind(22, "inside_dish");
        bind(23, "outer_disc");
        bind(15, "open_amount_yuan");

        const auto* previous = nested(row, {"pre_close_price"});
        const auto* high = nested(row, {"high_price"});
        const auto* low = nested(row, {"low_price"});
        const auto* amount = nested(row, {"amount"});
        const auto* total_hand = nested(row, {"total_hand"});
        const auto* last = nested(row, {"last_price"});
        if (requirements_.selectors.count(88)) values_["88"] = 0.0;
        if (requirements_.buy_volume)
            if (const auto* outside = nested(row, {"outer_disc"});
                outside && outside->is_number())
                symbols_["BUYVOL"] = *outside;
        if (requirements_.sell_volume)
            if (const auto* inside = nested(row, {"inside_dish"});
                inside && inside->is_number())
                symbols_["SELLVOL"] = *inside;
        if (previous && previous->is_number() &&
            std::abs(previous->as_number()) > 1e-12) {
            const double current = last && last->is_number() &&
                    last->as_number() > 0.00001
                ? last->as_number() : previous->as_number();
            values_["7"] = current;
            values_["12"] = current - previous->as_number();
            values_["14"] =
                (current - previous->as_number()) / previous->as_number();
            if (high && low && high->is_number() && low->is_number())
                values_["13"] =
                    (high->as_number() - low->as_number()) /
                    previous->as_number();
        }
        if (amount && total_hand && amount->is_number() &&
            total_hand->is_number() && total_hand->as_number() > 0)
            values_["11"] = amount->as_number() /
                (total_hand->as_number() * 100.0);

        bind_pe(last, previous);
        if (total_hand && total_hand->is_number() &&
            symbols_.as_object().count("CAPITAL"))
            values_["37"] = total_hand->as_number() * 100.0 /
                symbols_.at("CAPITAL").as_number();
        bind_historical_quote(previous, total_hand);
        bind_depth(row);
        bind_price_limits(row, previous, last);
    }

    void bind_pe(const Json* last, const Json* previous) {
        if (!requirements_.selectors.count(39) || !finance_record_) return;
        const auto* net_profit = nested(
            *finance_record_, {"income_statement", "net_profit_yuan"});
        const auto* total_shares = nested(
            *finance_record_, {"shares", "total"});
        const auto* report_months = nested(*finance_record_, {"reserved_2"});
        const double price = last && last->is_number() &&
                last->as_number() > 0.00001
            ? last->as_number()
            : (previous && previous->is_number()
                ? previous->as_number() : 0.0);
        if (!net_profit || !total_shares || !report_months ||
            !net_profit->is_number() || !total_shares->is_number() ||
            !report_months->is_number() || total_shares->as_number() <= 0.0 ||
            report_months->as_number() <= 0.0 || price <= 0.0)
            return;
        const float annualized_eps = static_cast<float>(
            net_profit->as_number() * 12.0 / report_months->as_number() /
            total_shares->as_number());
        if (annualized_eps <= 0.00009999999747378752f) return;
        values_["39"] = static_cast<double>(static_cast<float>(
            price / static_cast<double>(annualized_eps)));
        context_["dynainfo39_mode"] =
            "tdxw-type105-offset185-annualized-net-profit-per-total-share";
    }

    void bind_historical_quote(const Json* previous, const Json* total_hand) {
        Json daily;
        const Json* daily_pointer = nullptr;
        if (includes_any(requirements_.selectors, {17, 93})) {
            const auto period = kline_document_
                ? text_or(*kline_document_, "period") : std::string{};
            if (kline_document_ && period == "day")
                daily_pointer = kline_document_;
            else {
                daily = fetch_kline_document(
                    market_, code_, "auto", "day", 1, 32, 0,
                    "all", timeout_ms_);
                daily_pointer = &daily;
            }
        }
        if (previous && previous->is_number() && daily_pointer) {
            if (requirements_.selectors.count(93)) {
                const auto value = previous_amount(
                    *daily_pointer, previous->as_number());
                if (std::isfinite(value)) values_["93"] = value;
            }
            if (requirements_.selectors.count(17) && total_hand &&
                total_hand->is_number()) {
                const auto average = previous_average_volume_hand(
                    *daily_pointer, previous->as_number());
                if (std::isfinite(average) && average > 0)
                    values_["17"] = total_hand->as_number() /
                        (average * trading_day_fraction());
            }
        }
        if (requirements_.selectors.count(25)) {
            const auto minute = fetch_kline_document(
                market_, code_, "auto", "1m", 1, 32, 0,
                "latest", timeout_ms_);
            const auto value = price_minutes_ago(minute, 5);
            if (std::isfinite(value)) values_["25"] = value;
        }
    }

    void bind_depth(const Json& row) {
        if (!includes_any(requirements_.selectors, {20, 21, 58, 59})) return;
        const auto bind_level = [&](int id, std::string_view side,
                                    std::string_view field) {
            const auto* levels = optional(row, side);
            if (!levels || !levels->is_array() || levels->as_array().empty())
                return;
            if (const auto* value = optional(levels->as_array().front(), field);
                value && value->is_number())
                values_[std::to_string(id)] = *value;
        };
        bind_level(20, "buy_levels", "price");
        bind_level(21, "sell_levels", "price");
        bind_level(58, "buy_levels", "volume_hand");
        bind_level(59, "sell_levels", "volume_hand");
    }

    double level_number(const Json& row, std::string_view side,
                        std::string_view field) const {
        const auto* levels = optional(row, side);
        if (!levels || !levels->is_array() || levels->as_array().empty())
            return 0.0;
        const auto* value = optional(levels->as_array().front(), field);
        return value && value->is_number() ? value->as_number() : 0.0;
    }

    void bind_price_limits(const Json& row, const Json* previous,
                           const Json* last) {
        if (!previous || !previous->is_number() ||
            !includes_any(requirements_.selectors, {26, 27, 28, 29, 88}))
            return;
        const double rate = standard_limit_rate(security_type_);
        double upper_limit = rounded_limit(previous->as_number(), rate, true);
        double lower_limit = rounded_limit(previous->as_number(), rate, false);
        Json metadata = Json::object();
        metadata["source"] = "board-rate-fallback";
        metadata["table_match"] = false;
        try {
            const auto exact = cached_special_limit_price(
                root_, market_, code_, timeout_ms_);
            metadata["record_count"] = exact.record_count;
            if (!exact.endpoint.empty()) metadata["endpoint"] = exact.endpoint;
            if (exact.price) {
                upper_limit = exact.price->upper;
                lower_limit = exact.price->lower;
                metadata["source"] = "tdx-0x0452-special-limit-table";
                metadata["table_match"] = true;
            }
        } catch (const std::exception& error) {
            metadata["error"] = error.what();
        }
        metadata["upper"] = upper_limit;
        metadata["lower"] = lower_limit;
        metadata["fallback_rate"] = rate;
        context_["dynainfo_limit_values"] = std::move(metadata);
        context_["dynainfo_limit_mode"] =
            "tdx-0x0452-exact-when-listed-board-rate-fallback";
        if (requirements_.selectors.count(26)) values_["26"] = upper_limit;
        if (requirements_.selectors.count(27)) values_["27"] = lower_limit;
        bind_seal_state(row, previous->as_number(), last, upper_limit);
        bind_market_cage(row, previous->as_number(), last,
                         upper_limit, lower_limit);
    }

    void bind_seal_state(const Json& row, double previous_price,
                         const Json* last, double upper_limit) {
        if (!requirements_.selectors.count(88)) return;
        const double last_price = last && last->is_number()
            ? last->as_number() : 0.0;
        const double bid_price = level_number(row, "buy_levels", "price");
        const double ask_price = level_number(row, "sell_levels", "price");
        const double bid_volume = level_number(
            row, "buy_levels", "volume_hand");
        const double ask_volume = level_number(
            row, "sell_levels", "volume_hand");
        const bool sealed = previous_price > 0.0 && last_price > 0.0 &&
            bid_volume > 0.0 && std::abs(last_price - upper_limit) < 0.0001 &&
            std::abs(bid_price - upper_limit) < 0.0001 &&
            ask_price < 0.0001 && ask_volume < 0.0001;
        const double value = sealed
            ? static_cast<double>(static_cast<float>(
                (upper_limit - previous_price) * 100.0 / previous_price))
            : 0.0;
        values_["88"] = value;
        Json metadata = Json::object();
        metadata["value"] = value;
        metadata["sealed_limit_up"] = sealed;
        metadata["previous_close"] = previous_price;
        metadata["upper_limit"] = upper_limit;
        metadata["last_price"] = last_price;
        metadata["bid1_price"] = bid_price;
        metadata["bid1_volume_hand"] = bid_volume;
        metadata["ask1_price"] = ask_price;
        metadata["ask1_volume_hand"] = ask_volume;
        if (const auto* amount = optional(row, "bid1_amount_yuan");
            amount && amount->is_number())
            metadata["bid1_amount_yuan"] = *amount;
        context_["dynainfo_seal_state_mode"] =
            "tcalc-dynainfo88-type163-offset384-limit-up-percent-when-sealed";
        context_["dynainfo_seal_state_values"] = std::move(metadata);
    }

    void bind_market_cage(const Json& row, double previous_price,
                          const Json* last, double upper_limit,
                          double lower_limit) {
        if (!includes_any(requirements_.selectors, {28, 29})) return;
        auto normalized_market = lower_ascii(market_);
        if (normalized_market == "2") normalized_market = "bj";
        else if (normalized_market == "1") normalized_market = "sh";
        else if (normalized_market == "0") normalized_market = "sz";
        const auto cage = calculate_tdx_market_cage({
            level_number(row, "buy_levels", "price"),
            level_number(row, "sell_levels", "price"),
            last && last->is_number() ? last->as_number() : 0.0,
            previous_price, upper_limit, lower_limit,
            normalized_market == "bj",
            security_type_ >= 1 && security_type_ <= 5,
            normalized_market == "sh" && starts_with(code_, "900")});
        if (requirements_.selectors.count(28)) values_["28"] = cage.upper;
        if (requirements_.selectors.count(29)) values_["29"] = cage.lower;
        Json metadata = Json::object();
        metadata["upper"] = cage.upper;
        metadata["lower"] = cage.lower;
        metadata["upper_reference"] = cage.upper_reference;
        metadata["lower_reference"] = cage.lower_reference;
        metadata["tick_size"] = cage.tick_size;
        context_["dynainfo_cage_mode"] =
            "tdxw-type121-selector198-l1-core-exact-limit-cap-when-listed";
        context_["dynainfo_cage_values"] = std::move(metadata);
    }

    void bind_aliases() {
        Json metadata = Json::object();
        for (const auto& alias : kDynamicAliases) {
            if (!requirements_.aliases.count(std::string(alias.name))) continue;
            const auto key = std::to_string(alias.selector);
            const auto found = values_.as_object().find(key);
            const double value = found != values_.as_object().end() &&
                    found->second.is_number()
                ? found->second.as_number() : 0.0;
            symbols_[std::string(alias.name)] = value;
            Json item = Json::object();
            item["value"] = value;
            item["dynainfo_selector"] = alias.selector;
            item["tcalc_opcode"] = alias.tcalc_opcode;
            item["broadcast"] = true;
            item["source_command"] =
                alias.selector == 24 ? "0x053E" : "0x054C";
            metadata[std::string(alias.name)] = std::move(item);
        }
        if (!metadata.size()) return;
        context_["dynamic_alias_mode"] =
            "TCalc-opcodes1380-1383-DYNAINFO-selectors7-14-17-24-public-L1-broadcast";
        context_["dynamic_aliases"] = std::move(metadata);
    }

    Json& context_;
    Json& symbols_;
    const std::filesystem::path& root_;
    const std::string& market_;
    const std::string& code_;
    const Json* kline_document_;
    const DynamicQuoteRequirements& requirements_;
    const Json* finance_record_;
    int security_type_;
    int timeout_ms_;
    const BlockData* block_data_;
    Json values_{Json::object()};
};

}  // namespace

DynamicQuoteRequirements dynamic_quote_requirements(
    const Json& analysis,
    const std::set<std::string>& dependencies) {
    DynamicQuoteRequirements result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (values && values->is_array()) {
        constexpr std::string_view prefix = "DYNAINFO#";
        for (const auto& value : values->as_array()) {
            if (!value.is_string() ||
                value.as_string().rfind(prefix, 0) != 0)
                continue;
            try {
                result.selectors.insert(std::stoi(
                    value.as_string().substr(prefix.size())));
            } catch (...) {
                // The analyzer emits numeric selectors. Ignore malformed
                // externally supplied analysis documents.
            }
        }
    }
    for (const auto& alias : kDynamicAliases) {
        if (!dependencies.count(std::string(alias.name))) continue;
        result.aliases.insert(std::string(alias.name));
        result.selectors.insert(alias.selector);
    }
    result.buy_volume = dependencies.count("BUYVOL") != 0;
    result.sell_volume = dependencies.count("SELLVOL") != 0;
    return result;
}

void bind_dynamic_quote_context(
    Json& context,
    Json& symbols,
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const Json* kline_document,
    const DynamicQuoteRequirements& requirements,
    const Json* finance_record,
    int security_type,
    int timeout_ms,
    const BlockData* block_data) {
    DynamicQuoteContextBuilder(
        context, symbols, root, market, code, kline_document,
        requirements, finance_record, security_type, timeout_ms,
        block_data).build();
}

}  // namespace tdx::formula_context_detail
