#include "tdx/intelligence.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        securities[{0, "000545"}] = tdx::Security{0, "SZ", "深圳", "000545", "金浦钛业"};
        securities[{1, "600584"}] = tdx::Security{1, "SH", "上海", "600584", "长电科技"};
        securities[{2, "920982"}] = tdx::Security{2, "BJ", "北京", "920982", "锦波生物"};

        const auto attention = tdx::normalize_attention_rows(tdx::Json::parse(
            "[{\"$ZQDM\":\"000001\",\"$SC\":\"0\",\"tdx7\":\"15\","
            "\"tdx8\":\"5\",\"tdx2\":\"15001863\",\"tdx3\":\"1862666\","
            "\"zytdx1\":\"18093\",\"zytdx2\":\"99328\","
            "\"tdx5\":\"100011402\",\"tdx6\":\"13658102\","
            "\"kds\":\"841\",\"kks\":\"397\",\"kdzb\":\"67.93\"}]"), securities);
        const auto& attention_row = attention.as_array()[0];
        require(attention_row.at("security").at("name").as_string() == "平安银行" &&
                    attention_row.at("attention_total").as_number() == 16864529.0 &&
                    attention_row.at("professional_attention_total").as_number() == 117421.0,
                "attention normalization failed");

        const auto risks = tdx::normalize_risk_rows(tdx::Json::parse(
            "[{\"$ZQDM\":\"000001\",\"$SC\":\"0\","
            "\"bllx\":\"测试风险\",\"blxq\":\"风险详情\",\"zxfs\":\"52\"}]"),
            "observation", securities);
        require(risks.as_array()[0].at("category_label").as_string() == "风险观察" &&
                    risks.as_array()[0].at("safety_score").as_number() == 52.0,
                "risk normalization failed");

        const auto discredited = tdx::normalize_risk_rows(tdx::Json::parse(
            "[{\"$ZQDM1\":\"000545\",\"$SC1\":\"0\","
            "\"date\":\"20260425\",\"sjry\":\"郭金东\","
            "\"rwlx\":\"实际控制人\",\"cs\":\"1\",\"$ZQDM\":\"9001\"}]"),
            "discredited", securities);
        const auto& discredited_row = discredited.as_array()[0];
        require(discredited_row.at("security").at("name").as_string() ==
                    "金浦钛业" &&
                discredited_row.at("category_label").as_string() ==
                    "失信被执行" &&
                discredited_row.at("announcement_date").as_string() ==
                    "20260425" &&
                discredited_row.at("involved_subject").as_string() ==
                    "郭金东" &&
                discredited_row.at("object_type").as_string() ==
                    "实际控制人" &&
                discredited_row.at("occurrences_past_year").as_number() == 1.0 &&
                discredited_row.at("safety_score").is_null() &&
                discredited_row.at("raw").is_object(),
                "discredited subject risk normalization failed");

        auto highlights = tdx::normalize_highlight_rows(tdx::Json::parse(
            "[{\"$ZQDM\":\"600584\",\"$SC\":\"1\",\"lds\":\"7\"," 
            "\"ldlx\":\"盈利质量高\",\"ldxq\":\"收现比大于1\"," 
            "\"zxfs\":\"92\",\"ldfz\":\"36\"},"
            "{\"$ZQDM\":\"920982\",\"$SC\":\"2\",\"lds\":\"9\"," 
            "\"ldlx\":\"高ROE\",\"ldxq\":\"平均ROE大于20%持续5年以上\"," 
            "\"zxfs\":\"94\",\"ldfz\":\"47\"}]") , securities);
        require(highlights.size() == 2 &&
                    highlights.as_array()[0].at("security").at("name").as_string() ==
                        "长电科技" &&
                    highlights.as_array()[0].at("highlight_count").as_number() == 7.0 &&
                    highlights.as_array()[0].at("primary_highlight_type").as_string() ==
                        "盈利质量高" &&
                    highlights.as_array()[0].at("highlight_detail").as_string() ==
                        "收现比大于1" &&
                    highlights.as_array()[0].at("raw").is_object(),
                "highlight identity, fields and raw evidence failed");
        tdx::sort_highlight_rows(highlights, "highlight-count", "desc");
        require(highlights.as_array()[0].at("security").at("code").as_string() ==
                    "920982",
                "highlight-count ordering failed");
        tdx::sort_highlight_rows(highlights, "safety-score", "asc");
        require(highlights.as_array()[0].at("security").at("code").as_string() ==
                    "600584",
                "safety-score ordering failed");
        bool highlight_sort_rejected = false;
        try { tdx::sort_highlight_rows(highlights, "unknown", "desc"); }
        catch (const std::exception&) { highlight_sort_rejected = true; }
        require(highlight_sort_rejected, "unknown highlight sort must be rejected");

        const auto events = tdx::normalize_intelligence_event_rows(tdx::Json::parse(
            "[{\"date\":\"20260731\",\"title\":\"HBM4 放量\","
            "\"$S_ZQDM\":\"1|600584,0|000001,0|000001\",\"sl\":\"2\","
            "\"type\":\"利好\",\"Contents\":\"产业催化\",\"$ZQDM\":\"20115\","
            "\"zycd\":\"3\"}]"), "events", securities);
        const auto& event = events.as_array()[0];
        require(event.at("event_id").as_string() == "events:20115" &&
                    event.at("member_count").as_number() == 2.0 &&
                    event.at("members").as_array()[0].at("name").as_string() == "长电科技" &&
                    event.at("raw").at("$ZQDM").as_string() == "20115",
                "event/member normalization failed");

        const auto graph = tdx::build_intelligence_graph(events, 10);
        require(graph.at("counts").at("events").as_number() == 1.0 &&
                    graph.at("counts").at("securities").as_number() == 2.0 &&
                    graph.at("edges").size() == 2,
                "event graph construction failed");
        const auto truncated = tdx::build_intelligence_graph(events, 1);
        require(truncated.at("truncated").as_bool() &&
                    truncated.at("counts").at("omitted_relationships").as_number() == 1.0,
                "event graph truncation failed");

        const auto topics = tdx::normalize_intelligence_topic_rows(tdx::Json::parse(
            "[{\"DATE\":\"20260101\",\"MC\":\"半年报专题\","
            "\"LX\":\"公司专题\",\"MS\":\"业绩集中披露\","
            "\"gxdate\":\"20260730\",\"$ZQDM\":\"1039\"}]") );
        require(topics.size() == 1 &&
                    topics.as_array()[0].at("topic_id").as_string() == "1039" &&
                    topics.as_array()[0].at("category").as_string() == "公司专题",
                "topic catalog normalization failed");

        const auto timeline = tdx::normalize_intelligence_timeline_rows(tdx::Json::parse(
            R"([{"date":"20260730","title":"券商业绩TXT:<p>净利润增长</p><a href='https://example.test/a'>原文</a>"}])"),
            "topic");
        const auto& timeline_row = timeline.as_array()[0];
        require(timeline_row.at("headline").as_string() == "券商业绩" &&
                    timeline_row.at("content").as_string().find("净利润增长") != std::string::npos &&
                    timeline_row.at("source_url").as_string() == "https://example.test/a",
                "topic rich-text normalization failed");

        const auto anomalies = tdx::normalize_market_anomaly_rows(tdx::Json::parse(
            "[{\"fsrq\":\"20260701\",\"sblx\":\"创天量\","
            "\"szzs1\":\"100\",\"szzs2\":\"110\",\"szzs9\":\"121\","
            "\"CJL3\":\"10\",\"CJL1\":\"20\",\"szzs6\":\"30\","
            "\"szzs12\":\"40\",\"ztjs\":\"9\",\"dtjs\":\"2\","
            "\"ydyy\":\"资金博弈\"}]") );
        const auto& anomaly = anomalies.as_array()[0];
        require(anomaly.at("same_day_change_pct").as_number() == 10.0 &&
                    anomaly.at("next_week_change_pct").as_number() == 10.0 &&
                    anomaly.at("market_volume").as_number() == 30.0 &&
                    anomaly.at("market_turnover_yuan").as_number() == 70.0,
                "market anomaly client-formula reproduction failed");

        const auto dynamic_members = tdx::normalize_intelligence_event_member_rows(
            tdx::Json::parse(
                "[{\"$ZQDM\":\"600584\",\"$SC\":\"1\"},"
                "{\"$ZQDM\":\"600584\",\"$SC\":\"1\"},"
                "{\"$ZQDM\":\"000001\",\"$SC\":\"0\"}]") , securities);
        require(dynamic_members.size() == 2 &&
                    dynamic_members.as_array()[0].at("name").as_string() == "长电科技",
                "dynamic event-member normalization failed");

        const auto value_categories = tdx::normalize_value_attention_categories(
            tdx::Json::parse(
                R"([{"$ZQDM":"3109","flname":"跌破首次公开发行价","scount":"2","$S_ZQDM":"0|000001,1|600584"}])"),
            securities);
        const auto& value_category = value_categories.as_array().front();
        require(value_category.at("category_id").as_string() == "3109" &&
                    value_category.at("category_name").as_string() ==
                        "跌破首次公开发行价" &&
                    value_category.at("reported_member_count").as_number() == 2.0 &&
                    value_category.at("inline_member_count").as_number() == 2.0 &&
                    value_category.at("members").as_array().front().at("name").as_string() ==
                        "平安银行",
                "value-attention master category normalization failed");

        const auto value_details = tdx::normalize_value_attention_detail_rows(
            tdx::Json::parse(
                R"([{"$ZQDM":"000001","$SC":"0","aqjg":"10.5","fqaqjg":"11.2","price1":"9.8"}])"),
            "3109", securities);
        const auto& value_detail = value_details.as_array().front();
        require(value_detail.at("category_id").as_string() == "3109" &&
                    value_detail.at("security").at("security_id").as_string() ==
                        "SZ000001" &&
                    std::abs(value_detail.at("anchor_price_yuan").as_number() - 10.5) <
                        1e-12 &&
                    value_detail.at("breach_depth_pct").is_null() &&
                    value_detail.at("source_resource").as_string() ==
                        "jzgz1/3109.jsn",
                "value-attention detail normalization failed");

        std::cout << "intelligence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "intelligence test failed: " << error.what() << '\n';
        return 1;
    }
}
