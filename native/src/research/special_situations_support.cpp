#include "special_situations_internal.hpp"
#include "tdx/time.hpp"

namespace tdx::special_situations_detail {
namespace fs = std::filesystem;

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        return used == value.size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int normalized_market(int id) { return id == 44 ? 2 : id; }

int parsed_market(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    return -1;
}

int inferred_market(
    const std::string& value,
    const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto explicit_market = parsed_market(value);
    if (explicit_market >= 0) return explicit_market;
    int found = -1;
    for (const auto id : {0, 1, 2}) {
        if (!securities.count({id, code})) continue;
        if (found >= 0) return found;
        found = id;
    }
    if (found >= 0) return found;
    if (code.size() == 6 && (code.rfind("83", 0) == 0 ||
        code.rfind("87", 0) == 0 || code.rfind("92", 0) == 0 ||
        code.rfind("43", 0) == 0)) return 2;
    if (!code.empty() && (code.front() == '6' || code.front() == '9')) return 1;
    return 0;
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : "bj";
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ";
}

Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    id = normalized_market(id);
    if (id < 0 || id > 2 || !digits(code)) return Json(nullptr);
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market_id"] = id;
    result["market"] = market_name(id);
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

Json security_from_raw(
    const Json& raw, std::string_view market_key, std::string_view code_key,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto code = text_value(raw, code_key);
    if (!digits(code)) return Json(nullptr);
    return security_document(
        inferred_market(text_value(raw, market_key), code, securities),
        code, securities);
}

Json string_array(const std::string& value) {
    Json result = Json::array();
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(',', start);
        const auto item = trim(value.substr(start,
            end == std::string::npos ? std::string::npos : end - start));
        if (!item.empty()) result.push_back(item);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
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
        throw Error("local special-situation resource is unavailable: " +
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
    result["rows"] = std::move(rows);
    return result;
}

const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing special-situation resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    return result;
}

std::optional<double> quote_price(const Json& quote) {
    return number_value(quote, "last_price");
}

Json premium(const std::optional<double>& current,
             const std::optional<double>& reference) {
    if (!current || !reference || std::abs(*reference) < 0.000001)
        return Json(nullptr);
    return Json((*current - *reference) * 100.0 / *reference);
}

Json quote_for(const Json& security,
               const std::map<std::pair<int, std::string>, Json>& quotes) {
    if (security.is_null()) return Json(nullptr);
    const auto key = std::make_pair(
        static_cast<int>(security.at("market_id").as_number()),
        security.at("code").as_string());
    const auto found = quotes.find(key);
    return found == quotes.end() ? Json(nullptr) : found->second;
}

int bounded(const std::string& value, std::string_view name,
            int low, int high) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}


}  // namespace tdx::special_situations_detail
