#include "tdx/tqlex.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

tdx::Bytes text_bytes(const std::string& value) {
    return tdx::Bytes(value.begin(), value.end());
}

void test_config_body() {
    const auto request = tdx::parse_tqlex_config_body(
        "[{'ReqId':'500050','Type':'$$Type$$',"
        "'Page':'$$$STARTPOS$ $$','PageSize':'$$$PAGEROWS$ $$'}]",
        {{"Type", "1"}}, 2, 50);
    const auto& mapping = request.as_array()[0];
    require(mapping.at("ReqId").as_string() == "500050" &&
            mapping.at("Type").as_string() == "1" &&
            mapping.at("Page").as_string() == "2" &&
            mapping.at("PageSize").as_string() == "50",
            "TQLEX config parsing and paging macros");
}

void test_query_transport() {
    bool request_seen = false;
    const auto response = tdx::query_tqlex(
        "HQServ.test", tdx::Json::parse("[{\"ReqId\":\"1\"}]"),
        "http://example.invalid/TQLEX", 1000,
        [&](const std::string& url, const tdx::Bytes& payload, int timeout) {
            require(url == "http://example.invalid/TQLEX?Entry=HQServ.test",
                    "TQLEX encoded Entry URL");
            require(timeout == 1000, "TQLEX transport timeout");
            require(tdx::Json::parse(std::string(payload.begin(), payload.end()))
                        .as_array()[0].at("ReqId").as_string() == "1",
                    "TQLEX JSON request payload");
            request_seen = true;
            return text_bytes(
                "{\"ErrorCode\":0,\"ResultSets\":[{\"ColDes\":[{\"Name\":\"code\"}],"
                "\"Content\":[[\"000001\"]],\"RowNum\":1}]}");
        });
    require(request_seen &&
            response.at("ResultSets").as_array()[0].at("RowNum").as_number() == 1,
            "TQLEX JSON query response");
}

void test_pagination() {
    int page_call = 0;
    const auto response = tdx::query_tqlex_all_pages(
        "HQServ.test",
        tdx::Json::parse("[{\"ReqId\":\"1\",\"page\":\"0\",\"pageSize\":\"2\"}]"),
        0, 2, 10, "http://example.invalid/TQLEX", 1000,
        [&](const std::string&, const tdx::Bytes& payload, int) {
            const auto item =
                tdx::Json::parse(std::string(payload.begin(), payload.end()))
                    .as_array()[0];
            require(item.as_object().find("Page") == item.as_object().end(),
                    "TQLEX preserves lowercase paging key");
            require(item.at("pageSize").as_string() == "2",
                    "TQLEX lowercase page size override");
            const auto page = item.at("page").as_string();
            require(page == std::to_string(page_call),
                    "TQLEX page number sequence");
            ++page_call;
            if (page == "0") return text_bytes(
                "{\"ErrorCode\":0,\"ResultSets\":["
                "{\"ColDes\":[{\"Name\":\"code\"}],"
                "\"Content\":[[\"000001\"],[\"000002\"]],\"RowNum\":2},"
                "{\"ColDes\":[{\"Name\":\"total\"}],"
                "\"Content\":[[\"3\"]],\"RowNum\":1}]}");
            return text_bytes(
                "{\"ErrorCode\":0,\"ResultSets\":["
                "{\"ColDes\":[{\"Name\":\"code\"}],"
                "\"Content\":[[\"000003\"]],\"RowNum\":1},"
                "{\"ColDes\":[{\"Name\":\"total\"}],"
                "\"Content\":[[\"3\"]],\"RowNum\":1}]}");
        });
    require(page_call == 2 &&
            response.at("ResultSets").as_array()[0].at("Content").size() == 3 &&
            response.at("ResultSets").as_array()[1].at("Content").size() == 1,
            "TQLEX multi-result-set page merge");
}

void test_business_error() {
    bool rejected = false;
    try {
        (void)tdx::query_tqlex(
            "HQServ.test", tdx::Json::parse("[{\"ReqId\":\"1\"}]"),
            "http://example.invalid/TQLEX", 1000,
            [](const std::string&, const tdx::Bytes&, int) {
                return text_bytes("{\"ErrorCode\":5,\"ErrorInfo\":\"bad\"}");
            });
    } catch (const tdx::Error&) {
        rejected = true;
    }
    require(rejected, "TQLEX server ErrorCode rejection");
}

}  // namespace

int main() {
    try {
        test_config_body();
        test_query_transport();
        test_pagination();
        test_business_error();
        std::cout << "tqlex tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "tqlex tests failed: " << error.what() << '\n';
        return 1;
    }
}
