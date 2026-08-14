#include "tdx/roadshows.hpp"

#include "tdx/common.hpp"

#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json response(const tdx::Json& columns, const tdx::Json& content) {
    tdx::Json set = tdx::Json::object();
    set["ColName"] = columns;
    set["Content"] = content;
    tdx::Json sets = tdx::Json::array(); sets.push_back(std::move(set));
    tdx::Json result = tdx::Json::object();
    result["ErrorCode"] = 0;
    result["ResultSets"] = std::move(sets);
    return result;
}

tdx::Json master_response() {
    return response(
        tdx::Json::parse(R"(["stock_name","stock_code","stock_market","start_date","start_time","end_time","title","title_image","url","rec_id","roadshow_type","summary","stock_plates","liststatus"] )"),
        tdx::Json::parse(R"([
          ["平安银行","000001","0","20260323","15:00","17:00","平安银行年度业绩说明会",null,"https://example/1","1","业绩说明会",null,"012002","013001"],
          ["浦发银行","600000","1","20260401","10:00","11:00","浦发银行说明会",null,"https://example/2","2","业绩说明会","年度交流","012001","013001"],
          ["北交样本","830001","2","20250101","09:00","10:00","上市仪式",null,"https://example/3","3","上市仪式",null,"012046","013001"]
        ])"));
}

tdx::Json detail_response() {
    return response(
        tdx::Json::parse(R"(["title","roadshow_type","start_date","start_time","end_time","summary","url"] )"),
        tdx::Json::parse(R"([["平安银行年度业绩说明会","业绩说明会","20260323","15:00","17:00",null,"https://example/1"]])"));
}

}  // namespace

int main() {
    try {
        tdx::BlockData blocks;
        blocks.securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        int fetches = 0;
        std::string failure;
        tdx::RoadshowService service({}, blocks,
            [&](const std::string& key, int) {
                ++fetches;
                if (!failure.empty()) throw tdx::Error(failure);
                return key == "ly:1_zxly" ? master_response() : detail_response();
            });

        tdx::RoadshowQuery query;
        query.query = "银行";
        query.start_date = "2026-01-01";
        query.limit = 1;
        const auto market = service.query(query);
        require(market.at("mode").as_string() == "market", "roadshow market mode");
        require(market.at("counts").at("upstream_rows").as_number() == 3 &&
                    market.at("counts").at("matched").as_number() == 2 &&
                    market.at("counts").at("returned").as_number() == 1 &&
                    market.at("counts").at("has_more").as_bool(),
                "roadshow filter and pagination");
        require(market.at("records").as_array().front().at("security")
                    .at("security_id").as_string() == "SH600000",
                "roadshow descending date sort and identity");
        const auto cached = service.query(query);
        require(fetches == 1 && cached.at("cache").at("hit").as_bool(),
                "roadshow cache reuse");

        tdx::RoadshowQuery security;
        security.market = "sz"; security.code = "000001";
        const auto detail = service.query(security);
        require(fetches == 2 && detail.at("mode").as_string() == "security" &&
                    detail.at("counts").at("returned").as_number() == 1 &&
                    detail.at("security").at("name").as_string() == "平安银行" &&
                    detail.at("source").at("request_id").is_null(),
                "roadshow no-ReqId security detail route");

        failure = "TQLEX HTTP status 503"; security.refresh = true;
        const auto stale = service.query(security);
        require(stale.at("availability").as_string() == "stale-cache" &&
                    stale.at("cache").at("stale").as_bool() && fetches == 5 &&
                    stale.at("source").at("attempts").as_number() == 3,
                "roadshow stale fallback after bounded retry");

        failure = "TQLEX server returned ErrorCode 17: invalid key";
        bool rejected = false;
        try { (void)service.query(security); }
        catch (const tdx::Error&) { rejected = true; }
        require(rejected && fetches == 6,
                "roadshow non-transient errors must not use stale cache");
        std::cout << "roadshow tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
