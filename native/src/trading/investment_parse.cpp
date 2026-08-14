#include "investment_internal.hpp"

#include "tdx/blowfish.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::investment_detail {
namespace {

constexpr std::uintmax_t kMaximumIndexSize = 16 * 1024 * 1024;
constexpr std::uintmax_t kMaximumFeeRulesSize = 8 * 1024 * 1024;
constexpr std::uintmax_t kMaximumPortfolioSize = 256 * 1024 * 1024;

fs::path path_from_utf8(std::string_view value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string ascii_trim(std::string value) {
    const auto space = [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](unsigned char ch) { return !space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [&](unsigned char ch) { return !space(ch); }).base(), value.end());
    return value;
}

std::string configured_private_directory(const fs::path& ini_path) {
    if (!regular_file_available(ini_path)) return {};
    const auto data = read_bytes(ini_path);
    std::string section;
    std::size_t begin = 0;
    while (begin <= data.size()) {
        const auto found = std::find(data.begin() + static_cast<std::ptrdiff_t>(begin),
                                     data.end(), static_cast<std::uint8_t>('\n'));
        const auto end = static_cast<std::size_t>(found - data.begin());
        Bytes line_bytes(data.begin() + static_cast<std::ptrdiff_t>(begin), found);
        if (!line_bytes.empty() && line_bytes.back() == '\r') line_bytes.pop_back();
        auto ascii = ascii_trim(std::string(
            reinterpret_cast<const char*>(line_bytes.data()), line_bytes.size()));
        if (!ascii.empty() && ascii.front() == '[' && ascii.back() == ']') {
            section = lower_ascii(ascii_trim(
                ascii.substr(1, ascii.size() - 2)));
        } else if (section == "other") {
            const auto equals = std::find(line_bytes.begin(), line_bytes.end(),
                                          static_cast<std::uint8_t>('='));
            if (equals != line_bytes.end()) {
                const auto separator = static_cast<std::size_t>(
                    equals - line_bytes.begin());
                auto key = lower_ascii(ascii_trim(std::string(
                    reinterpret_cast<const char*>(line_bytes.data()), separator)));
                if (key == "investpath") {
                    Bytes value(equals + 1, line_bytes.end());
                    while (!value.empty() &&
                           (value.front() == ' ' || value.front() == '\t'))
                        value.erase(value.begin());
                    while (!value.empty() &&
                           (value.back() == ' ' || value.back() == '\t'))
                        value.pop_back();
                    auto decoded = trim(decode_gbk(value));
                    if (decoded.size() >= 2 &&
                        ((decoded.front() == '"' && decoded.back() == '"') ||
                         (decoded.front() == '\'' && decoded.back() == '\'')))
                        decoded = decoded.substr(1, decoded.size() - 2);
                    return decoded == "*****" ? std::string{} : decoded;
                }
            }
        }
        if (found == data.end()) break;
        begin = end + 1;
    }
    return {};
}

fs::path absolute_option_path(const fs::path& path) {
    if (path.empty()) return {};
    std::error_code error;
    const auto absolute = path.is_absolute() ? path : fs::absolute(path, error);
    if (error) throw Error("cannot resolve path: " + path_utf8(path));
    return absolute.lexically_normal();
}

Bytes terminated_field(const Bytes& data, std::size_t offset, std::size_t width,
                       std::string_view field, const fs::path& path,
                       std::size_t record_index) {
    if (offset > data.size() || width > data.size() - offset)
        throw Error("truncated " + std::string(field) + " in " + path_utf8(path));
    const auto begin = data.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end = begin + static_cast<std::ptrdiff_t>(width);
    const auto terminator = std::find(begin, end, static_cast<std::uint8_t>(0));
    if (terminator == end)
        throw Error(path_utf8(path) + " record " +
                    std::to_string(record_index) + " has unterminated " +
                    std::string(field));
    return Bytes(begin, terminator);
}

bool safe_portfolio_name(const std::string& name) {
    if (name.empty() || name == "." || name == ".." ||
        name.back() == ' ' || name.back() == '.')
        return false;
    constexpr std::string_view invalid = "<>:\"/\\|?*";
    return std::none_of(name.begin(), name.end(), [&](unsigned char ch) {
        return ch < 0x20 || (ch < 0x80 && invalid.find(
            static_cast<char>(ch)) != std::string_view::npos);
    });
}

double read_f64_le(const std::uint8_t* data) {
    std::uint64_t bits = 0;
    for (int index = 7; index >= 0; --index)
        bits = (bits << 8U) | data[index];
    double result = 0.0;
    static_assert(sizeof(result) == sizeof(bits), "unexpected double size");
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void require_file_size(const fs::path& path, std::uintmax_t maximum,
                       std::size_t record_size, std::string_view label) {
    const auto size = fs::file_size(path);
    if (size > maximum || size % record_size != 0)
        throw Error(std::string(label) + " has invalid size: " + path_utf8(path));
}

Bytes read_file_range(const fs::path& path, std::uintmax_t offset,
                      std::size_t length) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw Error("cannot open file: " + path_utf8(path));
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw Error("cannot seek file: " + path_utf8(path));
    Bytes result(length);
    if (length != 0)
        input.read(reinterpret_cast<char*>(result.data()),
                   static_cast<std::streamsize>(length));
    if (static_cast<std::size_t>(input.gcount()) != length)
        throw Error("cannot read requested file range: " + path_utf8(path));
    return result;
}

std::string decode_text_field(const Bytes& value, std::string_view field,
                              const fs::path& path, std::size_t record_index) {
    try {
        return trim(decode_gbk(value));
    } catch (const std::exception&) {
        throw Error(path_utf8(path) + " record " +
                    std::to_string(record_index) + " has invalid " +
                    std::string(field) + " encoding");
    }
}

}  // namespace

bool regular_file_available(const fs::path& path) {
    std::error_code error;
    const bool regular = fs::is_regular_file(path, error);
    return !error && regular;
}

InvestmentPaths resolve_paths(const fs::path& root,
                              const fs::path& private_directory_override,
                              const fs::path& fee_rules_override) {
    InvestmentPaths paths;
    const auto t0002 = root / "T0002";
    if (!private_directory_override.empty()) {
        paths.private_directory = absolute_option_path(private_directory_override);
        paths.private_directory_source = "option";
    } else {
        const auto configured = configured_private_directory(t0002 / "user.ini");
        if (!configured.empty()) {
            auto value = path_from_utf8(configured);
            paths.private_directory = (value.is_absolute() ? value : t0002 / value)
                .lexically_normal();
            paths.private_directory_source = "user.ini:[OTHER]/INVESTPATH";
        } else {
            paths.private_directory = (t0002 / "invest").lexically_normal();
            paths.private_directory_source = "conventional-fallback";
        }
    }
    paths.portfolio_index = paths.private_directory / "pinfo.dat";
    if (!fee_rules_override.empty()) {
        paths.fee_rules = absolute_option_path(fee_rules_override);
        paths.fee_rules_source = "option";
    } else {
        paths.fee_rules = t0002 / "trdpara.dat";
        paths.fee_rules_source = "conventional-path";
    }
    return paths;
}

std::vector<PortfolioRecord> parse_portfolio_index(const fs::path& path) {
    if (!regular_file_available(path)) return {};
    require_file_size(path, kMaximumIndexSize, kPortfolioRecordSize,
                      "investment portfolio index");
    const auto encrypted = read_bytes(path);
    const auto decrypted = blowfish_ecb_decrypt(
        encrypted, kInvestmentKey, BlowfishWordOrder::little_endian);
    std::vector<PortfolioRecord> result;
    result.reserve(decrypted.size() / kPortfolioRecordSize);
    for (std::size_t offset = 0, record = 0; offset < decrypted.size();
         offset += kPortfolioRecordSize, ++record) {
        auto name_bytes = terminated_field(
            decrypted, offset, 21, "portfolio name", path, record);
        auto password = terminated_field(
            decrypted, offset + 21, 21, "portfolio password", path, record);
        auto name = decode_text_field(name_bytes, "portfolio name", path, record);
        if (name.empty())
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has an empty portfolio name");
        result.push_back({std::move(name), std::move(password)});
    }
    return result;
}

PortfolioFileInfo inspect_portfolio_file(const fs::path& private_directory,
                                         const PortfolioRecord& portfolio) {
    PortfolioFileInfo result;
    result.lookup_allowed = safe_portfolio_name(portfolio.name);
    if (!result.lookup_allowed) {
        result.diagnostic = "unsafe portfolio name; detail lookup skipped";
        return result;
    }
    result.path = private_directory /
        path_from_utf8(portfolio.name + ".da0");
    result.available = regular_file_available(result.path);
    if (!result.available) {
        result.diagnostic = "detail file is absent";
        return result;
    }
    result.byte_size = fs::file_size(result.path);
    result.valid_record_size = result.byte_size >= kTransactionRecordSize &&
        result.byte_size <= kMaximumPortfolioSize &&
        result.byte_size % kTransactionRecordSize == 0;
    if (!result.valid_record_size) {
        result.diagnostic = "detail file size is not a valid 200-byte record stream";
        return result;
    }
    const auto decrypted = blowfish_ecb_decrypt(
        read_file_range(result.path, 0, kTransactionRecordSize),
        kInvestmentKey, BlowfishWordOrder::little_endian);
    result.record_count = static_cast<std::size_t>(
        result.byte_size / kTransactionRecordSize);
    result.transaction_count = result.record_count - 1;
    try {
        const auto header_password = terminated_field(
            decrypted, 0, 21, "detail header password", result.path, 0);
        result.header_password_readable = true;
        result.header_password_matches = header_password == portfolio.password;
        if (!result.header_password_matches)
            result.diagnostic = "detail header password does not match pinfo.dat";
    } catch (const std::exception&) {
        result.diagnostic = "detail header password is malformed";
    }
    return result;
}

std::vector<TransactionRecord> parse_transaction_records(
    const fs::path& path, const PortfolioRecord& portfolio,
    bool include_notes, std::size_t offset, std::size_t limit) {
    require_file_size(path, kMaximumPortfolioSize, kTransactionRecordSize,
                      "investment transaction file");
    const auto file_size = fs::file_size(path);
    if (file_size < kTransactionRecordSize)
        throw Error("investment transaction file has no header: " +
                    path_utf8(path));
    const auto header = blowfish_ecb_decrypt(
        read_file_range(path, 0, kTransactionRecordSize),
        kInvestmentKey, BlowfishWordOrder::little_endian);
    const auto header_password = terminated_field(
        header, 0, 21, "detail header password", path, 0);
    if (header_password != portfolio.password)
        throw Error("detail header password does not match pinfo.dat: " +
                    path_utf8(path));

    const auto valid_date = [](std::int32_t value) {
        const int year = value / 10000;
        const int month = value / 100 % 100;
        const int day = value % 100;
        if (year < 1900 || month < 1 || month > 12 || day < 1) return false;
        static constexpr int days[] = {
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int maximum = days[month - 1];
        if (month == 2 && (year % 400 == 0 ||
                           (year % 4 == 0 && year % 100 != 0)))
            maximum = 29;
        return day <= maximum;
    };
    const auto count = static_cast<std::size_t>(
        file_size / kTransactionRecordSize);
    const auto transaction_count = count - 1;
    const auto begin = std::min(offset, transaction_count);
    const auto returned = std::min(limit, transaction_count - begin);
    const auto page = returned == 0 ? Bytes{} : blowfish_ecb_decrypt(
        read_file_range(path, (begin + 1) * kTransactionRecordSize,
                        returned * kTransactionRecordSize),
        kInvestmentKey, BlowfishWordOrder::little_endian);
    const auto nonzero = [&](std::size_t local_begin, std::size_t local_end) {
        return std::any_of(page.begin() + static_cast<std::ptrdiff_t>(local_begin),
                           page.begin() + static_cast<std::ptrdiff_t>(local_end),
                           [](std::uint8_t value) { return value != 0; });
    };

    std::vector<TransactionRecord> result;
    result.reserve(returned);
    for (std::size_t record = begin + 1;
         record < begin + returned + 1; ++record) {
        const auto base = (record - begin - 1) * kTransactionRecordSize;
        const auto* raw = page.data() + base;
        TransactionRecord item;
        item.file_record_index = record;
        item.market_kind = read_i32_le(raw);
        item.date = read_i32_le(raw + 4);
        if (!valid_date(item.date))
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has an invalid YYYYMMDD date");
        const auto note = terminated_field(
            page, base + 8, 101, "transaction note", path, record);
        item.note_present = !note.empty();
        if (include_notes)
            item.note = decode_text_field(
                note, "transaction note", path, record);
        item.cash_direction = read_i32_le(raw + 109);
        if (item.cash_direction < -1 || item.cash_direction > 1)
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has invalid cash direction");
        const auto code = terminated_field(
            page, base + 113, 10, "security code", path, record);
        item.code.assign(code.begin(), code.end());
        if (!std::all_of(item.code.begin(), item.code.end(), [](char ch) {
                return ch >= '0' && ch <= '9';
            }))
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has invalid security code");
        item.type_code = read_i32_le(raw + 123);
        item.quantity = read_i32_le(raw + 147);
        item.unit_price = read_f64_le(raw + 151);
        item.fee = read_f64_le(raw + 159);
        item.recorded_total = read_f64_le(raw + 167);
        if (item.quantity < 0 || !std::isfinite(item.unit_price) ||
            !std::isfinite(item.fee) || !std::isfinite(item.recorded_total))
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has invalid numeric fields");
        if (transaction_has_security(item.type_code) && item.code.empty())
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has no security code");
        item.unparsed_bytes_present =
            nonzero(base + 127, base + 147) ||
            nonzero(base + 175, base + kTransactionRecordSize);
        result.push_back(std::move(item));
    }
    return result;
}

std::vector<FeeRule> parse_fee_rules(const fs::path& path) {
    if (!regular_file_available(path)) return {};
    require_file_size(path, kMaximumFeeRulesSize, kFeeRuleRecordSize,
                      "investment fee rules");
    const auto data = read_bytes(path);
    std::vector<FeeRule> result;
    result.reserve(data.size() / kFeeRuleRecordSize);
    for (std::size_t offset = 0, record = 0; offset < data.size();
         offset += kFeeRuleRecordSize, ++record) {
        FeeRule rule;
        rule.record_index = record;
        rule.market_kind = read_i32_le(data.data() + offset);
        if (rule.market_kind < 0 || rule.market_kind > 2)
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has invalid market kind");
        const auto prefix_bytes = terminated_field(
            data, offset + 4, 7, "security prefix", path, record);
        rule.prefix.assign(prefix_bytes.begin(), prefix_bytes.end());
        if (!std::all_of(rule.prefix.begin(), rule.prefix.end(), [](char ch) {
                return ch >= '0' && ch <= '9';
            }))
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has invalid security prefix");
        rule.market_name = decode_text_field(terminated_field(
            data, offset + 11, 20, "market name", path, record),
            "market name", path, record);
        if (rule.market_name.empty())
            throw Error(path_utf8(path) + " record " +
                        std::to_string(record) + " has an empty market name");
        double* fields[] = {
            &rule.commission_rate, &rule.minimum_commission,
            &rule.stamp_tax_rate, &rule.transfer_fee_rate,
            &rule.minimum_transfer_fee, &rule.fixed_fee,
        };
        for (std::size_t index = 0; index < 6; ++index) {
            *fields[index] = read_f64_le(data.data() + offset + 31 + index * 8);
            if (!std::isfinite(*fields[index]) || *fields[index] < 0.0)
                throw Error(path_utf8(path) + " record " +
                            std::to_string(record) + " has invalid fee value");
        }
        result.push_back(std::move(rule));
    }
    return result;
}

std::string market_name(int market_kind) {
    switch (market_kind) {
    case 0: return "sz";
    case 1: return "sh";
    case 2: return "bj";
    default: return "unknown";
    }
}

std::string transaction_type_name(int type_code) {
    switch (type_code) {
    case 0: return "security-buy";
    case 1: return "security-sell";
    case 2: return "cash-dividend";
    case 3: return "bonus-shares";
    case 4: return "rights-subscription";
    case 6: return "cash-deposit";
    case 7: return "cash-withdrawal";
    case 9: return "cash-red-reversal";
    case 10: return "cash-blue-adjustment";
    case 11: return "security-transfer-in";
    case 12: return "security-transfer-out";
    case 13: return "derivative-distribution";
    default: return "unknown";
    }
}

std::string transaction_type_label(int type_code) {
    switch (type_code) {
    case 0: return "买入股票";
    case 1: return "卖出股票";
    case 2: return "股票分红";
    case 3: return "送股";
    case 4: return "配股";
    case 6: return "追加资金";
    case 7: return "划出资金";
    case 9: return "现金红冲";
    case 10: return "现金蓝补";
    case 11: return "划入股票";
    case 12: return "划出股票";
    case 13: return "送衍生品种";
    default: return "未知旧类型";
    }
}

bool transaction_has_security(int type_code) {
    return (type_code >= 0 && type_code <= 4) ||
           (type_code >= 11 && type_code <= 13);
}

}  // namespace tdx::investment_detail
