#include "disclosures_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace tdx {
using namespace disclosure_detail;

Json normalize_disclosure_schedule_rows(
    const Json& rows, bool hong_kong,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("disclosure schedule rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto security = security_document(row, securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["kind"] = "schedule";
        item["region"] = hong_kong ? "hong-kong" : "mainland";
        item["security"] = security;
        item["report_period"] = hong_kong ? text_value(row, "jzri")
                                            : text_value(row, "bgq");
        item["report_type"] = hong_kong ? text_value(row, "bgq") : "";
        item["report_start"] = hong_kong ? text_value(row, "ksrq") : "";
        item["scheduled_disclosure_date"] = text_value(row, "ypldate");
        item["first_scheduled_date"] = text_value(row, "scpldate");
        item["change_dates"] = change_dates(row);
        item["change_status"] = text_value(row, "plqk");
        const auto actual = text_value(row, "spldate");
        item["actual_disclosure_date"] = actual;
        item["available_from"] = actual.empty() ? Json(nullptr) : Json(actual);
        item["status"] = disclosure_status(row);
        item["industry"] = text_value(row, "hy");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_disclosure_express_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("disclosure express rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto security = security_document(row, securities);
        if (security.is_null()) continue;
        const auto date = text_value(row, "GGRQ");
        Json item = Json::object();
        item["kind"] = "express";
        item["region"] = "mainland";
        item["security"] = security;
        item["report_period"] = text_value(row, "bgq");
        item["scheduled_disclosure_date"] = "";
        item["actual_disclosure_date"] = date;
        item["available_from"] = date.empty() ? Json(nullptr) : Json(date);
        item["status"] = "disclosed";
        item["net_profit_yuan"] = number_or_null(row, "jlr1");
        item["prior_net_profit_yuan"] = number_or_null(row, "jlr2");
        item["net_profit_yoy_pct"] = number_or_null(row, "jlr3");
        item["adjusted_net_profit_yuan"] = number_or_null(row, "kfjrl1");
        item["adjusted_net_profit_yoy_pct"] = number_or_null(row, "kfjrl3");
        item["weighted_roe_pct"] = number_or_null(row, "roe1");
        item["basic_eps"] = number_or_null(row, "MGSY");
        item["book_value_per_share"] = number_or_null(row, "JZC");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_recent_disclosure_rows(
    const Json& rows, bool hong_kong,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("recent disclosure rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto security = security_document(row, securities);
        if (security.is_null()) continue;
        const auto date = text_value(row, "plrq");
        Json item = Json::object();
        item["kind"] = "recent";
        item["region"] = hong_kong ? "hong-kong" : "mainland";
        item["security"] = security;
        item["report_period"] = hong_kong ? text_value(row, "jzri")
                                            : text_value(row, "bgq");
        item["report_type"] = hong_kong ? text_value(row, "bgq") : "";
        item["report_start"] = hong_kong ? text_value(row, "ksrq") : "";
        item["scheduled_disclosure_date"] = "";
        item["actual_disclosure_date"] = date;
        item["available_from"] = date.empty() ? Json(nullptr) : Json(date);
        item["status"] = "disclosed";
        item["currency"] = hong_kong ? text_value(row, "bz") : "CNY";
        item["net_profit"] = number_or_null(row, "jlr1");
        item["prior_net_profit"] = number_or_null(row, "jlr2");
        item["net_profit_yoy_pct"] = number_or_null(row, "jlr3");
        item["weighted_roe_pct"] = number_or_null(row, "roe1");
        item["revenue"] = number_or_null(row, "yysr1");
        item["prior_revenue"] = number_or_null(row, "yysr2");
        item["revenue_yoy_pct"] = number_or_null(row, "yysr3");
        item["operating_cash_flow"] = number_or_null(row, "xjl");
        item["basic_eps"] = number_or_null(row, "eps");
        item["industry"] = text_value(row, "hy");
        item["listing_board"] = text_value(row, "ssbk");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_disclosure_announcement_response(
    const Json& response, const std::string& market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = mainland_market_id(market);
    if (!digits(code, 6)) throw Error("announcement code must contain six digits");
    const auto source_rows = tqlex_rows(response);
    struct Group {
        std::string type;
        std::string date;
        Json selected;
        Json evidence{Json::array()};
        std::uint64_t full_report_count{};
    };
    std::map<std::string, Group> groups;
    for (const auto& row : source_rows.as_array()) {
        const auto classification = periodic_report_type_for_row(row);
        if (!classification) continue;
        const auto title = text_value(row, "title");
        const auto year = report_year(title, classification->title_phrase);
        if (year.empty()) continue;
        const auto period = year + classification->suffix;
        auto& group = groups[period];
        group.type = classification->key;
        group.evidence.push_back(announcement_evidence(row));
        if (title.find("摘要") != std::string::npos) continue;
        const auto date = compact_announcement_date(text_value(row, "issue_date"));
        if (date.empty()) continue;
        ++group.full_report_count;
        if (group.date.empty() || date < group.date ||
            (date == group.date && title < text_value(group.selected, "title"))) {
            group.date = date;
            group.selected = row;
        }
    }
    Json rows = Json::array();
    for (auto& [period, group] : groups) {
        if (group.date.empty()) continue;
        Json item = Json::object();
        item["kind"] = "announcement-report";
        item["region"] = "mainland";
        item["security"] = direct_security_document(id, code, securities);
        item["report_period"] = period;
        item["report_type"] = group.type;
        item["scheduled_disclosure_date"] = "";
        item["actual_disclosure_date"] = group.date;
        item["available_from"] = group.date;
        item["status"] = "disclosed";
        item["selected_announcement"] = announcement_evidence(group.selected);
        item["announcement_evidence"] = std::move(group.evidence);
        item["full_report_candidate_count"] = group.full_report_count;
        rows.push_back(std::move(item));
    }
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "report_period") > text_value(right, "report_period");
        });
    Json result = Json::object();
    result["schema"] = "tdx-disclosure-announcement-backfill-native-v1";
    result["generated_at"] = now_text();
    result["rows"] = std::move(rows);
    Json summary = Json::object();
    summary["announcement_count"] = static_cast<std::uint64_t>(source_rows.size());
    summary["full_report_period_count"] =
        static_cast<std::uint64_t>(result.at("rows").size());
    result["summary"] = std::move(summary);
    result["history_limit"] = "latest one year, at most 300 announcements";
    result["availability_semantics"] =
        "earliest observed full periodic-report issue_date; summaries do not unlock gpcw";
    return result;
}

Json fetch_disclosure_announcement_reports(
    const std::string& market, const std::string& code, int timeout_ms,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = mainland_market_id(market);
    if (!digits(code, 6)) throw Error("announcement code must contain six digits");
    Json request = Json::object();
    request["action"] = "get";
    request["key"] = "gg:" + std::to_string(id) + "_" + code;
    request["bin"] = "1";
    request["qsid"] = "tdx";
    auto result = normalize_disclosure_announcement_response(
        query_tqlex(announcement_entry, request,
                    cloud_endpoints::tqlex, timeout_ms),
        market_name(id), code, securities);
    Json source = Json::object();
    source["entry"] = announcement_entry;
    source["key"] = request.at("key");
    source["endpoint"] = cloud_endpoints::tqlex;
    source["history_limit"] = result.at("history_limit");
    result["source"] = std::move(source);
    return result;
}

std::vector<DisclosureBackfillSecurity> parse_tdx_watchlist_securities(
    std::string_view input,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    std::string text(input);
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef &&
        static_cast<unsigned char>(text[1]) == 0xbb &&
        static_cast<unsigned char>(text[2]) == 0xbf)
        text.erase(0, 3);
    std::map<std::pair<int, std::string>, DisclosureBackfillSecurity> unique;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(std::move(line));
        if (line.empty() || line.front() == '#') continue;
        if (line.size() != 7 || line.front() < '0' || line.front() > '2' ||
            !digits(line.substr(1), 6)) continue;
        const int id = line.front() - '0';
        const auto code = line.substr(1);
        const auto found = securities.find({id, code});
        unique[{id, code}] = DisclosureBackfillSecurity{
            market_name(id), code, found == securities.end() ? "" : found->second.name};
    }
    std::vector<DisclosureBackfillSecurity> result;
    result.reserve(unique.size());
    for (auto& [key, security] : unique) {
        (void)key;
        result.push_back(std::move(security));
    }
    return result;
}

Json parse_disclosure_listing_dates_dbf(const Bytes& data) {
    if (data.size() < 33) throw Error("listing-date DBF is shorter than its header");
    const auto declared_records = read_u32_le(data.data() + 4);
    const auto header_length = read_u16_le(data.data() + 8);
    const auto record_length = read_u16_le(data.data() + 10);
    if (header_length < 33 || header_length > data.size() || record_length < 2)
        throw Error("listing-date DBF header lengths are invalid");
    if (declared_records > (data.size() - header_length) / record_length)
        throw Error("listing-date DBF records exceed the file boundary");

    struct FieldSpec {
        std::string name;
        std::size_t offset{};
        std::size_t length{};
    };
    std::map<std::string, FieldSpec> fields;
    std::size_t record_offset = 1;
    bool terminator = false;
    for (std::size_t offset = 32; offset < header_length;) {
        if (data[offset] == 0x0d) {
            terminator = true;
            break;
        }
        if (offset + 32 > header_length)
            throw Error("listing-date DBF field descriptor is truncated");
        std::size_t name_length = 0;
        while (name_length < 11 && data[offset + name_length]) ++name_length;
        auto name = lower_ascii(std::string(
            reinterpret_cast<const char*>(data.data() + offset), name_length));
        const auto length = static_cast<std::size_t>(data[offset + 16]);
        if (name.empty() || !length || record_offset + length > record_length)
            throw Error("listing-date DBF field descriptor is invalid");
        fields[name] = FieldSpec{name, record_offset, length};
        record_offset += length;
        offset += 32;
    }
    if (!terminator) throw Error("listing-date DBF header terminator is missing");
    for (const auto* required : {"sc", "gpdm", "ssdate"})
        if (!fields.count(required))
            throw Error(std::string("listing-date DBF field is missing: ") + required);

    const auto valid_date = [](const std::string& value) {
        if (!digits(value, 8)) return false;
        const int year = std::stoi(value.substr(0, 4));
        const int month = std::stoi(value.substr(4, 2));
        const int day = std::stoi(value.substr(6, 2));
        if (year < 1900 || year > 2200 || month < 1 || month > 12 || day < 1)
            return false;
        static constexpr std::array<int, 12> days{
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const bool leap = year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
        return day <= days[static_cast<std::size_t>(month - 1)] +
            (month == 2 && leap ? 1 : 0);
    };
    const auto field_text = [&](const std::uint8_t* record, const FieldSpec& field) {
        return trim(std::string(
            reinterpret_cast<const char*>(record + field.offset), field.length));
    };
    std::map<std::string, Json> records;
    std::uint64_t deleted = 0, invalid = 0, duplicates = 0, conflicts = 0;
    for (std::uint32_t index = 0; index < declared_records; ++index) {
        const auto* record = data.data() + header_length +
            static_cast<std::size_t>(index) * record_length;
        if (record[0] == '*') {
            ++deleted;
            continue;
        }
        const auto market_value = field_text(record, fields.at("sc"));
        const auto code = field_text(record, fields.at("gpdm"));
        const auto date = field_text(record, fields.at("ssdate"));
        if (market_value.size() != 1 || market_value.front() < '0' ||
            market_value.front() > '2' || !digits(code, 6) || !valid_date(date)) {
            ++invalid;
            continue;
        }
        const int market_id = market_value.front() - '0';
        const auto security_id = market_prefix(market_id) + code;
        auto found = records.find(security_id);
        if (found != records.end()) {
            ++duplicates;
            const auto previous = text_value(found->second, "listing_date");
            if (previous != date) {
                ++conflicts;
                if (date < previous) found->second["listing_date"] = date;
            }
            continue;
        }
        Json item = Json::object();
        item["market"] = market_name(market_id);
        item["market_id"] = market_id;
        item["code"] = code;
        item["security_id"] = security_id;
        item["listing_date"] = date;
        item["source_field"] = "SSDATE";
        records.emplace(security_id, std::move(item));
    }
    Json values = Json::array();
    for (auto& [security_id, item] : records) {
        (void)security_id;
        values.push_back(std::move(item));
    }
    Json summary = Json::object();
    summary["declared_records"] = static_cast<std::uint64_t>(declared_records);
    summary["listing_date_count"] = static_cast<std::uint64_t>(values.size());
    summary["deleted_records"] = deleted;
    summary["invalid_records"] = invalid;
    summary["duplicate_records"] = duplicates;
    summary["conflicting_dates"] = conflicts;
    Json result = Json::object();
    result["schema"] = "tdx-listing-dates-dbf-native-v1";
    result["dbf_version"] = static_cast<std::uint64_t>(data[0]);
    const int dbf_year = data[1] < 80 ? 2000 + static_cast<int>(data[1])
                                     : 1900 + static_cast<int>(data[1]);
    result["dbf_updated_date"] =
        std::to_string(dbf_year) +
        (data[2] < 10 ? "0" : "") + std::to_string(data[2]) +
        (data[3] < 10 ? "0" : "") + std::to_string(data[3]);
    result["header_length"] = static_cast<std::uint64_t>(header_length);
    result["record_length"] = static_cast<std::uint64_t>(record_length);
    result["source_format"] = "dBASE III base.dbf SC+GPDM+SSDATE";
    result["records"] = std::move(values);
    result["summary"] = std::move(summary);
    return result;
}

}  // namespace tdx
