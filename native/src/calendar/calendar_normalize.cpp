#include "tdx/calendar.hpp"

#include "calendar_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <utility>

namespace tdx {

using namespace calendar_detail;

Json normalize_calendar_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("calendar rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item;
        if (resource == kMacro) {
            item = base_record("macro", text_value(row, "date"),
                               text_value(row, "title"), resource, row);
            item["importance"] = text_value(row, "zyd");
            item["region"] = text_value(row, "dq");
            item["frequency"] = text_value(row, "pl");
            item["previous"] = number(row, "ycz");
            item["consensus"] = number(row, "gbz");
            item["actual"] = number(row, "qz");
            item["revised"] = number(row, "xzz");
        } else if (resource == kMeetings) {
            item = base_record("meeting", text_value(row, "date"),
                               text_value(row, "title"), resource, row);
            item["importance"] = text_value(row, "zyd");
            item["region"] = text_value(row, "dq");
            item["industry"] = text_value(row, "glhy");
            item["event_id"] = text_value(row, "$ZQDM");
            item["event_type"] = "热点会议";
        } else if (resource == kListings) {
            const auto security = security_document(row, securities);
            item = base_record("listing", text_value(row, "ssdate"),
                               text_value(row, "ZQJC"), resource, row);
            item["security"] = security;
            if (!security.is_null()) item["related_securities"].push_back(security);
            item["subscription_start"] = text_value(row, "sgksr");
            item["subscription_end"] = text_value(row, "sgjsr");
            item["issue_price_low"] = number(row, "zgj2");
            item["issue_price_high"] = number(row, "zgj1");
            item["issue_shares"] = number(row, "zgs");
            item["raised_yuan"] = number(row, "mjzj");
            item["issue_price"] = number(row, "ssj");
            item["subscription_multiple"] = number(row, "rgbs");
            item["winning_rate_pct"] = number(row, "zql");
            item["listing_return_pct"] = number(row, "ljzf");
            item["event_type"] = "新股上市";
        } else if (resource == kCompanies) {
            const auto security = security_document(row, securities);
            item = base_record("company", text_value(row, "date"),
                               text_value(row, "sj"), resource, row);
            item["event_type"] = text_value(row, "sj");
            item["content"] = text_value(row, "sjcontent");
            item["content_excerpt"] = excerpt(item.at("content").as_string());
            item["security"] = security;
            if (!security.is_null()) item["related_securities"].push_back(security);
        } else if (resource == kIpoListingEvents ||
                   resource == kIpoIssueEvents ||
                   resource == kSpecialTreatmentEvents ||
                   resource == kListingStatusEvents ||
                   resource == kRightsIssueEvents ||
                   resource == kAdditionalIssuanceEvents ||
                   resource == kShareholderMeetingEvents) {
            const auto security = security_document(row, securities);
            if (security.is_null() && text_value(row, "date").empty() &&
                text_value(row, "sj").empty() &&
                text_value(row, "sjcontent").empty()) continue;
            std::string kind, fallback;
            if (resource == kIpoListingEvents) {
                kind = "ipo-listing-event"; fallback = "首发新股上市";
            } else if (resource == kIpoIssueEvents) {
                kind = "ipo-issue-event"; fallback = "首发新股发行";
            } else if (resource == kSpecialTreatmentEvents) {
                kind = "special-treatment"; fallback = "实施特别处理";
            } else if (resource == kListingStatusEvents) {
                kind = "listing-status"; fallback = "暂停、恢复或终止上市";
            } else if (resource == kRightsIssueEvents) {
                kind = "rights-issue"; fallback = "配股";
            } else if (resource == kAdditionalIssuanceEvents) {
                kind = "additional-issuance"; fallback = "增发";
            } else {
                kind = "shareholder-meeting"; fallback = "股东大会";
            }
            const auto event_type = text_value(row, "sj").empty()
                ? fallback : text_value(row, "sj");
            const auto content = text_value(row, "sjcontent");
            item = base_record(kind, text_value(row, "date"),
                               event_type, resource, row);
            item["event_type"] = event_type;
            item["content"] = content;
            item["content_excerpt"] = excerpt(content);
            if (kind == "rights-issue")
                item["rights_issue_stage"] = rights_issue_stage(event_type);
            item["security"] = security;
            if (!security.is_null()) {
                item["related_securities"].push_back(security);
                const auto name = security.at("name").as_string();
                item["title"] = name.empty() ? event_type : name + " · " + event_type;
                item["event_id"] = kind + ":" + text_value(row, "date") +
                    ":" + security.at("security_id").as_string();
            }
        } else if (resource == kSuspensionEvents) {
            const auto security = security_document(row, securities);
            const auto suspension_date = text_value(row, "tpdate");
            const auto reason = text_value(row, "tpyy");
            if (security.is_null() && suspension_date.empty() && reason.empty())
                continue;
            item = base_record("suspension-resumption", suspension_date,
                               reason.empty() ? "停复牌" : reason, resource, row);
            item["event_type"] = "停复牌";
            item["content"] = reason;
            item["content_excerpt"] = excerpt(reason);
            item["suspension_date"] = suspension_date;
            item["expected_resumption_date"] = text_value(row, "yjfpsj");
            item["resumption_date"] = text_value(row, "fpdate");
            item["suspension_days"] = number(row, "tpts");
            item["pre_suspension_day_change_pct"] = [&]() -> Json {
                const auto before = number_value(row, "Price5");
                const auto close = number_value(row, "Price6");
                return before && close && *before != 0.0
                    ? Json((*close / *before - 1.0) * 100.0) : Json(nullptr);
            }();
            item["resumption_day_change_pct"] = [&]() -> Json {
                const auto before = number_value(row, "Price3");
                const auto close = number_value(row, "Price4");
                return before && close && *before != 0.0
                    ? Json((*close / *before - 1.0) * 100.0) : Json(nullptr);
            }();
            item["security"] = security;
            if (!security.is_null()) {
                item["related_securities"].push_back(security);
                const auto name = security.at("name").as_string();
                item["title"] = name.empty() ? item.at("title")
                    : Json(name + " · " + item.at("title").as_string());
                item["event_id"] = "suspension-resumption:" + suspension_date +
                    ":" + security.at("security_id").as_string();
            }
        } else if (resource == kMajorEvents) {
            const auto security = security_document(
                row, securities, "$SC1", "$ZQDM1");
            const auto event_type = text_value(row, "lx");
            const auto content = text_value(row, "nr");
            item = base_record("major-event", text_value(row, "date"),
                               event_type, resource, row);
            item["event_type"] = event_type;
            item["content"] = content;
            item["content_excerpt"] = excerpt(content);
            item["security"] = security;
            if (!security.is_null()) {
                item["related_securities"].push_back(security);
                item["title"] = security.at("name").as_string().empty()
                    ? event_type : security.at("name").as_string() + " · " + event_type;
                item["event_id"] = "major-event:" + text_value(row, "date") + ":" +
                    security.at("security_id").as_string() + ":" + text_value(row, "$ZQDM");
            }
        } else if (resource == kRecentIpos) {
            const auto security = security_document(row, securities);
            const auto title = security.is_null() ? text_value(row, "$ZQDM")
                : security.at("name").as_string();
            item = base_record("recent-ipo", text_value(row, "SSRQ"),
                               title, resource, row);
            item["event_type"] = kind_label("recent-ipo");
            item["security"] = security;
            if (!security.is_null()) {
                item["related_securities"].push_back(security);
                item["event_id"] = "recent-ipo:" + security.at("security_id").as_string();
            }
            item["snapshot_date"] = text_value(row, "DATE");
            item["issue_price"] = number(row, "FXJ");
            item["winning_lot_shares"] = number(row, "gps");
            item["raised_yuan"] = number(row, "mjzj");
            item["overfunding_yuan"] = number(row, "cmzj");
            item["listing_return_pct"] = number(row, "ZF1");
            item["change_5d_pct"] = number(row, "ZF2");
            item["change_10d_pct"] = number(row, "ZF3");
            item["industry"] = text_value(row, "hy");
            item["pe"] = number(row, "PE");
            item["industry_pe"] = number(row, "HPE");
            item["issue_pe"] = number(row, "FXPE");
            item["winning_rate_pct"] = number(row, "ZQL");
            item["sponsor"] = text_value(row, "bjjg");
            item["listing_limit_up_count"] = number(row, "lbs");
        } else if (resource == kIpoGuidance) {
            const auto company_name = text_value(row, "qymc");
            const auto progress = text_value(row, "fdjd");
            item = base_record("ipo-guidance", text_value(row, "rq"),
                               company_name.empty() ? progress
                                   : company_name + " · " + progress,
                               resource, row);
            item["event_type"] = progress.empty()
                ? kind_label("ipo-guidance") : progress;
            item["event_id"] = "ipo-guidance:" + text_value(row, "$ZQDM");
            item["company_name"] = company_name;
            item["guidance_progress"] = progress;
            item["board_category"] = text_value(row, "bklb");
            item["guidance_institution"] = text_value(row, "fdjg");
            item["region"] = text_value(row, "dq");
            const auto profile = text_value(row, "jj");
            item["content"] = profile;
            item["content_excerpt"] = excerpt(profile);
            item["source_record_id"] = text_value(row, "$ZQDM");
        } else if (resource == kIpoReviews) {
            const auto company_name = text_value(row, "qymc");
            const auto announcement_date = text_value(row, "slrq");
            const auto review_status = text_value(row, "slzt");
            item = base_record("ipo-review", announcement_date,
                               company_name.empty() ? review_status
                                   : company_name + " · " + review_status,
                               resource, row);
            item["event_type"] = review_status.empty()
                ? kind_label("ipo-review") : review_status;
            item["event_id"] = "ipo-review:" + text_value(row, "$ZQDM");
            item["company_name"] = company_name;
            item["acceptance_date"] = text_value(row, "slrq1");
            item["review_status"] = review_status;
            item["board_category"] = text_value(row, "bklb");
            item["sponsor"] = text_value(row, "bjr");
            item["prospectus_url"] = text_value(row, "zgsms");
            item["source_url"] = item.at("prospectus_url");
            item["industry"] = text_value(row, "hy");
            const auto profile = text_value(row, "jj");
            item["content"] = profile;
            item["content_excerpt"] = excerpt(profile);
            item["financial_report_date"] = text_value(row, "jzrq");
            item["pre_issue_total_shares"] = number(row, "zgb");
            item["eps"] = number(row, "mgsy");
            item["net_assets_per_share"] = number(row, "mgjzc");
            item["cash_flow_per_share"] = number(row, "mgxjl");
            item["roe_pct"] = number(row, "jzcsyl");
            item["planned_issue_shares"] = number(row, "nfxsl");
            item["post_issue_share_pct"] = number(row, "zzgb");
            item["planned_financing_yuan"] = number(row, "rzje");
            item["source_record_id"] = text_value(row, "$ZQDM");
        } else if (resource == kIpoSubscriptions) {
            const auto security = security_document(
                row, securities, "$SC1", "$ZQDM1", "qymc");
            const auto subscription_date = text_value(row, "sgrq");
            const auto source_name = text_value(row, "qymc");
            const auto display_name = !security.is_null() &&
                    !security.at("name").as_string().empty()
                ? security.at("name").as_string() : source_name;
            item = base_record("ipo-subscription", subscription_date,
                               display_name.empty() ? "新股申购"
                                                   : display_name + " · 新股申购",
                               resource, row);
            item["event_type"] = kind_label("ipo-subscription");
            item["company_name"] = source_name;
            item["security"] = security;
            if (!security.is_null()) {
                item["related_securities"].push_back(security);
                item["event_id"] = "ipo-subscription:" + subscription_date +
                    ":" + security.at("security_id").as_string();
            }
            const auto profile = text_value(row, "jj");
            item["content"] = profile;
            item["content_excerpt"] = excerpt(profile);
            item["board_category"] = text_value(row, "bklb");
            item["issue_price"] = number(row, "fxj");
            item["winning_rate_pct"] = number(row, "zql");
            item["effective_subscription_yuan"] = number(row, "sgje");
            item["issue_pe"] = number(row, "SYL");
            item["listing_return_pct"] = number(row, "sszf");
            item["financial_report_date"] = text_value(row, "jzrq");
            item["pre_issue_total_shares"] = number(row, "zgb");
            item["eps"] = number(row, "mgsy");
            item["net_assets_per_share"] = number(row, "mgjzc");
            item["cash_flow_per_share"] = number(row, "mgxjl");
            item["roe_pct"] = number(row, "jzcsyl");
            item["planned_issue_shares"] = number(row, "nfxsl");
            item["post_issue_share_pct"] = number(row, "zzgb");
            item["planned_financing_yuan"] = number(row, "rzje");
            item["source_record_id"] = text_value(row, "$ZQDM");
        } else if (resource == kIpoSubscriptionDetails) {
            const auto security = security_document(
                row, securities, "$SC", "$ZQDM", "zqjc");
            const auto subscription_date = text_value(row, "sgrq");
            const auto company_name = text_value(row, "zqjc");
            item = base_record("ipo-subscription-detail", subscription_date,
                               company_name.empty() ? "北证新股申购详情"
                                                    : company_name + " · 申购详情",
                               resource, row);
            item["event_type"] = kind_label("ipo-subscription-detail");
            item["company_name"] = company_name;
            item["security"] = security;
            if (!security.is_null()) {
                item["related_securities"].push_back(security);
                item["event_id"] = "ipo-subscription-detail:" +
                    subscription_date + ":" +
                    security.at("security_id").as_string();
            }
            item["pre_issue_price_yuan"] = number(row, "ZS");
            item["subscription_code"] = text_value(row, "sgdm");
            item["issue_price_yuan"] = number(row, "FXJ");
            item["subscription_date"] = subscription_date;
            item["payment_date"] = text_value(row, "jkr");
            item["refund_date"] = text_value(row, "tkr");
            item["subscription_min_shares"] = number(row, "sgsx");
            item["subscription_max_shares"] = number(row, "sgxx");
            item["pricing_method"] = text_value(row, "djfs");
            item["inquiry_start_date"] = text_value(row, "ksrq");
            item["inquiry_end_date"] = text_value(row, "jsrq");
            item["inquiry_price_lower_yuan"] = number(row, "xjxx");
            item["inquiry_price_upper_yuan"] = number(row, "xjsx");
            item["issue_pe"] = number(row, "fxsyl");
            item["issue_total_shares"] = number(row, "fxzl");
            item["online_issue_shares"] = number(row, "fxws");
            item["raised_yuan"] = number(row, "fxmz");
            item["winning_rate_pct"] = number(row, "zql");
            item["listing_date"] = text_value(row, "ssrq");
            item["sponsor"] = text_value(row, "bjjg");
            item["region"] = text_value(row, "zcdq");
            item["source_record_id"] = text_value(row, "$ZQDM");
        } else if (resource == kIpoCompanionNews) {
            const auto rich = parse_rich_text(text_value(row, "title"));
            if (rich.headline.empty() && rich.content.empty()) continue;
            item = base_record("ipo-companion-news", text_value(row, "date"),
                               rich.headline, resource, row);
            item["event_type"] = kind_label("ipo-companion-news");
            item["content"] = rich.content;
            item["content_excerpt"] = excerpt(rich.content);
            item["source_url"] = rich.source_url;
            item["event_id"] = "ipo-companion-news:" + text_value(row, "date") +
                ":" + excerpt(rich.headline, 80);
        } else if (resource == kUsIpoApplications || resource == kUsIpoCalendar ||
                   resource == kUsIpoListed || resource == kUsIpoPending) {
            const bool million_units = resource == kUsIpoApplications ||
                resource == kUsIpoCalendar;
            const std::string kind = resource == kUsIpoApplications ? "us-ipo-application" :
                resource == kUsIpoCalendar ? "us-ipo-calendar" :
                resource == kUsIpoListed ? "us-ipo-listed" : "us-ipo-pending";
            const auto security = us_security_document(row);
            const auto date = text_value(row, "date");
            const auto name = text_value(row, "ZQJC");
            item = base_record(kind, date, name, resource, row);
            item["event_type"] = kind_label(kind);
            item["security"] = security;
            if (!security.is_null()) item["related_securities"].push_back(security);
            item["industry"] = text_value(row, "HY");
            item["bookrunner"] = text_value(row, "cxbjr");
            item["exchange"] = text_value(row, "jys");
            item["price_text"] = text_value(row, "fw");
            item["issue_amount_source_value"] = number(row, "fxze");
            item["issue_amount_source_unit"] = million_units ? "million-usd" : "usd";
            item["issue_amount_usd"] = scaled(row, "fxze", million_units ? 1000000.0 : 1.0);
            item["issue_shares_source_value"] = number(row, "fxgfs");
            item["issue_shares_source_unit"] = million_units ? "million-shares" : "shares";
            item["issue_shares"] = scaled(row, "fxgfs", million_units ? 1000000.0 : 1.0);
            item["application_date"] = resource == kUsIpoApplications ? Json(date) : Json("");
            item["scheduled_listing_date"] =
                resource == kUsIpoCalendar || resource == kUsIpoPending
                    ? Json(date) : Json("");
            item["listing_date"] = resource == kUsIpoListed ? Json(date) : Json("");
            item["offer_price_usd"] = resource == kUsIpoListed
                ? number(row, "fw") : Json(nullptr);
            const auto code = text_value(row, "$ZQDM");
            item["event_id"] = kind + ":US:" + code + ":" + date + ":" +
                text_value(row, "fxze");
        } else if (resource == kFuturesCalendar) {
            const auto title = text_value(row, "yjs");
            const auto content = text_value(row, "content");
            item = base_record("futures-calendar", text_value(row, "date"),
                               title, resource, row);
            item["event_type"] = kind_label("futures-calendar");
            item["content"] = content;
            item["content_excerpt"] = excerpt(content);
            item["exchange_code"] = text_value(row, "$ZQDM");
            item["raw"] = row;
            item["event_id"] = "futures-calendar:" + text_value(row, "date") +
                ":" + text_value(row, "$ZQDM");
        } else {
            const auto kind = article_kind(resource);
            if (kind.empty()) throw Error("unknown calendar resource: " + resource);
            const auto rich = parse_rich_text(text_value(row, "title"));
            if (rich.headline.empty() && rich.content.empty()) continue;
            item = base_record(kind, text_value(row, "date"), rich.headline,
                               resource, row);
            item["event_type"] = kind_label(kind);
            item["content"] = rich.content;
            item["content_excerpt"] = excerpt(rich.content);
            item["source_url"] = rich.source_url;
            item["related_securities"] = related_securities(rich.headline, kind, securities);
            if (item.at("related_securities").size())
                item["security"] = item.at("related_securities").as_array().front();
            item["event_id"] = kind + ":" + text_value(row, "date") + ":" +
                std::to_string(result.size());
        }
        result.push_back(std::move(item));
    }
    return result;
}


}  // namespace tdx
