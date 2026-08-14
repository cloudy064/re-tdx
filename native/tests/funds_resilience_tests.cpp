#include "tdx/common.hpp"
#include "tdx/funds.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::BlockData sample_blocks() {
    tdx::BlockData blocks;
    blocks.securities[{0, "000001"}] = {0, "SZ", "深圳", "000001", "平安银行"};

    tdx::Block industry;
    industry.block_id = "research-industry:881385";
    industry.family = "research-industry";
    industry.block_code = "881385";
    industry.name = "银行";
    industry.level = 1;
    blocks.blocks.push_back(industry);

    tdx::BlockMember member;
    member.block_id = industry.block_id;
    member.family = industry.family;
    member.market_id = 0;
    member.market = "SZ";
    member.code = "000001";
    member.security_id = "SZ000001";
    member.security_name = "平安银行";
    blocks.members.push_back(member);
    return blocks;
}

tdx::Json upstream_document(const std::string& market, const std::string& code) {
    auto document = tdx::Json::object();
    auto response = tdx::Json::object();
    response["ResultSets"] = tdx::Json::array();
    auto result_set = tdx::Json::object();
    result_set["ColDes"] = tdx::Json::array();
    for (const auto* name : {"market", "code", "xj", "zdf", "zd", "q5rjl",
                             "jlr_1", "cje_1", "zlzb_1"}) {
        auto column = tdx::Json::object();
        column["Name"] = name;
        result_set["ColDes"].push_back(std::move(column));
    }
    result_set["Content"] = tdx::Json::array();
    auto row = tdx::Json::array();
    for (const auto& value : {market, code, std::string("10.00"), std::string("1.00"),
                              std::string("0.10"), std::string("100"),
                              std::string("1200"), std::string("5000"),
                              std::string("24")})
        row.push_back(value);
    result_set["Content"].push_back(std::move(row));
    response["ResultSets"].push_back(std::move(result_set));
    document["response"] = std::move(response);
    return document;
}

}  // namespace

int main() {
    try {
        const auto blocks = sample_blocks();
        bool fail = false;
        int calls = 0;
        tdx::IntradayFundsService service({}, blocks,
            [&](const std::string& request_id, const std::string& market,
                const std::string& code, int timeout_ms) {
                ++calls;
                require(timeout_ms == 100, "fetch timeout was not forwarded");
                if (fail)
                    throw tdx::Error("PBRPC server rejected request with RpcID -1");
                if (request_id == "200340") return upstream_document("1", "881385");
                require(request_id == "200341" && market == "1" && code == "881385",
                        "detail request identity was not forwarded");
                return upstream_document("0", "000001");
            });

        tdx::IntradayFundsQuery query;
        query.market = "sz";
        query.code = "000001";
        query.timeout_ms = 100;
        auto warm = service.query(query);
        require(calls == 2 && warm.at("availability").as_string() == "live" &&
                    warm.at("found").as_bool(),
                "initial live request did not populate both caches");

        fail = true;
        query.refresh = true;
        const auto stale = service.query(query);
        require(calls == 8, "transient failures must be retried three times per request");
        require(stale.at("availability").as_string() == "stale-cache" &&
                    stale.at("found").as_bool() &&
                    stale.at("cache").at("hit").as_bool() &&
                    stale.at("cache").at("stale").as_bool() &&
                    stale.at("cache").at("master_stale").as_bool() &&
                    stale.at("cache").at("detail_stale").as_bool(),
                "cached data was not served after transient upstream exhaustion");
        require(stale.at("cache").at("master_upstream_error").as_string().find("RpcID -1") !=
                    std::string::npos &&
                    stale.at("cache").at("detail_upstream_error").as_string().find("RpcID -1") !=
                    std::string::npos,
                "stale response did not preserve the upstream reason");

        int cold_calls = 0;
        tdx::IntradayFundsService cold({}, blocks,
            [&](const std::string&, const std::string&, const std::string&, int) -> tdx::Json {
                ++cold_calls;
                throw tdx::Error("PBRPC server rejected request with RpcID -1");
            });
        bool cold_failed = false;
        try {
            (void)cold.query(query);
        } catch (const tdx::Error& error) {
            cold_failed = std::string(error.what()).find("RpcID -1") != std::string::npos;
        }
        require(cold_failed && cold_calls == 3,
                "cold-cache transient failure must retry and remain an upstream error");

        std::cout << "Intraday funds resilience tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
