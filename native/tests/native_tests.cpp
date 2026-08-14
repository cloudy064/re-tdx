#include "tdx/json.hpp"
#include "tdx/auction.hpp"
#include "tdx/minute.hpp"
#include "tdx/common.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/consensus.hpp"
#include "tdx/funds.hpp"
#include "tdx/hyzt.hpp"
#include "tdx/institution.hpp"
#include "tdx/lhb.hpp"
#include "tdx/market.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/ranking.hpp"
#include "tdx/stats.hpp"
#include "tdx/trades.hpp"
#include "tdx/unlocks.hpp"
#include "tdx/utf8.hpp"
#include "tdx/time.hpp"
#include "tdx/valuation.hpp"

#include "minute_download_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <zlib.h>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void append_u16(tdx::Bytes& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
}
void append_u32(tdx::Bytes& data, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        data.push_back(static_cast<std::uint8_t>(value >> shift));
}
void append_float(tdx::Bytes& data, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(data, bits);
}
tdx::Bytes encode_varint(std::int64_t value) {
    const bool negative = value < 0;
    std::uint64_t remaining = static_cast<std::uint64_t>(negative ? -value : value);
    tdx::Bytes output{static_cast<std::uint8_t>(remaining & 0x3F)};
    remaining >>= 6;
    if (negative) output[0] |= 0x40;
    if (remaining) output[0] |= 0x80;
    while (remaining) {
        auto byte = static_cast<std::uint8_t>(remaining & 0x7F);
        remaining >>= 7;
        if (remaining) byte |= 0x80;
        output.push_back(byte);
    }
    return output;
}
void append_varint(tdx::Bytes& output, std::int64_t value) {
    const auto encoded = encode_varint(value);
    output.insert(output.end(), encoded.begin(), encoded.end());
}

tdx::Bytes make_stored_zip(
    const std::vector<std::pair<std::string, tdx::Bytes>>& files) {
    struct Entry { std::string name; tdx::Bytes data; std::uint32_t crc{}, offset{}; };
    std::vector<Entry> entries;
    tdx::Bytes output;
    for (const auto& [name, data] : files) {
        uLong crc = crc32(0L, Z_NULL, 0);
        if (!data.empty()) crc = crc32(crc, data.data(), static_cast<uInt>(data.size()));
        Entry entry{name, data, static_cast<std::uint32_t>(crc),
                    static_cast<std::uint32_t>(output.size())};
        append_u32(output, 0x04034B50);
        append_u16(output, 20);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u32(output, entry.crc);
        append_u32(output, static_cast<std::uint32_t>(data.size()));
        append_u32(output, static_cast<std::uint32_t>(data.size()));
        append_u16(output, static_cast<std::uint16_t>(name.size()));
        append_u16(output, 0);
        output.insert(output.end(), name.begin(), name.end());
        output.insert(output.end(), data.begin(), data.end());
        entries.push_back(std::move(entry));
    }
    const auto central_offset = static_cast<std::uint32_t>(output.size());
    for (const auto& entry : entries) {
        append_u32(output, 0x02014B50);
        append_u16(output, 20);
        append_u16(output, 20);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u32(output, entry.crc);
        append_u32(output, static_cast<std::uint32_t>(entry.data.size()));
        append_u32(output, static_cast<std::uint32_t>(entry.data.size()));
        append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u32(output, 0);
        append_u32(output, entry.offset);
        output.insert(output.end(), entry.name.begin(), entry.name.end());
    }
    const auto central_size = static_cast<std::uint32_t>(output.size()) - central_offset;
    append_u32(output, 0x06054B50);
    append_u16(output, 0);
    append_u16(output, 0);
    append_u16(output, static_cast<std::uint16_t>(entries.size()));
    append_u16(output, static_cast<std::uint16_t>(entries.size()));
    append_u32(output, central_size);
    append_u32(output, central_offset);
    append_u16(output, 0);
    return output;
}

std::string joined_fields(std::vector<std::string> fields) {
    std::string result;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index) result.push_back('|');
        result += fields[index];
    }
    return result;
}

tdx::Bytes text_bytes(const std::string& value) {
    return tdx::Bytes(value.begin(), value.end());
}
std::string bytes_hex(const tdx::Bytes& value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(value.size() * 2, '0');
    for (std::size_t index = 0; index < value.size(); ++index) {
        result[index * 2] = digits[value[index] >> 4];
        result[index * 2 + 1] = digits[value[index] & 15];
    }
    return result;
}
}

int main() {
    try {
        namespace fs = std::filesystem;
        require(tdx::utf8_prefix(std::string(95, 'a') + "\xe4\xb8\xad", 96) ==
                    std::string(95, 'a') &&
                tdx::utf8_prefix("\xf0\x9f\x98\x80", 4) == "\xf0\x9f\x98\x80" &&
                tdx::utf8_prefix("\xf0\x9f\x98\x80", 3).empty() &&
                tdx::utf8_prefix("ok\xc0\x80tail", 64) == "ok" &&
                tdx::utf8_prefix("ok\xed\xa0\x80tail", 64) == "ok" &&
                tdx::utf8_prefix("ok\xf4\x90\x80\x80tail", 64) == "ok",
                "UTF-8 prefix preserves complete code points and rejects invalid sequences");
        const auto timestamp = tdx::local_timestamp_text();
        require(timestamp.size() == 24 && timestamp[4] == '-' &&
                    timestamp[7] == '-' && timestamp[10] == 'T' &&
                    timestamp[13] == ':' && timestamp[16] == ':' &&
                    (timestamp[19] == '+' || timestamp[19] == '-') &&
                    std::all_of(timestamp.begin(), timestamp.end(),
                        [index = std::size_t{0}](char value) mutable {
                            const bool separator = index == 4 || index == 7 ||
                                index == 10 || index == 13 || index == 16 || index == 19;
                            ++index;
                            return separator || (value >= '0' && value <= '9');
                        }) &&
                    !tdx::utf8_to_wide(timestamp).empty(),
                "local timestamp uses an ASCII numeric UTC offset");
        const auto hyzt_test_root = fs::temp_directory_path() /
            ("tdx-hyzt-valuation-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto hyzt_test_file =
            hyzt_test_root / "list" / "func_gx_hyzt101_1.jsn";
        const auto hyzt_document = [](std::string_view pe) {
            return std::string(
                R"([{"colheader":["$ZQDM","$SC","TDXHY","$ZQDM1","$SC1","hyPE","hyPB","$S_ZQDM","sszt"],"data":[["000001","0","Bank","880471","1",")") +
                std::string(pe) +
                R"(","0.5302","0|000001,1|600000",""],["600000","1","Bank","880471","1",")" +
                std::string(pe) +
                R"(","0.5302","0|000001,1|600000",""]]}])";
        };
        tdx::atomic_write_text(hyzt_test_file, hyzt_document("5.2105"));
        const auto hyzt_catalog =
            tdx::load_industry_valuation_catalog(hyzt_test_root);
        const auto hyzt_cached =
            tdx::load_industry_valuation_catalog(hyzt_test_root);
        require(hyzt_catalog == hyzt_cached && hyzt_catalog->row_count == 2 &&
                    hyzt_catalog->records.size() == 1 &&
                    hyzt_catalog->records.at("880471").member_count == 2 &&
                    std::abs(hyzt_catalog->records.at("880471").pe.value_or(0.0) -
                             5.2105) < 1e-12 &&
                    std::abs(hyzt_catalog->records.at("880471").pb_mrq.value_or(0.0) -
                             0.5302) < 1e-12,
                "HYZT industry valuation catalog parses and caches hyPE/hyPB");
        tdx::atomic_write_text(hyzt_test_file, hyzt_document("16.25000"));
        const auto hyzt_refreshed =
            tdx::load_industry_valuation_catalog(hyzt_test_root);
        require(hyzt_refreshed != hyzt_catalog &&
                    std::abs(hyzt_refreshed->records.at("880471").pe.value_or(0.0) -
                             16.25) < 1e-12,
                "HYZT valuation cache invalidates after an atomic resource refresh");
        fs::remove_all(hyzt_test_root);

        require(tdx::market_price_divisor_for_code("204001") == 100 &&
                    tdx::market_price_divisor_for_code("131810") == 100,
                "reverse repo snapshot price divisor");
        require(tdx::market_price_divisor_for_code("510300") == 10 &&
                    tdx::market_price_divisor_for_code("000001") == 1,
                "existing snapshot price divisors");
        tdx::Bytes speed_payload{1, 6, 1, 0, 0, '0', '0', '0', '0', '0', '1'};
        append_u16(speed_payload, 4573);
        for (const auto value : {1119, 8, 4, 7, -9, 15332782, -1119, 882977, 5466})
            append_varint(speed_payload, value);
        append_u32(speed_payload, 0x4E6B2B7A);
        for (const auto value : {491420, 391557, 0, 50872})
            append_varint(speed_payload, value);
        const std::int64_t speed_levels[5][4] = {
            {-1, 0, 1286, 147}, {-2, 1, 1374, 3518}, {-3, 2, 1346, 2601},
            {-4, 3, 1770, 3719}, {-5, 4, 1537, 3262},
        };
        for (const auto& level : speed_levels)
            for (const auto value : level) append_varint(speed_payload, value);
        append_u16(speed_payload, 0x56);
        speed_payload.push_back(0);
        append_varint(speed_payload, 0);
        append_varint(speed_payload, 0);
        append_varint(speed_payload, 0);
        append_u16(speed_payload, 123);
        append_u16(speed_payload, 4573);
        const auto speed_document = tdx::decode_market_speed_payload(speed_payload, 1);
        const auto& speed = speed_document.at("records").as_array().front();
        require(std::fabs(speed.at("rise_speed_pct").as_number() - 1.23) < 1e-12 &&
                    std::fabs(speed.at("buy_levels").as_array()[1].at("price").as_number() -
                              11.17) < 1e-12 &&
                    speed.at("sell_levels").as_array()[2].at("volume_hand").as_number() == 2601,
                "0x053E rise-speed and five-level decoder");
        tdx::Bytes iopv_payload{1, 6, 1, 0, 1, '5', '1', '0', '3', '0', '0'};
        append_u16(iopv_payload, 4573);
        // Current=4.000, auxiliary pseudo-price=40.100, hence IOPV=4.0100.
        for (const auto value : {4000, 0, 0, 0, 0, 15332782, 36100, 882977, 5466})
            append_varint(iopv_payload, value);
        append_u32(iopv_payload, 0x4E6B2B7A);
        for (const auto value : {491420, 391557, 0, 50872})
            append_varint(iopv_payload, value);
        for (std::size_t level = 0; level < 5; ++level) {
            append_varint(iopv_payload, -static_cast<std::int64_t>(level + 1));
            append_varint(iopv_payload, static_cast<std::int64_t>(level));
            append_varint(iopv_payload, 1000 + static_cast<std::int64_t>(level));
            append_varint(iopv_payload, 2000 + static_cast<std::int64_t>(level));
        }
        append_u16(iopv_payload, 0x56);
        iopv_payload.push_back(0);
        for (int index = 0; index < 3; ++index) append_varint(iopv_payload, 0);
        append_u16(iopv_payload, 0);
        append_u16(iopv_payload, 4573);
        const auto iopv_document = tdx::decode_market_speed_payload(iopv_payload, 1);
        const auto& iopv = iopv_document.at("records").as_array().front();
        require(std::fabs(iopv.at("fund_iopv").as_number() - 4.01) < 1e-12 &&
                    iopv.at("auxiliary_price_delta_raw").as_number() == 36100.0,
                "TdxW ETF IOPV derivation from quote-core auxiliary price");
        tdx::Bytes total_amount_payload;
        append_u16(total_amount_payload, 1);
        total_amount_payload.push_back(1);
        total_amount_payload.insert(total_amount_payload.end(),
                                    {'9', '9', '9', '9', '9', '7'});
        append_u16(total_amount_payload, 126);
        for (const auto value : {28131, -18, -35, 15, -35})
            append_varint(total_amount_payload, value);
        append_u32(total_amount_payload, 93736);
        for (const auto value : {65, 16649, 22}) append_varint(total_amount_payload, value);
        append_u32(total_amount_payload, 0);
        for (const auto value : {8906, 7743, 6048, 280870})
            append_varint(total_amount_payload, value);
        const std::int64_t total_amount_pairs[5][4] = {
            {76, 34, 16, 19}, {12, 18, 0, 0}, {0, 0, 21, 17},
            {122, 92, 0, 0}, {51, 79, 0, 0},
        };
        for (const auto& level : total_amount_pairs)
            for (const auto value : level) append_varint(total_amount_payload, value);
        for (auto& byte : total_amount_payload) byte ^= 0x93;
        const auto total_amount_document = tdx::decode_market_depth_payload(
            total_amount_payload, {"sh:999997"});
        const auto& total_amount = total_amount_document.at("records").as_array().front()
            .at("market_amount_summary");
        require(total_amount.at("buy1_total").as_number() == 110 &&
                    total_amount.at("sell1_total").as_number() == 30 &&
                    total_amount.at("buy_depth_total").as_number() == 214 &&
                    total_amount.at("sell_depth_total").as_number() == 130 &&
                    total_amount.at("values_by_selector").as_array()[5].is_null(),
                "SH999997 public-L1 total-market amount sentinel decoder");
        const auto json = tdx::Json::parse("{\"中文\":[1,true,null,\"x\"]}");
        require(json.at("中文").as_array().size() == 4, "JSON array size");
        require(tdx::Json::parse(json.dump()).at("中文").as_array()[1].as_bool(),
                "JSON roundtrip");

        const std::uint16_t encoded_date = static_cast<std::uint16_t>(
            (2026 - 2004) * 2048 + 8 * 100 + 2);
        tdx::Bytes lc1;
        append_u16(lc1, encoded_date);
        append_u16(lc1, 9 * 60 + 31);
        append_float(lc1, 10.0f);
        append_float(lc1, 10.5f);
        append_float(lc1, 9.9f);
        append_float(lc1, 10.2f);
        append_float(lc1, 123456.0f);
        append_u32(lc1, 789);
        append_u16(lc1, 0);
        append_u16(lc1, 0);
        const auto bars = tdx::parse_lc1(lc1);
        require(bars.size() == 1, "LC1 record count");
        require(bars[0].date == 20260802, "LC1 date");
        require(bars[0].hour == 9 && bars[0].minute == 31, "LC1 time");
        require(std::fabs(bars[0].close - 10.2f) < 0.0001f, "LC1 close");
        require(bars[0].volume == 789, "LC1 volume");

        const auto request = tdx::build_kline_request_data(1, "880471", 0, 800);
        require(request.size() == 42, "K-line request size");
        require(request[0] == 1 && request[2] == '8', "K-line request fields");
        require(request[14] == 0x20 && request[15] == 0x03, "K-line request count");
        const auto daily_request = tdx::build_kline_request_data(0, "000001", 0, 120, 4);
        require(daily_request[8] == 4 && daily_request[9] == 0,
                "daily K-line request period");

        tdx::Bytes wire;
        append_u16(wire, 2);
        for (int record = 0; record < 2; ++record) {
            append_u16(wire, static_cast<std::uint16_t>((2026 - 2004) * 2048 + 7 * 100 + 31));
            append_u16(wire, static_cast<std::uint16_t>(571 + record));
            append_varint(wire, record == 0 ? 3'500'000 : 10);
            append_varint(wire, record == 0 ? 100 : -20);
            append_varint(wire, record == 0 ? 200 : 40);
            append_varint(wire, record == 0 ? -50 : -30);
            append_u32(wire, 0);
            append_u32(wire, 0);
            append_u16(wire, static_cast<std::uint16_t>(22 - record));
            append_u16(wire, static_cast<std::uint16_t>(15 + record));
        }
        const auto online_bars = tdx::parse_kline_payload(wire, true);
        require(online_bars.size() == 2, "wire K-line record count");
        require(std::fabs(online_bars[0].close - 3500.1f) < 0.001f,
                "wire K-line first close");
        require(std::fabs(online_bars[1].open - 3500.11f) < 0.001f,
                "wire K-line previous-close base");
        require(online_bars[1].extra_1 == 21 && online_bars[1].extra_2 == 16,
                "wire K-line breadth fields");
        const auto roundtrip = tdx::parse_lc1(tdx::pack_lc1(online_bars));
        require(roundtrip.size() == 2 && roundtrip[0].extra_1 == 22,
                "downloaded LC1 roundtrip");

        tdx::Bytes daily_wire;
        append_u16(daily_wire, 2);
        for (int record = 0; record < 2; ++record) {
            append_u32(daily_wire, static_cast<std::uint32_t>(20260801 + record));
            append_varint(daily_wire, record == 0 ? 1'000'000 : 100);
            append_varint(daily_wire, record == 0 ? 1'000 : -50);
            append_varint(daily_wire, record == 0 ? 2'000 : 500);
            append_varint(daily_wire, record == 0 ? -1'000 : -500);
            append_u32(daily_wire, 0);
            append_u32(daily_wire, 0);
        }
        const auto daily_bars = tdx::parse_kline_payload(daily_wire, false, 4);
        require(daily_bars.size() == 2, "daily K-line record count");
        require(daily_bars[0].date == 20260801 && daily_bars[1].date == 20260802,
                "daily K-line uint32 dates");
        require(daily_bars[0].hour == 15 && daily_bars[0].minute == 0,
                "daily K-line display time");

        std::vector<tdx::MinuteBar> paged_bars(5);
        paged_bars[0].date = 20260810;
        paged_bars[0].hour = 9;
        paged_bars[0].minute = 31;
        paged_bars[0].close = 10.0F;
        paged_bars[1].date = 20260811;
        paged_bars[1].hour = 15;
        paged_bars[1].minute = 0;
        paged_bars[2].date = 20260803;
        paged_bars[2].hour = 10;
        paged_bars[2].minute = 51;
        paged_bars[3].date = 20260810;
        paged_bars[3].hour = 9;
        paged_bars[3].minute = 31;
        paged_bars[3].close = 10.1F;
        paged_bars[4].date = 20260806;
        paged_bars[4].hour = 13;
        paged_bars[4].minute = 40;
        tdx::minute_download_detail::sort_bars_chronologically(paged_bars);
        require(paged_bars[0].date == 20260803 &&
                    paged_bars[1].date == 20260806 &&
                    paged_bars[2].date == 20260810 &&
                    paged_bars[3].date == 20260810 &&
                    paged_bars[4].date == 20260811,
                "multi-page K-lines must be chronological across page boundaries");
        require(std::fabs(paged_bars[2].close - 10.0F) < 0.0001F &&
                    std::fabs(paged_bars[3].close - 10.1F) < 0.0001F,
                "chronological normalization must remain stable for equal timestamps");

        const auto expansion_request = tdx::build_expansion_kline_request_data(
            47, "IFL9", 1234, 2, 4);
        require(expansion_request.size() == 20, "expansion K-line request size");
        require(expansion_request[0] == 47 && expansion_request[1] == 'I' &&
                expansion_request[4] == '9', "expansion K-line security fields");
        require(expansion_request[10] == 4 && expansion_request[12] == 1,
                "expansion K-line period and flag");
        require(tdx::read_u32_le(expansion_request.data() + 14) == 1234 &&
                tdx::read_u16_le(expansion_request.data() + 18) == 2,
                "expansion K-line paging fields");
        const auto option_request = tdx::build_expansion_kline_request_data(
            5, "A 8X06SH", 0, 1, 4);
        require(option_request[1] == 'A' && option_request[2] == ' ' &&
                option_request[8] == 'H',
                "expansion option wire codes preserve their internal space");
        require(tdx::valid_expansion_code("A 8X06SH") &&
                !tdx::valid_expansion_code(" A8X06SH") &&
                !tdx::valid_expansion_code("A8X06SH "),
                "expansion option code validation");

        tdx::Bytes expansion_wire(18, 0);
        append_u16(expansion_wire, 2);
        for (int record = 0; record < 2; ++record) {
            append_u32(expansion_wire, static_cast<std::uint32_t>(20260801 + record));
            append_float(expansion_wire, 3500.0f + record);
            append_float(expansion_wire, 3520.0f + record);
            append_float(expansion_wire, 3490.0f + record);
            append_float(expansion_wire, 3510.0f + record);
            append_u32(expansion_wire, static_cast<std::uint32_t>(123456 + record));
            append_u32(expansion_wire, static_cast<std::uint32_t>(7890 + record));
            append_float(expansion_wire, 3508.0f + record);
        }
        const auto expansion_bars = tdx::parse_expansion_kline_payload(expansion_wire, 4);
        require(expansion_bars.size() == 2 && expansion_bars[0].date == 20260801,
                "expansion K-line record count and date");
        require(expansion_bars[0].open_interest == 123456 &&
                expansion_bars[1].volume == 7891,
                "expansion K-line position and volume");
        require(expansion_bars[0].has_expansion_fields &&
                !expansion_bars[0].amount_available &&
                std::fabs(expansion_bars[0].auxiliary_price - 3508.0f) < 0.001f,
                "expansion K-line auxiliary fields");

        tdx::Bytes instrument_count_payload(23, 0);
        std::copy_n(reinterpret_cast<const std::uint8_t*>("TDX_DS"), 6,
                    instrument_count_payload.begin());
        instrument_count_payload[19] = 0x8A;
        instrument_count_payload[20] = 0x2C;
        require(tdx::parse_expansion_instrument_count_payload(instrument_count_payload) == 11402,
                "expansion instrument count payload");

        tdx::Bytes instrument_info_payload(6 + 64, 0);
        instrument_info_payload[4] = 1;
        auto* instrument_row = instrument_info_payload.data() + 6;
        instrument_row[0] = 8;
        instrument_row[1] = 47;
        std::copy_n(reinterpret_cast<const std::uint8_t*>("IFL9"), 4, instrument_row + 5);
        std::copy_n(reinterpret_cast<const std::uint8_t*>("IF main"), 7, instrument_row + 14);
        std::copy_n(reinterpret_cast<const std::uint8_t*>("CFFEX"), 5, instrument_row + 31);
        instrument_row[56] = 0xC8;
        const auto instruments = tdx::parse_expansion_instrument_info_payload(
            instrument_info_payload);
        require(instruments.size() == 1 && instruments[0].market_id == 47 &&
                instruments[0].category == 8 && instruments[0].code == "IFL9" &&
                instruments[0].name == "IF main" && instruments[0].description == "CFFEX" &&
                instruments[0].contract_multiplier == 200,
                "expansion instrument info payload");

        tdx::Bytes quote_payload(150, 0);
        quote_payload[0] = 29;
        std::copy_n(reinterpret_cast<const std::uint8_t*>("A2609"), 5,
                    quote_payload.begin() + 1);
        const auto write_u32_at = [&](std::size_t offset, std::uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8)
                quote_payload[offset + static_cast<std::size_t>(shift / 8)] =
                    static_cast<std::uint8_t>(value >> shift);
        };
        const auto write_float_at = [&](std::size_t offset, float value) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            write_u32_at(offset, bits);
        };
        write_float_at(14, 4700.0f); write_float_at(18, 4710.0f);
        write_float_at(22, 4750.0f); write_float_at(26, 4690.0f);
        write_float_at(30, 4724.0f);
        write_u32_at(34, 1200); write_u32_at(42, 119938);
        write_u32_at(46, 8); write_u32_at(54, 50000);
        write_u32_at(58, 69938); write_u32_at(66, 161162);
        for (std::size_t level = 0; level < 5; ++level) {
            write_float_at(70 + level * 4, 4723.0f - static_cast<float>(level));
            write_u32_at(90 + level * 4, static_cast<std::uint32_t>(10 + level));
            write_float_at(110 + level * 4, 4725.0f + static_cast<float>(level));
            write_u32_at(130 + level * 4, static_cast<std::uint32_t>(20 + level));
        }
        const auto quote = tdx::parse_expansion_quote_payload(quote_payload);
        require(quote.at("market_id").as_number() == 29 &&
                quote.at("code").as_string() == "A2609" &&
                quote.at("price").as_number() == 4724.0 &&
                quote.at("open_interest").as_number() == 161162,
                "expansion quote identity and primary fields");
        require(quote.at("bids").as_array()[0].at("price").as_number() == 4723.0 &&
                quote.at("asks").as_array()[4].at("volume").as_number() == 24,
                "expansion quote five-level book");

        const auto minute_request = tdx::build_expansion_minute_request_data(29, "A2609");
        require(minute_request.size() == 10 && minute_request[0] == 29 &&
                minute_request[1] == 'A' && minute_request[5] == '9',
                "expansion minute request fields");
        const auto history_minute_request =
            tdx::build_expansion_history_minute_request_data(20260805, 29, "A2609");
        require(history_minute_request.size() == 14 &&
                tdx::read_u32_le(history_minute_request.data()) == 20260805 &&
                history_minute_request[4] == 29 && history_minute_request[5] == 'A',
                "expansion history minute request fields");
        tdx::Bytes minute_payload(10, 0);
        minute_payload[0] = 29;
        std::copy_n(reinterpret_cast<const std::uint8_t*>("A2609"), 5,
                    minute_payload.begin() + 1);
        append_u16(minute_payload, 1);
        append_u16(minute_payload, 565);
        append_float(minute_payload, 4724.0f);
        append_float(minute_payload, 4720.5f);
        append_u32(minute_payload, 119938);
        append_u32(minute_payload, 161162);
        const auto timeline = tdx::parse_expansion_minute_payload(minute_payload);
        require(timeline.at("wire_count").as_number() == 1 &&
                timeline.at("response_code").as_string() == "A2609" &&
                timeline.at("points").as_array()[0].at("time").as_string() == "09:25" &&
                timeline.at("points").as_array()[0].at("open_interest").as_number() == 161162,
                "expansion minute response fields");

        const auto trade_request =
            tdx::build_expansion_trade_request_data(29, "A2609", 1800, 2);
        require(trade_request.size() == 16 &&
                tdx::read_u32_le(trade_request.data() + 10) == 1800 &&
                tdx::read_u16_le(trade_request.data() + 14) == 2,
                "expansion trade request fields");
        const auto history_trade_request =
            tdx::build_expansion_history_trade_request_data(20260805, 29, "A2609", 3600, 2);
        require(history_trade_request.size() == 20 &&
                tdx::read_u32_le(history_trade_request.data()) == 20260805 &&
                tdx::read_u32_le(history_trade_request.data() + 14) == 3600,
                "expansion history trade request fields");
        tdx::Bytes trade_payload(14, 0);
        trade_payload[0] = 29;
        std::copy_n(reinterpret_cast<const std::uint8_t*>("A2609"), 5,
                    trade_payload.begin() + 1);
        append_u16(trade_payload, 2);
        append_u16(trade_payload, 565); append_u32(trade_payload, 4724000);
        append_u32(trade_payload, 5); append_u32(trade_payload, 5); append_u16(trade_payload, 25);
        append_u16(trade_payload, 566); append_u32(trade_payload, 4725000);
        append_u32(trade_payload, 10); append_u32(trade_payload, 0xFFFFFFFEu);
        append_u16(trade_payload, 10037);
        const auto expansion_trades = tdx::parse_expansion_trade_payload(trade_payload, 29, 1800);
        const auto& expansion_ticks = expansion_trades.at("trades").as_array();
        require(expansion_ticks.size() == 2 &&
                expansion_ticks[0].at("absolute_index").as_number() == 1800 &&
                expansion_ticks[0].at("price").as_number() == 4724.0 &&
                expansion_ticks[0].at("time").as_string() == "09:25:25",
                "expansion trade primary fields");
        require(expansion_ticks[0].at("nature").as_string() == "双开" &&
                expansion_ticks[0].at("side").as_string() == "buy" &&
                expansion_ticks[1].at("nature").as_string() == "多平" &&
                expansion_ticks[1].at("side").as_string() == "sell",
                "expansion trade open-close nature");

        const auto ranking_request = tdx::build_category_request_data(
            6, 0x001C, 3, 80, false, 0);
        require(ranking_request.size() == 18, "ranking request size");
        require(ranking_request == tdx::Bytes({
            0x06, 0x00, 0x1C, 0x00, 0x03, 0x00, 0x50, 0x00, 0x01,
            0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}),
            "ranking request fields");

        auto make_ranking_record = [](int market, const std::string& code,
                                      std::int64_t close_raw) {
            tdx::Bytes output{static_cast<std::uint8_t>(market)};
            output.insert(output.end(), code.begin(), code.end());
            append_u16(output, 7);
            for (const auto value : {close_raw, std::int64_t{-10}, std::int64_t{1},
                                     std::int64_t{2}, std::int64_t{-2},
                                     std::int64_t{150000}, std::int64_t{0},
                                     std::int64_t{1000}, std::int64_t{10}})
                append_varint(output, value);
            append_u32(output, 0);
            for (const auto value : {std::int64_t{400}, std::int64_t{600},
                                     std::int64_t{0}, std::int64_t{77},
                                     std::int64_t{0}, std::int64_t{1},
                                     std::int64_t{123}, std::int64_t{45}})
                append_varint(output, value);
            append_u16(output, 9);
            append_u16(output, 25);
            append_u16(output, 123);
            append_float(output, 456.5f);
            append_u16(output, static_cast<std::uint16_t>(static_cast<std::int16_t>(-75)));
            const std::string pair = "pair-data!";
            output.insert(output.end(), pair.begin(), pair.end());
            append_float(output, 3.5f);
            append_float(output, 4.5f);
            const std::string meta = "meta";
            output.insert(output.end(), meta.begin(), meta.end());
            output.insert(output.end(), 24 - meta.size(), 0);
            append_u16(output, 8);
            return output;
        };
        tdx::Bytes ranking_payload;
        append_u16(ranking_payload, 11);
        append_u16(ranking_payload, 2);
        const auto first_ranking = make_ranking_record(0, "000001", 1000);
        const auto second_ranking = make_ranking_record(1, "600000", 900);
        ranking_payload.insert(ranking_payload.end(), first_ranking.begin(), first_ranking.end());
        ranking_payload.insert(ranking_payload.end(), second_ranking.begin(), second_ranking.end());
        const auto ranking_page = tdx::parse_category_payload(ranking_payload);
        require(ranking_page.header == 11 && ranking_page.records.size() == 2,
                "ranking response header and count");
        const auto& ranking = ranking_page.records[0];
        require(ranking.market_id == 0 && ranking.code == "000001",
                "ranking security identifier");
        require(std::fabs(ranking.last_price - 10.0) < 0.0001 &&
                std::fabs(ranking.pre_close_price - 9.9) < 0.0001,
                "ranking prices");
        require(std::fabs(ranking.open_amount_yuan - 7700.0) < 0.0001 &&
                ranking.bid1_volume_hand == 123, "ranking order book fields");
        require(std::fabs(ranking.rise_speed - 0.25) < 0.0001 &&
                std::fabs(ranking.short_turnover - 1.23) < 0.0001 &&
                std::fabs(ranking.opening_rush + 0.75) < 0.0001,
                "ranking fixed metrics");
        require(ranking.active2 == 8, "ranking fixed tail");

        tdx::Bytes etf_payload;
        append_u16(etf_payload, 0);
        append_u16(etf_payload, 1);
        const auto etf_record = make_ranking_record(1, "510300", 43210);
        etf_payload.insert(etf_payload.end(), etf_record.begin(), etf_record.end());
        const auto etf_page = tdx::parse_category_payload(etf_payload);
        require(std::fabs(etf_page.records[0].last_price - 43.21) < 0.0001,
                "ranking ETF price divisor");

        const auto auction_request = tdx::build_auction_request_data(0, "000988");
        require(auction_request == tdx::Bytes({
            0x00, 0x00, 0x30, 0x30, 0x30, 0x39, 0x38, 0x38,
            0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xF4, 0x01, 0x00, 0x00}), "auction request fields");
        auto append_auction_point = [](tdx::Bytes& output, std::uint16_t minute,
                                       std::uint8_t second, float price,
                                       std::uint32_t matched, std::int32_t unmatched) {
            append_u16(output, minute);
            append_float(output, price);
            append_u32(output, matched);
            append_u32(output, static_cast<std::uint32_t>(unmatched));
            output.push_back(0);
            output.push_back(second);
        };
        tdx::Bytes auction_payload;
        append_u16(auction_payload, 3);
        append_auction_point(auction_payload, 555, 0, 10.0f, 100, 80);
        append_auction_point(auction_payload, 555, 5, 10.1f, 120, -60);
        append_auction_point(auction_payload, 565, 0, 10.2f, 150, 0);
        const auto auction = tdx::parse_auction_payload(auction_payload, 0, "000001");
        require(auction.points.size() == 3, "auction point count");
        require(auction.points[0].time_label == "09:15:00" &&
                auction.points[2].time_label == "09:25:00", "auction point times");
        require(auction.points[0].unmatched_direction_raw == 1 &&
                auction.points[0].unmatched_direction == "buy" &&
                auction.points[1].unmatched_direction_raw == -1 &&
                auction.points[1].unmatched_direction == "sell" &&
                auction.points[2].unmatched_direction == "balanced",
                "auction unmatched directions");
        require(std::fabs(auction.points[2].price * auction.points[2].matched_volume_hand * 100.0 -
                          153000.0) < 0.01, "auction matched amount");
        bool rejected_bad_auction_time = false;
        try {
            tdx::Bytes invalid_payload;
            append_u16(invalid_payload, 1);
            append_auction_point(invalid_payload, 555, 60, 10.0f, 1, 0);
            (void)tdx::parse_auction_payload(invalid_payload, 0, "000001");
        } catch (const tdx::Error&) {
            rejected_bad_auction_time = true;
        }
        require(rejected_bad_auction_time, "auction invalid time rejection");

        const auto today_trades_request = tdx::build_today_trades_request_data(
            0, "000001", 0, 115);
        require(today_trades_request == tdx::Bytes({
            0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31,
            0x00, 0x00, 0x73, 0x00}), "today trades request fields");
        const auto history_trades_request = tdx::build_history_trades_request_data(
            0, "300308", "2026-05-11", 0, 900);
        require(history_trades_request == tdx::Bytes({
            0x9F, 0x26, 0x35, 0x01, 0x00, 0x00, 0x33, 0x30,
            0x30, 0x33, 0x30, 0x38, 0x00, 0x00, 0x84, 0x03}),
            "history trades request fields");

        auto append_trade_tick = [](tdx::Bytes& output, std::uint16_t minute,
                                    std::int64_t price_delta, std::int64_t volume,
                                    std::int64_t orders, std::int64_t status,
                                    std::int64_t tail = 0) {
            append_u16(output, minute);
            for (const auto field : {price_delta, volume, orders, status, tail})
                append_varint(output, field);
        };
        tdx::Bytes today_trades_payload;
        append_u16(today_trades_payload, 3);
        append_trade_tick(today_trades_payload, 565, 1000, 20, 3, 0, 7);
        append_trade_tick(today_trades_payload, 565, 1, 10, 2, 1, 8);
        append_trade_tick(today_trades_payload, 900, -1, 30, 5, 2, 9);
        const auto today_trades = tdx::parse_today_trades_payload(
            today_trades_payload, 0, "000001", 100, 3, "20260803");
        require(today_trades.ticks.size() == 3 &&
                today_trades.ticks[0].absolute_index == 100 &&
                today_trades.ticks[2].absolute_index == 102,
                "today trades count and absolute indexes");
        require(today_trades.ticks[0].time_label == "09:25" &&
                today_trades.ticks[2].time_label == "15:00",
                "today trades time labels");
        require(std::fabs(today_trades.ticks[0].price - 10.0) < 0.0001 &&
                std::fabs(today_trades.ticks[1].price - 10.01) < 0.0001 &&
                std::fabs(today_trades.ticks[2].price - 10.0) < 0.0001,
                "today trades price deltas");
        require(today_trades.ticks[0].side == "buy" &&
                today_trades.ticks[1].side == "sell" &&
                today_trades.ticks[2].side == "neutral",
                "today trades sides");

        tdx::Bytes history_trades_payload;
        append_u16(history_trades_payload, 3);
        append_float(history_trades_payload, 10.0f);
        append_trade_tick(history_trades_payload, 565, 1000, 20, 3, 0);
        append_trade_tick(history_trades_payload, 565, 1, 10, 2, 1);
        append_trade_tick(history_trades_payload, 900, 1001, 30, 5, 2);
        const auto history_trades = tdx::parse_history_trades_payload(
            history_trades_payload, 0, "000001", "20260803", 0, 2000);
        require(history_trades.price_base_raw.has_value() &&
                std::fabs(*history_trades.price_base_raw - 10.0f) < 0.0001f,
                "history trades price base");
        const auto trade_summary = tdx::aggregate_trade_ticks(history_trades.ticks);
        require(trade_summary.at("auction_0925").at("tick_count").as_number() == 2 &&
                trade_summary.at("closing_1500").at("tick_count").as_number() == 1,
                "trade auction minute aggregation");
        require(std::fabs(trade_summary.at("auction_0925").at("amount_yuan").as_number() -
                          30010.0) < 0.01,
                "trade auction amount aggregation");

        tdx::Bytes fund_trades_payload;
        append_u16(fund_trades_payload, 1);
        append_trade_tick(fund_trades_payload, 565, 4680, 20, 3, 2);
        const auto fund_trades = tdx::parse_today_trades_payload(
            fund_trades_payload, 1, "510300", 0, 1);
        require(fund_trades.price_divisor == 1000 &&
                std::fabs(fund_trades.ticks[0].price - 4.680) < 0.0001,
                "fund trades three decimal prices");

        bool rejected_truncated_trade = false;
        try {
            (void)tdx::parse_today_trades_payload(
                tdx::Bytes{0x01, 0x00}, 0, "000001", 0, 1);
        } catch (const tdx::Error&) {
            rejected_truncated_trade = true;
        }
        require(rejected_truncated_trade, "truncated trade rejection");

        const auto stats_request = tdx::build_stats_file_request_data(
            "zhb.zip", 123, 30000);
        require(stats_request.size() == 308, "statistics request size");
        require(tdx::read_u32_le(stats_request.data()) == 123 &&
                tdx::read_u32_le(stats_request.data() + 4) == 30000,
                "statistics request offset and chunk size");
        require(std::string(stats_request.begin() + 8, stats_request.begin() + 15) ==
                "zhb.zip", "statistics request path");
        require(std::all_of(stats_request.begin() + 15, stats_request.end(),
                            [](std::uint8_t value) { return value == 0; }),
                "statistics request path padding");
        tdx::Bytes stats_chunk;
        append_u32(stats_chunk, 3);
        stats_chunk.insert(stats_chunk.end(), {'a', 'b', 'c'});
        require(tdx::parse_stats_file_chunk(stats_chunk, 4) ==
                tdx::Bytes({'a', 'b', 'c'}), "statistics response chunk");
        bool rejected_stats_chunk = false;
        try {
            (void)tdx::parse_stats_file_chunk(
                tdx::Bytes{0x03, 0x00, 0x00, 0x00, 'a', 'b'}, 4);
        } catch (const tdx::Error&) {
            rejected_stats_chunk = true;
        }
        require(rejected_stats_chunk, "statistics chunk length rejection");

        std::vector<std::string> stat_fields(35);
        stat_fields[0] = "0";
        stat_fields[1] = "000001";
        stat_fields[2] = "1.25";
        stat_fields[3] = "8.75";
        stat_fields[4] = "20260803";
        stat_fields[9] = "9.25";
        stat_fields[11] = "200.00";
        stat_fields[22] = "50601";
        stat_fields[26] = "9";
        stat_fields[31] = "7";
        stat_fields[32] = "5";
        stat_fields[33] = "3";
        std::vector<std::string> stat2_fields(21);
        stat2_fields[0] = "0";
        stat2_fields[1] = "000001";
        stat2_fields[2] = "20260803";
        stat2_fields[3] = "242501.84";
        stat2_fields[5] = "254664.58";
        stat2_fields[6] = "851.80";
        stat2_fields[7] = "134819.47";
        stat2_fields[8] = "3468.47";
        stat2_fields[9] = "11141";
        stat2_fields[10] = "9513";
        stat2_fields[14] = "6002.06";
        stat2_fields[15] = "4291.39";
        std::vector<std::string> tip_fields(22);
        tip_fields[0] = "0";
        tip_fields[1] = "000001";
        tip_fields[17] = "20260802";
        tip_fields[18] = "1.25";
        tip_fields[19] = "20260803";
        tip_fields[20] = "2.5";
        tip_fields[21] = "20260804";
        const auto stats_zip = make_stored_zip({
            {"tdxstat.cfg", text_bytes(joined_fields(stat_fields))},
            {"tdxstat2.cfg", text_bytes(joined_fields(stat2_fields))},
            {"tipinfo.dat", text_bytes(joined_fields(tip_fields))},
        });
        const auto stats_resource = tdx::parse_stats_archive(stats_zip);
        require(stats_resource.stat.size() == 1 && stats_resource.stat2.size() == 1 &&
                stats_resource.tip_info_events.size() == 1,
                "statistics ZIP member row counts");
        require(stats_resource.stats_date && *stats_resource.stats_date == "20260803" &&
                std::fabs(stats_resource.stats_date_coverage - 1.0) < 0.0001,
                "statistics dominant date and coverage");
        const auto& stat_row = stats_resource.stat.at({0, "000001"});
        const auto& stat2_row = stats_resource.stat2.at({0, "000001"});
        require(stat_row.beta_60d && std::fabs(*stat_row.beta_60d - 1.25) < 0.0001 &&
                stat_row.pe_ttm && std::fabs(*stat_row.pe_ttm - 8.75) < 0.0001 &&
                stat_row.pe_static && std::fabs(*stat_row.pe_static - 9.25) < 0.0001 &&
                stat_row.free_float_shares_10k &&
                std::fabs(*stat_row.free_float_shares_10k - 200.0) < 0.0001 &&
                stat_row.shape_packed && *stat_row.shape_packed == 50601 &&
                stat_row.shape_short && *stat_row.shape_short == 5 &&
                stat_row.shape_mid && *stat_row.shape_mid == 6 &&
                stat_row.shape_long && *stat_row.shape_long == 1 &&
                stat_row.limit_up_streak_days && *stat_row.limit_up_streak_days == 3,
                "tdxstat mapped columns");
        const auto valuation_snapshot = tdx::Json::parse(
            R"({"generated_at":"2026-08-08T10:00:00","records":[{"market_id":0,"code":"000001","last_price":20,"pre_close_price":19}]})");
        const auto valuation_finance = tdx::Json::parse(
            R"({"records":[{"market_id":0,"code":"000001","updated_date":"2026-04-25","shares":{"total":2000000000},"income_statement":{"net_profit_yuan":200000000},"reserved_2":3,"per_share":{"net_assets":10},"balance_sheet":{"net_assets_yuan":20000000000}}]})");
        const auto security_valuation = tdx::derive_security_valuation_document(
            0, "000001", &stat_row, &valuation_snapshot, &valuation_finance);
        require(security_valuation.at("availability").as_string() == "complete" &&
                std::fabs(security_valuation.at("metrics").at("pe_dynamic")
                              .at("value").as_number() - 50.0) < 0.0001 &&
                std::fabs(security_valuation.at("metrics").at("pe_static")
                              .at("value").as_number() - 9.25) < 0.0001 &&
                std::fabs(security_valuation.at("metrics").at("pe_ttm")
                              .at("value").as_number() - 8.75) < 0.0001 &&
                std::fabs(security_valuation.at("metrics").at("pb_mrq")
                              .at("value").as_number() - 2.0) < 0.0001,
                "TdxW system-column valuation reconstruction");
        auto tiny_profit_finance = valuation_finance;
        tiny_profit_finance["records"].as_array()[0]
            ["income_statement"]["net_profit_yuan"] = 25000.0;
        const auto tiny_profit_valuation = tdx::derive_security_valuation_document(
            0, "000001", &stat_row, &valuation_snapshot, &tiny_profit_finance);
        require(tiny_profit_valuation.at("metrics").at("pe_dynamic")
                    .at("value").is_null(),
                "dynamic PE must use TdxW's strict 0.0001f annualized-EPS threshold");
        require(stat2_row.amount_10k &&
                std::fabs(*stat2_row.amount_10k - 242501.84) < 0.001 &&
                !stat2_row.seal_amount_10k && stat2_row.prev_seal_amount_10k &&
                std::fabs(*stat2_row.prev_seal_amount_10k - 851.80) < 0.001 &&
                stat2_row.open_amount_10k &&
                std::fabs(*stat2_row.open_amount_10k - 6002.06) < 0.001,
                "tdxstat2 mapped columns");
        const auto& tip_row = stats_resource.tip_info_events.at({0, "000001"});
        require(tip_row.northbound_date && *tip_row.northbound_date == "20260802" &&
                tip_row.northbound_direction &&
                std::fabs(*tip_row.northbound_direction - 1.25) < 0.0001 &&
                tip_row.repurchase_plan_date &&
                *tip_row.repurchase_plan_date == "20260803" &&
                tip_row.incentive_plan_date &&
                *tip_row.incentive_plan_date == "20260804",
                "tipinfo FINANCE(88/90/91) mapped columns");
        bool rejected_missing_stats_member = false;
        try {
            (void)tdx::parse_stats_archive(make_stored_zip({
                {"tdxstat.cfg", text_bytes(joined_fields(stat_fields))},
            }));
        } catch (const tdx::Error&) {
            rejected_missing_stats_member = true;
        }
        require(rejected_missing_stats_member, "statistics missing ZIP member rejection");

        const std::string pbrpc_json =
            "{\"ReqId\":\"200404\",\"market\":\"0\",\"Page\":\"-1\","
            "\"PageSize\":\"100\",\"modname\":\"mod_peg.dll\"}";
        const auto pbrpc_payload = tdx::build_pbrpc_request("mod_peg.dll", pbrpc_json);
        require(bytes_hex(pbrpc_payload) ==
            "0a030a0131220b6d6f645f7065672e646c6c2a54"
            "7b225265714964223a22323030343034222c226d61726b6574223a2230222c"
            "2250616765223a222d31222c225061676553697a65223a22313030222c226d"
            "6f646e616d65223a226d6f645f7065672e646c6c227d",
            "PBRPC known request vector");

        const auto negative_rpc = tdx::parse_pbrpc_response(
            tdx::encode_pbrpc_varint_field(2, UINT64_MAX));
        require(negative_rpc.rpc_id == -1, "PBRPC negative int32 RpcID");

        const auto [descriptor_module, descriptor_request] = tdx::parse_pbrpc_descriptor(
            "pb_rpc_req:Head.CharSet=1;Head.Target=0;RpcID=0;StartPos=0;"
            "Moduledll=mod_peg.dll;ReqByte={\"ReqId\":\"200327\",\"N\":\"$$N$$\"}",
            {{"N", "60"}});
        require(descriptor_module == "mod_peg.dll" &&
                descriptor_request.at("N").as_string() == "60",
                "PBRPC descriptor and placeholder parsing");

        const auto pbrpc_business = text_bytes("{\"ErrorCode\":0,\"ResultSets\":[]}\0");
        int pbrpc_round = 0;
        const auto pbrpc_query = tdx::query_pbrpc_raw(
            "HQServ.PBRPC_PEG", "mod_peg.dll",
            tdx::Json::parse("{\"ReqId\":\"200404\"}"),
            "http://example.invalid/TQLEX", 1000, 4, 0,
            [&](const std::string& url, const tdx::Bytes& payload, int timeout) {
                require(url == "http://example.invalid/TQLEX?Entry=HQServ.PBRPC_PEG" &&
                        timeout == 1000, "PBRPC transport URL and timeout");
                ++pbrpc_round;
                if (pbrpc_round == 1)
                    return tdx::encode_pbrpc_varint_field(2, 123);
                const tdx::Bytes rpc_pattern{0x10, 0x7b};
                require(std::search(payload.begin(), payload.end(),
                                    rpc_pattern.begin(), rpc_pattern.end()) != payload.end(),
                        "PBRPC second round carries RpcID");
                auto response = tdx::encode_pbrpc_varint_field(2, 123);
                const auto total = tdx::encode_pbrpc_varint_field(4, pbrpc_business.size());
                response.insert(response.end(), total.begin(), total.end());
                const auto count = tdx::encode_pbrpc_varint_field(5, pbrpc_business.size());
                response.insert(response.end(), count.begin(), count.end());
                const auto bytes = tdx::encode_pbrpc_bytes_field(6, pbrpc_business);
                response.insert(response.end(), bytes.begin(), bytes.end());
                return response;
            });
        require(pbrpc_round == 2 && pbrpc_query.rpc_id == 123 &&
                pbrpc_query.rounds == 2 &&
                tdx::decode_pbrpc_result(pbrpc_query.data).at("ErrorCode").as_number() == 0,
                "PBRPC RpcID handshake, assembly and NUL JSON decode");

        const auto pbrpc_fragment_response = [](
            std::int32_t start_pos, std::int32_t total_len,
            const tdx::Bytes& fragment) {
            auto response = tdx::encode_pbrpc_varint_field(2, 321);
            const auto start = tdx::encode_pbrpc_varint_field(3, start_pos);
            response.insert(response.end(), start.begin(), start.end());
            const auto total = tdx::encode_pbrpc_varint_field(4, total_len);
            response.insert(response.end(), total.begin(), total.end());
            const auto count = tdx::encode_pbrpc_varint_field(5, fragment.size());
            response.insert(response.end(), count.begin(), count.end());
            const auto bytes = tdx::encode_pbrpc_bytes_field(6, fragment);
            response.insert(response.end(), bytes.begin(), bytes.end());
            return response;
        };
        int budget_round = 0;
        const auto budget_transport = [&](const std::string&, const tdx::Bytes&, int) {
            ++budget_round;
            if (budget_round == 1)
                return pbrpc_fragment_response(0, 4, tdx::Bytes{1, 2, 3});
            return pbrpc_fragment_response(3, 4, tdx::Bytes{4});
        };
        const auto exact_budget_query = tdx::query_pbrpc_raw(
            "HQServ.PBRPC_PEG", "mod_peg.dll",
            tdx::Json::parse("{\"ReqId\":\"1\"}"),
            "http://example.invalid/TQLEX", 1000, 2, 0,
            budget_transport, 4);
        require(budget_round == 2 &&
                    exact_budget_query.data == tdx::Bytes({1, 2, 3, 4}),
                "PBRPC exact assembled byte budget succeeds");
        budget_round = 0;
        bool rejected_budget_plus_one = false;
        try {
            (void)tdx::query_pbrpc_raw(
                "HQServ.PBRPC_PEG", "mod_peg.dll",
                tdx::Json::parse("{\"ReqId\":\"1\"}"),
                "http://example.invalid/TQLEX", 1000, 2, 0,
                budget_transport, 3);
        } catch (const tdx::Error& error) {
            rejected_budget_plus_one =
                std::string(error.what()).find(
                    "exceeds max_assembled_bytes 3") != std::string::npos;
        }
        require(rejected_budget_plus_one && budget_round == 2,
                "PBRPC rejects the next fragment before appending budget plus one");

        require(tdx::cloud_workflow_defaults("fund-holdings", "20260731")
                    .at("report_date").as_string() == "20251231" &&
                tdx::cloud_workflow_defaults("fund-holdings", "20260901")
                    .at("report_date").as_string() == "20260630",
                "cloud workflow fund disclosure cutoff");
        const auto workflow_rows = tdx::cloud_result_rows(tdx::Json::parse(
            "{\"ResultSets\":[{\"ColDes\":[{\"Name\":\"SetCode\"},"
            "{\"Name\":\"Code\"},{\"Name\":\"Name\"}],"
            "\"Content\":[\"0 000001 PingAn\",[\"1\",\"600000\",\"SPDB\"]]}]}"));
        require(workflow_rows.size() == 2 &&
                workflow_rows.as_array()[0].at("Code").as_string() == "000001" &&
                workflow_rows.as_array()[1].at("Name").as_string() == "SPDB",
                "cloud workflow normalizes string and array rows");

        tdx::BlockData fund_blocks;
        fund_blocks.securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        fund_blocks.securities[{2, "920092"}] =
            tdx::Security{2, "BJ", "北京", "920092", "汉鑫科技"};
        tdx::Block research_industry;
        research_industry.block_id = "research-industry:X50";
        research_industry.family = "research-industry";
        research_industry.block_code = "881385";
        research_industry.name = "银行";
        research_industry.level = 1;
        fund_blocks.blocks.push_back(research_industry);
        tdx::BlockMember research_member;
        research_member.block_id = research_industry.block_id;
        research_member.family = research_industry.family;
        research_member.block_code = research_industry.block_code;
        research_member.block_name = research_industry.name;
        research_member.market_id = 0;
        research_member.market = "SZ";
        research_member.code = "000001";
        fund_blocks.members.push_back(research_member);
        tdx::IntradayFundsService fund_service({}, fund_blocks);
        const auto* bank = fund_service.industry_for_security(0, "000001");
        require(bank && bank->code == "881385" && bank->name == "银行",
                "local level-1 research industry resolves PBRPC industry");

        const auto fund_row = tdx::Json::parse(
            "{\"market\":\"0\",\"code\":\"000001\",\"xj\":\"10.00\","
            "\"zdf\":\"1.00\",\"zd\":\"0.10\",\"q5rjl\":\"100\","
            "\"jlr_1\":\"1200\",\"cje_1\":\"5000\",\"zlzb_1\":\"24\","
            "\"jlr_7\":\"100\",\"cje_7\":\"800\",\"zlzb_7\":\"12.5\","
            "\"zf_7\":\"0.3\",\"cjl_7\":\"10\"}");
        const auto normalized_fund = tdx::normalize_intraday_fund_record(
            fund_row, fund_blocks.securities, true);
        require(normalized_fund.at("name").as_string() == "平安银行" &&
                normalized_fund.at("periods").at("today")
                    .at("net_main_inflow").as_string() == "1200" &&
                std::fabs(normalized_fund.at("periods").at("before-09:35")
                    .at("relative_volume").as_number() - 2.0) < 0.0001,
                "intraday fund field mapping and stock volume unit conversion");

        tdx::IntradayFundsService::FetchResult stale_master{
            tdx::Json::object(), false, 42, true,
            "PBRPC server rejected request with RpcID -1"};
        tdx::IntradayFundsService::FetchResult fresh_detail{
            tdx::Json::object(), true, 0, false, {}};
        const auto fund_cache = tdx::intraday_funds_cache_document(
            stale_master, &fresh_detail, 15);
        require(fund_cache.at("hit").as_bool() &&
                fund_cache.at("stale").as_bool() &&
                fund_cache.at("master_stale").as_bool() &&
                fund_cache.at("master_age_seconds").as_number() == 42 &&
                fund_cache.at("master_upstream_error").as_string().find("RpcID -1") !=
                    std::string::npos &&
                !fund_cache.at("detail_stale").as_bool(),
                "intraday funds stale-cache metadata");

        tdx::Json lhb_documents = tdx::Json::array();
        for (std::size_t view_index = 0; view_index < tdx::lhb_views().size(); ++view_index) {
            const auto& view = tdx::lhb_views()[view_index];
            tdx::Json document = tdx::Json::object();
            document["resource"] = view.resource;
            document["size"] = 100;
            document["endpoint"] = "127.0.0.1:7709";
            document["rows"] = tdx::Json::array();
            if (view_index < 2) {
                auto row = tdx::Json::parse(
                    "{\"$ZQDM\":\"7\",\"$SC1\":\"0\",\"$ZQDM1\":\"000001\","
                    "\"date\":\"20260803\",\"lx\":\"异动\",\"bzb\":\"10\","
                    "\"szb\":\"8\",\"jmr\":\"20\",\"zmr\":\"100\","
                    "\"zmc\":\"80\",\"sl1\":\"1\",\"sl2\":\"0\",\"lb\":\"1\"}");
                document["rows"].push_back(std::move(row));
            }
            if (view_index == 2) {
                auto row = tdx::Json::parse(
                    "{\"$ZQDM\":\"8\",\"$SC1\":\"44\",\"$ZQDM1\":\"920092\"," 
                    "\"date\":\"20260804\",\"lx\":\"异动\",\"bzb\":\"9\"," 
                    "\"szb\":\"7\",\"jmr\":\"10\",\"zmr\":\"60\"," 
                    "\"zmc\":\"50\",\"sl1\":\"0\",\"sl2\":\"0\",\"lb\":\"1\"}");
                document["rows"].push_back(std::move(row));
            }
            document["row_count"] = static_cast<std::uint64_t>(document.at("rows").size());
            lhb_documents.push_back(std::move(document));
        }
        const auto lhb_master = tdx::aggregate_lhb_master_documents(
            lhb_documents, fund_blocks.securities, "2026-08-03T00:00:00+0800");
        require(lhb_master.at("counts").at("master_rows").as_number() == 3 &&
                lhb_master.at("counts").at("events").as_number() == 2 &&
                lhb_master.at("events").as_array()[0].at("security")
                    .at("market_id").as_number() == 2 &&
                lhb_master.at("events").as_array()[0].at("security")
                    .at("name").as_string() == "汉鑫科技" &&
                lhb_master.at("events").as_array()[1].at("views").size() == 2 &&
                lhb_master.at("events").as_array()[1].at("security")
                    .at("name").as_string() == "平安银行",
                "LHB aggregates events and maps raw BJ market 44");

        auto lhb_detail_rows = tdx::Json::array();
        lhb_detail_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"000001\",\"sc\":\"0\",\"date\":\"20260803\","
            "\"lb\":\"3\",\"yyb\":\"买(1): 测试席位\",\"yyb1\":\"B\","
            "\"yyb2\":\"1\",\"bje\":\"100\",\"sje\":\"0\","
            "\"jmr\":\"100\",\"zb\":\"5\",\"mrcgl1\":\"50\","
            "\"mrcgl3\":\"40\",\"mrcgl5\":\"30\",\"ygcb\":\"\",\"ygsy\":\"\"}"));
        lhb_detail_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"000001\",\"sc\":\"0\",\"date\":\"20260803\","
            "\"lb\":\"1\",\"yyb\":\"买入合计\",\"yyb1\":\"B\","
            "\"yyb2\":\"0\",\"bje\":\"100\",\"sje\":\"0\","
            "\"jmr\":\"100\",\"zb\":\"5\",\"mrcgl1\":\"\","
            "\"mrcgl3\":\"\",\"mrcgl5\":\"\",\"ygcb\":\"\",\"ygsy\":\"\"}"));
        const auto normalized_lhb = tdx::normalize_lhb_detail_rows(lhb_detail_rows);
        require(normalized_lhb.size() == 2 &&
                normalized_lhb.as_array()[0].at("row_type").as_string() == "broker" &&
                normalized_lhb.as_array()[1].at("row_type").as_string() == "total" &&
                normalized_lhb.as_array()[0].at("broker").as_string() == "买(1): 测试席位",
                "LHB detail normalization preserves broker and total rows");

        const auto holder_reference = tdx::parse_holder_reference(
            "http://page1.tdx.com.cn/x?gdname=%E9%AB%98%E7%9B%9B&"
            "gdid=QF000034&tdxid=A1&gp=603221", "000001");
        require(holder_reference.at("holder_id").as_string() == "QF000034" &&
                holder_reference.at("variant_id").as_string() == "A1" &&
                holder_reference.at("holder_name").as_string() == "高盛" &&
                holder_reference.at("reference_code").as_string() == "603221" &&
                holder_reference.at("queryable").as_bool(),
                "holder URL exposes gdid, tdxid, name, and seed stock");

        const auto holder_history_response = tdx::Json::parse(
            "{\"ResultSets\":[{\"ColName\":[\"T001\",\"bh\",\"cnt\",\"rq\","
            "\"sc\",\"zqdm\",\"zqjc\",\"T006\",\"T007\",\"stype\","
            "\"T012\",\"T008\",\"T009\"],\"Content\":[[\"row1\",\"1\",1,"
            "\"2026-07-23\",1,\"688657\",\"浩辰软件\",889637,1.4903,\"1\","
            "\"流通A股\",\"新进\",1]]}]}" );
        const auto holder_history = tdx::normalize_holder_history_rows(
            tdx::tqlex_result_rows(holder_history_response));
        require(holder_history.size() == 1 &&
                holder_history.as_array()[0].at("security_id").as_string() == "SH688657" &&
                holder_history.as_array()[0].at("currently_held").as_bool() &&
                holder_history.as_array()[0].at("change_label").as_string() == "新进",
                "holder cross-stock ColName rows normalize market and change fields");

        const auto holder_stock_response = tdx::Json::parse(
            "{\"ResultSets\":[{\"ColName\":[\"rq\",\"T006\",\"T007\",\"T012\","
            "\"T009\",\"T008\",\"stype\"],\"Content\":[[\"2026-03-31\","
            "1359308,0.56,\"流通A股\",3,\"523500\",\"2\"]]}]}" );
        const auto holder_stock = tdx::normalize_holder_stock_rows(
            tdx::tqlex_result_rows(holder_stock_response));
        require(holder_stock.size() == 1 &&
                holder_stock.as_array()[0].at("change_label").as_string() == "增持" &&
                holder_stock.as_array()[0].at("share_nature").as_string() == "流通A股",
                "holder stock report-period rows preserve UTF-8 and change semantics");

        std::map<std::pair<int, std::string>, tdx::Security> valuation_securities;
        valuation_securities[{1, "000001"}] =
            tdx::Security{1, "SH", "上海", "000001", "上证指数"};
        valuation_securities[{0, "510050"}] =
            tdx::Security{0, "SZ", "深圳", "510050", "测试基金"};
        auto valuation_master_rows = tdx::Json::array();
        valuation_master_rows.push_back(tdx::Json::parse(
            "{\"date\":\"20260803\",\"$ZQDM1\":\"000001\",\"$SC1\":\"1\"," 
            "\"pe\":\"16.4\",\"pefws\":\"84.0\",\"pb\":\"1.33\"," 
            "\"pbfws\":\"71.4\",\"gxl\":\"1.6\",\"roe\":\"8.35\"," 
            "\"gzsp\":\"估值偏高\",\"syl\":\"6.1\",\"jwzf\":\"0.4\"," 
            "\"jszf\":\"1.8\",\"jezf\":\"-5.2\",\"jsszf\":\"-6.3\"," 
            "\"jsksrq\":\"20140102\",\"$ZQDM\":\"000001\"}"));
        const auto valuation_master = tdx::normalize_valuation_master_rows(
            valuation_master_rows, valuation_securities);
        require(valuation_master.size() == 1 &&
                valuation_master.as_array()[0].at("security").at("name").as_string() ==
                    "上证指数" &&
                valuation_master.as_array()[0].at("metrics")
                    .at("valuation_label").as_string() == "估值偏高" &&
                valuation_master.as_array()[0].at("detail_id").as_string() == "000001",
                "valuation master fields map metrics, returns, and hidden detail ID");

        auto valuation_fund_rows = tdx::Json::array();
        valuation_fund_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"510050\",\"$SC\":\"0\",\"dwjz\":\"2.8\"," 
            "\"yjl\":\"0.15\",\"zxfe\":\"1000\",\"zxssdw\":\"900000\"," 
            "\"jjlx\":\"ETF\"}"));
        const auto valuation_funds = tdx::normalize_valuation_fund_rows(
            valuation_fund_rows, valuation_securities);
        require(valuation_funds.size() == 1 &&
                valuation_funds.as_array()[0].at("name").as_string() == "测试基金" &&
                valuation_funds.as_array()[0].at("premium_pct").as_string() == "0.15",
                "valuation related fund rows preserve market data and resolve names");

        const auto valuation_history = tdx::merge_valuation_history_rows(
            tdx::Json::parse(
                "[{\"date\":\"20260801\",\"pebfw\":\"80\",\"pe\":\"16\"},"
                "{\"date\":\"20260802\",\"pebfw\":\"81\",\"pe\":\"17\"}]"),
            tdx::Json::parse(
                "[{\"date\":\"20260802\",\"pbbfw\":\"70\",\"pb\":\"1.3\"},"
                "{\"date\":\"20260803\",\"pbbfw\":\"71\",\"pb\":\"1.4\"}]"));
        require(valuation_history.size() == 3 &&
                valuation_history.as_array()[0].at("pb").is_null() &&
                valuation_history.as_array()[1].at("pe").as_string() == "17" &&
                valuation_history.as_array()[1].at("pb").as_string() == "1.3" &&
                valuation_history.as_array()[2].at("pe").is_null(),
                "valuation PE/PB histories merge by date and preserve missing values");

        std::map<std::pair<int, std::string>, tdx::Security> consensus_securities;
        consensus_securities[{2, "920179"}] =
            tdx::Security{2, "BJ", "北京", "920179", "凯德石英"};
        auto consensus_master_rows = tdx::Json::array();
        consensus_master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920179\",\"$SC\":\"44\",\"ZXRQ\":\"20260803\"," 
            "\"JGSL\":\"4\",\"ZHPJ\":\"4.50\",\"PE\":\"28.5\"," 
            "\"YCEPS\":\"30\",\"PEG\":\"0.95\",\"MBJ\":\"35.2\"," 
            "\"zxspj\":\"31.8\",\"jynzgj\":\"40.0\",\"zgdf\":\"-20.5\"," 
            "\"lzts\":\"6\",\"jynzdj\":\"20.0\",\"zdzf\":\"59.0\"," 
            "\"hy\":\"非金属材料\",\"BGQ\":\"2026\",\"MGSY0\":\"0.8\"," 
            "\"MGSY1\":\"1.1\",\"MGSY2\":\"1.5\",\"JLR0\":\"1.0\"," 
            "\"JLR1\":\"1.4\",\"JLR2\":\"1.9\",\"YYSR0\":\"5.0\"," 
            "\"YYSR1\":\"6.0\",\"YYSR2\":\"7.2\"}"));
        const auto consensus_master = tdx::normalize_consensus_master_rows(
            consensus_master_rows, "latest", consensus_securities);
        require(consensus_master.size() == 1 &&
                consensus_master.as_array()[0].at("security").at("market_id").as_number() == 2 &&
                consensus_master.as_array()[0].at("security").at("security_id").as_string() ==
                    "BJ920179" &&
                consensus_master.as_array()[0].at("forecasts").as_array()[1]
                    .at("year").as_number() == 2027 &&
                consensus_master.as_array()[0].at("forecasts").as_array()[1]
                    .at("eps").as_string() == "1.1",
                "consensus master maps raw BJ market 44 and three-year forecasts");
        const auto consensus_stage = tdx::normalize_consensus_master_rows(
            consensus_master_rows, "year-low-rise", consensus_securities);
        require(consensus_stage.as_array()[0].at("latest_close").as_string() == "31.8" &&
                    consensus_stage.as_array()[0].at("year_high_price").as_string() == "40.0" &&
                    consensus_stage.as_array()[0].at("change_from_year_high_pct").as_string() == "-20.5" &&
                    consensus_stage.as_array()[0].at("consecutive_rise_days").as_string() == "6" &&
                    consensus_stage.as_array()[0].at("year_low_price").as_string() == "20.0" &&
                    consensus_stage.as_array()[0].at("change_from_year_low_pct").as_string() == "59.0",
                "consensus price-stage fields must preserve client values");

        auto consensus_report_rows = tdx::Json::array();
        consensus_report_rows.push_back(tdx::Json::parse(
            "{\"BGRQ\":\"20260701\",\"YJJG\":\"机构甲\",\"jgyxl\":\"A\"," 
            "\"FXS\":\"分析师甲\",\"T003\":\"增持\",\"T004\":\"维持\"," 
            "\"T011\":\"30\",\"nd\":\"2026\",\"ybxq\":\"较早报告\"," 
            "\"EPS0\":\"1\",\"EPS1\":\"2\",\"EPS2\":\"3\"}"));
        consensus_report_rows.push_back(tdx::Json::parse(
            "{\"BGRQ\":\"20260801\",\"YJJG\":\"机构乙\",\"jgyxl\":\"AA\"," 
            "\"FXS\":\"分析师乙\",\"T003\":\"买入\",\"T004\":\"调高\"," 
            "\"T011\":\"36\",\"nd\":\"2026\",\"ybxq\":\"最新报告\"," 
            "\"EPS0\":\"1.2\",\"EPS1\":\"2.2\",\"EPS2\":\"3.2\"}"));
        const auto consensus_reports = tdx::normalize_consensus_report_rows(
            consensus_report_rows, false);
        require(consensus_reports.size() == 2 &&
                consensus_reports.as_array()[0].at("report_date").as_string() == "20260801" &&
                consensus_reports.as_array()[0].at("rating").as_string() == "买入" &&
                consensus_reports.as_array()[0].at("report_text_length").as_number() > 0 &&
                consensus_reports.as_array()[0].as_object().find("report_text") ==
                    consensus_reports.as_array()[0].as_object().end() &&
                consensus_reports.as_array()[0].at("forecasts").as_array()[2]
                    .at("eps").as_string() == "3.2",
                "consensus reports sort newest first and optionally omit long text");

        std::map<std::pair<int, std::string>, tdx::Security> unlock_securities;
        unlock_securities[{2, "920978"}] =
            tdx::Security{2, "BJ", "北京", "920978", "开特股份"};
        unlock_securities[{1, "600663"}] =
            tdx::Security{1, "SH", "上海", "600663", "陆家嘴"};
        auto unlock_master_rows = tdx::Json::array();
        unlock_master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"920978\",\"$SC1\":\"44\","
            "\"date\":\"20260831\",\"jjjd\":\"未实施\","
            "\"jjsl\":\"250000\",\"sdq\":\"11\",\"spj\":\"20.30\","
            "\"yy\":\"股权激励\",\"$ZQDM\":\"20260831920978\"}"));
        unlock_master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"920978\",\"$SC1\":\"2\","
            "\"date\":\"20260831\",\"jjjd\":\"未实施\","
            "\"jjsl\":\"708000\",\"sdq\":\"23\",\"spj\":\"20.30\","
            "\"yy\":\"股权激励\",\"$ZQDM\":\"20260831920978\"}"));
        unlock_master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"600663\",\"$SC1\":\"1\","
            "\"date\":\"20260810\",\"jjjd\":\"实施\","
            "\"jjsl\":\"778734017\",\"sdq\":\"36\",\"spj\":\"8.67\","
            "\"fxj\":\"8.66\",\"sdqsy\":\"4.8\","
            "\"yy\":\"非公开发行限售\",\"$ZQDM\":\"20260810600663\"}"));
        unlock_master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"600663\",\"$SC1\":\"1\","
            "\"date\":\"20260810\",\"jjjd\":\"未实施\","
            "\"jjsl\":\"20\",\"sdq\":\"12\",\"spj\":\"8.67\","
            "\"yy\":\"非公开发行限售\",\"$ZQDM\":\"20260810600663\"}"));
        unlock_master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"600663\",\"$SC1\":\"1\","
            "\"date\":\"20260810\",\"jjjd\":\"未实施\","
            "\"jjsl\":\"30\",\"sdq\":\"12\",\"spj\":\"8.67\","
            "\"yy\":\"股权激励\",\"$ZQDM\":\"20260810600663\"}"));
        const auto unlock_events = tdx::normalize_unlock_master_rows(
            unlock_master_rows, unlock_securities);
        require(unlock_events.size() == 2 &&
                unlock_events.as_array()[0].at("detail_id").as_string() ==
                    "20260810600663" &&
                unlock_events.as_array()[0].at("progress").as_string() ==
                    "混合状态" &&
                unlock_events.as_array()[0].at("reason").as_string() ==
                    "多种原因" &&
                unlock_events.as_array()[0].at("mixed_progress").as_bool() &&
                unlock_events.as_array()[0].at("mixed_reason").as_bool() &&
                unlock_events.as_array()[0].at("progresses").size() == 2 &&
                unlock_events.as_array()[0].at("reasons").size() == 2 &&
                unlock_events.as_array()[0].at("lot_count").as_number() == 3 &&
                unlock_events.as_array()[0].at("lots").as_array()[1]
                    .at("progress").as_string() == "未实施" &&
                unlock_events.as_array()[1].at("security").at("market_id").as_number() == 2 &&
                unlock_events.as_array()[1].at("security").at("name").as_string() ==
                    "开特股份" &&
                unlock_events.as_array()[1].at("lot_count").as_number() == 2 &&
                unlock_events.as_array()[1].at("unlock_shares").as_number() == 958000,
                "unlock master preserves mixed batch status/reason and normalizes BJ market 44");

        auto unlock_holder_rows = tdx::Json::array();
        unlock_holder_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920978\",\"$SC\":\"44\",\"xm\":\"股东甲\","
            "\"jd\":\"实施\",\"spj\":\"20.30\",\"jjsl\":\"600000\","
            "\"jjyy\":\"股权激励\"}"));
        unlock_holder_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920978\",\"$SC\":\"2\",\"xm\":\"股东乙\","
            "\"jd\":\"实施\",\"spj\":\"20.30\",\"jjsl\":\"358000\","
            "\"jjyy\":\"股权激励\"}"));
        const auto unlock_holders = tdx::normalize_unlock_shareholder_rows(
            unlock_holder_rows, unlock_securities);
        require(unlock_holders.size() == 2 &&
                unlock_holders.as_array()[0].at("shareholder").as_string() == "股东甲" &&
                unlock_holders.as_array()[0].at("security")
                    .at("security_id").as_string() == "BJ920978" &&
                std::abs(unlock_holders.as_array()[0]
                    .at("unlock_market_value").as_number() - 12180000.0) < 0.01,
                "unlock shareholder details preserve names, shares, and market value");

        auto recent_large_rows = tdx::Json::array();
        recent_large_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"600866\",\"$SC\":\"1\",\"jjrq\":\"20260804\"," 
            "\"jjsl\":\"405703300\",\"jjgzb\":\"0.2442\"," 
            "\"zgb\":\"1661472600\",\"jjyy\":\"非公开发行限售\"}"));
        recent_large_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920978\",\"$SC\":\"44\",\"jjrq\":\"20260729\"," 
            "\"jjsl\":\"24597300\",\"jjgzb\":\"0.141\"," 
            "\"zgb\":\"174500000\",\"jjyy\":\"首发限售\"}"));
        const auto recent_large = tdx::normalize_recent_large_unlock_rows(
            recent_large_rows, unlock_securities);
        require(recent_large.size() == 2 &&
                recent_large.as_array()[0].at("detail_id").as_string() ==
                    "20260804600866" &&
                recent_large.as_array()[0].at("progress").as_string() == "已解禁" &&
                std::abs(recent_large.as_array()[0].at("unlock_to_total_pct")
                    .as_number() - 24.42) < 0.000001 &&
                recent_large.as_array()[1].at("security").at("market_id").as_number() == 2,
                "recent large unlock history normalizes ratios, dates, and BJ market");

        std::cout << "native tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native test failed: " << error.what() << '\n';
        return 1;
    }
}
