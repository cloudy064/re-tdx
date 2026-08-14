#include "tdx/auction.hpp"

#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uint16_t type_auction_series = 0x056A;
constexpr std::uint16_t type_heartbeat = 0x0004;
constexpr std::size_t auction_record_size = 16;
constexpr std::uint32_t maximum_limit = 5000;

struct SecurityCode {
    int market_id{};
    std::string code;
    std::pair<int, std::string> key() const { return {market_id, code}; }
};

void append_u32(Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t parse_u32(const std::string& text, std::string_view name,
                        std::uint32_t minimum = 0,
                        std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max()) {
    try {
        std::size_t used = 0;
        const auto value = std::stoull(text, &used, 0);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return static_cast<std::uint32_t>(value);
    } catch (...) {
        throw Error(std::string(name) + " is outside the allowed uint32 range");
    }
}

int parse_timeout(const std::string& text) {
    const auto value = parse_u32(text, "--timeout-ms", 1, 600000);
    return static_cast<int>(value);
}

SecurityCode parse_security(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    int market = -1;
    std::string code;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        const auto prefix = value.substr(0, colon);
        code = value.substr(colon + 1);
        if (prefix == "sz" || prefix == "0") market = 0;
        else if (prefix == "sh" || prefix == "1") market = 1;
        else if (prefix == "bj" || prefix == "2") market = 2;
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        market = value.rfind("sz", 0) == 0 ? 0 : value.rfind("sh", 0) == 0 ? 1 : 2;
        code = value.substr(2);
    } else {
        code = value;
        if (!code.empty() && (code.front() == '6' || code.front() == '9')) market = 1;
        else if (!code.empty() && code.front() == '8') market = 2;
        else market = 0;
    }
    if (market < 0 || market > 2 || code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("invalid auction security: " + value);
    return {market, code};
}

std::string security_id(int market_id, const std::string& code) {
    return std::array<std::string, 3>{"SZ", "SH", "BJ"}.at(
        static_cast<std::size_t>(market_id)) + code;
}

std::string time_label(int minute_of_day, int second) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << minute_of_day / 60 << ':'
           << std::setw(2) << minute_of_day % 60 << ':' << std::setw(2) << second;
    return output.str();
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

std::string validate_trade_date(std::uint32_t value) {
    const int year = static_cast<int>(value / 10000);
    const int month = static_cast<int>(value / 100 % 100);
    const int day = static_cast<int>(value % 100);
    static const std::array<int, 12> month_days{31, 28, 31, 30, 31, 30,
                                                31, 31, 30, 31, 30, 31};
    if (year < 2000 || year > 2200 || month < 1 || month > 12)
        throw Error("server trade date is invalid: " + std::to_string(value));
    int maximum = month_days[static_cast<std::size_t>(month - 1)];
    const bool leap = year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
    if (month == 2 && leap) ++maximum;
    if (day < 1 || day > maximum)
        throw Error("server trade date is invalid: " + std::to_string(value));
    return std::to_string(value);
}

Json point_json(const AuctionPoint& point) {
    Json value = Json::object();
    value["index"] = static_cast<std::uint64_t>(point.index);
    value["minute_of_day_raw"] = static_cast<std::uint64_t>(point.minute_of_day_raw);
    value["second_raw"] = static_cast<std::uint64_t>(point.second_raw);
    value["time_label"] = point.time_label;
    value["time_seconds"] = point.time_seconds;
    value["price"] = point.price;
    value["matched_volume_hand"] = static_cast<std::uint64_t>(point.matched_volume_hand);
    value["matched_amount_yuan"] = point.price * point.matched_volume_hand * 100.0;
    value["unmatched_signed_hand"] = static_cast<std::int64_t>(point.unmatched_signed_hand);
    value["unmatched_volume_hand"] = static_cast<std::uint64_t>(point.unmatched_volume_hand);
    value["unmatched_direction_raw"] = point.unmatched_direction_raw;
    value["unmatched_direction"] = point.unmatched_direction;
    value["reserved_zero_0e"] = static_cast<std::uint64_t>(point.reserved_zero_0e);
    return value;
}

Json segment_summary(const std::vector<const AuctionPoint*>& points) {
    Json result = Json::object();
    result["point_count"] = static_cast<std::uint64_t>(points.size());
    if (points.empty()) {
        for (const auto* key : {"start_time", "end_time", "first_price", "last_sample_price",
                                "min_price", "max_price", "last_sample_matched_volume_hand",
                                "last_sample_matched_amount_yuan",
                                "last_sample_unmatched_signed_hand",
                                "last_sample_unmatched_volume_hand",
                                "last_sample_unmatched_direction_raw", "max_unmatched_volume_hand",
                                "max_unmatched_time"}) result[key] = nullptr;
        result["unmatched_direction_flips"] = 0;
        result["matched_volume_monotonic_violations"] = 0;
        return result;
    }
    const auto* first = points.front();
    const auto* last = points.back();
    double minimum_price = first->price, maximum_price = first->price;
    const AuctionPoint* maximum_unmatched = first;
    int flips = 0, violations = 0, previous_direction = 0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto* point = points[index];
        minimum_price = std::min(minimum_price, point->price);
        maximum_price = std::max(maximum_price, point->price);
        if (point->unmatched_volume_hand > maximum_unmatched->unmatched_volume_hand)
            maximum_unmatched = point;
        if (point->unmatched_direction_raw != 0) {
            if (previous_direction != 0 && previous_direction != point->unmatched_direction_raw) ++flips;
            previous_direction = point->unmatched_direction_raw;
        }
        if (index > 0 && point->matched_volume_hand < points[index - 1]->matched_volume_hand)
            ++violations;
    }
    result["start_time"] = first->time_label;
    result["end_time"] = last->time_label;
    result["first_price"] = first->price;
    result["last_sample_price"] = last->price;
    result["min_price"] = minimum_price;
    result["max_price"] = maximum_price;
    result["last_sample_matched_volume_hand"] =
        static_cast<std::uint64_t>(last->matched_volume_hand);
    result["last_sample_matched_amount_yuan"] =
        last->price * last->matched_volume_hand * 100.0;
    result["last_sample_unmatched_signed_hand"] =
        static_cast<std::int64_t>(last->unmatched_signed_hand);
    result["last_sample_unmatched_volume_hand"] =
        static_cast<std::uint64_t>(last->unmatched_volume_hand);
    result["last_sample_unmatched_direction_raw"] = last->unmatched_direction_raw;
    result["unmatched_direction_flips"] = flips;
    result["max_unmatched_volume_hand"] =
        static_cast<std::uint64_t>(maximum_unmatched->unmatched_volume_hand);
    result["max_unmatched_time"] = maximum_unmatched->time_label;
    result["matched_volume_monotonic_violations"] = violations;
    return result;
}

Json series_summary(const AuctionSeries& series) {
    std::vector<const AuctionPoint*> opening, closing;
    for (const auto& point : series.points)
        (point.minute_of_day_raw < 12 * 60 ? opening : closing).push_back(&point);
    int largest_gap = 0;
    const AuctionPoint* gap_after = nullptr;
    const AuctionPoint* gap_before = nullptr;
    int reserved_nonzero = 0;
    for (std::size_t index = 0; index < series.points.size(); ++index) {
        if (series.points[index].reserved_zero_0e != 0) ++reserved_nonzero;
        if (index > 0) {
            const int gap = series.points[index].time_seconds - series.points[index - 1].time_seconds;
            if (gap > largest_gap) {
                largest_gap = gap;
                gap_after = &series.points[index - 1];
                gap_before = &series.points[index];
            }
        }
    }
    Json result = Json::object();
    result["point_count"] = static_cast<std::uint64_t>(series.points.size());
    result["opening"] = segment_summary(opening);
    result["closing"] = segment_summary(closing);
    result["largest_gap_seconds"] = largest_gap;
    result["largest_gap_after"] = gap_after ? Json(gap_after->time_label) : Json(nullptr);
    result["largest_gap_before"] = gap_before ? Json(gap_before->time_label) : Json(nullptr);
    result["reserved_nonzero_points"] = reserved_nonzero;
    return result;
}

Json series_json(const AuctionSeries& series,
                 const std::map<std::pair<int, std::string>, Security>& names) {
    Json value = Json::object();
    value["market_id"] = series.market_id;
    value["code"] = series.code;
    value["security_id"] = security_id(series.market_id, series.code);
    const auto name = names.find({series.market_id, series.code});
    value["name"] = name == names.end() ? "" : name->second.name;
    value["name_resolved"] = name != names.end();
    value["selector"] = static_cast<std::uint64_t>(series.selector);
    value["start_raw"] = static_cast<std::uint64_t>(series.start_raw);
    value["limit"] = static_cast<std::uint64_t>(series.limit);
    value["summary"] = series_summary(series);
    Json points = Json::array();
    for (const auto& point : series.points) points.push_back(point_json(point));
    value["points"] = std::move(points);
    return value;
}

}  // namespace

Bytes build_auction_request_data(int market_id, std::string_view code,
                                 std::uint32_t selector, std::uint32_t start_raw,
                                 std::uint32_t limit) {
    if (market_id < 0 || market_id > 2 || code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("invalid auction security identifier");
    if (limit < 1 || limit > maximum_limit) throw Error("auction limit must be in 1..5000");
    Bytes result{static_cast<std::uint8_t>(market_id), 0};
    result.insert(result.end(), code.begin(), code.end());
    for (const auto value : {std::uint32_t{0}, selector, std::uint32_t{0}, start_raw, limit})
        append_u32(result, value);
    return result;
}

AuctionSeries parse_auction_payload(const Bytes& payload, int market_id,
                                    const std::string& code, std::uint32_t selector,
                                    std::uint32_t start_raw, std::uint32_t limit) {
    (void)build_auction_request_data(market_id, code, selector, start_raw, limit);
    if (payload.size() < 2) throw Error("auction response is shorter than two bytes");
    const auto count = read_u16_le(payload.data());
    const auto expected = 2 + static_cast<std::size_t>(count) * auction_record_size;
    if (payload.size() != expected)
        throw Error("auction response length mismatch: expected " + std::to_string(expected) +
                    ", got " + std::to_string(payload.size()));
    AuctionSeries series{market_id, code, selector, start_raw, limit, {}};
    series.points.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto* record = payload.data() + 2 + static_cast<std::size_t>(index) * auction_record_size;
        AuctionPoint point;
        point.index = index;
        point.minute_of_day_raw = read_u16_le(record);
        point.price = read_f32_le(record + 2);
        point.matched_volume_hand = read_u32_le(record + 6);
        point.unmatched_signed_hand = read_i32_le(record + 10);
        point.reserved_zero_0e = record[14];
        point.second_raw = record[15];
        if (point.minute_of_day_raw >= 24 * 60 || point.second_raw >= 60)
            throw Error(security_id(market_id, code) + " auction point has an invalid time");
        if (!std::isfinite(point.price) || point.price < 0)
            throw Error(security_id(market_id, code) + " auction point has an invalid price");
        point.time_label = time_label(point.minute_of_day_raw, point.second_raw);
        point.time_seconds = point.minute_of_day_raw * 60 + point.second_raw;
        const auto signed_value = static_cast<std::int64_t>(point.unmatched_signed_hand);
        point.unmatched_volume_hand = static_cast<std::uint32_t>(
            signed_value < 0 ? -signed_value : signed_value);
        point.unmatched_direction_raw = signed_value > 0 ? 1 : signed_value < 0 ? -1 : 0;
        point.unmatched_direction = signed_value > 0 ? "buy" : signed_value < 0 ? "sell" : "balanced";
        series.points.push_back(std::move(point));
    }
    return series;
}

Json fetch_market_auction_document(const fs::path& root,
                                   const std::vector<std::string>& securities,
                                   std::uint32_t selector, std::uint32_t start_raw,
                                   std::uint32_t limit, int timeout_ms,
                                   const BlockData* block_data,
                                   const std::vector<std::string>& hosts) {
    if (securities.empty()) throw Error("at least one auction security is required");
    if (limit < 1 || limit > maximum_limit) throw Error("auction limit must be in 1..5000");
    if (timeout_ms < 1 || timeout_ms > 600000) throw Error("timeout must be positive");
    std::vector<SecurityCode> codes;
    std::set<std::pair<int, std::string>> seen;
    for (const auto& value : securities) {
        auto code = parse_security(value);
        if (seen.insert(code.key()).second) codes.push_back(std::move(code));
    }
    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);

    std::vector<AuctionSeries> series;
    std::vector<std::string> failures;
    std::size_t next = 0;
    std::string endpoint_used, server_name, trade_date;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        if (next >= codes.size()) break;
        ++endpoints_attempted;
        int attempts = 0;
        try {
            detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                const auto heartbeat = connection.call(type_heartbeat);
                if (heartbeat.data.size() < 10)
                    throw Error("heartbeat has no server trade date");
                trade_date = validate_trade_date(read_u32_le(heartbeat.data.data() + 6));
                while (next < codes.size()) {
                    const auto& code = codes[next];
                    const auto response = connection.call(type_auction_series,
                        build_auction_request_data(code.market_id, code.code, selector,
                                                   start_raw, limit));
                    series.push_back(parse_auction_payload(
                        response.data, code.market_id, code.code, selector, start_raw, limit));
                    ++next;
                    endpoint_used = endpoint.address();
                    server_name = connection.server_name();
                }
                return true;
            }, attempts);
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
    }
    if (next < codes.size()) {
        std::string detail = "auction series completed " + std::to_string(next) + "/" +
                             std::to_string(codes.size());
        for (const auto& failure : failures) detail += "\n  " + failure;
        throw Error(detail);
    }
    const auto loaded_blocks = block_data ? BlockData{} : load_blocks(root, {});
    const auto& names = block_data ? block_data->securities : loaded_blocks.securities;
    Json records = Json::array();
    for (const auto& item : series) records.push_back(series_json(item, names));
    Json document = Json::object();
    document["schema"] = "tdx-auction-series-native-v1";
    document["generated_at"] = now_text();
    document["command"] = "0x056A";
    document["endpoint"] = endpoint_used;
    document["server_name"] = server_name;
    document["transport"] = public_quote_transport_document(
        endpoint_selection, connection_attempts, transient_retries,
        endpoints_attempted);
    document["server_trade_date"] = trade_date;
    document["requested"] = static_cast<std::uint64_t>(codes.size());
    document["received"] = static_cast<std::uint64_t>(series.size());
    document["selector_note"] =
        "实测 selector=0 仅返回开盘竞价；任意非零值返回开盘和收盘竞价";
    document["paging_note"] = "start_raw/limit 按合并后的逐点序列分页";
    document["unmatched_direction_note"] =
        "int32 正数=买方未匹配、负数=卖方未匹配、零=平衡";
    document["records"] = std::move(records);
    return document;
}

int command_market_auction(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market auction --security [MARKET:]CODE [options]\n\n"
            "Native public 7709 command 0x056A opening/closing auction point series.\n\n"
            "Options:\n"
            "  --security CODE        Repeatable, e.g. sz:000001\n"
            "  --selector N           0 opening only; nonzero opening+closing (default 3)\n"
            "  --start-raw N          Point offset (default 0)\n"
            "  --limit N              1..5000 (default 500)\n"
            "  --host HOST[:PORT]     Repeatable; default connect.cfg HQHOST primary-first\n"
            "  --timeout-ms N         Default 10000\n"
            "  --root PATH            TDX installation root\n"
            "  --output PATH          Default output/tdx-auction-series-native.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const auto securities = args.take_options("--security");
    const auto selector = parse_u32(args.take_option("--selector", "3"), "--selector");
    const auto start_raw = parse_u32(args.take_option("--start-raw", "0"), "--start-raw");
    const auto limit = parse_u32(args.take_option("--limit", "500"), "--limit", 1, maximum_limit);
    const int timeout_ms = parse_timeout(args.take_option("--timeout-ms", "10000"));
    const auto hosts = args.take_options("--host");
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-auction-series-native.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto document = fetch_market_auction_document(root, securities, selector, start_raw,
                                                        limit, timeout_ms, nullptr, hosts);
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("received").as_number()
              << " auction series -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
