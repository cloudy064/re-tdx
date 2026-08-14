#include "tdx/panorama.hpp"

#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json source(std::string resource, tdx::Json rows) {
    tdx::Json result = tdx::Json::object();
    result["resource"] = std::move(resource);
    result["size"] = 100;
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "fixture:7709";
    result["server"] = "fixture";
    result["rows"] = std::move(rows);
    tdx::Json transport = tdx::Json::object();
    transport["attempt_count"] = 1;
    transport["cache_status"] = "fresh";
    result["transport"] = std::move(transport);
    return result;
}

tdx::Json row(std::initializer_list<std::pair<std::string, tdx::Json>> fields) {
    tdx::Json result = tdx::Json::object();
    for (const auto& [name, value] : fields) result[name] = value;
    return result;
}

}  // namespace

int main() {
    try {
        tdx::PanoramaQuery catalog;
        const auto catalog_document = tdx::compose_panorama_document(
            catalog, tdx::Json::array());
        require(catalog_document.at("view_count").as_number() == 10,
                "panorama catalog has ten typed views");

        tdx::Json flow_rows = tdx::Json::array();
        flow_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                 {"date", "20260806"}, {"rzljlr5", 123.5}}));
        flow_rows.push_back(row({{"$SC", "1"}, {"$ZQDM", "600000"},
                                 {"date", "20260806"}, {"rzljlr5", -5.0}}));
        tdx::Json documents = tdx::Json::array();
        documents.push_back(source("list/func_gx_zjlx101_1.jsn", std::move(flow_rows)));
        tdx::PanoramaQuery flow;
        flow.view = "capital-flow";
        flow.market = "sz";
        flow.code = "000001";
        flow.limit = 20;
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        tdx::Security security;
        security.market_id = 0;
        security.market = "SZ";
        security.market_name = "深市";
        security.code = "000001";
        security.name = "平安银行";
        securities[{0, "000001"}] = security;
        const auto result = tdx::compose_panorama_document(flow, documents, securities);
        require(result.at("schema").as_string() == "tdx-market-panorama-native-v1" &&
                result.at("counts").at("matched").as_number() == 1,
                "panorama security filtering");
        const auto& record = result.at("sections").as_array().front()
            .at("records").as_array().front();
        require(record.at("name").as_string() == "平安银行" &&
                record.at("data").at("main_net_inflow_5d").as_number() == 123.5 &&
                record.at("raw").at("rzljlr5").as_number() == 123.5,
                "panorama typed projection and raw evidence");

        bool rejected = false;
        try {
            tdx::PanoramaQuery invalid;
            invalid.view = "security";
            (void)tdx::compose_panorama_document(invalid, tdx::Json::array());
        } catch (const tdx::Error&) { rejected = true; }
        require(rejected, "security view requires market and code");
        std::cout << "panorama tests passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
