#include "tdx/calendar.hpp"
#include "tdx/time.hpp"

#include "calendar_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace calendar_detail {

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}
bool string_is(const Json* value, std::string_view expected) {
    return value && value->is_string() && value->as_string() == expected;
}
std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}
std::optional<double> number_value(const Json& row, std::string_view name) {
    const auto text = text_value(row, name);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto value = std::stod(text, &used);
        return used == text.size() && std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
    } catch (...) { return std::nullopt; }
}
Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}
Json scaled(const Json& row, std::string_view name, double scale) {
    const auto value = number_value(row, name);
    return value ? Json(*value * scale) : Json(nullptr);
}
int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    if (normalized == "74" || normalized == "us") return 74;
    try { return std::stoi(normalized); }
    catch (...) { throw Error("market must be sz, sh, bj, or a numeric TDX market id"); }
}
std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj" :
        id == 74 ? "us" : "m" + std::to_string(id);
}
bool code_digits(const std::string& code) {
    return !code.empty() && std::all_of(code.begin(), code.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
}
Json security_document_values(
    int id, const std::string& code, const std::string& source_name,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (id < 0 || !code_digits(code)) return Json(nullptr);
    Json result = Json::object();
    result["market_id"] = id;
    result["market"] = market_name(id);
    result["code"] = code;
    result["security_id"] = market_name(id) + code;
    const auto found = securities.find({id, code});
    const auto name = found == securities.end() ? source_name : found->second.name;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}
Json security_document(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities,
    std::string_view market_field,
    std::string_view code_field,
    std::string_view source_name_field) {
    const auto code = text_value(row, code_field);
    int id = -1;
    try { id = market_id(text_value(row, market_field)); } catch (...) {}
    return security_document_values(
        id, code, text_value(row, source_name_field), securities);
}
Json us_security_document(const Json& row) {
    const auto code = text_value(row, "$ZQDM");
    if (code.empty()) return Json(nullptr);
    Json result = Json::object();
    result["market_id"] = 74;
    result["market"] = "us";
    result["code"] = code;
    result["security_id"] = "US:" + code;
    result["name"] = text_value(row, "ZQJC");
    result["name_resolved"] = !text_value(row, "ZQJC").empty();
    return result;
}

bool board_allows(const std::string& kind, int market, const std::string& code) {
    if (kind == "star-news") return market == 1 &&
        (code.rfind("688", 0) == 0 || code.rfind("689", 0) == 0);
    if (kind == "chinext-news") return market == 0 &&
        (code.rfind("300", 0) == 0 || code.rfind("301", 0) == 0);
    if (kind == "neeq-news") return market == 2 &&
        (code.rfind("43", 0) == 0 || code.rfind("83", 0) == 0 ||
         code.rfind("87", 0) == 0 || code.rfind("92", 0) == 0);
    return market >= 0 && market <= 2;
}

int inferred_market(const std::string& code) {
    if (code.rfind("30", 0) == 0 || code.rfind("00", 0) == 0) return 0;
    if (code.rfind("60", 0) == 0 || code.rfind("68", 0) == 0) return 1;
    if (code.rfind("43", 0) == 0 || code.rfind("83", 0) == 0 ||
        code.rfind("87", 0) == 0 || code.rfind("92", 0) == 0) return 2;
    return -1;
}

std::vector<std::string> six_digit_tokens(const std::string& text) {
    std::vector<std::string> result;
    for (std::size_t offset = 0; offset + 6 <= text.size(); ++offset) {
        if (offset && text[offset - 1] >= '0' && text[offset - 1] <= '9') continue;
        bool digits = true;
        for (std::size_t index = 0; index < 6; ++index)
            digits = digits && text[offset + index] >= '0' && text[offset + index] <= '9';
        if (!digits || (offset + 6 < text.size() && text[offset + 6] >= '0' &&
                        text[offset + 6] <= '9')) continue;
        result.push_back(text.substr(offset, 6));
        offset += 5;
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

Json related_securities(
    const std::string& headline, const std::string& kind,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    Json result = Json::array();
    std::set<std::pair<int, std::string>> selected;
    for (const auto& code : six_digit_tokens(headline)) {
        const int inferred = inferred_market(code);
        if (inferred >= 0 && board_allows(kind, inferred, code))
            selected.insert({inferred, code});
        else for (int market = 0; market <= 2; ++market)
            if (securities.count({market, code}) && board_allows(kind, market, code))
                selected.insert({market, code});
    }
    if (selected.empty()) {
        for (int market = 0; market <= 2 && selected.size() < 16; ++market) {
            if ((kind == "star-news" && market != 1) ||
                (kind == "chinext-news" && market != 0) ||
                (kind == "neeq-news" && market != 2)) continue;
            const auto begin = securities.lower_bound({market, {}});
            const auto end = securities.lower_bound({market + 1, {}});
            for (auto current = begin; current != end; ++current) {
                const auto& [key, security] = *current;
                if (!board_allows(kind, key.first, key.second) ||
                    security.name.size() < 6 ||
                    headline.find(security.name) == std::string::npos) continue;
                selected.insert(key);
                if (selected.size() >= 16) break;
            }
        }
    }
    for (const auto& [market, code] : selected)
        result.push_back(security_document_values(market, code, {}, securities));
    return result;
}

void replace_text(std::string& value, const std::string& from,
                  const std::string& to) {
    if (from.empty()) return;
    std::size_t offset = 0;
    while ((offset = value.find(from, offset)) != std::string::npos) {
        value.replace(offset, from.size(), to);
        offset += to.size();
    }
}

RichText parse_rich_text(const std::string& raw) {
    RichText result;
    const auto marker = raw.find("TXT:");
    result.headline = trim(marker == std::string::npos ? raw : raw.substr(0, marker));
    const auto body = marker == std::string::npos ? std::string{} : raw.substr(marker + 4);
    for (const auto quote : {'\'', '"'}) {
        const auto prefix = std::string("href=") + quote;
        const auto start = body.find(prefix);
        if (start == std::string::npos) continue;
        const auto value_start = start + prefix.size();
        const auto end = body.find(quote, value_start);
        if (end != std::string::npos)
            result.source_url = body.substr(value_start, end - value_start);
        break;
    }
    std::string plain;
    bool in_tag = false;
    for (std::size_t index = 0; index < body.size(); ++index) {
        const char ch = body[index];
        if (ch == '<') {
            in_tag = true;
            const auto end = body.find('>', index + 1);
            if (end != std::string::npos) {
                const auto tag = lower_ascii(body.substr(index + 1, end - index - 1));
                if (tag.rfind("p", 0) == 0 || tag.rfind("/p", 0) == 0 ||
                    tag.rfind("br", 0) == 0 || tag.rfind("li", 0) == 0 ||
                    tag.rfind("/li", 0) == 0) plain.push_back('\n');
            }
            continue;
        }
        if (ch == '>') { in_tag = false; continue; }
        if (!in_tag) plain.push_back(ch);
    }
    replace_text(plain, "&nbsp;", " ");
    replace_text(plain, "&amp;", "&");
    replace_text(plain, "&lt;", "<");
    replace_text(plain, "&gt;", ">");
    replace_text(plain, "&quot;", "\"");
    replace_text(plain, "&#39;", "'");
    std::string compact;
    bool previous_space = false, previous_newline = false;
    for (const char ch : plain) {
        if (ch == '\r') continue;
        if (ch == '\n') {
            while (!compact.empty() && compact.back() == ' ') compact.pop_back();
            if (!compact.empty() && !previous_newline) compact.push_back('\n');
            previous_newline = true;
            previous_space = false;
        } else if (ch == ' ' || ch == '\t') {
            if (!previous_space && !previous_newline) compact.push_back(' ');
            previous_space = true;
        } else {
            compact.push_back(ch);
            previous_space = false;
            previous_newline = false;
        }
    }
    result.content = trim(compact);
    return result;
}

std::string excerpt(const std::string& value, std::size_t limit) {
    if (value.size() <= limit) return value;
    auto end = limit;
    while (end && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80) --end;
    return value.substr(0, end) + "…";
}
std::string date_key(std::string value) {
    std::string digits;
    for (const auto ch : value) if (ch >= '0' && ch <= '9') digits.push_back(ch);
    return digits.size() >= 8 ? digits.substr(0, 8) : digits;
}
Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"}) result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized_rows);
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
        throw Error("local calendar resource is unavailable: " + path_utf8(path));
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
    throw Error("missing calendar resource: " + std::string(resource));
}
int bounded(const std::string& text, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high) throw std::invalid_argument("range");
        return value;
    } catch (...) { throw Error(std::string(name) + " must be in " + std::to_string(low) + ".." + std::to_string(high)); }
}

Json base_record(const std::string& kind, const std::string& date,
                 const std::string& title, const std::string& resource,
                 const Json& raw) {
    Json item = Json::object();
    item["kind"] = kind;
    item["date"] = date;
    item["title"] = title;
    item["importance"] = "";
    item["region"] = "";
    item["frequency"] = "";
    item["event_id"] = "";
    item["industry"] = "";
    item["event_type"] = "";
    item["content"] = "";
    item["content_excerpt"] = "";
    item["source_url"] = "";
    item["source_resource"] = resource;
    item["security"] = Json(nullptr);
    item["related_securities"] = Json::array();
    if (resource.rfind("list/func_gsrl", 0) == 0 ||
        resource == kMajorEvents || resource == kRecentIpos ||
        resource == kIpoGuidance || resource == kIpoReviews ||
        resource == kIpoSubscriptions || resource == kIpoSubscriptionDetails ||
        resource == kIpoCompanionNews || resource == kUsIpoApplications ||
        resource == kUsIpoCalendar || resource == kUsIpoListed ||
        resource == kUsIpoPending ||
        resource == kIpoAnnouncements || resource == kStarNews ||
        resource == kChinextNews || resource == kNeeqNews)
        item["raw"] = raw;
    return item;
}

std::string article_kind(const std::string& resource) {
    if (resource == kIpoAnnouncements) return "ipo-announcement";
    if (resource == kStarNews) return "star-news";
    if (resource == kChinextNews) return "chinext-news";
    if (resource == kNeeqNews) return "neeq-news";
    return {};
}

std::string kind_label(const std::string& kind) {
    if (kind == "major-event") return "重大事项提醒";
    if (kind == "ipo-announcement") return "新股发行公告";
    if (kind == "recent-ipo") return "次新股表现";
    if (kind == "ipo-guidance") return "IPO上市辅导";
    if (kind == "ipo-review") return "IPO审核";
    if (kind == "ipo-subscription") return "新股申购";
    if (kind == "ipo-subscription-detail") return "北证新股申购详情";
    if (kind == "ipo-companion-news") return "北证日历伴随资讯";
    if (kind == "us-ipo-application") return "美股申请上市";
    if (kind == "us-ipo-calendar") return "美股IPO日历";
    if (kind == "us-ipo-listed") return "美股已上市新股";
    if (kind == "us-ipo-pending") return "美股待上市新股";
    if (kind == "star-news") return "科创板资讯";
    if (kind == "chinext-news") return "创业板资讯";
    if (kind == "neeq-news") return "新三板资讯";
    if (kind == "futures-calendar") return "期货交易日历";
    if (kind == "rights-issue") return "配股日历";
    return kind;
}

std::string rights_issue_stage(const std::string& event_type) {
    if (event_type.find("股权登记") != std::string::npos) return "record-date";
    if (event_type.find("缴款开始") != std::string::npos) return "payment-start";
    if (event_type.find("缴款结束") != std::string::npos) return "payment-end";
    if (event_type.find("除权") != std::string::npos) return "ex-rights";
    return "other";
}

bool is_board_news(const std::string& kind) {
    return kind == "star-news" || kind == "chinext-news" || kind == "neeq-news";
}

const ViewDefinition &view_definition(std::string_view name) {
    const auto found = std::find_if(
        view_definitions.begin(), view_definitions.end(),
        [name](const ViewDefinition &definition) { return definition.name == name; });
    if (found == view_definitions.end()) {
        throw Error(
            "view must be all, macro, meetings, company, listings, rights-issues, major-events, "
            "ipo-announcements, recent-ipos, ipo-guidance, ipo-review, ipo-subscriptions, "
            "ipo-subscription-details, ipo-companion-news, us-ipo, us-ipo-applications, "
            "us-ipo-calendar, us-ipo-listed, us-ipo-pending, board-news, star-news, "
            "chinext-news, neeq-news, or futures");
    }
    return *found;
}

bool view_matches(CalendarView view, const std::string& kind) {
    if (view == CalendarView::all) return true;
    if (view == CalendarView::meetings) return kind == "meeting";
    if (view == CalendarView::listings) return kind == "listing";
    if (view == CalendarView::company) return kind == "company" ||
        kind == "ipo-listing-event" || kind == "ipo-issue-event" ||
        kind == "suspension-resumption" || kind == "special-treatment" ||
        kind == "listing-status" || kind == "rights-issue" ||
        kind == "additional-issuance" ||
        kind == "shareholder-meeting";
    if (view == CalendarView::rights_issues) return kind == "rights-issue";
    if (view == CalendarView::major_events) return kind == "major-event";
    if (view == CalendarView::ipo_announcements) return kind == "ipo-announcement";
    if (view == CalendarView::recent_ipos) return kind == "recent-ipo";
    if (view == CalendarView::ipo_guidance) return kind == "ipo-guidance";
    if (view == CalendarView::ipo_review) return kind == "ipo-review";
    if (view == CalendarView::ipo_subscriptions) return kind == "ipo-subscription";
    if (view == CalendarView::ipo_subscription_details) return kind == "ipo-subscription-detail";
    if (view == CalendarView::ipo_companion_news) return kind == "ipo-companion-news";
    if (view == CalendarView::us_ipo) return kind.rfind("us-ipo-", 0) == 0;
    if (view == CalendarView::us_ipo_applications) return kind == "us-ipo-application";
    if (view == CalendarView::us_ipo_calendar) return kind == "us-ipo-calendar";
    if (view == CalendarView::us_ipo_listed) return kind == "us-ipo-listed";
    if (view == CalendarView::us_ipo_pending) return kind == "us-ipo-pending";
    if (view == CalendarView::futures) return kind == "futures-calendar";
    if (view == CalendarView::board_news) return is_board_news(kind);
    const auto &definition = *std::find_if(
        view_definitions.begin(), view_definitions.end(),
        [view](const ViewDefinition &candidate) { return candidate.view == view; });
    return definition.name == kind;
}

bool same_security(const Json& security, int market, const std::string& code) {
    return !security.is_null() &&
        static_cast<int>(security.at("market_id").as_number()) == market &&
        security.at("code").as_string() == code;
}

bool row_has_security(const Json& row, int market, const std::string& code) {
    if (same_security(row.at("security"), market, code)) return true;
    for (const auto& security : row.at("related_securities").as_array())
        if (same_security(security, market, code)) return true;
    return false;
}

}  // namespace calendar_detail

}  // namespace tdx
