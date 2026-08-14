#include "stats_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace stats_detail;

namespace {

std::optional<std::string> optional_text(const std::string& value) {
    const auto result = trim(value);
    return result.empty() ? std::nullopt : std::optional<std::string>(result);
}

std::optional<double> optional_double(const std::string& value) {
    const auto text = trim(value);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double result = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(result)) return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> optional_int(const std::string& value) {
    const auto number = optional_double(value);
    if (!number || *number < std::numeric_limits<int>::min() ||
        *number > std::numeric_limits<int>::max()) return std::nullopt;
    return static_cast<int>(*number);
}

std::vector<std::string> payload_lines(const Bytes& payload) {
    const std::string text(reinterpret_cast<const char*>(payload.data()), payload.size());
    auto lines = split(text, '\n');
    for (auto& line : lines) if (!line.empty() && line.back() == '\r') line.pop_back();
    return lines;
}

std::string normalized_code(std::string code) {
    code = trim(std::move(code));
    if (code.size() < 6) code.insert(code.begin(), 6 - code.size(), '0');
    return code;
}

std::vector<TdxStatRow> parse_stat_rows(const Bytes& payload) {
    std::vector<TdxStatRow> rows;
    for (const auto& line : payload_lines(payload)) {
        const auto parts = split(line, '|');
        if (parts.size() < 35) continue;
        const auto market = optional_int(parts[0]);
        const auto code = normalized_code(parts[1]);
        if (!market || *market < 0 || *market > 2 || code.size() != 6 ||
            !std::all_of(code.begin(), code.end(), [](char ch) {
                return ch >= '0' && ch <= '9';
            })) continue;
        const auto shape_packed = optional_int(parts[22]);
        rows.push_back(TdxStatRow{
            *market, code, optional_text(parts[4]), optional_double(parts[2]),
            optional_double(parts[3]), optional_double(parts[9]),
            optional_double(parts[11]), shape_packed,
            shape_packed ? std::optional<int>(*shape_packed / 10000) : std::nullopt,
            shape_packed ? std::optional<int>(*shape_packed % 10000 / 100) : std::nullopt,
            shape_packed ? std::optional<int>(*shape_packed % 100) : std::nullopt,
            optional_int(parts[26]), optional_int(parts[31]), optional_int(parts[32]),
            optional_int(parts[33])});
    }
    return rows;
}

std::vector<TdxStat2Row> parse_stat2_rows(const Bytes& payload) {
    std::vector<TdxStat2Row> rows;
    for (const auto& line : payload_lines(payload)) {
        const auto parts = split(line, '|');
        if (parts.size() < 21) continue;
        const auto market = optional_int(parts[0]);
        const auto code = normalized_code(parts[1]);
        if (!market || *market < 0 || *market > 2 || code.size() != 6 ||
            !std::all_of(code.begin(), code.end(), [](char ch) {
                return ch >= '0' && ch <= '9';
            })) continue;
        rows.push_back(TdxStat2Row{
            *market, code, optional_text(parts[2]), optional_double(parts[3]),
            optional_double(parts[4]), optional_double(parts[5]), optional_double(parts[6]),
            optional_double(parts[7]), optional_double(parts[8]), optional_double(parts[9]),
            optional_double(parts[10]), optional_double(parts[14]), optional_double(parts[15])});
    }
    return rows;
}

std::vector<TdxTipInfoEventRow> parse_tip_info_event_rows(const Bytes& payload) {
    std::vector<TdxTipInfoEventRow> rows;
    for (const auto& line : payload_lines(payload)) {
        const auto parts = split(line, '|');
        if (parts.size() < 22) continue;
        const auto market = optional_int(parts[0]);
        const auto code = normalized_code(parts[1]);
        if (!market || *market < 0 || *market > 2 || code.size() != 6 ||
            !std::all_of(code.begin(), code.end(), [](char ch) {
                return ch >= '0' && ch <= '9';
            })) continue;
        rows.push_back(TdxTipInfoEventRow{
            *market, code, optional_text(parts[17]), optional_double(parts[18]),
            optional_text(parts[19]), optional_double(parts[20]),
            optional_text(parts[21])});
    }
    return rows;
}

template <typename Row>
std::pair<std::optional<std::string>, double> dominant_date(
    const std::map<std::pair<int, std::string>, Row>& rows) {
    std::map<std::string, std::size_t> counts;
    for (const auto& [key, row] : rows) {
        (void)key;
        if (row.stats_date) ++counts[*row.stats_date];
    }
    if (counts.empty()) return {std::nullopt, 0.0};
    auto best = counts.begin();
    for (auto item = counts.begin(); item != counts.end(); ++item)
        if (std::tie(item->second, item->first) > std::tie(best->second, best->first))
            best = item;
    return {best->first, static_cast<double>(best->second) /
                         static_cast<double>(std::max<std::size_t>(1, rows.size()))};
}


}  // namespace

TdxStatsResource parse_stats_files(const Bytes& stat_payload,
                                   const Bytes& stat2_payload,
                                   std::string source_path,
                                   const Bytes* tip_info_payload) {
    const auto stat_rows = parse_stat_rows(stat_payload);
    const auto stat2_rows = parse_stat2_rows(stat2_payload);
    if (stat_rows.empty() || stat2_rows.empty())
        throw Error("tdxstat.cfg/tdxstat2.cfg contain no usable records");
    TdxStatsResource resource;
    resource.source_path = std::move(source_path);
    for (const auto& row : stat_rows) {
        const auto key = std::make_pair(row.market_id, row.code);
        if (!resource.stat.emplace(key, row).second)
            throw Error("tdxstat.cfg contains duplicate security: " + security_id(row.market_id, row.code));
    }
    for (const auto& row : stat2_rows) {
        const auto key = std::make_pair(row.market_id, row.code);
        if (!resource.stat2.emplace(key, row).second)
            throw Error("tdxstat2.cfg contains duplicate security: " + security_id(row.market_id, row.code));
    }
    if (tip_info_payload) for (const auto& row : parse_tip_info_event_rows(*tip_info_payload)) {
        const auto key = std::make_pair(row.market_id, row.code);
        if (!resource.tip_info_events.emplace(key, row).second)
            throw Error("tipinfo.dat contains duplicate security: " +
                        security_id(row.market_id, row.code));
    }
    const auto [left_date, left_coverage] = dominant_date(resource.stat);
    const auto [right_date, right_coverage] = dominant_date(resource.stat2);
    resource.stats_date_coverage = left_date && left_date == right_date
        ? std::min(left_coverage, right_coverage) : 0.0;
    if (left_date && left_date == right_date && resource.stats_date_coverage >= 0.95)
        resource.stats_date = left_date;
    return resource;
}


TdxStatsResource load_local_stats(const fs::path& directory) {
    const auto resolved = fs::weakly_canonical(directory);
    const auto tip_path = resolved / "tipinfo.dat";
    const auto tip_payload = fs::is_regular_file(tip_path)
        ? std::optional<Bytes>(read_bytes(tip_path)) : std::nullopt;
    return parse_stats_files(read_bytes(resolved / "tdxstat.cfg"),
                             read_bytes(resolved / "tdxstat2.cfg"),
                             path_utf8(resolved),
                             tip_payload ? &*tip_payload : nullptr);
}


}  // namespace tdx
