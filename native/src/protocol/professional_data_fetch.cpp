#include "professional_data_internal.hpp"

#include "tdx/http.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace professional_data_detail;

std::vector<ProfessionalTradingRecord> fetch_professional_stock_trading_data(
    const std::string& market, const std::string& code, const fs::path& cache_directory,
    int timeout_ms, bool refresh) {
    const auto security = parse_security(market + ":" + code);
    const auto name = "gp" + security.market + security.code + ".dat";
    const auto entries = fetch_manifest("tdxgp/gpszsh.txt", timeout_ms, 4 * 1024 * 1024);
    const auto payload = load_verified_resource(find_manifest_entry(entries, name), "tdxgp/",
                                                cache_directory, timeout_ms, refresh,
                                                maximum_trading_bytes);
    return parse_professional_trading_data(payload);
}

std::vector<ProfessionalTradingRecord> fetch_professional_market_trading_data(
    const fs::path& cache_directory, int timeout_ms, bool refresh) {
    const std::string name = "gpsh999999.dat";
    const auto entries = fetch_manifest("tdxgp/gpszsh.txt", timeout_ms, 4 * 1024 * 1024);
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& item) {
        return item.name == name;
    });
    if (found != entries.end())
        return parse_professional_trading_data(load_verified_resource(
            *found, "tdxgp/", cache_directory, timeout_ms, refresh, maximum_trading_bytes));

    // The official data URL serves this synthetic market record even when a
    // CDN variant of gpszsh.txt omits the line. In that case HTTPS origin plus
    // strict 13-byte record validation is the integrity boundary.
    const auto directory = cache_directory.empty() ? default_cache_directory() : cache_directory;
    const auto target = directory / fs::u8path(name);
    if (!refresh && fs::is_regular_file(target)) {
        try { return parse_professional_trading_data(read_bytes(target)); }
        catch (...) { /* Replace a malformed cache entry below. */ }
    }
    const auto response = http_get(std::string(data_root) + "tdxgp/" + name, {}, timeout_ms,
                                   maximum_trading_bytes);
    if (response.status != 200)
        throw Error(name + " returned HTTP " + std::to_string(response.status));
    const auto parsed = parse_professional_trading_data(response.body);
    atomic_write_bytes(target, response.body);
    return parsed;
}

ProfessionalFinanceData fetch_professional_finance_data(
    std::uint32_t report_date, const fs::path& cache_directory, int timeout_ms, bool refresh) {
    if (report_date < 19000101 || report_date > 22001231)
        throw Error("professional finance report date is invalid");
    const auto stem = "gpcw" + std::to_string(report_date);
    const auto archive_name = stem + ".zip", member_name = stem + ".dat";
    const auto entries = fetch_manifest("tdxfin/gpcw.txt", timeout_ms, 4 * 1024 * 1024);
    const auto archive = load_verified_resource(find_manifest_entry(entries, archive_name),
                                                "tdxfin/", cache_directory, timeout_ms,
                                                refresh, maximum_archive_bytes);
    return parse_professional_finance_data(extract_finance_member(archive, member_name),
                                           std::string(data_root) + "tdxfin/" + archive_name);
}

ProfessionalFinanceData fetch_latest_professional_finance_data_for_security(
    int market_id, const std::string& code, const fs::path& cache_directory,
    int timeout_ms, bool refresh) {
    if (market_id < 0 || market_id > 2 || !valid_code(code))
        throw Error("invalid security for professional finance lookup");
    auto entries = fetch_manifest("tdxfin/gpcw.txt", timeout_ms, 4 * 1024 * 1024);
    std::vector<std::pair<std::uint32_t, ManifestEntry>> packages;
    for (const auto& entry : entries) {
        if (entry.name.size() != 16 || entry.name.rfind("gpcw", 0) != 0 ||
            entry.name.substr(12) != ".zip" || entry.size < 100) continue;
        try {
            const auto date = static_cast<std::uint32_t>(std::stoul(entry.name.substr(4, 8)));
            if (date <= current_local_date()) packages.emplace_back(date, entry);
        } catch (...) { /* Ignore malformed package names. */ }
    }
    std::sort(packages.begin(), packages.end(), [](const auto& left, const auto& right) {
        return left.first > right.first;
    });
    std::vector<std::string> checked;
    for (const auto& [date, entry] : packages) {
        try {
            const auto archive = load_verified_resource(entry, "tdxfin/", cache_directory,
                                                        timeout_ms, refresh, maximum_archive_bytes);
            const auto member = "gpcw" + std::to_string(date) + ".dat";
            auto data = parse_professional_finance_data(extract_finance_member(archive, member),
                std::string(data_root) + "tdxfin/" + entry.name);
            if (data.records.count({market_id, code})) return data;
            checked.push_back(std::to_string(date) + ": security absent");
        } catch (const std::exception& error) {
            checked.push_back(std::to_string(date) + ": " + error.what());
        }
        // A normal report-season partial package plus the last complete quarter
        // is sufficient to locate the latest available record.
        if (checked.size() >= 4) break;
    }
    std::string detail = "security is absent from recent professional finance packages";
    for (const auto& item : checked) detail += "\n  " + item;
    throw Error(detail);
}

Json fetch_professional_finance_series_document(
    int market_id, const std::string& code, const std::vector<int>& fields,
    std::uint32_t from, std::uint32_t to, std::size_t maximum_periods,
    const fs::path& cache_directory, int timeout_ms, bool refresh) {
    if (market_id < 0 || market_id > 2 || !valid_code(code))
        throw Error("invalid security for professional finance series");
    if (!maximum_periods || maximum_periods > 80)
        throw Error("professional finance series period limit must be in 1..80");
    const auto effective_to = std::min(to, current_local_date());
    if (from > effective_to) throw Error("professional finance series date range is empty");
    auto entries = fetch_manifest("tdxfin/gpcw.txt", timeout_ms, 4 * 1024 * 1024);
    std::vector<std::pair<std::uint32_t, ManifestEntry>> packages;
    for (const auto& entry : entries) {
        if (entry.name.size() != 16 || entry.name.rfind("gpcw", 0) != 0 ||
            entry.name.substr(12) != ".zip" || entry.size < 100) continue;
        try {
            const auto date = static_cast<std::uint32_t>(std::stoul(entry.name.substr(4, 8)));
            if (date >= from && date <= effective_to) packages.emplace_back(date, entry);
        } catch (...) { /* Ignore malformed package names. */ }
    }
    std::sort(packages.begin(), packages.end(), [](const auto& left, const auto& right) {
        return left.first > right.first;
    });
    Json rows = Json::array(), errors = Json::array();
    std::size_t attempts = 0;
    for (const auto& [date, entry] : packages) {
        if (rows.size() >= maximum_periods || attempts >= maximum_periods + 4) break;
        ++attempts;
        try {
            const auto archive = load_verified_resource(entry, "tdxfin/", cache_directory,
                                                        timeout_ms, refresh, maximum_archive_bytes);
            const auto member = "gpcw" + std::to_string(date) + ".dat";
            const auto data = parse_professional_finance_data(
                extract_finance_member(archive, member),
                std::string(data_root) + "tdxfin/" + entry.name);
            const auto found = data.records.find({market_id, code});
            if (found == data.records.end()) throw Error("security absent from report package");
            std::vector<int> selected = fields;
            if (selected.empty()) for (std::size_t id = 1; id <= data.field_count; ++id)
                selected.push_back(static_cast<int>(id));
            Json values = Json::array();
            for (const int id : selected) {
                if (id < 0 || static_cast<std::size_t>(id) > data.field_count)
                    throw Error("FINVALUE field ID is outside the package field range");
                Json value = Json::object(); value["id"] = id;
                value["value"] = id == 0 ? Json(static_cast<std::uint64_t>(data.report_date))
                    : number_json(found->second.fields[static_cast<std::size_t>(id - 1)]);
                values.push_back(std::move(value));
            }
            Json row = Json::object(); row["report_date_raw"] = static_cast<std::uint64_t>(date);
            row["report_date"] = date_text(date); row["fields"] = std::move(values);
            rows.push_back(std::move(row));
        } catch (const std::exception& error) {
            Json failure = Json::object(); failure["report_date_raw"] = static_cast<std::uint64_t>(date);
            failure["report_date"] = date_text(date); failure["package"] = entry.name;
            failure["error"] = error.what(); errors.push_back(std::move(failure));
        }
    }
    std::reverse(rows.as_array().begin(), rows.as_array().end());
    Json result = Json::object(); result["schema"] = "tdx-professional-finance-series-v1";
    result["security_id"] = (market_id == 0 ? "SZ" : market_id == 1 ? "SH" : "BJ") + code;
    result["mode"] = "report-period-series-no-announcement-date";
    result["from_raw"] = static_cast<std::uint64_t>(from);
    result["requested_to_raw"] = static_cast<std::uint64_t>(to);
    result["effective_to_raw"] = static_cast<std::uint64_t>(effective_to);
    result["chronological"] = true; result["attempted_packages"] = static_cast<std::uint64_t>(attempts);
    result["period_count"] = static_cast<std::uint64_t>(rows.size());
    result["periods"] = std::move(rows); result["package_errors"] = std::move(errors);
    return result;
}

}  // namespace tdx
