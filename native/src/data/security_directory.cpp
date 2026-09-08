#include "tdx/security_directory.hpp"
#include "tdx/time.hpp"

#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uint16_t type_security_list = 0x044D;
constexpr std::uint16_t type_security_count = 0x044E;
constexpr std::size_t record_size = 37;

// A caller's cancellation is not a provider/transport error, even when its
// message resembles a transient socket failure. Cross the generic retry
// helper without being caught there, then restore the original exception.
struct DirectoryCheckInterrupted { std::exception_ptr cause; };

void append_u16(Bytes& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t client_date() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    return static_cast<std::uint32_t>((local.tm_year + 1900) * 10000 +
                                      (local.tm_mon + 1) * 100 + local.tm_mday);
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
    output << local_timestamp_text(local);
    return output.str();
}

double read_f32(const std::uint8_t* data) {
    const auto bits = read_u32_le(data);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value) ? static_cast<double>(value) : 0.0;
}

std::string bytes_hex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index)
        output << std::setw(2) << static_cast<unsigned>(data[index]);
    return output.str();
}

std::string market_name(int market_id) {
    if (market_id == 0) return "sz";
    if (market_id == 1) return "sh";
    if (market_id == 2) return "bj";
    throw Error("market id must be 0, 1, or 2");
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz" || value == "0") return 0;
    if (value == "sh" || value == "1") return 1;
    if (value == "bj" || value == "2" || value == "44") return 2;
    throw Error("market must be all, sz, sh, bj, 0, 1, 2, or 44");
}

std::string board_for(int id, const std::string& code,
                      const std::string& category) {
    if (category != "a_share") return "none";
    if (id == 1 && (code.rfind("600", 0) == 0 || code.rfind("601", 0) == 0 ||
                    code.rfind("603", 0) == 0 || code.rfind("605", 0) == 0))
        return "sse_main_board";
    if (id == 1 && (code.rfind("688", 0) == 0 || code.rfind("689", 0) == 0))
        return "sse_star_market";
    if (id == 0 && (code.rfind("000", 0) == 0 || code.rfind("001", 0) == 0 ||
                    code.rfind("002", 0) == 0 || code.rfind("003", 0) == 0 ||
                    code.rfind("004", 0) == 0)) return "szse_main_board";
    if (id == 0 && (code.rfind("300", 0) == 0 || code.rfind("301", 0) == 0))
        return "szse_chinext";
    if (id == 2 && code.rfind("92", 0) == 0) return "bse_listed_stock";
    return "none";
}

std::string category_reason(int id, const std::string& code,
                            const std::string& category) {
    (void)id;
    (void)code;
    if (category == "unknown") return "no known exchange prefix matched";
    return "inferred from exchange and code prefix";
}

bool contains_folded(const SecurityDirectoryRecord& row,
                     const std::string& folded) {
    if (folded.empty()) return true;
    const auto id = market_name(row.market_id) + row.code;
    return lower_ascii(row.code).find(folded) != std::string::npos ||
           lower_ascii(row.name).find(folded) != std::string::npos ||
           id.find(folded) != std::string::npos ||
           lower_ascii(row.category).find(folded) != std::string::npos;
}

bool exact_match(const SecurityDirectoryRecord& row,
                 const std::string& folded) {
    return !folded.empty() &&
        (lower_ascii(row.code) == folded || lower_ascii(row.name) == folded ||
         market_name(row.market_id) + row.code == folded);
}

Json record_json(const SecurityDirectoryRecord& row) {
    Json result = Json::object();
    result["security_id"] = market_name(row.market_id) + row.code;
    result["market"] = market_name(row.market_id);
    result["market_id"] = row.market_id;
    result["code"] = row.code;
    result["name"] = row.name;
    result["multiple"] = static_cast<std::uint64_t>(row.multiple);
    result["decimal"] = static_cast<std::uint64_t>(row.decimal);
    result["previous_close_price"] = row.previous_close_price;
    result["volume_ratio_base"] = row.volume_ratio_base;
    result["category"] = row.category;
    result["category_reason"] = row.category_reason;
    result["board"] = row.board;
    result["raw_tail_hex"] = row.raw_tail_hex;
    return result;
}

Bytes count_request(int id) {
    Bytes result;
    append_u16(result, static_cast<std::uint16_t>(id));
    append_u32(result, client_date());
    return result;
}

Bytes list_request(int id, std::uint32_t start, std::uint32_t limit) {
    Bytes result;
    append_u16(result, static_cast<std::uint16_t>(id));
    append_u32(result, start);
    append_u32(result, limit);
    append_u32(result, 0);
    return result;
}

int bounded(const std::string& text, std::string_view name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(minimum) + ".." + std::to_string(maximum));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace

std::string classify_security_directory_record(int id, const std::string& code) {
    if ((id == 1 && (code.rfind("000", 0) == 0 || code.rfind("880", 0) == 0 ||
                     code.rfind("881", 0) == 0 || code.rfind("999", 0) == 0)) ||
        (id == 0 && code.rfind("399", 0) == 0) ||
        (id == 2 && code.rfind("899", 0) == 0)) return "index";
    if ((id == 1 && (code.rfind("510", 0) == 0 || code.rfind("511", 0) == 0 ||
                     code.rfind("512", 0) == 0 || code.rfind("513", 0) == 0 ||
                     code.rfind("515", 0) == 0 || code.rfind("516", 0) == 0 ||
                     code.rfind("517", 0) == 0 || code.rfind("518", 0) == 0 ||
                     code.rfind("520", 0) == 0 || code.rfind("560", 0) == 0 ||
                     code.rfind("561", 0) == 0 || code.rfind("562", 0) == 0 ||
                     code.rfind("563", 0) == 0 || code.rfind("588", 0) == 0)) ||
        (id == 0 && (code.rfind("158", 0) == 0 || code.rfind("159", 0) == 0)))
        return "etf";
    if ((id == 1 && (code.rfind("110", 0) == 0 || code.rfind("111", 0) == 0 ||
                     code.rfind("113", 0) == 0 || code.rfind("118", 0) == 0 ||
                     code.rfind("132", 0) == 0)) ||
        (id == 0 && (code.rfind("123", 0) == 0 || code.rfind("127", 0) == 0 ||
                     code.rfind("128", 0) == 0)) ||
        (id == 2 && code.rfind("810", 0) == 0)) return "convertible_bond";
    if ((id == 1 && (code.rfind("600", 0) == 0 || code.rfind("601", 0) == 0 ||
                     code.rfind("603", 0) == 0 || code.rfind("605", 0) == 0 ||
                     code.rfind("688", 0) == 0 || code.rfind("689", 0) == 0)) ||
        (id == 0 && (code.rfind("000", 0) == 0 || code.rfind("001", 0) == 0 ||
                     code.rfind("002", 0) == 0 || code.rfind("003", 0) == 0 ||
                     code.rfind("004", 0) == 0 || code.rfind("300", 0) == 0 ||
                     code.rfind("301", 0) == 0)) ||
        (id == 2 && code.rfind("92", 0) == 0)) return "a_share";
    if ((id == 1 && code.rfind("900", 0) == 0) ||
        (id == 0 && code.rfind("20", 0) == 0)) return "b_share";
    if ((id == 1 && (code.rfind("501", 0) == 0 || code.rfind("502", 0) == 0 ||
                     code.rfind("505", 0) == 0 || code.rfind("506", 0) == 0)) ||
        (id == 0 && (code.rfind("16", 0) == 0 || code.rfind("18", 0) == 0)))
        return "fund";
    if ((id == 1 && code.rfind("204", 0) == 0) ||
        (id == 0 && code.rfind("1318", 0) == 0)) return "repo";
    if ((id == 1 && (code.rfind("01", 0) == 0 || code.rfind("02", 0) == 0 ||
                     code.rfind("10", 0) == 0 || code.rfind("12", 0) == 0)) ||
        (id == 0 && (code.rfind("10", 0) == 0 || code.rfind("11", 0) == 0)) ||
        (id == 2 && code.rfind("82", 0) == 0)) return "bond";
    return "unknown";
}

std::vector<SecurityDirectoryRecord> parse_security_directory_page(
    const Bytes& payload, int id) {
    (void)market_name(id);
    if (payload.size() < 2) throw Error("security-directory page is too short");
    const auto count = read_u16_le(payload.data());
    const auto expected = 2 + static_cast<std::size_t>(count) * record_size;
    if (payload.size() < expected)
        throw Error("security-directory page is truncated: expected " +
                    std::to_string(expected) + ", got " +
                    std::to_string(payload.size()));
    std::vector<SecurityDirectoryRecord> result;
    result.reserve(count);
    std::size_t offset = 2;
    for (std::uint16_t index = 0; index < count; ++index, offset += record_size) {
        const auto* row = payload.data() + offset;
        const std::string code(reinterpret_cast<const char*>(row), 6);
        if (!std::all_of(code.begin(), code.end(), [](char ch) {
                return ch >= '0' && ch <= '9';
            })) throw Error("security-directory page contains invalid code");
        Bytes encoded_name(row + 8, row + 24);
        while (!encoded_name.empty() && encoded_name.back() == 0) encoded_name.pop_back();
        SecurityDirectoryRecord item;
        item.market_id = id;
        item.code = code;
        item.name = trim(decode_gbk(encoded_name));
        item.multiple = read_u16_le(row + 6);
        item.volume_ratio_base = read_f32(row + 24);
        item.decimal = row[28];
        item.previous_close_price = read_f32(row + 29);
        item.category = classify_security_directory_record(id, code);
        item.category_reason = category_reason(id, code, item.category);
        item.board = board_for(id, code, item.category);
        item.raw_tail_hex = bytes_hex(row + 33, 4);
        result.push_back(std::move(item));
    }
    return result;
}

SecurityDirectoryService::MarketCache SecurityDirectoryService::fetch_market(
    int id, const SecurityDirectoryQuery& options) {
    if (options.check) options.check();
    const auto check = [&] {
        if (!options.check) return;
        try { options.check(); }
        catch (...) { throw DirectoryCheckInterrupted{std::current_exception()}; }
    };
    auto hosts = options.hosts;
    if (hosts.empty() && !trim(options.endpoint).empty())
        hosts.push_back(trim(options.endpoint));
    const auto endpoint_selection = select_public_quote_endpoints(options.root, hosts);
    std::vector<std::string> failures;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        if (options.check) options.check();
        ++endpoints_attempted;
        int attempts = 0;
        MarketCache result;
        bool completed = false;
        try {
            detail::retry_quote_transport([&] {
                check();
                QuoteConnection connection(endpoint, options.timeout_ms);
                check();
                const auto count_response = connection.call(
                    type_security_count, count_request(id));
                check();
                if (count_response.data.size() < 2)
                    throw Error("security-directory count response is too short");
                const auto reported = read_u16_le(count_response.data.data());
                MarketCache candidate;
                candidate.reported_count = reported;
                candidate.endpoint = connection.endpoint().address();
                candidate.server_name = connection.server_name();
                candidate.records.reserve(reported);
                std::uint32_t start = 0;
                while (start < reported) {
                    check();
                    const auto requested = static_cast<std::uint32_t>(std::min<int>(
                        options.page_size, static_cast<int>(reported - start)));
                    const auto response = connection.call(
                        type_security_list, list_request(id, start, requested));
                    check();
                    auto page = parse_security_directory_page(response.data, id);
                    if (page.empty()) break;
                    start += static_cast<std::uint32_t>(page.size());
                    candidate.records.insert(candidate.records.end(),
                        std::make_move_iterator(page.begin()),
                        std::make_move_iterator(page.end()));
                }
                if (candidate.records.size() != reported)
                    throw Error("security-directory download incomplete for " +
                        market_name(id) + ": " +
                        std::to_string(candidate.records.size()) + "/" +
                        std::to_string(reported));
                candidate.fetched_at = std::time(nullptr);
                check();
                result = std::move(candidate);
                return true;
            }, attempts);
            check();
            completed = true;
        } catch (const DirectoryCheckInterrupted& interrupted) {
            std::rethrow_exception(interrupted.cause);
        } catch (const std::exception& error) {
            if (options.check) options.check();
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
        if (completed) {
            result.transport = public_quote_transport_document(
                endpoint_selection, connection_attempts, transient_retries,
                endpoints_attempted);
            return result;
        }
    }
    std::string detail = "all security-directory endpoints failed for " + market_name(id);
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}

const SecurityDirectoryService::MarketCache& SecurityDirectoryService::ensure_market(
    int id, const SecurityDirectoryQuery& options) {
    if (options.check) options.check();
    const auto found = caches_.find(id);
    const auto now = std::time(nullptr);
    if (!options.refresh && found != caches_.end() && found->second.fetched_at &&
        now - found->second.fetched_at < options.cache_ttl_seconds)
        return found->second;
    auto downloaded = fetch_market(id, options);
    if (options.check) options.check();
    caches_[id] = std::move(downloaded);
    return caches_.at(id);
}

Json SecurityDirectoryService::query(const SecurityDirectoryQuery& options) {
    if (options.check) options.check();
    if (options.limit < 1 || options.limit > 100000)
        throw Error("limit must be in 1..100000");
    if (options.page_size < 1 || options.page_size > 1700)
        throw Error("page_size must be in 1..1700");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const std::set<std::string> categories{
        "all", "a_share", "b_share", "index", "etf", "fund",
        "convertible_bond", "bond", "repo", "unknown"};
    if (!categories.count(options.category))
        throw Error("unknown security category: " + options.category);
    std::vector<int> markets;
    if (lower_ascii(trim(options.market)) == "all") markets = {0, 1, 2};
    else markets = {market_id(options.market)};

    std::lock_guard<std::mutex> guard(mutex_);
    if (options.check) options.check();
    std::vector<const SecurityDirectoryRecord*> matches;
    std::vector<const SecurityDirectoryRecord*> exact;
    std::map<std::string, std::uint64_t> category_counts;
    Json sources = Json::array();
    const auto folded = lower_ascii(trim(options.query));
    std::uint64_t available = 0;
    for (const auto id : markets) {
        if (options.check) options.check();
        const auto& cache = ensure_market(id, options);
        if (options.check) options.check();
        available += cache.records.size();
        Json source = Json::object();
        source["market"] = market_name(id);
        source["reported_count"] = cache.reported_count;
        source["received_count"] = static_cast<std::uint64_t>(cache.records.size());
        source["endpoint"] = cache.endpoint;
        source["server_name"] = cache.server_name;
        source["transport"] = cache.transport;
        source["age_seconds"] = static_cast<std::uint64_t>(
            std::max<std::time_t>(0, std::time(nullptr) - cache.fetched_at));
        sources.push_back(std::move(source));
        for (const auto& row : cache.records) {
            ++category_counts[row.category];
            if (options.category != "all" && row.category != options.category) continue;
            if (contains_folded(row, folded)) matches.push_back(&row);
            if (exact_match(row, folded)) exact.push_back(&row);
        }
    }
    const auto& selected = exact.empty() ? matches : exact;
    Json rows = Json::array();
    for (const auto* row : selected) {
        if (static_cast<int>(rows.size()) >= options.limit) break;
        rows.push_back(record_json(*row));
    }
    Json summary = Json::object();
    summary["available"] = available;
    Json by_category = Json::object();
    for (const auto& [category, count] : category_counts)
        by_category[category] = count;
    summary["by_category"] = std::move(by_category);
    Json result = Json::object();
    result["schema"] = "tdx-market-security-directory-native-v1";
    result["generated_at"] = now_text();
    result["command_count"] = "0x044E";
    result["command_list"] = "0x044D";
    result["market"] = lower_ascii(trim(options.market));
    result["category"] = options.category;
    result["query"] = options.query;
    result["match_count"] = static_cast<std::uint64_t>(selected.size());
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["truncated"] = selected.size() > rows.size();
    result["summary"] = std::move(summary);
    result["sources"] = std::move(sources);
    result["securities"] = std::move(rows);
    if (options.check) options.check();
    return result;
}

int command_market_securities(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market securities [options]\n\n"
            "Download and query the native 0x044D/0x044E server security directory.\n\n"
            "Options:\n"
            "  --market NAME       all|sz|sh|bj (default all)\n"
            "  --category NAME     all|a_share|b_share|index|etf|fund|convertible_bond|bond|repo|unknown\n"
            "  --query TEXT        Filter code, name, full code, or category\n"
            "  --limit N           Default 100000\n"
            "  --page-size N       Default 1600\n"
            "  --host HOST:PORT    Repeatable; default connect.cfg HQHOST primary-first\n"
            "  --root PATH         TDX installation root\n"
            "  --timeout-ms N      Default 15000\n"
            "  --output PATH       Default output/tdx-market-securities-native.json\n"
            "  --compact           Write compact JSON\n";
        return 0;
    }
    SecurityDirectoryQuery query;
    query.market = lower_ascii(trim(args.take_option("--market", "all")));
    query.category = lower_ascii(trim(args.take_option("--category", "all")));
    query.query = trim(args.take_option("--query"));
    query.hosts = args.take_options("--host");
    const auto root_text = trim(args.take_option("--root"));
    query.root = root_text.empty() ? find_tdx_root({}) : find_tdx_root(fs::u8path(root_text));
    query.limit = bounded(args.take_option("--limit", "100000"), "--limit", 1, 100000);
    query.page_size = bounded(args.take_option("--page-size", "1600"), "--page-size", 1, 1700);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-securities-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    SecurityDirectoryService service;
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << static_cast<std::uint64_t>(
        document.at("returned").as_number()) << " server securities -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
