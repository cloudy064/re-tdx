#include "level2_tcalc_order_flow.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

namespace tdx::level2_detail {
namespace {

constexpr std::size_t record_size = 184;
constexpr std::size_t order_side_record_size = 104;
constexpr std::uint32_t tcalc_missing_bits = 0xf8f8f8f8U;

float read_float(const Bytes& payload, std::size_t offset) {
    return read_f32_le(payload.data() + offset);
}

bool is_tcalc_missing(float value) {
    static_assert(sizeof(value) == sizeof(tcalc_missing_bits));
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits == tcalc_missing_bits;
}

Json numeric(float value) {
    if (!std::isfinite(value) || is_tcalc_missing(value)) return nullptr;
    return static_cast<double>(value);
}

void bind(Json& bindings, const std::string& name, float value) {
    bindings[name] = numeric(value);
}

void bind_count(Json& bindings, const std::string& name, float value) {
    const auto adjusted = static_cast<double>(value) + 0.503000020980835;
    if (!std::isfinite(value) || is_tcalc_missing(value) ||
        adjusted < static_cast<double>(std::numeric_limits<int>::min()) ||
        adjusted > static_cast<double>(std::numeric_limits<int>::max())) {
        bindings[name] = nullptr;
        return;
    }
    bindings[name] = static_cast<double>(static_cast<int>(adjusted));
}

void bind_guarded(Json& bindings, const std::string& name,
                  float anchor, float value) {
    if (is_tcalc_missing(anchor)) bindings[name] = nullptr;
    else bind(bindings, name, value);
}

void bind_count_guarded(Json& bindings, const std::string& name,
                        float anchor, float value) {
    if (is_tcalc_missing(anchor)) bindings[name] = nullptr;
    else bind_count(bindings, name, value);
}

Json matrix(const Bytes& payload, std::size_t base, std::size_t width,
            std::size_t height) {
    Json rows = Json::array();
    for (std::size_t first = 0; first < height; ++first) {
        Json row = Json::array();
        for (std::size_t second = 0; second < width; ++second)
            row.push_back(numeric(read_float(
                payload, base + 4 * (first * width + second))));
        rows.push_back(std::move(row));
    }
    return rows;
}

float sum_direction(const Bytes& payload, std::size_t base,
                    std::size_t direction) {
    float result = 0.0f;
    for (std::size_t size_class = 0; size_class < 4; ++size_class)
        result += read_float(
            payload, base + 4 * (size_class * 4 + direction));
    return result;
}

std::string formula_stamp(std::uint32_t raw) {
    const int year = static_cast<int>(raw / 10000);
    const int month = static_cast<int>((raw / 100) % 100);
    const int day = static_cast<int>(raw % 100);
    if (year < 1900 || year > 9999 || month < 1 || month > 12 || day < 1)
        return {};
    static constexpr int month_days[]{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maximum = month_days[month - 1];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        maximum = 29;
    if (day > maximum) return {};
    char text[17]{};
    std::snprintf(text, sizeof(text), "%04d-%02d-%02d|15:00", year, month, day);
    return text;
}

}  // namespace

Json decode_tcalc_order_flow_document(const Bytes& payload, int limit) {
    if (payload.empty() || payload.size() % record_size != 0)
        throw Error("TCalc Level2 type-31 payload must contain complete 184-byte records");
    const auto count = payload.size() / record_size;
    const auto selected = std::min<std::size_t>(
        count, static_cast<std::size_t>(limit));
    Json records = Json::array();
    Json series = Json::object();
    std::size_t context_records = 0;
    std::size_t invalid_context_dates = 0;
    for (std::size_t index = 0; index < selected; ++index) {
        const auto base = index * record_size;
        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["date_raw"] = static_cast<std::uint64_t>(
            read_u32_le(payload.data() + base));
        row["host_auxiliary_f32"] = numeric(read_float(payload, base + 4));
        row["l2_vol"] = matrix(payload, base + 8, 4, 4);
        row["l2_amo"] = matrix(payload, base + 72, 4, 4);
        row["l2_volnum_raw"] = matrix(payload, base + 136, 2, 2);

        Json bindings = Json::object();
        for (int size_class = 0; size_class < 4; ++size_class) {
            for (int direction = 0; direction < 4; ++direction) {
                const auto suffix = "#" + std::to_string(size_class) + "#" +
                                    std::to_string(direction);
                bind(bindings, "L2_VOL" + suffix,
                     read_float(payload, base + 8 +
                         4 * (size_class * 4 + direction)));
                bind(bindings, "L2_AMO" + suffix,
                     read_float(payload, base + 72 +
                         4 * (size_class * 4 + direction)));
            }
        }
        for (int first = 0; first < 2; ++first) {
            for (int second = 0; second < 2; ++second) {
                const auto raw = read_float(
                    payload, base + 136 + 4 * (first * 2 + second));
                const auto name = "L2_VOLNUM#" + std::to_string(first) + "#" +
                                  std::to_string(second);
                bind_count(bindings, name, raw);
            }
        }
        const auto large_in_volume = read_float(payload, base + 8);
        const auto large_out_volume = read_float(payload, base + 12);
        const auto active_in_volume = read_float(payload, base + 16);
        const auto active_out_volume = read_float(payload, base + 20);
        bind_guarded(bindings, "ACTINVOL", active_in_volume,
                     sum_direction(payload, base + 8, 2));
        bind_guarded(bindings, "ACTOUTVOL", active_out_volume,
                     sum_direction(payload, base + 8, 3));
        bind_guarded(bindings, "LARGEINTRDVOL", large_in_volume,
                     large_in_volume + read_float(payload, base + 24));
        bind_guarded(bindings, "LARGEOUTTRDVOL", large_out_volume,
                     large_out_volume + read_float(payload, base + 28));
        bind(bindings, "BIDORDERVOL", read_float(payload, base + 152));
        bind(bindings, "BIDCANCELVOL", read_float(payload, base + 156));
        bind(bindings, "OFFERORDERVOL", read_float(payload, base + 160));
        bind(bindings, "OFFERCANCELVOL", read_float(payload, base + 164));
        bind(bindings, "AVGBIDPX", read_float(payload, base + 168));
        bind(bindings, "AVGOFFERPX", read_float(payload, base + 172));
        bind(bindings, "CUR_BUYORDER", read_float(payload, base + 176));
        bind(bindings, "CUR_SELLORDER", read_float(payload, base + 180));
        const auto large_in_count = read_float(payload, base + 136);
        const auto large_out_count = read_float(payload, base + 140);
        bind_count(bindings, "TRADENUM", read_float(payload, base + 4));
        bind_count_guarded(bindings, "TRADEINNUM", large_in_count,
                           large_in_count + read_float(payload, base + 144));
        bind_count_guarded(bindings, "TRADEOUTNUM", large_out_count,
                           large_out_count + read_float(payload, base + 148));
        bind_count(bindings, "LARGETRDINNUM", large_in_count);
        bind_count(bindings, "LARGETRDOUTNUM", large_out_count);
        const auto stamp = formula_stamp(read_u32_le(payload.data() + base));
        if (stamp.empty()) {
            ++invalid_context_dates;
        } else {
            ++context_records;
            for (const auto& [name, value] : bindings.as_object())
                series[name][stamp] = value;
            row["formula_context_stamp"] = stamp;
        }
        row["formula_bindings"] = std::move(bindings);
        records.push_back(std::move(row));
    }

    Json result = Json::object();
    result["schema"] = "tdx-level2-tcalc-order-flow-v1";
    result["tdx_callback_type"] = 31;
    result["record_size"] = static_cast<std::uint64_t>(record_size);
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    result["truncated"] = selected < count;
    result["series"] = std::move(series);
    Json context = Json::object();
    context["schema"] = "tdx-formula-explicit-context-from-tcalc-l2-v1";
    context["direct_context_file"] = true;
    context["stamp_source"] = "type-31 YYYYMMDD date plus daily K-line close time 15:00";
    context["record_count"] = static_cast<std::uint64_t>(context_records);
    context["invalid_date_record_count"] =
        static_cast<std::uint64_t>(invalid_context_dates);
    context["binding_count"] = static_cast<std::uint64_t>(
        result.at("series").size());
    context["context_complete"] = selected == count;
    context["period_scope"] = "daily";
    context["usage"] =
        "Pass this entire decoder JSON to formulas evaluate --context-file";
    context["authorization_boundary"] =
        "Values are copied only from caller-owned TCalc type-31 callback records";
    result["_formula_context"] = std::move(context);
    result["unavailable_binding"] = "ISBUYORDER";
    result["unavailable_binding_reason"] =
        "ISBUYORDER is TCalc callback type 104 byte 46, not part of type-31 records";
    result["evidence"] =
        "TCalc sub_100353B0 and handlers 0x10036D40..0x10037570";
    return result;
}

Json decode_tcalc_order_side_document(const Bytes& payload) {
    if (payload.size() != order_side_record_size)
        throw Error("TCalc Level2 type-104 payload must contain exactly 104 bytes");
    const auto direction = payload[46];
    const double is_buy_order = direction == 0 ? 1.0 : 0.0;
    Json result = Json::object();
    result["schema"] = "tdx-level2-tcalc-order-side-v1";
    result["tdx_callback_type"] = 104;
    result["record_size"] = static_cast<std::uint64_t>(order_side_record_size);
    result["direction_byte_raw"] = static_cast<std::uint64_t>(direction);
    result["is_buy_order"] = is_buy_order;
    result["formula_scalar_bindings"] = Json::object();
    result["formula_scalar_bindings"]["ISBUYORDER"] = is_buy_order;
    result["direct_context_file"] = true;
    result["binding_scope"] =
        "TCalc repeats byte46==0 across every evaluated bar";
    result["authorization_boundary"] =
        "Value is copied only from a caller-owned TCalc type-104 callback body";
    result["evidence"] = "TCalc sub_1002AA70 -> sub_1000FD60(type=104)";
    return result;
}

}  // namespace tdx::level2_detail
