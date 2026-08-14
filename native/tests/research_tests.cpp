#include "tdx/research.hpp"

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        const auto& categories = tdx::research_categories();
        std::set<std::string> ids, resources;
        for (const auto& category : categories) {
            ids.insert(category.id);
            resources.insert(category.resource);
        }
        require(categories.size() == 9 && ids.size() == 9 && resources.size() == 9,
                "research category catalog must expose nine unique ids and resources");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920179"}] =
            tdx::Security{2, "BJ", "北京", "920179", "凯德石英"};
        auto master_rows = tdx::Json::array();
        master_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"920179\",\"$SC1\":\"44\",\"$ZQDM\":\"j920179\","
            "\"date\":\"20260803\",\"sydycs\":\"2\",\"cyjgsl\":\"8\","
            "\"syzaf\":\"12.5\",\"hy\":\"非金属材料\"}"));
        const auto master = tdx::normalize_research_master_rows(
            master_rows, "institution-research", securities);
        require(master.size() == 1 &&
                master.as_array()[0].at("entity").at("market_id").as_number() == 2 &&
                master.as_array()[0].at("entity").at("id").as_string() == "BJ920179" &&
                master.as_array()[0].at("detail_id").as_string() == "j920179" &&
                master.as_array()[0].at("data")
                    .at("latest_institution_count").as_string() == "8",
                "research master maps raw BJ market 44 and preserves its detail key");

        auto activity_rows = tdx::Json::array();
        activity_rows.push_back(tdx::Json::parse(
            "{\"date\":\"20260701\",\"title\":\"较早活动TXT:正文一\"}"));
        activity_rows.push_back(tdx::Json::parse(
            "{\"date\":\"20260801\",\"title\":\"最新活动TXT:正文二 http://example.com/a.pdf\"}"));
        const auto activities = tdx::normalize_research_activity_rows(activity_rows, false);
        require(activities.size() == 2 &&
                activities.as_array()[0].at("title").as_string() == "最新活动" &&
                activities.as_array()[0].at("text_length").as_number() > 0 &&
                activities.as_array()[0].at("source_urls").size() == 1 &&
                activities.as_array()[0].as_object().find("text") ==
                    activities.as_array()[0].as_object().end(),
                "research activities split, extract URLs, sort, and omit text");

        auto regulatory_rows = tdx::Json::array();
        regulatory_rows.push_back(tdx::Json::parse(
            "{\"sjrq\":\"20260802\",\"sjry\":\"上市公司\",\"sjlx\":\"监管函\","
            "\"sjjz\":\"已回复\",\"sjjs\":\"监管事项TXT:https://example.com/r.pdf\"}"));
        const auto regulatory = tdx::normalize_research_regulatory_rows(
            regulatory_rows, true);
        require(regulatory.size() == 1 &&
                regulatory.as_array()[0].at("summary").as_string() == "监管事项" &&
                regulatory.as_array()[0].at("source_urls").as_array()[0]
                    .as_string() == "https://example.com/r.pdf",
                "research regulatory events separate descriptions and attachments");

        std::cout << "research tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
