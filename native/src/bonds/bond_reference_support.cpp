#include "bond_reference_internal.hpp"
#include "tdx/time.hpp"

namespace tdx::bond_reference_detail {
namespace fs = std::filesystem;

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    auto found = value.as_object().find(key);
    if (found != value.as_object().end()) return &found->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::string first_text_value(const Json& value,
                             std::initializer_list<std::string_view> keys) {
    for (const auto key : keys) {
        auto result = text_value(value, key);
        if (!result.empty()) return result;
    }
    return {};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto raw = text_value(value, key);
    if (raw.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

std::optional<double> first_number_value(
    const Json& value, std::initializer_list<std::string_view> keys) {
    for (const auto key : keys)
        if (auto result = number_value(value, key)) return result;
    return std::nullopt;
}


bool contains_text(const Json& row, const std::string& needle) {
    if (needle.empty()) return true;
    for (const auto* path : {"bond_type", "bond_credit_rating",
                             "issuer_credit_rating", "rate_type"}) {
        const auto& value = row.at(path);
        if (value.is_string() &&
            lower_ascii(value.as_string()).find(needle) != std::string::npos) return true;
    }
    const auto& security = row.at("security");
    return lower_ascii(security.at("code").as_string() + " " +
                       security.at("name").as_string()).find(needle) != std::string::npos;
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

int bounded(const std::string& text, const std::string& name, int low, int high) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(low) + ".." +
                    std::to_string(high));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

Json load_local_resource_rows(const fs::path& root,
                              const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local bond-reference resource is unavailable: " +
                    path_utf8(path));
    const auto tables = load_jsn_tables(path);
    Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group) {
        const auto& table = tables[group];
        for (const auto& cells : table.rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < table.headers.size(); ++column)
                row[table.headers[column]] = cells[column];
            row["_group"] = static_cast<std::uint64_t>(group);
            rows.push_back(std::move(row));
        }
    }
    Json result = Json::object();
    result["resource"] = resource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path);
    result["attempts"] = 0;
    result["stale"] = false;
    result["age_seconds"] = 0;
    result["upstream_error"] = nullptr;
    result["rows"] = std::move(rows);
    return result;
}

}  // namespace tdx::bond_reference_detail
