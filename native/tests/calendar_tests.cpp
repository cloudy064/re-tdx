#include "tdx/calendar.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-8) {
    return std::abs(left - right) <= epsilon;
}

tdx::Json row(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json result = tdx::Json::object();
    for (const auto& [key, value] : fields) result[key] = value;
    return result;
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920925"}] = {2, "bj", "北京", "920925", "锦好医疗"};
        securities[{0, "301655"}] = {0, "sz", "深圳", "301655", "绿控传动"};
        securities[{0, "301707"}] = {0, "sz", "深圳", "301707", "展芯股份"};
        securities[{1, "688111"}] = {1, "sh", "上海", "688111", "科创样本"};
        securities[{0, "301001"}] = {0, "sz", "深圳", "301001", "创业样本"};
        securities[{0, "300176"}] = {0, "sz", "深圳", "300176", "派生科技"};

        tdx::Json major_rows = tdx::Json::array();
        major_rows.push_back(row({
            {"$ZQDM1", "920925"}, {"$SC1", "2"}, {"date", "20260807"},
            {"lx", "股东大会"}, {"nr", "1、第1次临时股东会"}, {"$ZQDM", "1"}}));
        const auto major = tdx::normalize_calendar_rows(
            "list/func_dsjtx101_1.jsn", major_rows, securities);
        require(major.size() == 1, "major-event row count");
        const auto& event = major.as_array().front();
        require(event.at("kind").as_string() == "major-event", "major-event kind");
        require(event.at("security").at("market").as_string() == "bj",
                "major-event uses $SC1/$ZQDM1");
        require(event.at("title").as_string() == "锦好医疗 · 股东大会",
                "major-event title combines resolved security and type");
        require(event.at("content_excerpt").as_string() == "1、第1次临时股东会",
                "major-event content retained");

        tdx::Json recent_rows = tdx::Json::array();
        recent_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "301707"}, {"SSRQ", "20260807"},
            {"FXJ", "23.45"}, {"gps", "500"}, {"mjzj", "964264000.0000"},
            {"cmzj", "-25456100.0000"}, {"ZF1", "114.72"}, {"ZF2", "-5.33"},
            {"ZF3", "7.25"}, {"hy", "元器件"}, {"HPE", "66.2129"},
            {"FXPE", "44.23"}, {"ZQL", "0.016"}, {"bjjg", "华泰联合证券"},
            {"lbs", "0"}, {"DATE", "20260806"}}));
        const auto recent = tdx::normalize_calendar_rows(
            "list/func_xgrl102_1.jsn", recent_rows, securities);
        const auto& ipo = recent.as_array().front();
        require(ipo.at("kind").as_string() == "recent-ipo", "recent IPO kind");
        require(ipo.at("raised_yuan").as_number() == 964264000.0,
                "recent IPO raised amount remains yuan");
        require(ipo.at("overfunding_yuan").as_number() == -25456100.0,
                "negative underfunding is retained");
        require(ipo.at("winning_lot_shares").as_number() == 500.0,
                "winning lot shares retained");
        require(ipo.at("listing_return_pct").as_number() == 114.72,
                "listing return retains percent-point semantics");

        tdx::Json guidance_rows = tdx::Json::array();
        guidance_rows.push_back(row({
            {"qymc", "埃夫科纳聚合物股份有限公司"},
            {"fdjd", "辅导备案"}, {"bklb", "北交所"},
            {"rq", "20260708"}, {"fdjg", "华福证券有限责任公司"},
            {"dq", "江苏"}, {"jj", "公司简介：专业添加剂企业"},
            {"$ZQDM", "2"}}));
        const auto guidance = tdx::normalize_calendar_rows(
            "list/func_zdgz_qzkcbd101_1.jsn", guidance_rows, securities);
        const auto& guidance_item = guidance.as_array().front();
        require(guidance_item.at("kind").as_string() == "ipo-guidance" &&
                    guidance_item.at("guidance_progress").as_string() == "辅导备案" &&
                    guidance_item.at("date").as_string() == "20260708",
                "IPO guidance date and progress retained");
        require(guidance_item.at("guidance_institution").as_string() ==
                    "华福证券有限责任公司" &&
                    guidance_item.at("region").as_string() == "江苏" &&
                    guidance_item.at("board_category").as_string() == "北交所",
                "IPO guidance institution, region and board retained");
        require(guidance_item.at("raw").is_object() &&
                    guidance_item.at("source_record_id").as_string() == "2",
                "IPO guidance raw evidence and source ID retained");

        tdx::Json review_rows = tdx::Json::array();
        review_rows.push_back(row({
            {"qymc", "湘滨电子"}, {"slrq", "20260721"},
            {"slrq1", "20260623"}, {"slzt", "提交注册"},
            {"bklb", "创业板"}, {"bjr", "中信证券股份有限公司"},
            {"zgsms", "https://example.test/prospectus.pdf"},
            {"jzrq", "20251231"}, {"zgb", "42333300.00"},
            {"mgsy", "0.88"}, {"mgjzc", "12.56"}, {"mgxjl", "0.58"},
            {"jzcsyl", "24.34"}, {"nfxsl", "14111100.00"},
            {"zzgb", "25.0000"}, {"rzje", "1051260000.0000"},
            {"jj", "公司简介：汽车电子控制器企业"},
            {"hy", "汽车制造业"}, {"$ZQDM", "2959920"}}));
        const auto reviews = tdx::normalize_calendar_rows(
            "list/func_zdgz_kcbsq101_1.jsn", review_rows, securities);
        const auto& review = reviews.as_array().front();
        require(review.at("kind").as_string() == "ipo-review" &&
                    review.at("review_status").as_string() == "提交注册" &&
                    review.at("acceptance_date").as_string() == "20260623",
                "IPO review dates and status retained");
        require(review.at("sponsor").as_string() == "中信证券股份有限公司" &&
                    review.at("industry").as_string() == "汽车制造业" &&
                    review.at("prospectus_url").as_string() ==
                        "https://example.test/prospectus.pdf",
                "IPO review sponsor, industry and prospectus retained");
        require(review.at("planned_financing_yuan").as_number() == 1051260000.0 &&
                    review.at("post_issue_share_pct").as_number() == 25.0 &&
                    review.at("raw").is_object(),
                "IPO review source units and raw evidence retained");

        tdx::Json subscription_rows = tdx::Json::array();
        subscription_rows.push_back(row({
            {"$SC1", "2"}, {"$ZQDM1", "920059"}, {"qymc", "双英股份"},
            {"sgrq", "20260810"}, {"fxj", "11.130"}, {"zql", "0.03982984"},
            {"sgje", "1406337420000.0000"}, {"SYL", "12.990"},
            {"sszf", "302.22"}, {"bklb", "北交所"}, {"jzrq", "20251231"},
            {"zgb", "114064700.00"}, {"mgsy", "1.14"}, {"mgjzc", "6.74"},
            {"mgxjl", "1.27"}, {"jzcsyl", "18.52"},
            {"nfxsl", "38021600.00"}, {"zzgb", "25.00"},
            {"rzje", "498360000.0000"}, {"jj", "公司简介：汽车零部件企业"},
            {"$ZQDM", "920059"}}));
        const auto subscriptions = tdx::normalize_calendar_rows(
            "list/func_zdgz_kcbsq103_1.jsn", subscription_rows, securities);
        const auto& subscription = subscriptions.as_array().front();
        require(subscription.at("kind").as_string() == "ipo-subscription" &&
                    subscription.at("security").at("market").as_string() == "bj" &&
                    subscription.at("security").at("name").as_string() == "双英股份",
                "IPO subscription identity and pre-listing source name");
        require(subscription.at("winning_rate_pct").as_number() == 0.03982984 &&
                    subscription.at("effective_subscription_yuan").as_number() ==
                        1406337420000.0 &&
                    subscription.at("planned_financing_yuan").as_number() ==
                        498360000.0,
                "IPO source percent and yuan units remain unchanged");
        require(subscription.at("planned_issue_shares").as_number() == 38021600.0 &&
                    subscription.at("post_issue_share_pct").as_number() == 25.0 &&
                    subscription.at("content_excerpt").as_string().find("汽车零部件") !=
                        std::string::npos,
                "IPO issuance structure and company profile retained");

        tdx::Json subscription_detail_rows = tdx::Json::array();
        subscription_detail_rows.push_back(row({
            {"$SC", "2"}, {"$ZQDM", "920161"}, {"zqjc", "龙辰科技"},
            {"ZS", "25.150"}, {"sgdm", "920161"}, {"FXJ", "9.210"},
            {"sgrq", "20260518"}, {"jkr", "20260518"}, {"tkr", "20260520"},
            {"sgsx", "100"}, {"sgxx", "1529900"}, {"djfs", "直接定价"},
            {"fxsyl", "14.98"}, {"fxzl", "33998500"}, {"fxws", "30598650"},
            {"fxmz", "271494700"}, {"zql", "0.02740647"},
            {"ssrq", "20260527"}}));
        const auto subscription_details = tdx::normalize_calendar_rows(
            "list/func_sbxg101_1.jsn", subscription_detail_rows, securities);
        const auto& subscription_detail = subscription_details.as_array().front();
        require(subscription_detail.at("kind").as_string() ==
                    "ipo-subscription-detail" &&
                    subscription_detail.at("security").at("market").as_string() == "bj" &&
                    subscription_detail.at("subscription_code").as_string() == "920161",
                "BSE IPO subscription detail identity");
        require(subscription_detail.at("issue_total_shares").as_number() == 33998500.0 &&
                    subscription_detail.at("raised_yuan").as_number() == 271494700.0 &&
                    subscription_detail.at("winning_rate_pct").as_number() == 0.02740647,
                "BSE IPO detail source units remain base shares/yuan/percentage points");

        tdx::Json us_calendar_rows = tdx::Json::array();
        us_calendar_rows.push_back(row({
            {"$SC", "74"}, {"$ZQDM", "TCGX"}, {"ZQJC", "TCGX Acquisition"},
            {"date", "20260805"}, {"cxbjr", "Jefferies"}, {"jys", "NASDAQ"},
            {"fxze", "75.00"}, {"fxgfs", "7.50"}, {"fw", "10.00"}}));
        const auto us_calendar = tdx::normalize_calendar_rows(
            "list/func_mgrl102_1.jsn", us_calendar_rows, securities);
        const auto& scheduled = us_calendar.as_array().front();
        require(scheduled.at("kind").as_string() == "us-ipo-calendar" &&
                    scheduled.at("security").at("market").as_string() == "us" &&
                    scheduled.at("security").at("security_id").as_string() == "US:TCGX",
                "US IPO calendar identity");
        require(scheduled.at("issue_amount_usd").as_number() == 75000000.0 &&
                    scheduled.at("issue_shares").as_number() == 7500000.0 &&
                    scheduled.at("issue_amount_source_unit").as_string() == "million-usd",
                "MGRL million-dollar and million-share units");

        tdx::Json us_listed_rows = tdx::Json::array();
        us_listed_rows.push_back(row({
            {"$SC", "74"}, {"$ZQDM", "BRVE"}, {"ZQJC", "Braveheart Bio"},
            {"date", "20260806"}, {"HY", "医疗保健"},
            {"jys", "纳斯达克交易所"}, {"fxze", "382500000"},
            {"fxgfs", "21250000"}, {"fw", "18.000"}}));
        const auto us_listed = tdx::normalize_calendar_rows(
            "list/func_mgxg101_1.jsn", us_listed_rows, securities);
        const auto& listed = us_listed.as_array().front();
        require(listed.at("kind").as_string() == "us-ipo-listed" &&
                    listed.at("issue_amount_usd").as_number() == 382500000.0 &&
                    listed.at("issue_shares").as_number() == 21250000.0 &&
                    listed.at("offer_price_usd").as_number() == 18.0,
                "MGXG base-dollar, base-share and offer-price units");

        tdx::Json announcement_rows = tdx::Json::array();
        announcement_rows.push_back(row({
            {"date", "20260806"},
            {"title", "绿控传动（301655）：发行公告TXT:<p>发行正文</p><p><a href='https://example.test/ipo.pdf'>附件</a></p>"}}));
        const auto announcements = tdx::normalize_calendar_rows(
            "list/func_xgrl101_1.jsn", announcement_rows, securities);
        const auto& announcement = announcements.as_array().front();
        require(announcement.at("title").as_string() == "绿控传动（301655）：发行公告",
                "embedded TXT body split from headline");
        require(announcement.at("content").as_string().find('<') == std::string::npos,
                "article HTML is not exposed as display text");
        require(announcement.at("source_url").as_string() == "https://example.test/ipo.pdf",
                "article source URL extracted");
        require(announcement.at("security").at("code").as_string() == "301655",
                "headline code associates IPO announcement");

        tdx::Json star_rows = tdx::Json::array();
        star_rows.push_back(row({
            {"date", "20260807"}, {"title", "科创样本（688111）盘中走强TXT:<p>正文</p>"}}));
        const auto star = tdx::normalize_calendar_rows(
            "list/func_xgrl103_1.jsn", star_rows, securities);
        require(star.as_array().front().at("kind").as_string() == "star-news",
                "xgrl103 classified as STAR Market news");
        require(star.as_array().front().at("related_securities").size() == 1,
                "STAR headline security associated");

        tdx::Json chinext_rows = tdx::Json::array();
        chinext_rows.push_back(row({
            {"date", "20260807"}, {"title", "创业样本发布新产品TXT:<p>正文</p>"}}));
        const auto chinext = tdx::normalize_calendar_rows(
            "list/func_xgrl104_1.jsn", chinext_rows, securities);
        require(chinext.as_array().front().at("security").at("code").as_string() ==
                    "301001",
                "board-constrained name association works without code");

        tdx::Json neeq_rows = tdx::Json::array();
        neeq_rows.push_back(row({
            {"date", "20260807"}, {"title", "挂牌企业（874983）获同意函TXT:<p>正文</p>"}}));
        const auto neeq = tdx::normalize_calendar_rows(
            "list/func_xgrl105_1.jsn", neeq_rows, securities);
        const auto& neeq_security = neeq.as_array().front().at("security");
        require(neeq_security.at("market").as_string() == "bj" &&
                neeq_security.at("code").as_string() == "874983",
                "NEEQ headline retains inferred market even before directory listing");
        require(!neeq_security.at("name_resolved").as_bool(),
                "unlisted NEEQ name is explicitly unresolved");

        tdx::Json futures_rows = tdx::Json::array();
        futures_rows.push_back(row({
            {"$ZQDM", "SHFE"}, {"date", "20260810"},
            {"yjs", "上海期货交易所"}, {"content", "铜期货交割提示"}}));
        const auto futures = tdx::normalize_calendar_rows(
            "list/func_qhrl400_1.jsn", futures_rows, securities);
        const auto& futures_event = futures.as_array().front();
        require(futures_event.at("kind").as_string() == "futures-calendar",
                "futures calendar kind");
        require(futures_event.at("exchange_code").as_string() == "SHFE" &&
                futures_event.at("content").as_string() == "铜期货交割提示",
                "futures exchange and content retained");

        tdx::Json suspension_rows = tdx::Json::array();
        suspension_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "301655"},
            {"tpdate", "20260803"}, {"yjfpsj", "20260810"},
            {"fpdate", "20260811"}, {"tpts", "6"},
            {"Price3", "20"}, {"Price4", "22"},
            {"Price5", "18"}, {"Price6", "19.8"},
            {"tpyy", "重大事项"}}));
        const auto suspension = tdx::normalize_calendar_rows(
            "list/func_gsrl203_1.jsn", suspension_rows, securities);
        const auto& suspension_event = suspension.as_array().front();
        require(suspension_event.at("kind").as_string() ==
                    "suspension-resumption", "GSRL203 kind");
        require(close(suspension_event.at("resumption_day_change_pct").as_number(),
                      10.0), "GSRL203 client calculation");
        require(suspension_event.at("security").at("name").as_string() ==
                    "绿控传动", "GSRL security resolution");

        tdx::Json meeting_rows = tdx::Json::array();
        meeting_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "301655"}, {"date", "20260820"},
            {"sj", "临时股东大会"}, {"sjcontent", "审议重大资产购买议案"}}));
        const auto shareholder_meeting = tdx::normalize_calendar_rows(
            "list/func_gsrl209_1.jsn", meeting_rows, securities);
        require(shareholder_meeting.as_array().front().at("kind").as_string() ==
                    "shareholder-meeting", "GSRL209 kind");
        require(shareholder_meeting.as_array().front().at("content").as_string() ==
                    "审议重大资产购买议案", "GSRL event content");

        tdx::Json rights_issue_rows = tdx::Json::array();
        rights_issue_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "300176"}, {"date", "20260813"},
            {"sj", "配股缴款开始日"},
            {"sjcontent", "配股价格3.36元/股，10配4股，配股代码380176"}}));
        const auto rights_issue = tdx::normalize_calendar_rows(
            "list/func_gsrl207_1.jsn", rights_issue_rows, securities);
        const auto& rights_event = rights_issue.as_array().front();
        require(rights_event.at("kind").as_string() == "rights-issue" &&
                    rights_event.at("rights_issue_stage").as_string() ==
                        "payment-start",
                "GSRL207 rights-issue stage");
        require(rights_event.at("security").at("name").as_string() ==
                    "派生科技" &&
                    rights_event.at("event_type").as_string() ==
                        "配股缴款开始日" &&
                    rights_event.at("content").as_string().find("380176") !=
                        std::string::npos,
                "GSRL207 security and source content");

        std::cout << "calendar tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
