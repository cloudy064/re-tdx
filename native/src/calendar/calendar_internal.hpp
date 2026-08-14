#pragma once

#include "tdx/calendar.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::calendar_detail {

enum class CalendarView {
    all,
    macro,
    meetings,
    company,
    listings,
    rights_issues,
    major_events,
    ipo_announcements,
    recent_ipos,
    ipo_guidance,
    ipo_review,
    ipo_subscriptions,
    ipo_subscription_details,
    ipo_companion_news,
    us_ipo,
    us_ipo_applications,
    us_ipo_calendar,
    us_ipo_listed,
    us_ipo_pending,
    board_news,
    star_news,
    chinext_news,
    neeq_news,
    futures,
};

struct ViewDefinition {
    std::string_view name;
    CalendarView view;
};

inline constexpr std::array<ViewDefinition, 24> view_definitions{{
    {"all", CalendarView::all},
    {"macro", CalendarView::macro},
    {"meetings", CalendarView::meetings},
    {"company", CalendarView::company},
    {"listings", CalendarView::listings},
    {"rights-issues", CalendarView::rights_issues},
    {"major-events", CalendarView::major_events},
    {"ipo-announcements", CalendarView::ipo_announcements},
    {"recent-ipos", CalendarView::recent_ipos},
    {"ipo-guidance", CalendarView::ipo_guidance},
    {"ipo-review", CalendarView::ipo_review},
    {"ipo-subscriptions", CalendarView::ipo_subscriptions},
    {"ipo-subscription-details", CalendarView::ipo_subscription_details},
    {"ipo-companion-news", CalendarView::ipo_companion_news},
    {"us-ipo", CalendarView::us_ipo},
    {"us-ipo-applications", CalendarView::us_ipo_applications},
    {"us-ipo-calendar", CalendarView::us_ipo_calendar},
    {"us-ipo-listed", CalendarView::us_ipo_listed},
    {"us-ipo-pending", CalendarView::us_ipo_pending},
    {"board-news", CalendarView::board_news},
    {"star-news", CalendarView::star_news},
    {"chinext-news", CalendarView::chinext_news},
    {"neeq-news", CalendarView::neeq_news},
    {"futures", CalendarView::futures},
}};

inline constexpr char kMacro[] = "list/func_cjrl101_1.jsn";
inline constexpr char kMeetings[] = "list/func_cjrl105_1.jsn";
inline constexpr char kListings[] = "list/func_ggrl101_1.jsn";
inline constexpr char kCompanies[] = "list/func_gsrl206_1.jsn";
inline constexpr char kIpoListingEvents[] = "list/func_gsrl201_1.jsn";
inline constexpr char kIpoIssueEvents[] = "list/func_gsrl202_1.jsn";
inline constexpr char kSuspensionEvents[] = "list/func_gsrl203_1.jsn";
inline constexpr char kSpecialTreatmentEvents[] = "list/func_gsrl204_1.jsn";
inline constexpr char kListingStatusEvents[] = "list/func_gsrl205_1.jsn";
inline constexpr char kRightsIssueEvents[] = "list/func_gsrl207_1.jsn";
inline constexpr char kAdditionalIssuanceEvents[] = "list/func_gsrl208_1.jsn";
inline constexpr char kShareholderMeetingEvents[] = "list/func_gsrl209_1.jsn";
inline constexpr char kMajorEvents[] = "list/func_dsjtx101_1.jsn";
inline constexpr char kIpoAnnouncements[] = "list/func_xgrl101_1.jsn";
inline constexpr char kRecentIpos[] = "list/func_xgrl102_1.jsn";
inline constexpr char kIpoGuidance[] = "list/func_zdgz_qzkcbd101_1.jsn";
inline constexpr char kIpoReviews[] = "list/func_zdgz_kcbsq101_1.jsn";
inline constexpr char kIpoSubscriptions[] = "list/func_zdgz_kcbsq103_1.jsn";
inline constexpr char kIpoSubscriptionDetails[] = "list/func_sbxg101_1.jsn";
inline constexpr char kIpoCompanionNews[] = "list/func_sbxg102_1.jsn";
inline constexpr char kUsIpoApplications[] = "list/func_mgrl101_1.jsn";
inline constexpr char kUsIpoCalendar[] = "list/func_mgrl102_1.jsn";
inline constexpr char kUsIpoListed[] = "list/func_mgxg101_1.jsn";
inline constexpr char kUsIpoPending[] = "list/func_mgxg102_1.jsn";
inline constexpr char kStarNews[] = "list/func_xgrl103_1.jsn";
inline constexpr char kChinextNews[] = "list/func_xgrl104_1.jsn";
inline constexpr char kNeeqNews[] = "list/func_xgrl105_1.jsn";
inline constexpr char kFuturesCalendar[] = "list/func_qhrl400_1.jsn";

inline const std::vector<std::string> kResources{
    kMacro, kMeetings, kListings, kCompanies, kIpoListingEvents,
    kIpoIssueEvents, kSuspensionEvents, kSpecialTreatmentEvents,
    kListingStatusEvents, kRightsIssueEvents, kAdditionalIssuanceEvents,
    kShareholderMeetingEvents, kMajorEvents, kIpoAnnouncements, kRecentIpos,
    kIpoGuidance, kIpoReviews, kIpoSubscriptions, kIpoSubscriptionDetails,
    kIpoCompanionNews, kUsIpoApplications, kUsIpoCalendar, kUsIpoListed,
    kUsIpoPending, kStarNews, kChinextNews, kNeeqNews, kFuturesCalendar};

struct RichText {
    std::string headline;
    std::string content;
    std::string source_url;
};

const ViewDefinition &view_definition(std::string_view name);
const Json *field(const Json &row, std::string_view name);
bool string_is(const Json *value, std::string_view expected);
std::string text_value(const Json &row, std::string_view name);
std::optional<double> number_value(const Json &row, std::string_view name);
Json number(const Json &row, std::string_view name);
Json scaled(const Json &row, std::string_view name, double scale);
int market_id(const std::string &value);
std::string market_name(int id);
bool code_digits(const std::string &code);
Json security_document_values(
    int id, const std::string &code, const std::string &fallback_name,
    const std::map<std::pair<int, std::string>, Security> &securities);
Json security_document(
    const Json &row,
    const std::map<std::pair<int, std::string>, Security> &securities,
    std::string_view market_field = "$SC", std::string_view code_field = "$ZQDM",
    std::string_view source_name_field = "ZQJC");
Json us_security_document(const Json &row);
bool board_allows(const std::string &kind, int market, const std::string &code);
int inferred_market(const std::string &code);
Json related_securities(
    const std::string &headline, const std::string &kind,
    const std::map<std::pair<int, std::string>, Security> &securities);
RichText parse_rich_text(const std::string &raw);
std::string excerpt(const std::string &value, std::size_t limit = 240);
std::string date_key(std::string value);
Json source_summary(const Json &document, std::size_t normalized_rows);
std::string now_text();
std::filesystem::path native_path(const std::string &value);
Json load_local_resource_rows(const std::filesystem::path &root,
                              const std::string &resource);
const Json &document_for(const Json &documents, std::string_view resource);
int bounded(const std::string &text, std::string_view name, int low, int high);
Json base_record(const std::string &kind, const std::string &date,
                 const std::string &title, const std::string &content,
                 const Json &raw);
std::string article_kind(const std::string &resource);
std::string kind_label(const std::string &kind);
std::string rights_issue_stage(const std::string &event_type);
bool view_matches(CalendarView view, const std::string &kind);
bool row_has_security(const Json &row, int market, const std::string &code);

}  // namespace tdx::calendar_detail
