#include "corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>

namespace tdx::corporate_detail {
std::string hex(const Bytes& value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : value) output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
}

std::string hex(const std::uint8_t* data, std::size_t size) {
    return hex(Bytes(data, data + size));
}


std::string market_name(int market_id) {
    if (market_id == 0) return "sz";
    if (market_id == 1) return "sh";
    if (market_id == 2) return "bj";
    return "unknown";
}

std::string security_id(int market_id, const std::string& code) {
    if (market_id == 0) return "SZ" + code;
    if (market_id == 1) return "SH" + code;
    if (market_id == 2) return "BJ" + code;
    return "M" + std::to_string(market_id) + code;
}

Json date_json(std::uint32_t raw) {
    const auto text = std::to_string(raw);
    if (text.size() != 8) return Json(nullptr);
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(4, 2));
    const int day = std::stoi(text.substr(6, 2));
    if (year < 1900 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31)
        return Json(nullptr);
    return text.substr(0, 4) + "-" + text.substr(4, 2) + "-" + text.substr(6, 2);
}

double finite_float(const std::uint8_t* data, std::string_view field) {
    const double value = read_f32_le(data);
    if (!std::isfinite(value)) throw Error(std::string(field) + " is not finite");
    return value;
}

double wire_number(std::uint32_t value) {
    if (value == 0) return 0.0;
    std::int32_t signed_value = 0;
    std::memcpy(&signed_value, &value, sizeof(value));
    const int exponent = signed_value >> 24;
    const unsigned high_byte = (value >> 16) & 0xFF;
    const unsigned middle_byte = (value >> 8) & 0xFF;
    const unsigned low_byte = value & 0xFF;
    const double base = std::pow(2.0, static_cast<double>(exponent * 2 - 0x7F));
    const double high = high_byte > 0x80
        ? base * (64.0 + static_cast<double>(high_byte & 0x7F)) / 64.0
        : base * static_cast<double>(high_byte) / 128.0;
    const double scale = high_byte & 0x80 ? 2.0 : 1.0;
    const double result = base + high + base * middle_byte / 32768.0 * scale +
                          base * low_byte / 8388608.0 * scale;
    if (!std::isfinite(result)) throw Error("capital-change share count is not finite");
    return result;
}


Json finance_record_json(const std::uint8_t* record, bool include_raw) {
    const int market_id = record[0];
    if (market_id < 0 || market_id > 2) throw Error("finance record has invalid market");
    const std::string code(reinterpret_cast<const char*>(record + 1), 6);
    if (!std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("finance record has invalid security code");
    const auto* info = record + 7;
    std::size_t offset = 0;
    auto next_float = [&](std::string_view name) {
        if (offset + 4 > finance_info_size) throw Error("finance record float overflow");
        const auto value = finite_float(info + offset, name);
        offset += 4;
        return value;
    };
    const double circulating_shares_raw = next_float("circulating_shares");
    const auto province_id = read_u16_le(info + offset); offset += 2;
    const auto industry_id = read_u16_le(info + offset); offset += 2;
    const auto updated_date_raw = read_u32_le(info + offset); offset += 4;
    const auto listing_date_raw = read_u32_le(info + offset); offset += 4;
    const double total_shares_raw = next_float("total_shares");
    const double national_shares_raw = next_float("national_shares");
    const double promoter_legal_shares_raw = next_float("promoter_legal_shares");
    const double legal_shares_raw = next_float("legal_shares");
    const double b_shares_raw = next_float("b_shares");
    const double h_shares_raw = next_float("h_shares");
    const double eps = next_float("eps");
    const double total_assets_raw = next_float("total_assets");
    const double current_assets_raw = next_float("current_assets");
    const double fixed_assets_raw = next_float("fixed_assets");
    const double intangible_assets_raw = next_float("intangible_assets");
    const double shareholder_count = next_float("shareholder_count");
    const double current_liabilities_raw = next_float("current_liabilities");
    const double long_term_liabilities_raw = next_float("long_term_liabilities");
    const double capital_reserve_raw = next_float("capital_reserve");
    const double net_assets_raw = next_float("net_assets");
    const double revenue_raw = next_float("revenue");
    const double main_profit_raw = next_float("main_profit");
    const double accounts_receivable_raw = next_float("accounts_receivable");
    const double operating_profit_raw = next_float("operating_profit");
    const double investment_income_raw = next_float("investment_income");
    const double operating_cash_flow_raw = next_float("operating_cash_flow");
    const double total_cash_flow_raw = next_float("total_cash_flow");
    const double inventory_raw = next_float("inventory");
    const double total_profit_raw = next_float("total_profit");
    const double after_tax_profit_raw = next_float("after_tax_profit");
    const double net_profit_raw = next_float("net_profit");
    const double undistributed_profit_raw = next_float("undistributed_profit");
    const double net_assets_per_share = next_float("net_assets_per_share");
    const double reserved_2 = next_float("reserved_2");
    if (offset != finance_info_size) throw Error("finance record size mismatch");

    const auto shares = [](double raw) { return raw * 10000.0; };
    const auto yuan = [](double raw) { return raw * 1000.0; };
    Json value = Json::object();
    value["security_id"] = security_id(market_id, code);
    value["market"] = market_name(market_id);
    value["market_id"] = market_id;
    value["code"] = code;
    value["province_id"] = static_cast<std::uint64_t>(province_id);
    value["industry_id"] = static_cast<std::uint64_t>(industry_id);
    value["updated_date_raw"] = static_cast<std::uint64_t>(updated_date_raw);
    value["updated_date"] = date_json(updated_date_raw);
    value["listing_date_raw"] = static_cast<std::uint64_t>(listing_date_raw);
    value["listing_date"] = date_json(listing_date_raw);
    Json share_values = Json::object();
    share_values["circulating"] = shares(circulating_shares_raw);
    share_values["total"] = shares(total_shares_raw);
    share_values["national"] = shares(national_shares_raw);
    share_values["promoter_legal_person"] = shares(promoter_legal_shares_raw);
    share_values["legal_person"] = shares(legal_shares_raw);
    share_values["b_share"] = shares(b_shares_raw);
    share_values["h_share"] = shares(h_shares_raw);
    value["shares"] = std::move(share_values);
    Json per_share = Json::object();
    per_share["eps"] = eps;
    per_share["net_assets"] = net_assets_per_share;
    value["per_share"] = std::move(per_share);
    value["shareholder_count"] = shareholder_count;
    Json balance = Json::object();
    balance["total_assets_yuan"] = yuan(total_assets_raw);
    balance["current_assets_yuan"] = yuan(current_assets_raw);
    balance["fixed_assets_yuan"] = yuan(fixed_assets_raw);
    balance["intangible_assets_yuan"] = yuan(intangible_assets_raw);
    balance["current_liabilities_yuan"] = yuan(current_liabilities_raw);
    balance["long_term_liabilities_yuan"] = yuan(long_term_liabilities_raw);
    balance["capital_reserve_yuan"] = yuan(capital_reserve_raw);
    balance["net_assets_yuan"] = yuan(net_assets_raw);
    balance["accounts_receivable_yuan"] = yuan(accounts_receivable_raw);
    balance["inventory_yuan"] = yuan(inventory_raw);
    value["balance_sheet"] = std::move(balance);
    Json income = Json::object();
    income["revenue_yuan"] = yuan(revenue_raw);
    income["main_profit_yuan"] = yuan(main_profit_raw);
    income["operating_profit_yuan"] = yuan(operating_profit_raw);
    income["investment_income_yuan"] = yuan(investment_income_raw);
    income["total_profit_yuan"] = yuan(total_profit_raw);
    income["after_tax_profit_yuan"] = yuan(after_tax_profit_raw);
    income["net_profit_yuan"] = yuan(net_profit_raw);
    income["undistributed_profit_yuan"] = yuan(undistributed_profit_raw);
    value["income_statement"] = std::move(income);
    Json cash = Json::object();
    cash["operating_yuan"] = yuan(operating_cash_flow_raw);
    cash["total_yuan"] = yuan(total_cash_flow_raw);
    value["cash_flow"] = std::move(cash);
    value["reserved_2"] = reserved_2;
    if (include_raw) {
        value["record_hex"] = hex(record, finance_record_size);
        value["raw_units"] = Json::parse(
            "{\"share_fields\":\"ten_thousand_shares\",\"currency_fields\":\"thousand_yuan\"}");
    }
    return value;
}

std::string capital_category_name(int category) {
    static const std::map<int, std::string> names{
        {1, "除权除息"}, {2, "送配股上市"}, {3, "非流通股上市"},
        {4, "国家股配售"}, {5, "股本变化"}, {6, "增发新股"},
        {7, "股份回购"}, {8, "增发新股上市"}, {9, "转配股上市"},
        {10, "可转债上市"}, {11, "扩缩股"}, {12, "非流通股缩股"},
        {13, "送认购权证"}, {14, "送认沽权证"}, {15, "重整调整"},
    };
    const auto found = names.find(category);
    return found == names.end() ? "未知" : found->second;
}

Json capital_record_json(const std::uint8_t* record, bool include_raw) {
    const int market_id = record[0];
    if (market_id < 0 || market_id > 2) throw Error("capital record has invalid market");
    const std::string code(reinterpret_cast<const char*>(record + 1), 6);
    if (!std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("capital record has invalid security code");
    const auto date_raw = read_u32_le(record + 8);
    const int category = record[12];
    std::array<double, 4> float_values{};
    std::array<double, 4> share_values{};
    for (std::size_t index = 0; index < 4; ++index) {
        float_values[index] = finite_float(record + 13 + index * 4, "capital value");
        share_values[index] = wire_number(read_u32_le(record + 13 + index * 4)) * 10000.0;
    }

    Json value = Json::object();
    value["security_id"] = security_id(market_id, code);
    value["market"] = market_name(market_id);
    value["market_id"] = market_id;
    value["code"] = code;
    value["reserved"] = static_cast<std::uint64_t>(record[7]);
    value["date_raw"] = static_cast<std::uint64_t>(date_raw);
    value["date"] = date_json(date_raw);
    value["category"] = category;
    value["category_name"] = capital_category_name(category);
    Json details = Json::object();
    if (category == 1) {
        // 线上样本（如平安银行 2024 年 10 派 7.19）证明首字段口径为每 10 股，
        // 同时给出每股派息派生值，避免复权公式误把现金红利放大十倍。
        details["dividend_per_10_shares_yuan"] = float_values[0];
        details["dividend_per_share_yuan"] = float_values[0] / 10.0;
        details["rights_price_yuan"] = float_values[1];
        details["bonus_transfer_per_10_shares"] = float_values[2];
        details["rights_per_10_shares"] = float_values[3];
    } else if (category == 11 || category == 12) {
        details["shrink_ratio"] = float_values[2];
    } else if (category == 13 || category == 14) {
        details["exercise_price_yuan"] = float_values[0];
        details["warrant_units"] = float_values[2];
    } else if (category == 15) {
        for (std::size_t index = 0; index < 4; ++index)
            details["value_" + std::to_string(index + 1)] = float_values[index];
    } else {
        details["before_circulating_shares"] = share_values[0];
        details["before_total_shares"] = share_values[1];
        details["after_circulating_shares"] = share_values[2];
        details["after_total_shares"] = share_values[3];
    }
    value["details"] = std::move(details);
    if (include_raw) {
        Json floats = Json::array();
        Json shares = Json::array();
        for (std::size_t index = 0; index < 4; ++index) {
            floats.push_back(float_values[index]);
            shares.push_back(share_values[index]);
        }
        value["float32_values"] = std::move(floats);
        value["share_values"] = std::move(shares);
        value["record_hex"] = hex(record, capital_record_size);
    }
    return value;
}

}  // namespace tdx::corporate_detail

namespace tdx {

using namespace corporate_detail;
Json parse_finance_batch_payload(const Bytes& payload, bool include_raw) {
    if (payload.size() < 2) throw Error("finance payload is shorter than two bytes");
    const auto count = read_u16_le(payload.data());
    const std::size_t expected = 2 + static_cast<std::size_t>(count) * finance_record_size;
    if (payload.size() != expected)
        throw Error("finance payload length mismatch: expected " + std::to_string(expected) +
                    ", got " + std::to_string(payload.size()));
    Json records = Json::array();
    for (std::size_t index = 0; index < count; ++index)
        records.push_back(finance_record_json(payload.data() + 2 + index * finance_record_size,
                                              include_raw));
    Json result = Json::object();
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    if (include_raw) result["payload_hex"] = hex(payload);
    return result;
}

Json parse_capital_changes_payload(const Bytes& payload, bool include_raw) {
    if (payload.size() < 11) throw Error("capital changes payload is shorter than 11 bytes");
    const auto block_count = read_u16_le(payload.data());
    const int market_id = payload[2];
    if (market_id < 0 || market_id > 2) throw Error("capital changes header has invalid market");
    const std::string code(reinterpret_cast<const char*>(payload.data() + 3), 6);
    const auto count = read_u16_le(payload.data() + 9);
    const std::size_t expected = 11 + static_cast<std::size_t>(count) * capital_record_size;
    if (payload.size() != expected)
        throw Error("capital changes payload length mismatch: expected " +
                    std::to_string(expected) + ", got " + std::to_string(payload.size()));
    Json records = Json::array();
    for (std::size_t index = 0; index < count; ++index)
        records.push_back(capital_record_json(payload.data() + 11 + index * capital_record_size,
                                              include_raw));
    Json result = Json::object();
    result["security_id"] = security_id(market_id, code);
    result["market"] = market_name(market_id);
    result["market_id"] = market_id;
    result["code"] = code;
    result["block_count"] = static_cast<std::uint64_t>(block_count);
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    if (include_raw) result["payload_hex"] = hex(payload);
    return result;
}

Json parse_special_limits_payload(const Bytes& payload, int start_index, bool include_raw) {
    if (payload.size() < 2) throw Error("special limits payload is shorter than two bytes");
    const auto count = read_u16_le(payload.data());
    const std::size_t expected = 2 + static_cast<std::size_t>(count) * limit_record_size;
    if (payload.size() != expected)
        throw Error("special limits payload length mismatch: expected " +
                    std::to_string(expected) + ", got " + std::to_string(payload.size()));
    Json records = Json::array();
    for (std::size_t index = 0; index < count; ++index) {
        const auto* record = payload.data() + 2 + index * limit_record_size;
        const int market_id = record[0];
        if (market_id < 0 || market_id > 2) throw Error("special limit record has invalid market");
        const auto code_number = read_u32_le(record + 1);
        std::ostringstream code;
        code << std::setw(6) << std::setfill('0') << code_number;
        const auto code_text = code.str();
        const auto upper = finite_float(record + 5, "special upper limit");
        const auto lower = finite_float(record + 9, "special lower limit");
        Json value = Json::object();
        value["index"] = static_cast<std::uint64_t>(start_index + static_cast<int>(index));
        value["security_id"] = security_id(market_id, code_text);
        value["market"] = market_name(market_id);
        value["market_id"] = market_id;
        value["code"] = code_text;
        value["code_number"] = static_cast<std::uint64_t>(code_number);
        value["limit_up_price"] = upper;
        value["limit_down_price"] = lower;
        if (include_raw) value["record_hex"] = hex(record, limit_record_size);
        records.push_back(std::move(value));
    }
    Json result = Json::object();
    result["start_index"] = start_index;
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    if (include_raw) result["payload_hex"] = hex(payload);
    return result;
}

std::optional<SpecialLimitPrice> find_special_limit_price(
    const Json& document, const std::string& requested_market, const std::string& code) {
    if (!document.is_object()) return std::nullopt;
    const auto records = document.as_object().find("records");
    if (records == document.as_object().end() || !records->second.is_array())
        return std::nullopt;
    auto market = lower_ascii(trim(requested_market));
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2") market = "bj";
    for (const auto& row : records->second.as_array()) {
        if (!row.is_object()) continue;
        const auto row_market = row.as_object().find("market");
        const auto row_code = row.as_object().find("code");
        const auto upper = row.as_object().find("limit_up_price");
        const auto lower = row.as_object().find("limit_down_price");
        if (row_market == row.as_object().end() || !row_market->second.is_string() ||
            row_code == row.as_object().end() || !row_code->second.is_string() ||
            upper == row.as_object().end() || !upper->second.is_number() ||
            lower == row.as_object().end() || !lower->second.is_number()) continue;
        if (lower_ascii(row_market->second.as_string()) == market &&
            row_code->second.as_string() == code)
            return SpecialLimitPrice{upper->second.as_number(), lower->second.as_number()};
    }
    return std::nullopt;
}

}  // namespace tdx