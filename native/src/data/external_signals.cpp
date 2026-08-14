#include "tdx/external_signals.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

std::string native_text(const Bytes& bytes) {
    return decode_gbk(bytes);
}

std::string raw_trim(std::string value) {
    const auto non_space = [](unsigned char ch) {
        return ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n';
    };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), non_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), non_space).base(),
                value.end());
    return value;
}

std::string decode_field(std::string_view value) {
    return native_text(Bytes(value.begin(), value.end()));
}

int host_atol(std::string_view value) {
    const std::string owned(value);
    errno = 0;
    const long parsed = std::strtol(owned.c_str(), nullptr, 10);
    if (parsed > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    if (parsed < std::numeric_limits<int>::min())
        return std::numeric_limits<int>::min();
    return static_cast<int>(parsed);
}

float host_atof(std::string_view value) {
    const std::string owned(value);
    return std::strtof(owned.c_str(), nullptr);
}

std::vector<std::string_view> first_five_fields(const std::string& line) {
    std::vector<std::string_view> fields;
    fields.reserve(5);
    std::size_t begin = 0;
    while (fields.size() < 5) {
        const auto separator = line.find('|', begin);
        if (separator == std::string::npos) {
            fields.emplace_back(line.data() + begin, line.size() - begin);
            break;
        }
        fields.emplace_back(line.data() + begin, separator - begin);
        begin = separator + 1;
    }
    while (fields.size() < 5) fields.emplace_back();
    return fields;
}

void load_namespace(const fs::path& path, bool system_namespace,
                    ExternalSignalCatalog& result) {
    if (!fs::is_regular_file(path)) return;
    const auto bytes = read_bytes(path);
    std::size_t offset = 0;
    std::size_t source_line = 0;
    while (offset < bytes.size()) {
        // TdxW uses fgets(buffer, 255), so one host read contains at most 254
        // bytes.  A physical line longer than that is consequently processed
        // as multiple records; retain that boundary here.
        const auto begin = offset;
        std::size_t count = 0;
        while (offset < bytes.size() && count < 254) {
            const auto ch = bytes[offset++];
            ++count;
            if (ch == '\n') break;
        }
        ++source_line;
        std::string line(reinterpret_cast<const char*>(bytes.data() + begin),
                         count);
        line = raw_trim(std::move(line));
        const auto fields = first_five_fields(line);

        ExternalSignalRecord record;
        record.system_namespace = system_namespace;
        record.market_id = static_cast<unsigned char>(host_atol(fields[0]));
        std::string raw_code(fields[1]);
        if (raw_code.size() > 10) raw_code.resize(10);
        record.code = decode_field(raw_code);
        record.numeric_code = record.market_id <= 2 ? host_atol(raw_code) : 0;
        record.external_id = host_atol(fields[2]);
        record.text = decode_field(fields[3]);
        record.value = host_atof(fields[4]);
        record.source_line = source_line;
        result.records.push_back(std::move(record));
    }
}

bool security_matches(const ExternalSignalRecord& record, int market_id,
                      const std::string& code) {
    if (market_id <= 2) {
        if (record.market_id != market_id) return false;
        return record.numeric_code == host_atol(code);
    }
    const bool compatible_derivative_market =
        (market_id == 71 || market_id == 31) &&
        (record.market_id == 71 || record.market_id == 31);
    return (record.market_id == market_id || compatible_derivative_market) &&
           record.code == code;
}

int market_id_from_text(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz") return 0;
    if (value == "sh") return 1;
    if (value == "bj") return 2;
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < 0 || parsed > 255)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error("market must be sz/sh/bj or an integer in 0..255");
    }
}

std::optional<int> optional_integer(const std::string& value,
                                    std::string_view name) {
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const long long parsed = std::stoll(value, &used);
        if (used != value.size() ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max())
            throw std::invalid_argument("range");
        return static_cast<int>(parsed);
    } catch (...) {
        throw Error(std::string(name) + " must be a 32-bit integer");
    }
}

}  // namespace

ExternalSignalCatalog load_external_signal_catalog(const fs::path& tdx_root) {
    ExternalSignalCatalog result;
    const auto directory = tdx_root / "T0002" / "signals";
    result.user_path = directory / "extern_user.txt";
    result.system_path = directory / "extern_sys.txt";
    result.user_exists = fs::is_regular_file(result.user_path);
    result.system_exists = fs::is_regular_file(result.system_path);
    load_namespace(result.system_path, true, result);
    load_namespace(result.user_path, false, result);
    return result;
}

const ExternalSignalRecord* find_external_signal(
    const ExternalSignalCatalog& catalog, bool system_namespace,
    int market_id, const std::string& code, int external_id) {
    for (const auto& record : catalog.records) {
        if (record.system_namespace != system_namespace ||
            record.external_id != external_id)
            continue;
        if (security_matches(record, market_id, code)) return &record;
    }
    return nullptr;
}

Json external_signal_catalog_document(
    const fs::path& tdx_root, std::optional<int> market_id,
    const std::string& code, std::optional<bool> system_namespace,
    std::optional<int> external_id) {
    if (market_id.has_value() != !code.empty())
        throw Error("external signal filtering requires both market and code");
    const auto catalog = load_external_signal_catalog(tdx_root);
    Json result = Json::object();
    result["schema"] = "tdx-external-signal-catalog-v1";
    result["tdx_root"] = path_utf8(tdx_root);
    result["user_path"] = path_utf8(catalog.user_path);
    result["system_path"] = path_utf8(catalog.system_path);
    result["user_exists"] = catalog.user_exists;
    result["system_exists"] = catalog.system_exists;
    result["record_format"] = "market|code|external_id|text|float_value";
    result["lookup_order"] = "first matching record in namespace file order";
    result["namespace_selector"] = "0=user; low-byte-nonzero=system";
    result["missing_numeric_value"] = 0.0;
    result["missing_text_value"] = " ";
    result["system_runtime_gate"] =
        "file decoded when present; proprietary TdxW edition gate is not applied";
    Json records = Json::array();
    std::size_t user_count = 0, system_count = 0;
    for (const auto& record : catalog.records) {
        if (record.system_namespace) ++system_count;
        else ++user_count;
        if (system_namespace && record.system_namespace != *system_namespace)
            continue;
        if (external_id && record.external_id != *external_id) continue;
        if (market_id && !security_matches(record, *market_id, code)) continue;
        Json row = Json::object();
        row["namespace"] = record.system_namespace ? "system" : "user";
        row["namespace_selector"] = record.system_namespace ? 1 : 0;
        row["market_id"] = record.market_id;
        row["code"] = record.code;
        row["external_id"] = record.external_id;
        row["text"] = trim(record.text);
        row["value"] = static_cast<double>(record.value);
        row["source_line"] = static_cast<std::uint64_t>(record.source_line);
        records.push_back(std::move(row));
    }
    result["user_record_count"] = static_cast<std::uint64_t>(user_count);
    result["system_record_count"] = static_cast<std::uint64_t>(system_count);
    result["record_count"] = static_cast<std::uint64_t>(catalog.records.size());
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    return result;
}

int command_formulas_external_signals(const std::vector<std::string>& values) {
    Args arguments(values);
    if (arguments.take_flag("--help") || arguments.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas extern-signals [options]\n\n"
            "  --root PATH              TDX installation root\n"
            "  --market sz|sh|bj|ID     Filter current security market\n"
            "  --code CODE              Filter current security code\n"
            "  --namespace all|user|system\n"
            "  --id N                   Filter external identifier\n"
            "  --output FILE            Write JSON instead of stdout\n"
            "  --compact                Compact JSON\n";
        return 0;
    }
    const auto root_text = arguments.take_option("--root");
    const auto market_text = arguments.take_option("--market");
    const auto code = trim(arguments.take_option("--code"));
    const auto namespace_text = lower_ascii(trim(
        arguments.take_option("--namespace", "all")));
    const auto id = optional_integer(arguments.take_option("--id"), "id");
    const auto output_text = arguments.take_option("--output");
    const bool compact = arguments.take_flag("--compact");
    arguments.require_empty();

    std::optional<int> market_id;
    if (!market_text.empty()) market_id = market_id_from_text(market_text);
    std::optional<bool> system_namespace;
    if (namespace_text == "user") system_namespace = false;
    else if (namespace_text == "system") system_namespace = true;
    else if (namespace_text != "all")
        throw Error("namespace must be all, user, or system");

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto document = external_signal_catalog_document(
        root, market_id, code, system_namespace, id);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = fs::u8path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "exported " << document.at("returned").as_number()
                  << " external-signal records -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
