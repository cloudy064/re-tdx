#include "tdx/blocks.hpp"
#include "tdx/blocks_quote.hpp"
#include "tdx/json.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void write_text(const fs::path& path, std::string_view text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw std::runtime_error("cannot write test fixture");
}

void write_bytes(const fs::path& path, const tdx::Bytes& data) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    if (!output) throw std::runtime_error("cannot write binary test fixture");
}

struct TemporaryRoot {
    fs::path path;

    TemporaryRoot() {
        const auto suffix = std::chrono::steady_clock::now()
                                .time_since_epoch().count();
        path = fs::temp_directory_path() /
               ("tdx-blocks-tests-" + std::to_string(suffix));
        fs::create_directories(path);
    }

    ~TemporaryRoot() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

void verify_tnf_parser() {
    tdx::Bytes data(50 + 360, 0);
    const std::string code = "000001";
    const std::string name = "PINGAN";
    std::copy(code.begin(), code.end(), data.begin() + 50);
    std::copy(name.begin(), name.end(), data.begin() + 50 + 31);
    const float trade_unit = 100.0F;
    data[50 + 76] = 3;
    std::memcpy(data.data() + 50 + 78, &trade_unit, sizeof(trade_unit));
    data[50 + 282] = 12;

    const auto securities = tdx::parse_tnf(data, 0);
    require(securities.size() == 1 &&
                securities[0].security_id() == "SZ000001" &&
                securities[0].name == "PINGAN" &&
                securities[0].trade_unit == 100.0 &&
                securities[0].tdx_category == 12 &&
                securities[0].price_precision &&
                *securities[0].price_precision == 3,
            "TNF parser must preserve identity and 360-byte metadata");

    tdx::Bytes legacy(50 + 314, 0);
    std::copy(code.begin(), code.end(), legacy.begin() + 50);
    std::copy(name.begin(), name.end(), legacy.begin() + 50 + 23);
    legacy[50 + 76] = 4;
    const auto legacy_securities = tdx::parse_tnf(legacy, 0);
    require(legacy_securities.size() == 1 &&
                !legacy_securities[0].price_precision,
            "legacy 314-byte TNF records must not guess an unverified decimal offset");
}

void verify_security_metadata_enrichment() {
    tdx::SecurityCatalog catalog;
    tdx::Security security;
    security.market_id = 0;
    security.market = "SZ";
    security.code = "000001";
    security.name = "LOCAL";
    security.price_precision = 3;
    catalog[{0, security.code}] = security;

    auto document = tdx::Json::object();
    document["name"] = "CALLER";
    require(tdx::enrich_security_metadata(
                catalog, document, "SZ", "000001") &&
                document.at("name").as_string() == "CALLER" &&
                document.at("price_precision").as_number() == 3.0 &&
                document.at("price_precision_source").as_string() ==
                    "local-tnf-security-master",
            "an existing name must not prevent TNF precision enrichment");

    auto explicit_values = tdx::Json::object();
    explicit_values["name"] = "CALLER";
    explicit_values["price_precision"] = 4;
    explicit_values["price_precision_source"] = "caller";
    explicit_values["min_tick"] = 0.2;
    require(!tdx::enrich_security_metadata(
                catalog, explicit_values, "0", "000001") &&
                explicit_values.at("name").as_string() == "CALLER" &&
                explicit_values.at("price_precision").as_number() == 4.0 &&
                explicit_values.at("price_precision_source").as_string() ==
                    "caller" &&
                explicit_values.at("min_tick").as_number() == 0.2,
            "caller precision and minimum tick must have absolute priority");

    auto missing = tdx::Json::object();
    require(!tdx::enrich_security_metadata(
                catalog, missing, "sz", "999999") &&
                missing.as_object().find("price_precision") ==
                    missing.as_object().end(),
            "pure catalog enrichment must not invent missing security metadata");
}

tdx::SecurityCatalog verify_industry_parsers() {
    auto blocks = tdx::parse_industry_catalog(
        "银行|880001|2|unused|0|T01\n"
        "股份行|880002|2|unused|1|T0101\n"
        "银行研究|880003|12|unused|1|X01\n");
    const auto assignments = tdx::parse_industry_assignments(
        "0|000001|T0101|unused|unused|X01\n");
    tdx::SecurityCatalog securities;
    securities[{0, "000001"}] =
        tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
    const auto members = tdx::build_industry_members(
        blocks, assignments, securities);

    require(blocks.size() == 3 && members.size() == 3 &&
                blocks[1].parent_block_id == "industry:T01" &&
                blocks[0].member_count == 1 &&
                blocks[1].member_count == 1 &&
                blocks[2].member_count == 1,
            "industry hierarchy and ancestor/direct membership must be preserved");
    return securities;
}

void verify_infoharbor_parser(const tdx::SecurityCatalog& securities) {
    const auto [blocks, members] = tdx::parse_infoharbor(
        "#GN_测试概念,1,880900,20260801,20260811,x,x\n"
        "0#000001\n",
        securities);
    require(blocks.size() == 1 && members.size() == 1 &&
                blocks[0].block_id == "concept:880900" &&
                blocks[0].member_count == 1 &&
                members[0].security_name == "平安银行" &&
                members[0].name_resolved,
            "infoharbor parser must preserve family and resolve security names");
}

void verify_main_business_parser() {
    const auto catalog = tdx::parse_main_business_catalog(
        "0|000001|零售金融|92|7|\n"
        "0|000001|后出现记录|1|1|\n",
        "fixture/specgpext.txt");
    require(catalog.records.size() == 1 &&
                tdx::main_business_for(catalog, 0, "000001") == "零售金融" &&
                tdx::safety_score_for(catalog, 0, "000001") == 92.0 &&
                tdx::shine_score_for(catalog, 0, "000001") == 7.0,
            "MAINBUSINESS parser must preserve first-record and score semantics");
}

void verify_local_block_files() {
    TemporaryRoot fixture;
    write_text(fixture.path / "T0002" / "user.ini",
               "[Other]\nUseTdxL3Hy=1\n");
    write_text(fixture.path / "T0002" / "blocknew" / "zxg.blk",
               "0000001\n");

    require(tdx::tdx_user_industry_mode(fixture.path) == 1,
            "user industry mode must read the Other section");
    const auto custom = tdx::custom_block_membership_for(
        fixture.path, 0, "000001");
    const auto counts = tdx::custom_block_member_counts(fixture.path);
    const auto members = tdx::load_custom_block_members(fixture.path, "zxg");
    require(custom.text == "自选股 " && custom.count == 1 &&
                counts.entries.size() == 2 && counts.entries[0].count == 1 &&
                members.size() == 1 && members[0].market_id == 0 &&
                members[0].code == "000001",
            "custom block directory, count and members must agree");

    tdx::Bytes directory(320, 0);
    const std::string key = "combo1";
    const std::string name = "Combo";
    std::copy(key.begin(), key.end(), directory.begin() + 4);
    std::copy(name.begin(), name.end(), directory.begin() + 11);
    write_bytes(fixture.path / "T0002" / "lc" / "lcidx.lii", directory);

    tdx::Bytes combination_member(16, 0);
    std::copy_n("000001", 6, combination_member.begin() + 2);
    write_bytes(fixture.path / "T0002" / "lc" / "combo1.cis",
                combination_member);
    const auto combination = tdx::combination_block_membership_for(
        fixture.path, 0, "000001");
    require(combination.text == "Combo " && combination.count == 1 &&
                combination.directory_count == 1 &&
                combination.readable_member_file_count == 1,
            "combination block directory and membership must remain linked");
}

void verify_block_quote_resolution() {
    tdx::BlockData data;
    auto add = [&](std::string id, std::string family, std::string code,
                   std::string name) {
        tdx::Block block;
        block.block_id = std::move(id);
        block.family = std::move(family);
        block.block_code = std::move(code);
        block.name = std::move(name);
        data.blocks.push_back(std::move(block));
    };
    add("industry:T1001", "industry", "880471", "银行");
    add("research-industry:X50", "research-industry", "881385", "银行");
    add("concept:880900", "concept", "880900", "半导体");
    add("style:local-only", "style", "LOCAL", "本地风格");

    const auto industry = tdx::resolve_block_quote_target(
        data, "industry:T1001");
    require(industry.market_id == 1 && industry.market == "sh" &&
                industry.code == "880471" &&
                industry.block.name == "银行",
            "block id must resolve to the Shanghai public index quote");
    const auto research = tdx::resolve_block_quote_target(data, "881385");
    require(research.code == "881385" &&
                research.block.family == "research-industry",
            "881xxx research-industry quote codes must remain supported");
    const auto concept = tdx::resolve_block_quote_target(data, "半导体");
    require(concept.code == "880900",
            "a unique exact block name must resolve without its id");

    bool ambiguous_rejected = false;
    try {
        (void)tdx::resolve_block_quote_target(data, "银行");
    } catch (const tdx::Error& error) {
        const std::string message = error.what();
        ambiguous_rejected =
            message.find("industry:T1001") != std::string::npos &&
            message.find("research-industry:X50") != std::string::npos;
    }
    require(ambiguous_rejected,
            "an ambiguous block name must list explicit block ids");

    bool partial_rejected = false;
    try {
        (void)tdx::resolve_block_quote_target(data, "半导");
    } catch (const tdx::Error&) { partial_rejected = true; }
    require(partial_rejected,
            "K-line block resolution must not guess from partial names");

    bool local_only_rejected = false;
    try {
        (void)tdx::resolve_block_quote_target(data, "style:local-only");
    } catch (const tdx::Error&) { local_only_rejected = true; }
    require(local_only_rejected,
            "a block without a public quote code must be rejected");
}

}  // namespace

int main() {
    try {
        verify_tnf_parser();
        verify_security_metadata_enrichment();
        const auto securities = verify_industry_parsers();
        verify_infoharbor_parser(securities);
        verify_main_business_parser();
        verify_local_block_files();
        verify_block_quote_resolution();
        std::cout << "blocks tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "blocks tests failed: " << error.what() << '\n';
        return 1;
    }
}
