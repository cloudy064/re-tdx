#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t quote_body_size = 380;
constexpr float quote_epsilon = 0.0001F;

struct DepthLevel {
    float price{};
    float quantity_raw{};
};

struct QuoteAggregate {
    float average_bid_price{};
    float total_bid_quantity_raw{};
    float average_ask_price{};
    float total_ask_quantity_raw{};
};

struct HostBaseQuote {
    float pre_close_price{};
    float open_price{};
    float high_price{};
    float low_price{};
    float last_price{};
    std::uint32_t time_hhmmss_raw{};
    float special_volume_projected_raw{};
    std::uint32_t cumulative_volume_raw{};
    std::uint32_t last_positive_volume_delta_raw{};
    float amount_raw{};
    std::uint32_t first_volume_bucket_raw{};
    std::uint32_t second_volume_bucket_raw{};
    std::uint16_t host_status_flags_raw{};
    std::uint32_t host_auxiliary_142_raw{};
};

struct HostQuoteState {
    HostBaseQuote base;
    std::array<DepthLevel, 10> ask_levels;
    std::array<DepthLevel, 10> bid_levels;
    QuoteAggregate aggregate;
};

struct ProjectionContext {
    std::int32_t security_class_raw{};
    std::int32_t price_transition_guard_raw{};
    bool small_last_price_fallback_predicate_raw{};
    float special_volume_multiplier_raw{};
    std::uint32_t security_auxiliary_dword_73_raw{};
    std::uint16_t host_word_280_raw{};
};

struct SourceQuote {
    std::int64_t epoch_ms_raw{};
    double last_price{};
    double open_price{};
    double high_price{};
    double low_price{};
    double amount_raw{};
    std::uint64_t volume_raw{};
    std::uint64_t special_volume_raw{};
    std::uint32_t quote_kind_raw{};
    std::uint32_t auxiliary_68_raw{};
    std::uint32_t auxiliary_72_raw{};
    std::array<DepthLevel, 10> ask_levels;
    std::array<DepthLevel, 10> bid_levels;
    QuoteAggregate aggregate;
    std::int64_t aggregate_bid_guard_i64_raw{};
    std::int64_t aggregate_ask_guard_i64_raw{};
    std::uint32_t aggregate_bid_guard_tail_raw{};
    std::uint32_t aggregate_ask_guard_tail_raw{};
};

const Json& member(const Json& object, const char* name,
                   const std::string& path) {
    if (!object.is_object()) throw Error(path + " must be an object");
    const auto iterator = object.as_object().find(name);
    if (iterator == object.as_object().end())
        throw Error(path + " requires " + name);
    return iterator->second;
}

double finite_number(const Json& value, const std::string& path) {
    if (!value.is_number() || !std::isfinite(value.as_number()))
        throw Error(path + " must be a finite number");
    return value.as_number();
}

std::uint32_t u32_number(const Json& value, const std::string& path) {
    const auto number = finite_number(value, path);
    if (std::floor(number) != number || number < 0.0 ||
        number > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
        throw Error(path + " must be an unsigned 32-bit integer");
    return static_cast<std::uint32_t>(number);
}

std::uint16_t u16_number(const Json& value, const std::string& path) {
    const auto number = u32_number(value, path);
    if (number > std::numeric_limits<std::uint16_t>::max())
        throw Error(path + " must be an unsigned 16-bit integer");
    return static_cast<std::uint16_t>(number);
}

std::int32_t i32_number(const Json& value, const std::string& path) {
    const auto number = finite_number(value, path);
    if (std::floor(number) != number ||
        number < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
        number > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        throw Error(path + " must be a signed 32-bit integer");
    return static_cast<std::int32_t>(number);
}

float host_float(const Json& value, const std::string& path) {
    if (value.is_null()) return std::numeric_limits<float>::quiet_NaN();
    const auto number = finite_number(value, path);
    const auto narrowed = static_cast<float>(number);
    if (!std::isfinite(narrowed))
        throw Error(path + " must fit a finite 32-bit host float or be null");
    return narrowed;
}

bool boolean(const Json& value, const std::string& path) {
    if (!value.is_bool()) throw Error(path + " must be a boolean");
    return value.as_bool();
}

HostBaseQuote parse_base(const Json& value) {
    constexpr const char* path = "previous.base_quote";
    HostBaseQuote result;
    result.pre_close_price = host_float(member(value, "pre_close_price", path),
                                        std::string(path) + ".pre_close_price");
    result.open_price = host_float(member(value, "open_price", path),
                                   std::string(path) + ".open_price");
    result.high_price = host_float(member(value, "high_price", path),
                                   std::string(path) + ".high_price");
    result.low_price = host_float(member(value, "low_price", path),
                                  std::string(path) + ".low_price");
    result.last_price = host_float(member(value, "last_price", path),
                                   std::string(path) + ".last_price");
    result.time_hhmmss_raw = u32_number(member(value, "time_hhmmss_raw", path),
                                        std::string(path) + ".time_hhmmss_raw");
    result.special_volume_projected_raw = host_float(
        member(value, "special_volume_projected_raw", path),
        std::string(path) + ".special_volume_projected_raw");
    result.cumulative_volume_raw = u32_number(
        member(value, "cumulative_volume_raw", path),
        std::string(path) + ".cumulative_volume_raw");
    result.last_positive_volume_delta_raw = u32_number(
        member(value, "last_positive_volume_delta_raw", path),
        std::string(path) + ".last_positive_volume_delta_raw");
    result.amount_raw = host_float(member(value, "amount_raw", path),
                                   std::string(path) + ".amount_raw");
    result.first_volume_bucket_raw = u32_number(
        member(value, "first_volume_bucket_raw", path),
        std::string(path) + ".first_volume_bucket_raw");
    result.second_volume_bucket_raw = u32_number(
        member(value, "second_volume_bucket_raw", path),
        std::string(path) + ".second_volume_bucket_raw");
    result.host_status_flags_raw = u16_number(
        member(value, "host_status_flags_raw", path),
        std::string(path) + ".host_status_flags_raw");
    result.host_auxiliary_142_raw = u32_number(
        member(value, "host_auxiliary_142_raw", path),
        std::string(path) + ".host_auxiliary_142_raw");
    return result;
}

std::array<DepthLevel, 10> parse_levels(const Json& value,
                                        const std::string& path) {
    if (!value.is_array() || value.size() != 10)
        throw Error(path + " must contain exactly 10 levels");
    std::array<DepthLevel, 10> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto& row = value.as_array()[index];
        const auto row_path = path + "[" + std::to_string(index) + "]";
        const auto level = u32_number(member(row, "level", row_path),
                                      row_path + ".level");
        if (level != index + 1)
            throw Error(row_path + ".level must equal its one-based position");
        result[index].price = host_float(member(row, "price", row_path),
                                         row_path + ".price");
        result[index].quantity_raw = host_float(
            member(row, "quantity_raw", row_path),
            row_path + ".quantity_raw");
    }
    return result;
}

QuoteAggregate parse_aggregate(const Json& value) {
    constexpr const char* path = "previous.aggregate";
    QuoteAggregate result;
    result.average_bid_price = host_float(
        member(value, "average_bid_price", path),
        std::string(path) + ".average_bid_price");
    result.total_bid_quantity_raw = host_float(
        member(value, "total_bid_quantity_raw", path),
        std::string(path) + ".total_bid_quantity_raw");
    result.average_ask_price = host_float(
        member(value, "average_ask_price", path),
        std::string(path) + ".average_ask_price");
    result.total_ask_quantity_raw = host_float(
        member(value, "total_ask_quantity_raw", path),
        std::string(path) + ".total_ask_quantity_raw");
    return result;
}

HostQuoteState parse_previous(const Json& value) {
    if (!value.is_object()) throw Error("previous must be an object");
    const std::string previous_path = "previous";
    const auto& schema = member(value, "schema", previous_path);
    if (!schema.is_string() ||
        schema.as_string() != "tdx-level2-sdk-host-quote-state-v1")
        throw Error(
            "previous.schema must be tdx-level2-sdk-host-quote-state-v1");
    HostQuoteState result;
    result.base = parse_base(member(value, "base_quote", previous_path));
    result.ask_levels = parse_levels(member(value, "ask_levels", previous_path),
                                     "previous.ask_levels");
    result.bid_levels = parse_levels(member(value, "bid_levels", previous_path),
                                     "previous.bid_levels");
    result.aggregate = parse_aggregate(
        member(value, "aggregate", previous_path));
    return result;
}

ProjectionContext parse_context(const Json& value) {
    constexpr const char* path = "context";
    if (!value.is_object()) throw Error("context must be an object");
    ProjectionContext result;
    result.security_class_raw = i32_number(
        member(value, "security_class_raw", path),
        "context.security_class_raw");
    result.price_transition_guard_raw = i32_number(
        member(value, "price_transition_guard_raw", path),
        "context.price_transition_guard_raw");
    result.small_last_price_fallback_predicate_raw = boolean(
        member(value, "small_last_price_fallback_predicate_raw", path),
        "context.small_last_price_fallback_predicate_raw");
    result.special_volume_multiplier_raw = host_float(
        member(value, "special_volume_multiplier_raw", path),
        "context.special_volume_multiplier_raw");
    if (!std::isfinite(result.special_volume_multiplier_raw))
        throw Error("context.special_volume_multiplier_raw cannot be null");
    result.security_auxiliary_dword_73_raw = u32_number(
        member(value, "security_auxiliary_dword_73_raw", path),
        "context.security_auxiliary_dword_73_raw");
    result.host_word_280_raw = u16_number(
        member(value, "host_word_280_raw", path),
        "context.host_word_280_raw");
    return result;
}

std::uint32_t read_u32(const Bytes& body, std::size_t offset) {
    return static_cast<std::uint32_t>(body[offset]) |
           (static_cast<std::uint32_t>(body[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(body[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(body[offset + 3]) << 24U);
}

std::uint64_t read_u64(const Bytes& body, std::size_t offset) {
    const auto low = static_cast<std::uint64_t>(read_u32(body, offset));
    const auto high = static_cast<std::uint64_t>(read_u32(body, offset + 4));
    return low | (high << 32U);
}

std::int64_t read_i64(const Bytes& body, std::size_t offset) {
    const auto raw = read_u64(body, offset);
    std::int64_t result{};
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

float read_f32(const Bytes& body, std::size_t offset) {
    const auto raw = read_u32(body, offset);
    float result{};
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

double read_f64(const Bytes& body, std::size_t offset) {
    const auto raw = read_u64(body, offset);
    double result{};
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

SourceQuote parse_source(const Bytes& body) {
    SourceQuote result;
    result.epoch_ms_raw = read_i64(body, 0);
    result.last_price = read_f64(body, 8);
    result.open_price = read_f64(body, 16);
    result.high_price = read_f64(body, 24);
    result.low_price = read_f64(body, 32);
    result.amount_raw = read_f64(body, 40);
    result.volume_raw = read_u64(body, 48);
    result.special_volume_raw = read_u64(body, 56);
    result.quote_kind_raw = read_u32(body, 64);
    result.auxiliary_68_raw = read_u32(body, 68);
    result.auxiliary_72_raw = read_u32(body, 72);
    for (std::size_t index = 0; index < 10; ++index) {
        result.ask_levels[index] = {
            static_cast<float>(read_f64(body, 108 + 8 * index)),
            read_f32(body, 188 + 4 * index)};
        result.bid_levels[index] = {
            static_cast<float>(read_f64(body, 228 + 8 * index)),
            read_f32(body, 308 + 4 * index)};
    }
    result.aggregate = {
        static_cast<float>(read_f64(body, 348)), read_f32(body, 356),
        static_cast<float>(read_f64(body, 364)), read_f32(body, 372)};
    result.aggregate_bid_guard_i64_raw = read_i64(body, 356);
    result.aggregate_ask_guard_i64_raw = read_i64(body, 372);
    result.aggregate_bid_guard_tail_raw = read_u32(body, 360);
    result.aggregate_ask_guard_tail_raw = read_u32(body, 376);
    return result;
}

Json host_number(float value) {
    return std::isfinite(value) ? Json(static_cast<double>(value)) : Json(nullptr);
}

Json base_document(const HostBaseQuote& base) {
    Json result = Json::object();
    result["pre_close_price"] = host_number(base.pre_close_price);
    result["open_price"] = host_number(base.open_price);
    result["high_price"] = host_number(base.high_price);
    result["low_price"] = host_number(base.low_price);
    result["last_price"] = host_number(base.last_price);
    result["time_hhmmss_raw"] = static_cast<std::uint64_t>(base.time_hhmmss_raw);
    result["special_volume_projected_raw"] =
        host_number(base.special_volume_projected_raw);
    result["cumulative_volume_raw"] =
        static_cast<std::uint64_t>(base.cumulative_volume_raw);
    result["last_positive_volume_delta_raw"] =
        static_cast<std::uint64_t>(base.last_positive_volume_delta_raw);
    result["amount_raw"] = host_number(base.amount_raw);
    result["first_volume_bucket_raw"] =
        static_cast<std::uint64_t>(base.first_volume_bucket_raw);
    result["second_volume_bucket_raw"] =
        static_cast<std::uint64_t>(base.second_volume_bucket_raw);
    result["host_status_flags_raw"] =
        static_cast<std::uint64_t>(base.host_status_flags_raw);
    result["host_auxiliary_142_raw"] =
        static_cast<std::uint64_t>(base.host_auxiliary_142_raw);
    return result;
}

Json levels_document(const std::array<DepthLevel, 10>& levels) {
    Json result = Json::array();
    for (std::size_t index = 0; index < levels.size(); ++index) {
        Json row = Json::object();
        row["level"] = static_cast<std::uint64_t>(index + 1);
        row["price"] = host_number(levels[index].price);
        row["quantity_raw"] = host_number(levels[index].quantity_raw);
        result.push_back(std::move(row));
    }
    return result;
}

Json aggregate_document(const QuoteAggregate& aggregate) {
    Json result = Json::object();
    result["average_bid_price"] = host_number(aggregate.average_bid_price);
    result["total_bid_quantity_raw"] =
        host_number(aggregate.total_bid_quantity_raw);
    result["average_ask_price"] = host_number(aggregate.average_ask_price);
    result["total_ask_quantity_raw"] =
        host_number(aggregate.total_ask_quantity_raw);
    return result;
}

Json state_document(const HostQuoteState& state) {
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-host-quote-state-v1";
    result["base_quote"] = base_document(state.base);
    result["ask_levels"] = levels_document(state.ask_levels);
    result["bid_levels"] = levels_document(state.bid_levels);
    result["aggregate"] = aggregate_document(state.aggregate);
    return result;
}

std::pair<std::uint32_t, bool> local_hhmmss(std::int64_t epoch_ms) {
    // sub_689E80 receives __time64_t and divides as a signed value.  Thus
    // -1 ms truncates toward zero and resolves to the local Unix epoch.
    const auto seconds = epoch_ms / 1000;
    if (seconds < static_cast<std::int64_t>(
                      std::numeric_limits<std::time_t>::min()) ||
        seconds > static_cast<std::int64_t>(
                      std::numeric_limits<std::time_t>::max()))
        return {0, false};
    const auto time = static_cast<std::time_t>(seconds);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &time)) return {0, false};
#else
    if (!localtime_r(&time, &local)) return {0, false};
#endif
    return {static_cast<std::uint32_t>(
                local.tm_sec + 100 * (local.tm_min + 100 * local.tm_hour)),
            true};
}

int quote_kind_class(std::uint32_t raw) {
    switch (raw) {
    case 11: return 1;
    case 12: return 2;
    case 13: return 3;
    case 14: return 13;
    case 15:
    case 23: return 5;
    case 16: return 12;
    case 17: return 8;
    case 18: return 4;
    case 19: return 9;
    case 20: return 10;
    case 22: return 7;
    default: return 0;
    }
}

int price_transition_case(std::int32_t guard_raw, float last_price,
                          float previous_bid1, float previous_ask1,
                          float current_bid1, float current_ask1) {
    if (guard_raw != 0) return 0;
    if (previous_bid1 < quote_epsilon && previous_ask1 < quote_epsilon)
        return 1;
    if (std::fabs(previous_ask1 - previous_bid1) < quote_epsilon) return 2;
    if (previous_bid1 < quote_epsilon)
        return current_bid1 >= quote_epsilon ? 8 : 4;
    if (previous_ask1 >= quote_epsilon) {
        if (current_bid1 < quote_epsilon && current_ask1 > quote_epsilon)
            return 6;
        if (current_bid1 > quote_epsilon && current_ask1 < quote_epsilon)
            return 5;
        float midpoint = static_cast<float>(
            (static_cast<double>(previous_ask1) + previous_bid1) * 0.5);
        if (midpoint < quote_epsilon && current_bid1 > quote_epsilon &&
            current_ask1 > quote_epsilon)
            midpoint = static_cast<float>(
                (static_cast<double>(current_ask1) + current_bid1) * 0.5);
        if (midpoint >= quote_epsilon) {
            const auto midpoint_wide = static_cast<double>(midpoint);
            const auto epsilon_wide = static_cast<double>(quote_epsilon);
            const auto last_wide = static_cast<double>(last_price);
            if (midpoint_wide + epsilon_wide < last_wide) return 9;
            if (midpoint_wide - epsilon_wide > last_wide) return 10;
        }
        return 0;
    }
    return current_ask1 >= quote_epsilon ? 7 : 3;
}

int price_transition_class(int transition_case) {
    switch (transition_case) {
    case 3:
    case 6:
    case 7:
    case 10: return 1;
    case 4:
    case 5:
    case 8:
    case 9: return 0;
    default: return 2;
    }
}

std::int32_t wrapped_volume_delta(std::uint32_t current,
                                  std::uint32_t previous) {
    const auto raw = static_cast<std::uint32_t>(current - previous);
    std::int32_t result{};
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

}  // namespace

Json project_level2_sdk_quote_transition(
    const Level2SdkQuoteTransitionRequest& request) {
    const auto data_type = static_cast<int>(request.data_type);
    if (request.data_type != Level2SdkCallbackDataType::quote_update &&
        request.data_type != Level2SdkCallbackDataType::extended_quote)
        throw Error("SDK quote transition data-type must be 1807 or 18071");
    if (request.body.size() != quote_body_size)
        throw Error("SDK quote transition " + std::to_string(data_type) +
                    " requires exactly 380 body bytes");

    const auto previous = parse_previous(request.previous);
    const auto context = parse_context(request.context);
    const auto source = parse_source(request.body);
    auto candidate = previous;

    const auto [host_time, time_conversion_valid] =
        local_hhmmss(source.epoch_ms_raw);
    candidate.base.time_hhmmss_raw = host_time;
    const auto kind_class = quote_kind_class(source.quote_kind_raw);
    candidate.base.host_status_flags_raw = static_cast<std::uint16_t>(
        candidate.base.host_status_flags_raw ^
        ((candidate.base.host_status_flags_raw ^ (4U * kind_class)) & 0x3CU));

    const bool special_security_branch =
        context.security_class_raw == 22 || context.security_class_raw == 23;
    if (special_security_branch) {
        std::int64_t signed_volume{};
        std::memcpy(&signed_volume, &source.volume_raw, sizeof(signed_volume));
        candidate.base.special_volume_projected_raw = static_cast<float>(
            static_cast<double>(signed_volume) *
            context.special_volume_multiplier_raw);
        candidate.base.host_auxiliary_142_raw =
            context.security_auxiliary_dword_73_raw + source.auxiliary_72_raw;
    } else {
        candidate.base.last_price = static_cast<float>(source.last_price);
        candidate.base.open_price = static_cast<float>(source.open_price);
        candidate.base.high_price = static_cast<float>(source.high_price);
        candidate.base.low_price = static_cast<float>(source.low_price);
        candidate.base.amount_raw = static_cast<float>(source.amount_raw);
        candidate.base.cumulative_volume_raw =
            static_cast<std::uint32_t>(source.volume_raw);
        candidate.base.pre_close_price =
            static_cast<float>(read_f64(request.body, 76));
        candidate.base.host_auxiliary_142_raw = source.auxiliary_72_raw;
    }

    candidate.ask_levels = source.ask_levels;
    candidate.bid_levels = source.bid_levels;
    const bool aggregate_update_applied = context.host_word_280_raw <= 1U &&
        (source.aggregate_bid_guard_i64_raw > 0 ||
         source.aggregate_ask_guard_i64_raw > 0);
    if (aggregate_update_applied) candidate.aggregate = source.aggregate;

    const auto volume_delta = wrapped_volume_delta(
        candidate.base.cumulative_volume_raw,
        previous.base.cumulative_volume_raw);
    std::optional<int> transition_case;
    std::optional<int> transition_class;
    std::uint32_t first_bucket_delta = 0;
    std::uint32_t second_bucket_delta = 0;
    if (volume_delta > 0) {
        transition_case = price_transition_case(
            context.price_transition_guard_raw, candidate.base.last_price,
            previous.bid_levels[0].price, previous.ask_levels[0].price,
            candidate.bid_levels[0].price, candidate.ask_levels[0].price);
        transition_class = price_transition_class(*transition_case);
        candidate.base.host_status_flags_raw = static_cast<std::uint16_t>(
            candidate.base.host_status_flags_raw ^
            ((candidate.base.host_status_flags_raw ^ *transition_class) & 3U));
        const auto positive_delta = static_cast<std::uint32_t>(volume_delta);
        if (*transition_class == 1) {
            first_bucket_delta = positive_delta;
        } else if (*transition_class == 2) {
            second_bucket_delta = positive_delta / 2U;
            first_bucket_delta = positive_delta - second_bucket_delta;
        } else {
            second_bucket_delta = positive_delta;
        }
        candidate.base.first_volume_bucket_raw += first_bucket_delta;
        candidate.base.second_volume_bucket_raw += second_bucket_delta;
        candidate.base.last_positive_volume_delta_raw = positive_delta;
    }

    const bool base_quote_update_applied =
        !(candidate.base.time_hhmmss_raw < previous.base.time_hhmmss_raw &&
          candidate.base.cumulative_volume_raw <
              previous.base.cumulative_volume_raw);
    HostQuoteState projected = base_quote_update_applied ? candidate : previous;
    if (!base_quote_update_applied) {
        for (std::size_t index = 5; index < 10; ++index) {
            projected.ask_levels[index] = candidate.ask_levels[index];
            projected.bid_levels[index] = candidate.bid_levels[index];
        }
        projected.aggregate = candidate.aggregate;
    }

    // sub_52CB00 loads the stored float and compares the promoted value with
    // double 0.009.  Keep that mixed-precision boundary: float(0.009) is
    // slightly below the double literal and therefore still enters fallback.
    const bool small_last_price_fallback_applied =
        static_cast<double>(projected.base.last_price) < 0.009 &&
        projected.base.last_price > quote_epsilon &&
        context.small_last_price_fallback_predicate_raw;
    if (small_last_price_fallback_applied)
        projected.base.last_price = projected.base.pre_close_price;

    Json source_raw = Json::object();
    source_raw["time_epoch_ms_raw"] = source.epoch_ms_raw;
    source_raw["volume_raw"] = source.volume_raw;
    source_raw["special_volume_raw"] = source.special_volume_raw;
    source_raw["special_volume_host_projection_semantics"] = Json(nullptr);
    source_raw["quote_kind_raw"] =
        static_cast<std::uint64_t>(source.quote_kind_raw);
    source_raw["auxiliary_68_raw"] =
        static_cast<std::uint64_t>(source.auxiliary_68_raw);
    source_raw["auxiliary_68_host_projection_semantics"] = Json(nullptr);
    source_raw["auxiliary_72_raw"] =
        static_cast<std::uint64_t>(source.auxiliary_72_raw);
    source_raw["aggregate_bid_guard_i64_raw"] =
        static_cast<std::int64_t>(source.aggregate_bid_guard_i64_raw);
    source_raw["aggregate_ask_guard_i64_raw"] =
        static_cast<std::int64_t>(source.aggregate_ask_guard_i64_raw);
    source_raw["aggregate_bid_guard_tail_raw"] =
        static_cast<std::uint64_t>(source.aggregate_bid_guard_tail_raw);
    source_raw["aggregate_ask_guard_tail_raw"] =
        static_cast<std::uint64_t>(source.aggregate_ask_guard_tail_raw);

    Json transition = Json::object();
    transition["time_conversion_valid"] = time_conversion_valid;
    transition["host_time_hhmmss_raw"] =
        static_cast<std::uint64_t>(host_time);
    transition["host_time_advanced"] =
        host_time > previous.base.time_hhmmss_raw;
    transition["security_class_raw"] =
        static_cast<std::int64_t>(context.security_class_raw);
    transition["special_security_branch"] = special_security_branch;
    transition["quote_kind_class_raw"] = kind_class;
    transition["price_transition_evaluated"] = volume_delta > 0;
    transition["price_transition_case_raw"] = transition_case
        ? Json(*transition_case) : Json(nullptr);
    transition["price_transition_class_raw"] = transition_class
        ? Json(*transition_class) : Json(nullptr);
    transition["observed_volume_delta_raw"] =
        static_cast<std::int64_t>(volume_delta);
    transition["positive_volume_delta_applied"] = volume_delta > 0;
    transition["first_volume_bucket_delta_raw"] =
        static_cast<std::uint64_t>(first_bucket_delta);
    transition["second_volume_bucket_delta_raw"] =
        static_cast<std::uint64_t>(second_bucket_delta);
    transition["aggregate_update_applied"] = aggregate_update_applied;
    transition["base_quote_update_applied"] = base_quote_update_applied;
    transition["extended_depth_update_applied"] = true;
    transition["small_last_price_fallback_applied"] =
        small_last_price_fallback_applied;

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-quote-transition-v1";
    result["format"] = "sdk-quote-transition";
    result["normalization_kind"] =
        "offline-sub_68C750-host-quote-transition";
    result["data_type"] = data_type;
    result["body_size"] = static_cast<std::uint64_t>(request.body.size());
    result["source_raw"] = std::move(source_raw);
    result["transition"] = std::move(transition);
    result["candidate_state"] = state_document(candidate);
    result["projected_state"] = state_document(projected);
    result["input_body_retained"] = false;
    result["sdk_called"] = false;
    result["sdk_callback_invoked"] = false;
    result["sdk_callback_executed"] = false;
    result["callback_executed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["transport_scope"] =
        "authorized 1807/18071 callback snapshots supplied offline; no SDK, "
        "callback, host message, request construction, or network operation";
    result["evidence"] =
        "TdxW sub_68C750 and quote helpers sub_52CB00/sub_689E80/"
        "sub_689ED0/sub_689F20/sub_68A970";
    result["evidence_sources"] = Json::array();
    result["evidence_sources"].push_back(
        "output/ida-tdxw-level2-remaining-sdk-20260812.log:749-904");
    result["evidence_sources"].push_back(
        "output/ida-tdxw-level2-quote-helpers-20260812.log:101-277");
    return result;
}

}  // namespace tdx
