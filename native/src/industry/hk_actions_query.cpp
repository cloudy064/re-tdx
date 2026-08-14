#include "hk_actions_internal.hpp"
#include "tdx/time.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::array<std::string_view, 9> kKinds{{
    "all", "dividend", "bonus", "rights", "split", "consolidation",
    "mixed", "adjustment", "other",
}};

bool digits(std::string_view value, std::size_t count) {
    return value.size() == count &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string generated_at() {
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

bool has_flag(const std::vector<std::string>& flags, const std::string& kind) {
    return std::find(flags.begin(), flags.end(), kind) != flags.end();
}

bool matches_kind(const hk_actions_detail::ActionRecord& record,
                  const std::string& kind) {
    if (kind == "all") return true;
    const auto flags = hk_actions_detail::classify_flags(record.description);
    const auto primary = hk_actions_detail::primary_kind(flags, record);
    return primary == kind || has_flag(flags, kind);
}

bool matches_query(const hk_actions_detail::ActionRecord& record,
                   const HkActionQuery& query) {
    if (!query.code.empty() && record.code != query.code) return false;
    if (!query.date_from.empty() && record.date < query.date_from) return false;
    if (!query.date_to.empty() && record.date > query.date_to) return false;
    if (!matches_kind(record, query.kind)) return false;
    const auto needle = lower_ascii(trim(query.query));
    if (needle.empty()) return true;
    const auto flags = hk_actions_detail::classify_flags(record.description);
    std::string haystack = record.code + " " + record.date + " " +
        record.description + " " +
        hk_actions_detail::primary_kind(flags, record);
    for (const auto& flag : flags) haystack += " " + flag;
    return lower_ascii(haystack).find(needle) != std::string::npos;
}

Json source_json(const fs::path& path, std::size_t row_count) {
    Json source = Json::object();
    source["file"] = path.filename().string();
    source["path"] = path_utf8(path);
    source["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    source["row_count"] = static_cast<std::uint64_t>(row_count);
    source["endpoint"] = "local-file:" + path_utf8(path);
    source["encrypted"] = true;
    source["cipher"] = "Blowfish-ECB/native-little-endian-words";
    return source;
}

void validate_query(HkActionQuery& query) {
    query.code = trim(query.code);
    query.query = trim(query.query);
    query.kind = lower_ascii(trim(query.kind));
    query.date_from = trim(query.date_from);
    query.date_to = trim(query.date_to);
    query.order = lower_ascii(trim(query.order));
    if (!query.code.empty() && !digits(query.code, 5))
        throw Error("code must contain exactly five digits");
    if (!query.date_from.empty() && !digits(query.date_from, 8))
        throw Error("from must be YYYYMMDD");
    if (!query.date_to.empty() && !digits(query.date_to, 8))
        throw Error("to must be YYYYMMDD");
    if (!query.date_from.empty() && !query.date_to.empty() &&
        query.date_from > query.date_to)
        throw Error("from must not be after to");
    if (std::find(kKinds.begin(), kKinds.end(), query.kind) == kKinds.end())
        throw Error("kind must be all, dividend, bonus, rights, split, "
                    "consolidation, mixed, adjustment, or other");
    if (query.order != "asc" && query.order != "desc")
        throw Error("order must be asc or desc");
    if (query.offset < 0 || query.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (query.limit < 1 || query.limit > 2000)
        throw Error("limit must be in 1..2000");
}

}  // namespace

Json load_local_hk_actions(const fs::path& root,
                           const HkActionQuery& input) {
    HkActionQuery query = input;
    validate_query(query);
    const auto record_set = hk_actions_detail::load_records(root);
    const auto& records = *record_set;

    std::vector<const hk_actions_detail::ActionRecord*> matched;
    matched.reserve(records.size());
    for (const auto& record : records)
        if (matches_query(record, query)) matched.push_back(&record);
    std::stable_sort(matched.begin(), matched.end(),
        [&](const auto* left, const auto* right) {
            if (left->date != right->date)
                return query.order == "asc"
                    ? left->date < right->date : left->date > right->date;
            if (left->code != right->code) return left->code < right->code;
            return left->description < right->description;
        });

    std::set<std::string> securities;
    std::array<std::size_t, kKinds.size()> kind_counts{};
    std::array<std::size_t, 5> flag_counts{};
    constexpr std::array<std::string_view, 5> flag_names{{
        "dividend", "bonus", "rights", "split", "consolidation",
    }};
    std::string earliest;
    std::string latest;
    for (const auto* record : matched) {
        securities.insert(record->code);
        if (earliest.empty() || record->date < earliest) earliest = record->date;
        if (latest.empty() || record->date > latest) latest = record->date;
        const auto flags = hk_actions_detail::classify_flags(record->description);
        const auto primary = hk_actions_detail::primary_kind(flags, *record);
        const auto found = std::find(kKinds.begin(), kKinds.end(), primary);
        if (found != kKinds.end())
            ++kind_counts[static_cast<std::size_t>(found - kKinds.begin())];
        for (std::size_t index = 0; index < flag_names.size(); ++index)
            if (has_flag(flags, std::string(flag_names[index]))) ++flag_counts[index];
    }

    Json rows = Json::array();
    const auto begin = std::min<std::size_t>(
        static_cast<std::size_t>(query.offset), matched.size());
    const auto end = std::min<std::size_t>(
        begin + static_cast<std::size_t>(query.limit), matched.size());
    for (std::size_t index = begin; index < end; ++index)
        rows.push_back(hk_actions_detail::record_json(*matched[index]));

    Json by_kind = Json::object();
    for (std::size_t index = 1; index < kKinds.size(); ++index)
        by_kind[std::string(kKinds[index])] =
            static_cast<std::uint64_t>(kind_counts[index]);
    Json by_flag = Json::object();
    for (std::size_t index = 0; index < flag_names.size(); ++index)
        by_flag[std::string(flag_names[index])] =
            static_cast<std::uint64_t>(flag_counts[index]);
    Json summary = Json::object();
    summary["securities"] = static_cast<std::uint64_t>(securities.size());
    summary["earliest_date"] = earliest.empty() ? Json(nullptr) : Json(earliest);
    summary["latest_date"] = latest.empty() ? Json(nullptr) : Json(latest);
    summary["by_primary_kind"] = std::move(by_kind);
    summary["by_flag"] = std::move(by_flag);

    const auto cache = root / "T0002" / "hq_cache";
    Json sources = Json::array();
    for (const auto* name : {"hkqxinfo2.dat", "hkqxinfo.dat"}) {
        const auto path = cache / name;
        if (!fs::is_regular_file(path)) continue;
        const auto count = std::count_if(records.begin(), records.end(),
            [&](const auto& record) { return record.source_file == name; });
        sources.push_back(source_json(path, static_cast<std::size_t>(count)));
    }

    Json native = Json::object();
    native["loader"] = "TdxW!sub_51EA60";
    native["record_size"] = 85;
    native["factor_offsets"] = Json::array();
    native["factor_offsets"].push_back(73);
    native["factor_offsets"].push_back(77);
    native["forward_step"] =
        "price * previous_multiplier / cumulative_multiplier - "
        "(cumulative_offset - previous_offset) / cumulative_multiplier";
    native["backward_step"] =
        "price * cumulative_multiplier / previous_multiplier + "
        "(cumulative_offset - previous_offset) / previous_multiplier";

    Json transport = Json::object();
    transport["kind"] = "local-encrypted-files";
    transport["network_requests"] = 0;

    Json result = Json::object();
    result["schema"] = "tdx-market-hk-actions-native-v1";
    result["generated_at"] = generated_at();
    result["source_mode"] = "local";
    result["mode"] = query.code.empty() ? "catalog" : "security";
    result["availability"] = matched.empty() ? "empty" : "local";
    result["kind"] = query.kind;
    result["order"] = query.order;
    result["offset"] = query.offset;
    result["match_count"] = static_cast<std::uint64_t>(matched.size());
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["summary"] = std::move(summary);
    result["rows"] = std::move(rows);
    result["sources"] = std::move(sources);
    result["native_semantics"] = std::move(native);
    result["transport"] = std::move(transport);
    result["semantics"] =
        "Long-history HK corporate actions decoded from the client's encrypted "
        "hkqxinfo resources. The two native factors are cumulative; each event's "
        "share multiplier and additive price adjustment are derived from adjacent "
        "records using the exact TdxW adjustment consumer formulas.";
    return result;
}

}  // namespace tdx
