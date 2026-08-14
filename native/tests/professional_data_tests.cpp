#include "tdx/professional_data.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void put_u16(tdx::Bytes& output, std::size_t offset, std::uint16_t value) {
    output[offset] = static_cast<std::uint8_t>(value);
    output[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(tdx::Bytes& output, std::size_t offset, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output[offset + static_cast<std::size_t>(shift / 8)] =
            static_cast<std::uint8_t>(value >> shift);
}

void put_f32(tdx::Bytes& output, std::size_t offset, float value) {
    std::uint32_t raw = 0; std::memcpy(&raw, &value, sizeof(value)); put_u32(output, offset, raw);
}

tdx::Bytes finance_fixture() {
    constexpr std::size_t header = 20, index_size = 11, fields = 3, data_size = fields * 4;
    tdx::Bytes result(header + 2 * index_size + 2 * data_size, 0);
    put_u16(result, 0, 1); put_u32(result, 2, 20260331); put_u16(result, 6, 2);
    put_u16(result, 10, index_size); put_u32(result, 12, data_size);
    const std::string first = "000001", second = "600000";
    std::copy(first.begin(), first.end(), result.begin() + 20); result[26] = 0;
    put_u32(result, 27, static_cast<std::uint32_t>(header + 2 * index_size));
    std::copy(second.begin(), second.end(), result.begin() + 31); result[37] = 1;
    put_u32(result, 38, static_cast<std::uint32_t>(header + 2 * index_size + data_size));
    put_f32(result, 42, 1.25F); put_f32(result, 46, -2.5F);
    put_f32(result, 50, std::numeric_limits<float>::quiet_NaN());
    put_f32(result, 54, 3.0F); put_f32(result, 58, 4.0F); put_f32(result, 62, 5.0F);
    return result;
}

void append_trading(tdx::Bytes& output, int id, std::uint32_t date, float one, float two) {
    const auto offset = output.size(); output.resize(offset + 13);
    output[offset] = static_cast<std::uint8_t>(id); put_u32(output, offset + 1, date);
    put_f32(output, offset + 5, one); put_f32(output, offset + 9, two);
}

}  // namespace

int main() {
    try {
        const auto finance = tdx::parse_professional_finance_data(finance_fixture(), "fixture");
        require(finance.report_date == 20260331, "finance report date");
        require(finance.field_count == 3 && finance.records.size() == 2, "finance dimensions");
        const auto& ping_an = finance.records.at({0, "000001"});
        require(std::abs(*ping_an.fields[0] - 1.25) < 1e-6, "finance first field");
        require(!ping_an.fields[2], "non-finite finance field must be null");
        const auto finance_json = tdx::professional_finance_document(finance, 0, "000001", {0, 1, 3});
        require(finance_json.at("found").as_bool(), "finance JSON found");
        require(finance_json.at("fields").size() == 3, "finance JSON filter");
        require(finance_json.at("fields").as_array()[0].at("value").as_number() == 20260331,
                "FINVALUE(0) report date");

        tdx::ProfessionalFinanceRecord growth;
        growth.fields.resize(184);
        growth.fields[182] = -12.5;  // FN183: revenue YoY
        growth.fields[183] = 0.0;    // FN184: net-profit YoY
        require(tdx::professional_finance_growth_value(growth, 44) == -12.5,
                "FINANCE(44) maps to FN183 and preserves negative growth");
        require(tdx::professional_finance_growth_value(growth, 43) == 0.0,
                "FINANCE(43) maps to FN184 and preserves zero growth");
        growth.fields[183] = std::numeric_limits<double>::quiet_NaN();
        require(!tdx::professional_finance_growth_value(growth, 43),
                "non-finite professional growth must stay unavailable");
        require(!tdx::professional_finance_growth_value(growth, 42),
                "unrelated FINANCE IDs must not alias growth fields");

        tdx::Bytes payload;
        append_trading(payload, 3, 20260102, 10.0F, 20.0F);
        append_trading(payload, 3, 20260105, 11.0F, 21.0F);
        append_trading(payload, 44, 20260105, 88.0F, 0.0F);
        const auto records = tdx::parse_professional_trading_data(payload);
        require(records.size() == 3 && records[0].id == 3, "trading parse");
        const std::vector<std::uint32_t> dates{20260102, 20260103, 20260105};
        const auto raw = tdx::professional_trading_series(records, 3, 1, 0, dates);
        require(raw[0] && !raw[1] && raw[2] && *raw[2] == 11.0, "TYPE=0 exact dates");
        const auto smooth = tdx::professional_trading_series(records, 3, 2, 1, dates);
        require(smooth[1] && *smooth[1] == 20.0 && *smooth[2] == 21.0, "TYPE=1 forward fill");
        const auto zero = tdx::professional_trading_series(records, 3, 1, 2, dates);
        require(zero[1] && *zero[1] == 0.0, "TYPE=2 missing to zero");
        require(tdx::professional_trading_one(records, 3, 2, 2026, 105) == 21.0,
                "JYONE exact date lookup");
        require(tdx::professional_trading_one(records, 3, 1, 0, 0) == 11.0,
                "JYONE zero selects latest matching field record");
        require(tdx::professional_trading_one(records, 3, 1, 0, 1) == 10.0,
                "JYONE sub-10000 selector is a reverse ordinal");
        require(!tdx::professional_trading_one(records, 3, 1, 2026, 103),
                "JYONE complete dates require an exact record");
        const std::vector<std::pair<std::uint32_t, std::optional<double>>> finance_points{
            {20241231, 1.0}, {20250331, 2.0}, {20250630, 3.0},
            {20250930, 4.0}, {20251231, 5.0}, {20260331, 6.0}};
        require(tdx::professional_finance_one(finance_points, 2025, 501) == 2.0,
                "FINONE full date remains within its quarter window");
        require(tdx::professional_finance_one(finance_points, 25, 630) == 3.0,
                "FINONE two-digit years follow the TCalc conversion");
        require(tdx::professional_finance_one(finance_points, 0, 0) == 6.0,
                "FINONE zero date selects latest");
        require(tdx::professional_finance_one(finance_points, 1, 0) == 2.0,
                "FINONE year-only selector means years back");
        require(tdx::professional_finance_one(finance_points, 0, 1) == 5.0,
                "FINONE small MMDD selector means quarters back");
        require(tdx::professional_finance_one(finance_points, 0, 630) == 3.0,
                "FINONE standalone MMDD selects its latest prior occurrence");
        const auto document = tdx::professional_trading_document(
            records, "stock", "SZ000001", {3}, 0, 99999999, 100, true);
        require(document.at("fields").size() == 1 && document.at("history").size() == 2,
                "trading JSON filter");
        std::cout << "professional data tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
