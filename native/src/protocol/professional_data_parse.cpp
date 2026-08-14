#include "professional_data_internal.hpp"

#include <cmath>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace tdx {

using namespace professional_data_detail;

ProfessionalFinanceData parse_professional_finance_data(const Bytes& payload,
                                                         std::string source) {
    if (payload.size() < finance_header_size) throw Error("professional finance file is truncated");
    const auto version = read_u16_le(payload.data());
    const auto report_date = read_u32_le(payload.data() + 2);
    const auto record_count = read_u16_le(payload.data() + 6);
    const auto index_size = read_u16_le(payload.data() + 10);
    const auto data_size = read_u32_le(payload.data() + 12);
    if (version != 1 || !record_count || index_size != finance_index_size ||
        !data_size || data_size % sizeof(float) != 0 || data_size > 16 * 1024)
        throw Error("professional finance header is unsupported");
    const auto index_end = finance_header_size + static_cast<std::size_t>(record_count) * index_size;
    if (index_end > payload.size()) throw Error("professional finance index exceeds file bounds");
    ProfessionalFinanceData result;
    result.report_date = report_date; result.field_count = data_size / sizeof(float);
    result.source = std::move(source);
    for (std::size_t index = 0; index < record_count; ++index) {
        const auto offset = finance_header_size + index * index_size;
        const std::string code(reinterpret_cast<const char*>(payload.data() + offset), 6);
        // The byte after the six-byte code is reserved in current official
        // packages (it is zero for SZ/SH/BJ alike), so infer the exchange from
        // the code exactly as the terminal does elsewhere.
        const int market_id = inferred_market_id(code);
        const auto data_offset = read_u32_le(payload.data() + offset + 7);
        if (!valid_code(code) || data_offset < index_end ||
            data_offset > payload.size() || data_size > payload.size() - data_offset)
            throw Error("professional finance index record is invalid");
        ProfessionalFinanceRecord record; record.market_id = market_id; record.code = code;
        record.fields.reserve(result.field_count);
        for (std::size_t field = 0; field < result.field_count; ++field)
            record.fields.push_back(finite_float(payload.data() + data_offset + field * 4));
        // A small number of official quarter packages repeat a code. The
        // terminal consumes the later index entry; mirror that deterministic
        // last-record-wins behavior instead of rejecting an otherwise valid package.
        result.records[std::make_pair(market_id, code)] = std::move(record);
    }
    return result;
}

std::vector<ProfessionalTradingRecord> parse_professional_trading_data(const Bytes& payload) {
    if (payload.empty() || payload.size() % trading_record_size)
        throw Error("professional trading file size is not a multiple of 13 bytes");
    std::vector<ProfessionalTradingRecord> result;
    result.reserve(payload.size() / trading_record_size);
    for (std::size_t offset = 0; offset < payload.size(); offset += trading_record_size) {
        const auto date = read_u32_le(payload.data() + offset + 1);
        if (date) {
            const auto month = date / 100 % 100, day = date % 100;
            if (date / 10000 < 1900 || date / 10000 > 2200 || month < 1 || month > 12 ||
                day < 1 || day > 31) throw Error("professional trading record has invalid date");
        }
        result.push_back({payload[offset], date, finite_float(payload.data() + offset + 5),
                          finite_float(payload.data() + offset + 9)});
    }
    return result;
}

std::optional<double> professional_finance_growth_value(
    const ProfessionalFinanceRecord& record, int finance_id) {
    // Official field definitions: FN183 = revenue YoY (%),
    // FN184 = net-profit YoY (%).
    const int field_id = finance_id == 43 ? 184 : finance_id == 44 ? 183 : 0;
    if (!field_id || record.fields.size() < static_cast<std::size_t>(field_id))
        return std::nullopt;
    const auto value = record.fields[static_cast<std::size_t>(field_id - 1)];
    if (!value || !std::isfinite(*value)) return std::nullopt;
    return value;
}

std::vector<std::optional<double>> professional_trading_series(
    const std::vector<ProfessionalTradingRecord>& records, int id, int field, int type,
    const std::vector<std::uint32_t>& dates) {
    if (id < 0 || id > 255) throw Error("professional trading ID must be in 0..255");
    if (field < 1 || field > 2) throw Error("professional trading field must be 1 or 2");
    if (type < 0 || type > 2) throw Error("professional trading TYPE must be 0, 1 or 2");
    std::map<std::uint32_t, std::optional<double>> by_date;
    for (const auto& record : records) if (record.id == id && record.date)
        by_date[record.date] = field == 1 ? record.value1 : record.value2;
    std::vector<std::optional<double>> result; result.reserve(dates.size());
    std::optional<double> last;
    for (const auto date : dates) {
        const auto found = by_date.find(date);
        if (found != by_date.end()) last = found->second;
        if (type == 1) result.push_back(last);
        else if (found != by_date.end()) result.push_back(found->second);
        else if (type == 2) result.push_back(0.0);
        else result.push_back(std::nullopt);
    }
    return result;
}

namespace {

int tcalc_one_date(int year, int mmdd) {
    if (year < 0 || year > 9999 || mmdd < 0 || mmdd > 9999)
        throw Error("professional one-point date arguments must be in 0..9999");
    if (year > 0 && mmdd > 0 && year < 1900)
        year += year <= 90 ? 2000 : 1900;
    return year * 10000 + mmdd;
}

int finone_partial_date(std::uint32_t latest, int selector) {
    const int latest_year = static_cast<int>(latest / 10000);
    const int latest_month = static_cast<int>(latest % 10000 / 100);
    if (selector > 300) {
        const int target_month = selector / 100;
        return selector + 10000 * (latest_year - (latest_month < target_month ? 1 : 0));
    }
    int latest_quarter = 0;
    if (latest_month == 3) latest_quarter = 1;
    else if (latest_month == 6) latest_quarter = 2;
    else if (latest_month == 9) latest_quarter = 3;
    else if (latest_month == 12) latest_quarter = 4;
    const int target_year =
        (4 * latest_year - selector + latest_quarter - 7601) / 4 + 1900;
    const int target_quarter =
        (latest_quarter + 4 * latest_year - 7600 - selector) % 4;
    const int target_mmdd = target_quarter == 1 ? 331 :
                            target_quarter == 2 ? 630 :
                            target_quarter == 3 ? 930 : 1231;
    return target_year * 10000 + target_mmdd;
}

}  // namespace

std::optional<double> professional_trading_one(
    const std::vector<ProfessionalTradingRecord>& records,
    int id, int field, int year, int mmdd) {
    if (id < 0 || id > 255) throw Error("professional trading ID must be in 0..255");
    if (field < 1 || field > 2) throw Error("professional trading field must be 1 or 2");
    const int selector = tcalc_one_date(year, mmdd);
    int ordinal = 0;
    for (auto record = records.rbegin(); record != records.rend(); ++record) {
        if (record->id != id) continue;
        if (selector >= 10000) {
            if (record->date != static_cast<std::uint32_t>(selector)) continue;
        } else if (ordinal++ != selector) {
            continue;
        }
        return field == 1 ? record->value1 : record->value2;
    }
    return std::nullopt;
}

std::optional<double> professional_finance_one(
    const std::vector<std::pair<std::uint32_t, std::optional<double>>>& records,
    int year, int mmdd) {
    if (records.empty()) return std::nullopt;
    const int selector = tcalc_one_date(year, mmdd);
    int lower_bound = 0, upper_bound = selector;
    if (year > 0 && mmdd > 0) {
        const int date_year = selector / 10000, date_mmdd = selector % 10000;
        lower_bound = date_mmdd < 331 ? (date_year - 1) * 10000 + 1231 :
                      date_mmdd < 630 ? date_year * 10000 + 331 :
                      date_mmdd < 930 ? date_year * 10000 + 630 :
                      date_mmdd < 1231 ? date_year * 10000 + 930 :
                      date_year * 10000 + 1231;
    } else if (selector == 0) {
        return records.back().second;
    } else if (year > 0) {
        upper_bound = static_cast<int>(records.back().first) - year * 10000;
    } else {
        upper_bound = finone_partial_date(records.back().first, mmdd);
    }
    for (auto record = records.rbegin(); record != records.rend(); ++record) {
        const int date = static_cast<int>(record->first);
        if (lower_bound > 0 && date < lower_bound) continue;
        if (date <= upper_bound) return record->second;
    }
    return std::nullopt;
}

}  // namespace tdx
