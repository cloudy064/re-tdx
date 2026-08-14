#include "cloud_calc_host_internal.hpp"

#include "cloud_calc_builtins_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace tdx::cloud_calc_detail {

const Json* host_optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

bool meaningful(const Json* value) {
    if (!value || value->is_null()) return false;
    return !value->is_string() || !trim(value->as_string()).empty();
}

std::string scalar_text(const Json& value) {
    if (value.is_string()) return trim(value.as_string());
    if (value.is_number()) {
        const double number = value.as_number();
        if (std::floor(number) == number) return std::to_string(static_cast<std::int64_t>(number));
        std::ostringstream output;
        output << std::setprecision(15) << number;
        return output.str();
    }
    return {};
}

std::optional<int> host_market(const Json& value) {
    auto text = lower_ascii(scalar_text(value));
    if (text == "sz") return 0;
    if (text == "sh") return 1;
    if (text == "bj") return 2;
    double number = 0.0;
    if (!parse_double(text, number) || std::floor(number) != number) return std::nullopt;
    int market = static_cast<int>(number);
    if (market == 44) market = 2;
    return market >= 0 && market <= 2 ? std::optional<int>(market) : std::nullopt;
}

std::optional<std::pair<int, std::string>> row_security(const Json& row,
                                                        std::string_view suffix) {
    const auto market_name = "$SC" + std::string(suffix);
    const auto code_name = "$ZQDM" + std::string(suffix);
    const auto* market_value = host_optional(row, market_name);
    const auto* code_value = host_optional(row, code_name);
    if (!market_value || !code_value) return std::nullopt;
    const auto market = host_market(*market_value);
    auto code = scalar_text(*code_value);
    if (code.size() < 6 && !code.empty() &&
        std::all_of(code.begin(), code.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); }))
        code.insert(code.begin(), 6 - code.size(), '0');
    if (!market || code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); }))
        return std::nullopt;
    return std::make_pair(*market, std::move(code));
}

std::string security_text(const std::pair<int, std::string>& value) {
    static constexpr const char* prefixes[] = {"sz", "sh", "bj"};
    return std::string(prefixes[value.first]) + ":" + value.second;
}

const Json::Array& snapshot_records(const Json& document) {
    if (document.is_array()) return document.as_array();
    if (document.is_object()) {
        const auto* records = host_optional(document, "records");
        if (records && records->is_array()) return records->as_array();
    }
    throw Error("host snapshot must be a record array or an object with a records array");
}

Json::Array& mutable_snapshot_records(Json& document) {
    if (document.is_array()) return document.as_array();
    if (document.is_object()) {
        auto records = document.as_object().find("records");
        if (records != document.as_object().end() && records->second.is_array())
            return records->second.as_array();
    }
    throw Error("host snapshot must be a record array or an object with a records array");
}

QuoteIndex index_snapshots(const Json& document) {
    QuoteIndex result;
    for (const auto& record : snapshot_records(document)) {
        if (!record.is_object()) continue;
        const auto* market_value = host_optional(record, "market_id");
        const auto* code_value = host_optional(record, "code");
        if (!market_value || !code_value) continue;
        const auto market = host_market(*market_value);
        const auto code = scalar_text(*code_value);
        if (market && code.size() == 6) result[{*market, code}] = &record;
    }
    return result;
}

std::optional<double> quote_number(const Json& quote, std::string_view name) {
    const auto* value = host_optional(quote, name);
    if (!value || value->is_null()) return std::nullopt;
    try { return json_number(*value, name); } catch (...) { return std::nullopt; }
}

std::optional<HostValue> quote_host_value(std::string_view syscol, const Json& quote) {
    if (syscol == "$NOW" || syscol == "$NOW3") {
        const auto last = quote_number(quote, "last_price");
        if (last && *last > 0.0) return HostValue{*last, "public-l1-last-price"};
        return std::nullopt;
    }
    if (syscol == "$NOW2") {
        const auto last = quote_number(quote, "last_price");
        if (last && *last > 0.0) return HostValue{*last, "public-l1-last-price"};
        const auto previous = quote_number(quote, "pre_close_price");
        if (previous && *previous > 0.0) return HostValue{*previous, "public-l1-pre-close"};
        return std::nullopt;
    }
    if (syscol == "$ZAF") {
        const auto value = host_optional(quote, "change_pct");
        if (meaningful(value)) return HostValue{*value, "public-l1-change-pct"};
    } else if (syscol == "$CLOSE") {
        const auto value = quote_number(quote, "pre_close_price");
        if (value) return HostValue{*value, "public-l1-pre-close"};
    } else if (syscol == "$OPEN") {
        const auto value = quote_number(quote, "open_price");
        if (value) return HostValue{*value, "public-l1-open-price"};
    } else if (syscol == "$MAX") {
        const auto value = quote_number(quote, "high_price");
        if (value) return HostValue{*value, "public-l1-high-price"};
    } else if (syscol == "$MIN") {
        const auto value = quote_number(quote, "low_price");
        if (value) return HostValue{*value, "public-l1-low-price"};
    } else if (syscol == "$NOWV") {
        const auto value = quote_number(quote, "current_hand");
        if (value) return HostValue{*value, "public-l1-current-hand"};
    } else if (syscol == "$QRSD") {
        const auto current = quote_host_value("$NOW", quote);
        const auto previous = quote_number(quote, "pre_close_price");
        if (current && previous && *previous > 0.0)
            return HostValue{current->value.as_number() - *previous,
                             "public-l1-last-minus-pre-close"};
    } else if (syscol == "$ZEF") {
        const auto previous = quote_number(quote, "pre_close_price");
        const auto high = quote_number(quote, "high_price");
        const auto low = quote_number(quote, "low_price");
        if (previous && high && low && *previous > 0.0 && *high > 0.0 && *low > 0.0)
            return HostValue{(*high - *low) * 100.0 / *previous,
                             "tbigdata-id18-high-minus-low-over-pre-close"};
    } else if (syscol == "$ZS") {
        const auto value = quote_number(quote, "rise_speed_pct");
        if (value) return HostValue{*value, "public-l1-rise-speed-0x053e"};
    } else if (syscol == "$JJJZ") {
        const auto value = quote_number(quote, "fund_iopv");
        if (value && *value > 0.0)
            return HostValue{*value, "tdx-quote-core-etf-iopv"};
    } else if (syscol == "$FCAMO") {
        const auto value = quote_number(quote, "seal_amount_yuan");
        if (value) return HostValue{*value, "tdxw-type163-seal-amount"};
    } else if (syscol == "$FCB") {
        const auto value = quote_number(quote, "seal_ratio");
        if (value) return HostValue{*value, "tdxw-type163-seal-ratio"};
    } else if (syscol == "$ZQJC") {
        const auto value = host_optional(quote, "name");
        if (meaningful(value)) return HostValue{*value, "local-security-name"};
    } else if (syscol == "$ZCJJE") {
        const auto value = quote_number(quote, "amount");
        if (value) return HostValue{*value, "public-l1-amount"};
    } else if (syscol == "$CJL") {
        const auto value = quote_number(quote, "total_hand");
        if (value) return HostValue{*value, "public-l1-total-hand"};
    } else if (syscol == "$INP") {
        const auto value = quote_number(quote, "inside_dish");
        if (value) return HostValue{*value, "public-l1-inside-dish"};
    } else if (syscol == "$OUTP") {
        const auto value = quote_number(quote, "outer_disc");
        if (value) return HostValue{*value, "public-l1-outer-disc"};
    } else if (syscol == "$BP1" || syscol == "$BPV1" ||
               syscol == "$SP1" || syscol == "$SPV1") {
        const bool buy = syscol == "$BP1" || syscol == "$BPV1";
        const bool volume = syscol == "$BPV1" || syscol == "$SPV1";
        const auto* levels = host_optional(quote, buy ? "buy_levels" : "sell_levels");
        if (!levels || !levels->is_array() || levels->as_array().empty()) return std::nullopt;
        const auto* value = host_optional(levels->as_array().front(), volume ? "volume_hand" : "price");
        if (!value || !value->is_number()) return std::nullopt;
        return HostValue{*value, buy
            ? (volume ? "public-l1-depth-bid1-volume-hand" : "public-l1-depth-bid1-price")
            : (volume ? "public-l1-depth-ask1-volume-hand" : "public-l1-depth-ask1-price")};
    }
    return std::nullopt;
}

}  // namespace tdx::cloud_calc_detail
