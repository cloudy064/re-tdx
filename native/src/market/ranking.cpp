#include "tdx/ranking.hpp"

#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <cstring>
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

constexpr std::uint16_t type_category_quotes = 0x054B;
constexpr std::uint16_t category_a_shares = 6;
constexpr std::uint16_t sort_seal_amount = 0x001C;
constexpr int maximum_page_size = 80;
constexpr int maximum_sealed_pages = 10;

const std::map<std::string, std::uint16_t> sort_types{
    {"code", 0x0000}, {"price", 0x0006}, {"amount", 0x000A},
    {"change-pct", 0x000E}, {"seal-amount", 0x001C},
    {"opening-amount", 0x001D}, {"rise-speed", 0x002E},
    {"short-turnover", 0x00CC}, {"volume-rise-speed", 0x00D0},
    {"opening-rush", 0x010A}, {"two-minute-amount", 0x010C},
    {"opening-change", 0x0119}, {"highest-change", 0x011A},
    {"lowest-change", 0x011B}, {"drawdown", 0x011E}, {"attack", 0x011F},
    {"代码", 0x0000}, {"现价", 0x0006}, {"成交额", 0x000A},
    {"涨幅", 0x000E}, {"封单额", 0x001C}, {"开盘金额", 0x001D},
    {"涨速", 0x002E}, {"短换手", 0x00CC}, {"量涨速", 0x00D0},
    {"开盘抢筹", 0x010A}, {"2分钟金额", 0x010C},
    {"开盘涨幅", 0x0119}, {"最高涨幅", 0x011A},
    {"最低涨幅", 0x011B}, {"回撤", 0x011E}, {"攻击", 0x011F},
};

std::uint16_t parse_u16(std::string text, std::string_view name) {
    text = trim(std::move(text));
    try {
        std::size_t used = 0;
        const auto value = std::stoul(text, &used, 0);
        if (used != text.size() || value > 0xFFFF) throw std::invalid_argument("range");
        return static_cast<std::uint16_t>(value);
    } catch (...) {
        throw Error(std::string(name) + " must be a uint16 number or known alias");
    }
}

int parse_integer(const std::string& text, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used, 0);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

void append_u16(Bytes& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

std::int64_t consume_varint(const Bytes& payload, std::size_t& offset) {
    if (offset >= payload.size()) throw Error("category quote varint starts past payload end");
    const auto first = payload[offset++];
    std::uint64_t magnitude = first & 0x3F;
    int shift = 6;
    auto current = first;
    while (current & 0x80) {
        if (offset >= payload.size()) throw Error("category quote varint has no terminator");
        current = payload[offset++];
        if (shift > 55) throw Error("category quote varint is too long");
        magnitude += static_cast<std::uint64_t>(current & 0x7F) << shift;
        shift += 7;
    }
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw Error("category quote varint exceeds int64");
    const auto value = static_cast<std::int64_t>(magnitude);
    return first & 0x40 ? -value : value;
}

double decode_wire_number(std::uint32_t value) {
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
    return base + high + base * middle_byte / 32768.0 * scale +
           base * low_byte / 8388608.0 * scale;
}

int price_divisor(const std::string& code) {
    for (const auto prefix : {"10", "11", "12"})
        if (code.rfind(prefix, 0) == 0) return 100;
    for (const auto prefix : {"15", "16", "50", "51", "52", "53", "56", "58"})
        if (code.rfind(prefix, 0) == 0) return 10;
    return 1;
}

double price(std::int64_t raw, const std::string& code) {
    return static_cast<double>(raw) / 100.0 / price_divisor(code);
}

std::string bytes_hex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index)
        output << std::setw(2) << static_cast<unsigned>(data[index]);
    return output.str();
}

CategoryQuote parse_record(const Bytes& payload, std::size_t& offset) {
    if (offset + 9 > payload.size()) throw Error("category quote record header is incomplete");
    CategoryQuote result;
    result.market_id = payload[offset];
    result.code.assign(payload.begin() + static_cast<std::ptrdiff_t>(offset + 1),
                       payload.begin() + static_cast<std::ptrdiff_t>(offset + 7));
    if (result.market_id < 0 || result.market_id > 2 || result.code.size() != 6 ||
        !std::all_of(result.code.begin(), result.code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("category quote contains an invalid security identifier");
    result.active1 = read_u16_le(payload.data() + offset + 7);
    offset += 9;

    std::array<std::int64_t, 9> values{};
    for (auto& value : values) value = consume_varint(payload, offset);
    const auto close_raw = values[0];
    result.last_price = price(close_raw, result.code);
    result.pre_close_price = price(close_raw + values[1], result.code);
    result.open_price = price(close_raw + values[2], result.code);
    result.high_price = price(close_raw + values[3], result.code);
    result.low_price = price(close_raw + values[4], result.code);
    result.server_time_raw = values[5];
    result.neg_price_raw = values[6];
    result.total_hand = values[7];
    result.current_hand = values[8];

    if (offset + 4 > payload.size()) throw Error(result.code + " category quote has no amount");
    result.amount_raw = read_u32_le(payload.data() + offset);
    result.amount = decode_wire_number(result.amount_raw);
    offset += 4;
    result.inside_dish = consume_varint(payload, offset);
    result.outer_disc = consume_varint(payload, offset);
    result.after_outer_raw = consume_varint(payload, offset);
    result.open_amount_yuan = static_cast<double>(consume_varint(payload, offset)) * 100.0;
    result.bid1_price = price(close_raw + consume_varint(payload, offset), result.code);
    result.ask1_price = price(close_raw + consume_varint(payload, offset), result.code);
    result.bid1_volume_hand = consume_varint(payload, offset);
    result.ask1_volume_hand = consume_varint(payload, offset);

    if (offset + 56 > payload.size())
        throw Error(result.code + " category quote fixed tail is incomplete");
    const auto* tail = payload.data() + offset;
    result.status_or_sort_raw = read_u16_le(tail);
    result.rise_speed = static_cast<std::int16_t>(read_u16_le(tail + 2)) / 100.0;
    result.short_turnover = static_cast<std::int16_t>(read_u16_le(tail + 4)) / 100.0;
    result.two_minute_amount = read_f32_le(tail + 6);
    result.opening_rush = static_cast<std::int16_t>(read_u16_le(tail + 10)) / 100.0;
    result.extra_pair_hex = bytes_hex(tail + 12, 10);
    result.volume_rise_speed = read_f32_le(tail + 22);
    result.depth = read_f32_le(tail + 26);
    result.extra_meta_hex = bytes_hex(tail + 30, 24);
    result.active2 = read_u16_le(tail + 54);
    offset += 56;

    for (const double value : {result.last_price, result.pre_close_price, result.open_price,
                               result.high_price, result.low_price, result.amount,
                               result.two_minute_amount, result.volume_rise_speed,
                               result.depth})
        if (!std::isfinite(value)) throw Error(result.code + " category quote is non-finite");
    return result;
}

bool is_sealed(const CategoryQuote& item) {
    return std::fabs(item.bid1_price - item.last_price) < 0.000001 &&
           (item.ask1_price <= 0 || item.ask1_volume_hand <= 0);
}

std::string security_id(const CategoryQuote& item) {
    return std::array<std::string, 3>{"SZ", "SH", "BJ"}.at(
        static_cast<std::size_t>(item.market_id)) + item.code;
}

Json record_json(const CategoryQuote& item,
                 const std::map<std::pair<int, std::string>, Security>& names) {
    Json value = Json::object();
    value["market_id"] = item.market_id;
    value["code"] = item.code;
    value["security_id"] = security_id(item);
    const auto found = names.find({item.market_id, item.code});
    value["name"] = found == names.end() ? "" : found->second.name;
    value["name_resolved"] = found != names.end();
    value["active1"] = static_cast<std::uint64_t>(item.active1);
    value["active2"] = static_cast<std::uint64_t>(item.active2);
    value["last_price"] = item.last_price;
    value["pre_close_price"] = item.pre_close_price;
    value["open_price"] = item.open_price;
    value["high_price"] = item.high_price;
    value["low_price"] = item.low_price;
    value["change_pct"] = item.pre_close_price > 0
        ? Json((item.last_price / item.pre_close_price - 1.0) * 100.0) : Json(nullptr);
    value["server_time_raw"] = item.server_time_raw;
    value["neg_price_raw"] = item.neg_price_raw;
    value["total_hand"] = item.total_hand;
    value["current_hand"] = item.current_hand;
    value["amount"] = item.amount;
    value["amount_raw"] = static_cast<std::uint64_t>(item.amount_raw);
    value["inside_dish"] = item.inside_dish;
    value["outer_disc"] = item.outer_disc;
    value["after_outer_raw"] = item.after_outer_raw;
    value["open_amount_yuan"] = item.open_amount_yuan;
    value["bid1_price"] = item.bid1_price;
    value["ask1_price"] = item.ask1_price;
    value["bid1_volume_hand"] = item.bid1_volume_hand;
    value["ask1_volume_hand"] = item.ask1_volume_hand;
    value["seal_amount_yuan"] = item.bid1_price * item.bid1_volume_hand * 100.0;
    value["is_sealed"] = is_sealed(item);
    value["status_or_sort_raw"] = static_cast<std::uint64_t>(item.status_or_sort_raw);
    value["rise_speed"] = item.rise_speed;
    value["short_turnover"] = item.short_turnover;
    value["two_minute_amount"] = item.two_minute_amount;
    value["opening_rush"] = item.opening_rush;
    value["volume_rise_speed"] = item.volume_rise_speed;
    value["depth"] = item.depth;
    value["extra_pair_hex"] = item.extra_pair_hex;
    value["extra_meta_hex"] = item.extra_meta_hex;
    return value;
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

std::string hex_u16(std::uint16_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
           << value;
    return output.str();
}

}  // namespace

std::uint16_t normalize_ranking_category(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "a-shares" || normalized == "a股" || normalized == "沪深a股")
        return category_a_shares;
    return parse_u16(normalized, "category");
}

std::uint16_t normalize_ranking_sort(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    const auto found = sort_types.find(normalized);
    return found == sort_types.end() ? parse_u16(normalized, "sort") : found->second;
}

Bytes build_category_request_data(std::uint16_t category, std::uint16_t sort_type,
                                  std::uint16_t start, std::uint16_t count,
                                  bool ascending, std::uint16_t filter_raw) {
    if (count < 1 || count > maximum_page_size)
        throw Error("category ranking count must be in 1..80");
    const std::uint16_t reverse = sort_type == 0 ? 0 : ascending ? 2 : 1;
    Bytes result;
    result.reserve(18);
    for (const auto value : {category, sort_type, start, count, reverse,
                             static_cast<std::uint16_t>(5), filter_raw,
                             static_cast<std::uint16_t>(1), static_cast<std::uint16_t>(0)})
        append_u16(result, value);
    return result;
}

CategoryQuotePage parse_category_payload(const Bytes& payload) {
    if (payload.size() < 4) throw Error("category ranking response is shorter than four bytes");
    CategoryQuotePage result;
    result.header = read_u16_le(payload.data());
    const auto count = read_u16_le(payload.data() + 2);
    if (count > maximum_page_size) throw Error("category ranking response exceeds 80 records");
    result.records.reserve(count);
    std::size_t offset = 4;
    for (std::uint16_t index = 0; index < count; ++index)
        result.records.push_back(parse_record(payload, offset));
    if (offset != payload.size())
        throw Error("category ranking response has " + std::to_string(payload.size() - offset) +
                    " trailing bytes");
    return result;
}

Json fetch_market_ranking_document(const fs::path& root, const std::string& category,
                                   const std::string& sort, int start, int count,
                                   bool ascending, int filter_raw, bool all_sealed,
                                   int timeout_ms, const BlockData* block_data,
                                   const std::vector<std::string>& hosts) {
    const auto category_id = normalize_ranking_category(category);
    const auto sort_id = normalize_ranking_sort(sort);
    if (start < 0 || start > 65535) throw Error("ranking start must be in 0..65535");
    if (count < 1 || count > maximum_page_size) throw Error("ranking count must be in 1..80");
    if (filter_raw < 0 || filter_raw > 65535) throw Error("filter_raw must be in 0..65535");
    if (timeout_ms < 1 || timeout_ms > 600000) throw Error("timeout must be positive");
    if (all_sealed && (sort_id != sort_seal_amount || ascending))
        throw Error("all_sealed requires descending seal-amount sort");

    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);
    std::vector<std::string> failures;
    std::vector<CategoryQuote> selected;
    std::size_t scanned = 0;
    int pages = 0;
    std::string endpoint_used, server_name;
    std::uint16_t response_header = 0;
    bool completed = false;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        ++endpoints_attempted;
        int attempts = 0;
        try {
            detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                selected.clear();
                scanned = 0;
                pages = 0;
                int page_start = start;
                while (true) {
                    if (page_start > 65535)
                        throw Error("ranking page start exceeds uint16");
                    const auto response = connection.call(type_category_quotes,
                        build_category_request_data(
                            category_id, sort_id, static_cast<std::uint16_t>(page_start),
                            static_cast<std::uint16_t>(count), ascending,
                            static_cast<std::uint16_t>(filter_raw)));
                    auto page = parse_category_payload(response.data);
                    response_header = page.header;
                    ++pages;
                    scanned += page.records.size();
                    if (!all_sealed) {
                        selected = std::move(page.records);
                        break;
                    }
                    const auto first_unsealed = std::find_if(
                        page.records.begin(), page.records.end(),
                        [](const CategoryQuote& item) { return !is_sealed(item); });
                    selected.insert(selected.end(), page.records.begin(), first_unsealed);
                    if (first_unsealed != page.records.end() ||
                        page.records.size() < static_cast<std::size_t>(count)) break;
                    if (pages >= maximum_sealed_pages)
                        throw Error("all_sealed reached the ten-page safety limit");
                    page_start += count;
                }
                endpoint_used = endpoint.address();
                server_name = connection.server_name();
                return true;
            }, attempts);
            completed = true;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
        if (completed) break;
    }
    if (!completed) {
        std::string detail = "all category ranking endpoints failed";
        for (const auto& failure : failures) detail += "\n  " + failure;
        throw Error(detail);
    }

    const auto loaded_blocks = block_data ? BlockData{} : load_blocks(root, {});
    const auto& names = block_data ? block_data->securities : loaded_blocks.securities;
    Json records = Json::array();
    for (const auto& item : selected) records.push_back(record_json(item, names));
    Json document = Json::object();
    document["schema"] = "tdx-market-ranking-native-v1";
    document["generated_at"] = now_text();
    document["command"] = "0x054B";
    document["endpoint"] = endpoint_used;
    document["server_name"] = server_name;
    document["transport"] = public_quote_transport_document(
        endpoint_selection, connection_attempts, transient_retries,
        endpoints_attempted);
    document["category"] = static_cast<std::uint64_t>(category_id);
    document["category_scope_note"] = category_id == category_a_shares
        ? "分类 6 当前主站实际返回深圳、上海和北京市场证券" : "";
    document["sort"] = sort;
    document["sort_type"] = hex_u16(sort_id);
    document["sort_reverse"] = sort_id == 0 ? 0 : ascending ? 2 : 1;
    document["ascending"] = ascending;
    document["start"] = start;
    document["page_size"] = count;
    document["pages"] = pages;
    document["scanned"] = static_cast<std::uint64_t>(scanned);
    document["all_sealed"] = all_sealed;
    document["response_header"] = static_cast<std::uint64_t>(response_header);
    document["received"] = static_cast<std::uint64_t>(selected.size());
    document["records"] = std::move(records);
    return document;
}

int command_market_ranking(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market ranking [options]\n\n"
            "Native public 7709 command 0x054B server-side market ranking.\n\n"
            "Options:\n"
            "  --sort NAME           rise-speed, change-pct, amount, seal-amount,\n"
            "                        opening-rush, short-turnover, volume-rise-speed, ...\n"
            "  --category ID         Default a-shares (6)\n"
            "  --start N             Default 0\n"
            "  --count N             1..80, default 80\n"
            "  --ascending           Ascending instead of descending\n"
            "  --all-sealed          Scan descending seal-amount pages until first unsealed\n"
            "  --filter-raw N        Default 0\n"
            "  --host HOST[:PORT]    Repeatable; default connect.cfg HQHOST primary-first\n"
            "  --timeout-ms N        Default 10000\n"
            "  --root PATH           TDX installation root\n"
            "  --output PATH         Default output/tdx-market-ranking-native.json\n"
            "  --compact             Compact JSON\n";
        return 0;
    }
    const auto category = args.take_option("--category", "a-shares");
    const auto sort = args.take_option("--sort", "rise-speed");
    const int start = parse_integer(args.take_option("--start", "0"), "--start", 0, 65535);
    const int count = parse_integer(args.take_option("--count", "80"), "--count", 1, 80);
    const int filter_raw = parse_integer(args.take_option("--filter-raw", "0"),
                                         "--filter-raw", 0, 65535);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const bool ascending = args.take_flag("--ascending");
    const bool all_sealed = args.take_flag("--all-sealed");
    const bool compact = args.take_flag("--compact");
    const auto hosts = args.take_options("--host");
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-market-ranking-native.json");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto document = fetch_market_ranking_document(root, category, sort, start, count,
        ascending, filter_raw, all_sealed, timeout_ms, nullptr, hosts);
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("received").as_number()
              << " category ranking records -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
