#include "tdx/event_impact.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
namespace tdx {
namespace {

constexpr const char* kShanghai = "list/func_zdsj101_1.jsn";
constexpr const char* kHangSeng = "list/func_zdsj102_1.jsn";
constexpr const char* kNasdaq = "list/func_zdsj103_1.jsn";
const std::vector<std::string> kResources{kShanghai, kHangSeng, kNasdaq};

const Json* field(const Json& row, std::string_view name) { if (!row.is_object()) return nullptr; const auto found = row.as_object().find(name); return found == row.as_object().end() ? nullptr : &found->second; }
std::string text_value(const Json& row, std::string_view name) { const auto* value = field(row, name); return value ? trim(jsn_scalar_text(*value)) : std::string{}; }
std::optional<double> number_value(const Json& row, std::string_view name) { const auto value = text_value(row, name); if (value.empty()) return std::nullopt; try { std::size_t used = 0; const auto parsed = std::stod(value, &used); if (used == value.size() && std::isfinite(parsed)) return parsed; } catch (...) {} return std::nullopt; }
Json number(const Json& row, std::string_view name) { const auto value = number_value(row, name); return value ? Json(*value) : Json(nullptr); }
struct Benchmark { const char* id; const char* label; const char* resource; };
Benchmark benchmark_for(const std::string& resource) { if (resource == kShanghai) return {"shanghai-composite", "上证指数", kShanghai}; if (resource == kHangSeng) return {"hang-seng", "恒生指数", kHangSeng}; if (resource == kNasdaq) return {"nasdaq-composite", "纳斯达克指数", kNasdaq}; throw Error("unknown event-impact resource: " + resource); }
std::pair<std::string, std::string> content(const std::string& value) { const auto marker = value.rfind("TXT:"); return marker == std::string::npos ? std::make_pair(trim(value), std::string{}) : std::make_pair(trim(value.substr(0, marker)), trim(value.substr(marker + 4))); }
std::string now_text() { const auto now = std::time(nullptr); std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output; output << local_timestamp_text(local); return output.str(); }
fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}
int bounded(const std::string& value, std::string_view name, int low, int high) { try { std::size_t used = 0; const auto parsed = std::stoi(value, &used); if (used != value.size() || parsed < low || parsed > high) throw std::invalid_argument("range"); return parsed; } catch (...) { throw Error(std::string(name) + " must be in " + std::to_string(low) + ".." + std::to_string(high)); } }
Json load_local(const fs::path& root, const std::string& resource) { const auto path = root / native_path(resource); if (!fs::is_regular_file(path)) throw Error("local event-impact resource is unavailable: " + path_utf8(path)); const auto tables = load_jsn_tables(path); Json rows = Json::array(); for (std::size_t group = 0; group < tables.size(); ++group) for (const auto& cells : tables[group].rows) { Json row = Json::object(); for (std::size_t column = 0; column < tables[group].headers.size(); ++column) row[tables[group].headers[column]] = cells[column]; row["_group"] = static_cast<std::uint64_t>(group); rows.push_back(std::move(row)); } Json result = Json::object(); result["resource"] = resource; result["size"] = static_cast<std::uint64_t>(fs::file_size(path)); result["row_count"] = static_cast<std::uint64_t>(rows.size()); result["endpoint"] = "local-jsn:" + path_utf8(path); result["rows"] = std::move(rows); return result; }
const Json& document_for(const Json& documents, const std::string& resource) { for (const auto& document : documents.as_array()) if (text_value(document, "resource") == resource) return document; throw Error("missing event-impact resource: " + resource); }
Json source_summary(const Json& document, std::size_t normalized) { Json result = Json::object(); for (const auto* key : {"resource", "size", "row_count", "endpoint"}) result[key] = document.at(key); result["normalized_row_count"] = static_cast<std::uint64_t>(normalized); return result; }
std::optional<double> record_number(const Json& row, std::string_view name) { const auto* value = field(row, name); return value && value->is_number() ? std::optional<double>(value->as_number()) : std::nullopt; }
}  // namespace

Json normalize_event_impact_rows(const std::string& resource, const Json& rows) {
    if (!rows.is_array())
        throw Error("event-impact rows must be an array");
    const auto benchmark = benchmark_for(resource);
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto parsed = content(text_value(raw, "ms")); if (parsed.first.empty()) continue;
        Json item = Json::object(); item["benchmark"] = benchmark.id; item["benchmark_label"] = benchmark.label; item["event_type"] = text_value(raw, "sjmc"); item["description"] = parsed.first; item["source_url"] = parsed.second; item["start_date"] = text_value(raw, "date1"); item["end_date"] = text_value(raw, "date2");
        Json closes = Json::object(); closes["week_before_start"] = number(raw, "szzs1"); closes["day_before_start"] = number(raw, "szzs5"); closes["start_day"] = number(raw, "szzs2"); closes["end_day"] = number(raw, "szzs3"); closes["day_after_end"] = number(raw, "szzs6"); closes["week_after_end"] = number(raw, "szzs4"); item["closes"] = std::move(closes);
        Json impacts = Json::object(); impacts["week_before_start_pct"] = number(raw, "qjzf1"); impacts["start_day_pct"] = number(raw, "qjzf4"); impacts["event_interval_pct"] = number(raw, "qjzf2"); impacts["day_after_end_pct"] = number(raw, "qjzf5"); impacts["week_after_end_pct"] = number(raw, "qjzf3"); item["impacts"] = std::move(impacts);
        item["source_resource"] = resource; item["raw"] = raw; item["record_id"] = std::string(benchmark.id) + ":" + item.at("start_date").as_string() + ":" + std::to_string(result.size()); result.push_back(std::move(item));
    }
    return result;
}

EventImpactService::EventImpactService(fs::path jsn_root) : jsn_root_(std::move(jsn_root)) {}
Json EventImpactService::fetch_master(const EventImpactQuery& options, bool& refreshed, int& age_seconds) { const auto now = std::time(nullptr); age_seconds = cache_time_ ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0; if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) { refreshed = false; return cache_; } Json documents = Json::array(); if (!options.refresh && !jsn_root_.empty()) try { for (const auto& resource : kResources) documents.push_back(load_local(jsn_root_, resource)); } catch (...) { documents = Json::array(); } if (!documents.size()) documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms); Json records = Json::array(), sources = Json::array(); for (const auto& resource : kResources) { const auto& document = document_for(documents, resource); auto rows = normalize_event_impact_rows(resource, document.at("rows")); sources.push_back(source_summary(document, rows.size())); for (auto& row : rows.as_array()) records.push_back(std::move(row)); } Json result = Json::object(); result["records"] = std::move(records); result["sources"] = std::move(sources); cache_ = result; cache_time_ = std::time(nullptr); refreshed = true; age_seconds = 0; return result; }

Json EventImpactService::query(const EventImpactQuery& options) {
    const std::set<std::string> benchmarks{"all", "shanghai-composite", "hang-seng", "nasdaq-composite"}; if (!benchmarks.count(options.benchmark)) throw Error("benchmark must be all, shanghai-composite, hang-seng, or nasdaq-composite"); if (options.sort != "date" && options.sort != "interval" && options.sort != "after-week") throw Error("sort must be date, interval, or after-week"); if (options.order != "asc" && options.order != "desc") throw Error("order must be asc or desc"); if (options.limit < 1 || options.limit > 10000) throw Error("limit must be in 1..10000");
    bool refreshed = false; int age_seconds = 0; const auto master = fetch_master(options, refreshed, age_seconds); const auto needle = lower_ascii(trim(options.query)); const auto type = lower_ascii(trim(options.type)); Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) { if (options.benchmark != "all" && text_value(row, "benchmark") != options.benchmark) continue; if (!type.empty() && lower_ascii(text_value(row, "event_type")) != type) continue; const auto date = text_value(row, "start_date"); if (!options.date_from.empty() && date < options.date_from) continue; if (!options.date_to.empty() && date > options.date_to) continue; if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue; records.push_back(row); }
    const bool asc = options.order == "asc"; std::stable_sort(records.as_array().begin(), records.as_array().end(), [&](const Json& left, const Json& right) { if (options.sort == "date") { const auto a = text_value(left, "start_date"), b = text_value(right, "start_date"); if (a != b) return asc ? a < b : a > b; } else { const auto key = options.sort == "interval" ? "event_interval_pct" : "week_after_end_pct"; const auto a = record_number(left.at("impacts"), key), b = record_number(right.at("impacts"), key); if (a && b && *a != *b) return asc ? *a < *b : *a > *b; if (a.has_value() != b.has_value()) return a.has_value(); } return left.at("record_id").as_string() < right.at("record_id").as_string(); });
    std::map<std::string, std::uint64_t> benchmark_counts, type_counts; std::set<std::string> dates; for (const auto& row : records.as_array()) { ++benchmark_counts[text_value(row, "benchmark")]; ++type_counts[text_value(row, "event_type")]; dates.insert(text_value(row, "start_date")); }
    const auto matched = records.size(); if (records.size() > static_cast<std::size_t>(options.limit)) records.as_array().resize(options.limit); if (!options.include_raw) for (auto& row : records.as_array()) row.as_object().erase("raw");
    Json summary = Json::object(); summary["shanghai_composite"] = benchmark_counts["shanghai-composite"]; summary["hang_seng"] = benchmark_counts["hang-seng"]; summary["nasdaq_composite"] = benchmark_counts["nasdaq-composite"]; summary["event_types"] = static_cast<std::uint64_t>(type_counts.size()); summary["date_count"] = static_cast<std::uint64_t>(dates.size()); summary["earliest_date"] = dates.empty() ? "" : *dates.begin(); summary["latest_date"] = dates.empty() ? "" : *dates.rbegin();
    Json result = Json::object(); result["schema"] = "tdx-market-event-impact-native-v1"; result["generated_at"] = now_text(); result["benchmark"] = options.benchmark; result["availability"] = matched ? "live" : "empty"; result["match_count"] = static_cast<std::uint64_t>(matched); result["returned"] = static_cast<std::uint64_t>(records.size()); result["records"] = std::move(records); result["summary"] = std::move(summary); result["sources"] = master.at("sources"); result["semantics"] = "Client-curated global events with source benchmark closes and percentage-point impact windows. Empty end dates and empty source returns remain empty; the service does not infer event windows."; Json cache = Json::object(); cache["refreshed"] = refreshed; cache["age_seconds"] = age_seconds; result["cache"] = std::move(cache); return result;
}

int command_market_event_impact(const std::vector<std::string>& raw_args) { Args args(raw_args); if (args.take_flag("--help") || args.take_flag("-h")) { std::cout << "Usage: tdx-tool market event-impact [--benchmark all|shanghai-composite|hang-seng|nasdaq-composite] [--type TEXT] [--query TEXT] [--from DATE] [--to DATE] [--sort date|interval|after-week] [--order asc|desc] [--without-raw] [--refresh] [--input-dir PATH] [--limit N] [--output PATH] [--compact]\n"; return 0; } EventImpactQuery query; query.benchmark = lower_ascii(trim(args.take_option("--benchmark", "all"))); query.type = trim(args.take_option("--type")); query.query = trim(args.take_option("--query")); query.date_from = trim(args.take_option("--from")); query.date_to = trim(args.take_option("--to")); query.sort = lower_ascii(trim(args.take_option("--sort", "date"))); query.order = lower_ascii(trim(args.take_option("--order", "desc"))); query.include_raw = !args.take_flag("--without-raw"); query.refresh = args.take_flag("--refresh"); query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 10000); query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000); const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn")); const auto output = native_path(args.take_option("--output", "output/tdx-market-event-impact-native.json")); const bool compact = args.take_flag("--compact"); args.require_empty(); EventImpactService service(input); const auto document = service.query(query); atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n"); std::cout << "returned " << document.at("returned").as_number() << " event-impact rows -> " << path_utf8(output) << '\n'; return 0; }

}  // namespace tdx
