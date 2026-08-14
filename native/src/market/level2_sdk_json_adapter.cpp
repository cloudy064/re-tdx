#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t max_json_records = 10000;
constexpr std::size_t queue_native_capacity = (242 - 40) / 2;

const Json* member(const Json& object, std::string_view name) {
    if (!object.is_object()) return nullptr;
    const auto& values = object.as_object();
    const auto found = values.find(name);
    return found == values.end() ? nullptr : &found->second;
}

const Json& required_member(const Json& object, std::string_view name,
                            std::string_view context) {
    const auto* value = member(object, name);
    if (!value)
        throw Error(std::string(context) + " is missing " + std::string(name));
    return *value;
}

Json sdk_data(const Json& document) {
    if (!document.is_object()) throw Error("SDK JSON response must be an object");
    Json value = required_member(document, "Data", "SDK JSON response");
    if (value.is_string()) {
        try {
            value = Json::parse(value.as_string());
        } catch (const std::exception& error) {
            throw Error(std::string("SDK JSON Data string is invalid JSON: ") +
                        error.what());
        }
    }
    return value;
}

double number_value(const Json& value, std::string_view name) {
    double result{};
    if (value.is_number()) {
        result = value.as_number();
    } else if (value.is_string()) {
        const auto text = trim(value.as_string());
        try {
            std::size_t consumed = 0;
            result = std::stod(text, &consumed);
            if (consumed != text.size()) throw std::invalid_argument("trailing");
        } catch (...) {
            throw Error(std::string(name) + " must be numeric");
        }
    } else {
        throw Error(std::string(name) + " must be numeric");
    }
    if (!std::isfinite(result))
        throw Error(std::string(name) + " must be finite");
    return result;
}

double optional_number(const Json& object, std::string_view name) {
    const auto* value = member(object, name);
    return value ? number_value(*value, name) : 0.0;
}

std::int64_t integer_value(const Json& value, std::string_view name,
                           std::int64_t minimum, std::int64_t maximum) {
    const double number = number_value(value, name);
    if (number < static_cast<double>(minimum) ||
        number > static_cast<double>(maximum) || std::trunc(number) != number)
        throw Error(std::string(name) + " is outside its integer range");
    return static_cast<std::int64_t>(number);
}

std::uint64_t optional_u32(const Json& object, std::string_view name) {
    const auto* value = member(object, name);
    if (!value) return 0;
    return static_cast<std::uint64_t>(integer_value(
        *value, name, 0, std::numeric_limits<std::uint32_t>::max()));
}

int clock_seconds(std::int64_t hhmmss, std::string_view name) {
    if (hhmmss < 0 || hhmmss > 235959)
        throw Error(std::string(name) + " is not a valid HHMMSS value");
    const int second = static_cast<int>(hhmmss % 100);
    const int minute = static_cast<int>(hhmmss / 100 % 100);
    const int hour = static_cast<int>(hhmmss / 10000);
    if (hour > 23 || minute > 59 || second > 59)
        throw Error(std::string(name) + " is not a valid HHMMSS value");
    return hour * 3600 + minute * 60 + second;
}

std::string clock_label(int seconds) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << seconds / 3600 << ':'
           << std::setw(2) << seconds / 60 % 60 << ':'
           << std::setw(2) << seconds % 60;
    return output.str();
}

void common_metadata(Json& result, int function_id) {
    result["function_id"] = function_id;
    result["adapter"] = "tpbus ProtocolSZSDK2TDX::SetAnsData";
    result["input_scope"] = "authorized post-transport SDK JSON response";
    result["network_request_bytes"] = false;
    result["request_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
}

Json normalize_transactions(const Json& document, int limit) {
    const auto data = sdk_data(document);
    if (!data.is_array()) throw Error("SDK 4655 Data must be an array");
    const auto count = data.size();
    if (count > max_json_records)
        throw Error("SDK 4655 record count exceeds the offline safety limit");

    Json records = Json::array();
    std::size_t returned = 0;
    for (std::size_t output_index = 0; output_index < count; ++output_index) {
        const std::size_t source_index = count - output_index - 1;
        const auto& item = data.as_array()[source_index];
        if (!item.is_object()) throw Error("SDK 4655 Data item must be an object");
        const auto time_raw = integer_value(
            required_member(item, "transactionTime", "SDK 4655 item"),
            "transactionTime", 0, std::numeric_limits<std::int32_t>::max());
        const int seconds = clock_seconds(time_raw / 100, "transactionTime");
        const double price = number_value(
            required_member(item, "transactionPrice", "SDK 4655 item"),
            "transactionPrice");
        const auto volume = integer_value(
            required_member(item, "singleVolume", "SDK 4655 item"),
            "singleVolume", 0, std::numeric_limits<std::uint32_t>::max());
        std::string status;
        if (const auto* value = member(item, "transactionStatus")) {
            if (!value->is_string())
                throw Error("transactionStatus must be a string");
            status = value->as_string();
        }
        const int status_raw = status == "B" ? 0 : status == "S" ? 1 : -1;
        if (output_index < static_cast<std::size_t>(limit)) {
            Json row = Json::object();
            row["index"] = static_cast<std::uint64_t>(output_index);
            row["source_index"] = static_cast<std::uint64_t>(source_index);
            row["time_seconds"] = seconds;
            row["time"] = clock_label(seconds);
            row["price"] = price;
            row["volume_raw"] = static_cast<std::uint64_t>(volume);
            row["status_raw"] = status_raw;
            row["side"] = status_raw == 0 ? "buy" : status_raw == 1 ? "sell" : "unknown";
            records.push_back(std::move(row));
            ++returned;
        }
    }

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-json-4655-v1";
    common_metadata(result, 4655);
    result["native_record_size"] = 18;
    result["source_order"] = "last-to-first";
    result["count"] = static_cast<std::uint64_t>(count);
    result["returned"] = static_cast<std::uint64_t>(returned);
    result["records"] = std::move(records);
    result["truncated"] = returned < count;
    return result;
}

const Json::Array& quantity_array(const Json& data, std::string_view side_name,
                                  bool select_last) {
    static const Json::Array empty;
    const auto* side = member(data, side_name);
    if (!side) return empty;
    if (!side->is_array())
        throw Error(std::string("SDK 4671 ") + std::string(side_name) +
                    " must be an array");
    if (side->as_array().empty()) return empty;
    const auto& selected = select_last ? side->as_array().back()
                                       : side->as_array().front();
    if (!selected.is_object())
        throw Error(std::string("SDK 4671 selected ") + std::string(side_name) +
                    " item must be an object");
    const auto* quantities = member(selected, "QUANTITY_");
    if (!quantities) return empty;
    if (!quantities->is_array())
        throw Error(std::string("SDK 4671 ") + std::string(side_name) +
                    " QUANTITY_ must be an array");
    return quantities->as_array();
}

Json normalize_queue(const Json& document, int limit) {
    const auto data = sdk_data(document);
    if (!data.is_object()) throw Error("SDK 4671 Data must be an object");
    const auto& buy_source = quantity_array(data, "buyList", false);
    const auto& sell_source = quantity_array(data, "sellList", true);
    if (buy_source.size() + sell_source.size() > queue_native_capacity)
        throw Error("SDK 4671 quantities exceed the recovered 242-byte body capacity");

    std::vector<std::uint16_t> buys;
    std::vector<std::uint16_t> sells;
    const auto convert = [](const Json::Array& source,
                            std::vector<std::uint16_t>& target,
                            std::string_view name) {
        target.reserve(source.size());
        for (const auto& value : source) {
            const auto raw = integer_value(value, name, 0, 65535LL * 100 + 99);
            target.push_back(static_cast<std::uint16_t>(raw / 100));
        }
    };
    convert(buy_source, buys, "buyList QUANTITY_");
    convert(sell_source, sells, "sellList QUANTITY_");

    std::size_t buy_limit = std::min<std::size_t>(
        buys.size(), (static_cast<std::size_t>(limit) + 1) / 2);
    std::size_t sell_limit = std::min<std::size_t>(
        sells.size(), static_cast<std::size_t>(limit) - buy_limit);
    auto remaining = static_cast<std::size_t>(limit) - buy_limit - sell_limit;
    const auto add_buy = std::min(buys.size() - buy_limit, remaining);
    buy_limit += add_buy;
    remaining -= add_buy;
    sell_limit += std::min(sells.size() - sell_limit, remaining);

    Json buy_values = Json::array();
    Json sell_values = Json::array();
    for (std::size_t index = 0; index < buy_limit; ++index)
        buy_values.push_back(static_cast<std::uint64_t>(buys[index]));
    for (std::size_t index = 0; index < sell_limit; ++index)
        sell_values.push_back(static_cast<std::uint64_t>(sells[index]));

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-json-4671-v1";
    common_metadata(result, 4671);
    result["native_body_size"] = 242;
    result["native_quantity_capacity"] =
        static_cast<std::uint64_t>(queue_native_capacity);
    result["selection"] = "buyList[0] and sellList[last]";
    result["quantity_scale_divisor"] = 100;
    result["buy_count"] = static_cast<std::uint64_t>(buys.size());
    result["sell_count"] = static_cast<std::uint64_t>(sells.size());
    result["buy_returned"] = static_cast<std::uint64_t>(buy_limit);
    result["sell_returned"] = static_cast<std::uint64_t>(sell_limit);
    result["buy_quantities_hands"] = std::move(buy_values);
    result["sell_quantities_hands"] = std::move(sell_values);
    result["truncated"] = buy_limit < buys.size() || sell_limit < sells.size();
    return result;
}

const Json::Array& numeric_array(const Json& data, std::string_view name) {
    const auto* value = member(data, name);
    if (!value || !value->is_array())
        throw Error(std::string("SDK 4680 ") + std::string(name) +
                    " must be an array");
    if (value->size() > max_json_records)
        throw Error(std::string("SDK 4680 ") + std::string(name) +
                    " exceeds the offline safety limit");
    return value->as_array();
}

std::uint64_t scaled_quantity(const Json& value, std::string_view name) {
    const double raw = number_value(value, name);
    constexpr double maximum =
        static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    const double scaled = raw * 1000.0;
    if (raw < 0 || !std::isfinite(scaled) || scaled > maximum)
        throw Error(std::string(name) + " is outside the scaled u32 range");
    return static_cast<std::uint64_t>(scaled);
}

Json normalize_depth(const Json& document, int requested_depth) {
    const auto data = sdk_data(document);
    if (!data.is_object()) throw Error("SDK 4680 Data must be an object");
    const auto& buy_prices = numeric_array(data, "buyPrices");
    const auto& buy_volumes = numeric_array(data, "buyVolumes");
    const auto& sell_prices = numeric_array(data, "sellPrices");
    const auto& sell_volumes = numeric_array(data, "sellVolumes");
    if (buy_prices.size() != buy_volumes.size() ||
        sell_prices.size() != sell_volumes.size())
        throw Error("SDK 4680 price and volume arrays must be paired per side");

    const auto* datetime = member(data, "datetime");
    if (!datetime || !datetime->is_string())
        throw Error("SDK 4680 datetime must be a string");
    const auto& datetime_raw = datetime->as_string();
    if (datetime_raw.size() < 6 ||
        !std::all_of(datetime_raw.end() - 6, datetime_raw.end(),
                     [](unsigned char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("SDK 4680 datetime must end in six HHMMSS digits");
    const int seconds = clock_seconds(
        std::stoll(datetime_raw.substr(datetime_raw.size() - 6)), "datetime");

    const std::size_t depth = std::min<std::size_t>(
        static_cast<std::size_t>(requested_depth),
        std::max(buy_prices.size(), sell_prices.size()));
    const std::size_t buy_count = std::min(depth, buy_prices.size());
    const std::size_t sell_count = std::min(depth, sell_prices.size());
    Json buys = Json::array();
    Json sells = Json::array();
    for (std::size_t index = 0; index < buy_count; ++index) {
        const std::size_t source_index = buy_count - index - 1;
        Json row = Json::object();
        row["level"] = static_cast<std::uint64_t>(index + 1);
        row["source_index"] = static_cast<std::uint64_t>(source_index);
        row["price"] = number_value(buy_prices[source_index], "buyPrices");
        row["volume_raw"] = scaled_quantity(
            buy_volumes[source_index], "buyVolumes");
        buys.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < sell_count; ++index) {
        Json row = Json::object();
        row["level"] = static_cast<std::uint64_t>(index + 1);
        row["source_index"] = static_cast<std::uint64_t>(index);
        row["price"] = number_value(sell_prices[index], "sellPrices");
        row["volume_raw"] = scaled_quantity(sell_volumes[index], "sellVolumes");
        sells.push_back(std::move(row));
    }

    Json quote = Json::object();
    quote["pre_close"] = optional_number(data, "preClosePrice");
    quote["open"] = optional_number(data, "openPrice");
    quote["high"] = optional_number(data, "highPrice");
    quote["low"] = optional_number(data, "lowPrice");
    quote["last"] = optional_number(data, "lastPrice");
    quote["volume_raw"] = optional_u32(data, "volume");
    quote["amount"] = optional_number(data, "amount");
    quote["open_interest_raw"] = optional_u32(data, "openInterest");

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-json-4680-v1";
    common_metadata(result, 4680);
    result["native_header_size"] = 99;
    result["native_level_size"] = 20;
    result["requested_depth"] = requested_depth;
    result["depth_count"] = static_cast<std::uint64_t>(depth);
    result["time_seconds"] = seconds;
    result["time"] = clock_label(seconds);
    result["datetime_raw"] = datetime_raw;
    result["quote"] = std::move(quote);
    result["buy_orientation"] = "reversed-source-order";
    result["sell_orientation"] = "source-order";
    result["quantity_scale_multiplier"] = 1000;
    result["buy_levels"] = std::move(buys);
    result["sell_levels"] = std::move(sells);
    result["truncated"] = buy_count < buy_prices.size() ||
                          sell_count < sell_prices.size();
    return result;
}

}  // namespace

Json normalize_level2_sdk_json(int function_id, const Json& document,
                               int limit, int requested_depth) {
    if (limit < 0 || limit > static_cast<int>(max_json_records))
        throw Error("SDK JSON adapter limit must be 0..10000");
    if (requested_depth != 5 && requested_depth != 10)
        throw Error("SDK 4680 requested depth must be 5 or 10");
    if (function_id == 4653)
        return normalize_level2_sdk_json_4653(document, limit);
    if (function_id == 4655) return normalize_transactions(document, limit);
    if (function_id == 4671) return normalize_queue(document, limit);
    if (function_id == 4680) return normalize_depth(document, requested_depth);
    throw Error("SDK JSON adapter function must be 4653, 4655, 4671, or 4680");
}

}  // namespace tdx
