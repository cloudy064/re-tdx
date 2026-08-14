#include "tdx/bond_reference.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

namespace fs = std::filesystem;

// A synthetic AA+ resource in the on-disk JSN shape: an array of groups, each
// holding a colheader and its data rows.  Kept ASCII so it round-trips through
// the loader's GBK decode unchanged, and small enough that every query phase has
// a hand-checkable expected answer.
//
// code  mkt name      rate  yrs  credit maturity  rate-type bond-type
// 100001 sh AlphaBond 3.10  5.0  AA+    20300101  Fixed     Corporate
// 100002 sh BetaBond  1.85  2.0  AA+    20280101  Fixed     Corporate
// 100003 sz GammaBond 4.20  9.0  AA     20350101  Floating  Enterprise
// 100004 sz DeltaBond 3.10  1.0  AA+    20270101  Fixed     Enterprise
// 100005 sh EpsilonBd (none) 7.0 AA     20320101  Progress  Corporate
// 100006 sz ZetaBond  2.50  3.0  (none) (none)    (none)    (none)
const char* kSyntheticJsn = R"([{
  "colheader":["$ZQDM","$SC","ZQJC","DQLL","SYNX","ZQXY","DQSJ","LLLX","ZQLX","MZ"],
  "data":[
    ["100001","1","AlphaBond","3.10","5.0","AA+","20300101","Fixed","Corporate","100"],
    ["100002","1","BetaBond","1.85","2.0","AA+","20280101","Fixed","Corporate","100"],
    ["100003","0","GammaBond","4.20","9.0","AA","20350101","Floating","Enterprise","100"],
    ["100004","0","DeltaBond","3.10","1.0","AA+","20270101","Fixed","Enterprise","100"],
    ["100005","1","EpsilonBd","","7.0","AA","20320101","Progress","Corporate","100"],
    ["100006","0","ZetaBond","2.50","3.0","","","","","100"]
  ]
}])";

// Every row carries the same coupon rate, so ordering is decided purely by the
// security_id tie-break.  Document order is deliberately the reverse of
// security_id order -- SZ200002 is written first but sorts after SH200001 -- so a
// comparator that dropped the tie-break and leaned on stable_sort's preservation
// of input order would produce a visibly different sequence.
const char* kTieBreakJsn = R"([{
  "colheader":["$ZQDM","$SC","ZQJC","DQLL","SYNX","ZQXY","DQSJ","LLLX","ZQLX","MZ"],
  "data":[
    ["200002","0","TieB","3.00","4.0","AA+","20300101","Fixed","Corporate","100"],
    ["200001","1","TieA","3.00","4.0","AA+","20300101","Fixed","Corporate","100"],
    ["200003","0","TieC","3.00","4.0","AA+","20300101","Fixed","Corporate","100"]
  ]
}])";

// Writes a synthetic resource where fetch_source will find it, so query() runs
// entirely offline with no live JSN endpoint.
fs::path write_root(const char* name, const char* payload) {
    const auto root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root / "list");
    std::ofstream out(root / "list" / "zq_aaj201.jsn", std::ios::binary);
    out << payload;
    out.close();
    return root;
}

tdx::BondReferenceQuery synthetic_query() {
    tdx::BondReferenceQuery query;
    query.group = "rating";
    query.bucket = "aa-plus";
    return query;
}

// The record codes a query returns, in order, so ordering and paging assertions
// read as plain sequences.
std::vector<std::string> record_codes(const tdx::Json& result) {
    std::vector<std::string> codes;
    for (const auto& record : result.at("records").as_array())
        codes.push_back(record.at("security").at("code").as_string());
    return codes;
}

bool codes_are(const tdx::Json& result, const std::vector<std::string>& expected) {
    return record_codes(result) == expected;
}

// A facet tally as a flat "name=count" list for compact comparison.
std::vector<std::string> facet(const tdx::Json& summary, const char* key) {
    std::vector<std::string> pairs;
    for (const auto& row : summary.at(key).as_array())
        pairs.push_back(row.at("name").as_string() + "=" +
                        std::to_string(static_cast<std::uint64_t>(
                            row.at("count").as_number())));
    return pairs;
}

bool throws_with(const std::function<void()>& action, const std::string& expected) {
    try {
        action();
    } catch (const std::exception& error) {
        return std::string(error.what()) == expected;
    }
    return false;
}
}

int main() {
    try {
        require(tdx::bond_reference_sources().size() == 24,
                "bond reference must preserve rating, rate and category buckets");
        auto rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(R"({
          "$ZQDM":"115455","$SC":"1","ZQJC":"23海旅01",
          "XGFXRQ":"20270626","DQLL":"1.850","SYNX":"1.884931",
          "LLLX":"固定利率附息","LLLXBZ":"1","ZQXY":"AA+","ZTXY":"AA+",
          "QXSJ":"20230626","DQSJ":"20280626","FXPL1":"12",
          "MZ":"100","FXJG":"100","GM":"1000000000","SFDB":"否","ZQLX":"公司债",
          "FXRQXL":"20240626,20250626,20260626,20270626,20280626",
          "FXLLXL":"0.033,0.033,0.033,0.0185,0.0185","SYFXCS":"2",
          "SYFXRQXL":"20270626,20280626","SYFXLLXL":"0.0185,0.0185",
          "SGFXRQ":"20260626"
        })"));
        const auto normalized = tdx::normalize_bond_reference_rows(
            rows, tdx::bond_reference_sources()[1]);
        const auto& row = normalized.as_array().front();
        require(row.at("security").at("security_id").as_string() == "SH115455" &&
                    row.at("security").at("name").as_string() == "23海旅01" &&
                    row.at("bond_credit_rating").as_string() == "AA+" &&
                    std::abs(row.at("current_coupon_rate_pct").as_number() - 1.85) < 1e-9 &&
                    row.at("coupon_schedule").size() == 5 &&
                    std::abs(row.at("coupon_schedule").as_array()[0]
                        .at("rate_pct").as_number() - 3.3) < 1e-9 &&
                    row.at("remaining_coupon_schedule").size() == 2 &&
                    std::abs(row.at("remaining_coupon_schedule").as_array()[0]
                        .at("rate_pct").as_number() - 1.85) < 1e-9 &&
                    row.at("issue_size_yuan").as_number() == 1000000000.0 &&
                    row.at("guarantee_status").as_string() == "否" &&
                    row.at("raw").is_object(),
                "bond terms and decimal coupon schedule must normalize faithfully");
        auto convertible_rows = tdx::Json::array();
        convertible_rows.push_back(tdx::Json::parse(R"({
          "$ZQDM":"110099","$SC":"1","ZQJC":"阿拉转债",
          "$ZQDM1":"600483","$SC1":"1","MZ":"100","ZGJ":"9.49",
          "ZGQSR":"20260417","ZGJZR":"20311012","SSRQ":"20251030",
          "QXRQ":"20251013","DQRQ":"20311013","SYNX":"5.184",
          "ZQPJ":"AA+","ZTPJ":"AA+","XXCFBL":"85","HSCFBL":"70",
          "QSCFBL":"130","ZQLX":"可转债","LLLX":"累进利率",
          "FXRQXL":"20261013,20311013","FXLLXL":"0.002,0.06",
          "SYFXCS":"2","SYFXRQXL":"20261013,20311013",
          "SYFXLLXL":"0.002,0.06","XGFXRQ":"20261013"
        })"));
        const auto& sources = tdx::bond_reference_sources();
        const auto all_bonds = std::find_if(
            sources.begin(), sources.end(), [](const auto& source) {
                return source.group == "category" && source.bucket == "all" &&
                    source.resource == "list/zq_zqqb201.jsn";
            });
        require(all_bonds != sources.end(),
                "all-bond master source must be registered");
        const auto government = std::find_if(
            sources.begin(), sources.end(), [](const auto& source) {
                return source.group == "category" && source.bucket == "government";
            });
        require(government != sources.end() &&
                    government->resource == "list/zqgz201.jsn",
                "government category must use the client all-market master");
        auto government_rows = tdx::Json::array();
        government_rows.push_back(tdx::Json::parse(R"({
          "$ZQDM1":"019707","$SC1":"1","$ZQDM":"9900045001",
          "ZQJC":"23国债14","GM":"86300000000","MZ":"100","DQLL":"2.62",
          "ZQLX":"国债","DQSJ":"20300625","FXRQXL":"20300625",
          "FXLLXL":"0.0262"
        })"));
        const auto government_normalized = tdx::normalize_bond_reference_rows(
            government_rows, *government);
        const auto& government_row = government_normalized.as_array().front();
        require(government_row.at("security").at("security_id").as_string() ==
                    "SH019707" &&
                    government_row.at("client_instrument_id").as_string() ==
                    "9900045001" &&
                    government_row.at("source_scale_raw").as_number() ==
                        86300000000.0 &&
                    government_row.at("source_scale_semantics").as_string() ==
                        "client-master-hidden-unit" &&
                    government_row.at("issue_size_yuan").is_null() &&
                    government_row.at("underlying").is_null(),
                "client master aliases, ambiguous scale and non-underlying semantics failed");
        const auto recent_convertible = std::find_if(
            sources.begin(), sources.end(), [](const auto& source) {
                return source.bucket == "recent-convertible";
            });
        require(recent_convertible != sources.end(),
                "recent convertible source must remain registered");
        const auto convertible = tdx::normalize_bond_reference_rows(
            convertible_rows, *recent_convertible);
        const auto& convertible_row = convertible.as_array().front();
        require(convertible_row.at("bond_credit_rating").as_string() == "AA+" &&
                    convertible_row.at("accrual_start_date").as_string() == "20251013" &&
                    convertible_row.at("maturity_date").as_string() == "20311013" &&
                    convertible_row.at("underlying").at("security_id").as_string() ==
                        "SH600483" &&
                    std::abs(convertible_row.at("conversion_price_yuan").as_number() -
                             9.49) < 1e-9 &&
                    convertible_row.at("revision_trigger_pct").as_number() == 85 &&
                    convertible_row.at("put_trigger_pct").as_number() == 70 &&
                    convertible_row.at("call_trigger_pct").as_number() == 130,
                "recent convertible aliases and conversion terms must normalize");

        auto policy_rows = tdx::Json::array();
        policy_rows.push_back(tdx::Json::parse(R"({
          "$ZQDM":"201","$ZQDM1":"018015","$SC1":"1",
          "ZQJC":"国开2105","GM":"20","MZ":"100","DQLL":"2.5"
        })"));
        const auto policy_source = std::find_if(
            sources.begin(), sources.end(), [](const auto& source) {
                return source.bucket == "policy-financial";
            });
        require(policy_source != sources.end(),
                "policy-financial source must be registered");
        const auto policy = tdx::normalize_bond_reference_rows(
            policy_rows, *policy_source);
        const auto& policy_row = policy.as_array().front();
        require(policy_row.at("security").at("security_id").as_string() ==
                    "SH018015" &&
                    policy_row.at("issue_size_source_100m").as_number() == 20.0 &&
                    policy_row.at("issue_size_yuan").as_number() == 2000000000.0,
                "policy-financial master aliases and 100m-yuan scaling failed");

        // ---- BondReferenceService::query pipeline -------------------------------
        // Driven through the public API against a synthetic local resource, so the
        // filter, sort, paginate, summary and validation phases each have a
        // hand-checkable expectation that does not depend on a downloaded corpus.
        const auto synthetic_root =
            write_root("tdx-bondref-query-tests", kSyntheticJsn);
        const auto run = [&](tdx::BondReferenceQuery query) {
            tdx::BondReferenceService service({}, synthetic_root);
            return service.query(query);
        };

        const auto baseline = run(synthetic_query());
        require(baseline.at("schema").as_string() ==
                        "tdx-market-bond-reference-native-v1" &&
                    baseline.at("match_count").as_number() == 6 &&
                    baseline.at("returned").as_number() == 6 &&
                    baseline.at("availability").as_string() == "live" &&
                    baseline.at("projection_reconciliation").is_null(),
                "query must return every synthetic row and skip the projection audit");
        const auto& summary = baseline.at("summary");
        require(summary.at("source_group").as_string() == "rating" &&
                    summary.at("source_bucket").as_string() == "aa-plus" &&
                    summary.at("source_row_count").as_number() == 6 &&
                    summary.at("matched_count").as_number() == 6 &&
                    summary.at("matched_unique_security_count").as_number() == 6,
                "query summary must report the source and matched counts");
        require(facet(summary, "credit_ratings") ==
                        std::vector<std::string>{"AA=2", "AA+=3"} &&
                    facet(summary, "rate_types") ==
                        std::vector<std::string>{"Fixed=3", "Floating=1", "Progress=1"} &&
                    facet(summary, "bond_types") ==
                        std::vector<std::string>{"Corporate=3", "Enterprise=2"},
                "query facets must tally only non-empty values, in sorted order");
        require(summary.at("earliest_maturity_date").as_string() == "20270101" &&
                    summary.at("latest_maturity_date").as_string() == "20350101",
                "maturity range must ignore rows with no maturity date");

        // Text sort keys, both directions.
        auto sorted = synthetic_query();
        sorted.sort = "maturity";
        require(codes_are(run(sorted), {"100006", "100004", "100002", "100001",
                                        "100005", "100003"}),
                "ascending maturity sort must place the empty date first");
        sorted.order = "desc";
        require(codes_are(run(sorted), {"100003", "100005", "100001", "100002",
                                        "100004", "100006"}),
                "descending maturity sort must reverse the ascending order");
        sorted = synthetic_query();
        sorted.sort = "name";
        require(codes_are(run(sorted), {"100001", "100002", "100004", "100005",
                                        "100003", "100006"}),
                "name sort must order by the security name");
        sorted.sort = "code";
        require(codes_are(run(sorted), {"100001", "100002", "100003", "100004",
                                        "100005", "100006"}),
                "code sort must order by the security code");

        // Numeric sort keys.  The rate-less row has no key, so the infinity
        // sentinel must park it last in BOTH directions rather than flipping ends.
        sorted = synthetic_query();
        sorted.sort = "rate";
        auto rate_asc = record_codes(run(sorted));
        sorted.order = "desc";
        auto rate_desc = record_codes(run(sorted));
        require(rate_asc == std::vector<std::string>{"100002", "100006", "100001",
                                                     "100004", "100003", "100005"} &&
                    rate_desc == std::vector<std::string>{"100003", "100001",
                                                          "100004", "100006",
                                                          "100002", "100005"},
                "a missing numeric sort key must sort last in both directions");
        // Equal sort keys break on security_id, and that tie-break is always
        // ascending -- reversing the order must not reverse it.  The fixture writes
        // SZ200002 before SH200001, so preserving document order would show.
        const auto tie_root = write_root("tdx-bondref-tiebreak-tests", kTieBreakJsn);
        const auto run_tie = [&](tdx::BondReferenceQuery query) {
            tdx::BondReferenceService service({}, tie_root);
            return service.query(query);
        };
        for (const char* sort : {"rate", "remaining", "maturity", "code", "name"})
            for (const char* order : {"asc", "desc"}) {
                auto tie = synthetic_query();
                tie.sort = sort;
                tie.order = order;
                const auto tied = run_tie(tie);
                // "code" and "name" have distinct keys per row, so only the three
                // genuinely tied keys assert the ascending security_id fallback.
                if (std::string(sort) == "code" || std::string(sort) == "name") continue;
                require(codes_are(tied, {"200001", "200002", "200003"}),
                        "tied sort keys must fall back to ascending security_id");
            }
        fs::remove_all(tie_root);
        sorted = synthetic_query();
        sorted.sort = "remaining";
        require(codes_are(run(sorted), {"100004", "100002", "100006", "100001",
                                        "100005", "100003"}),
                "remaining sort must order by remaining years");

        // Filters.  Market accepts both the name and the numeric id.
        auto filtered = synthetic_query();
        filtered.market = "sh";
        filtered.sort = "code";
        require(codes_are(run(filtered), {"100001", "100002", "100005"}) &&
                    run(filtered).at("match_count").as_number() == 3,
                "market filter must accept the market name");
        filtered.market = "1";
        require(codes_are(run(filtered), {"100001", "100002", "100005"}),
                "market filter must accept the numeric market id");
        filtered.market = "0";
        require(codes_are(run(filtered), {"100003", "100004", "100006"}),
                "market filter must select Shenzhen by numeric id");
        filtered = synthetic_query();
        filtered.code = "100003";
        require(codes_are(run(filtered), {"100003"}),
                "code filter must select the exact security");
        filtered = synthetic_query();
        filtered.query = "gamma";
        require(codes_are(run(filtered), {"100003"}),
                "text query must match case-insensitively");
        filtered.query = "nosuchtext";
        const auto empty = run(filtered);
        require(empty.at("match_count").as_number() == 0 &&
                    empty.at("returned").as_number() == 0 &&
                    empty.at("availability").as_string() == "empty" &&
                    empty.at("summary").at("earliest_maturity_date").is_null() &&
                    empty.at("summary").at("latest_maturity_date").is_null(),
                "a query matching nothing must report empty availability and null range");

        // Paging keeps match_count at the full matched size.
        auto paged = synthetic_query();
        paged.sort = "code";
        paged.offset = 2;
        paged.limit = 3;
        const auto page = run(paged);
        require(codes_are(page, {"100003", "100004", "100005"}) &&
                    page.at("match_count").as_number() == 6 &&
                    page.at("returned").as_number() == 3,
                "paging must window the records without changing match_count");
        paged.offset = 99;
        const auto past_end = run(paged);
        require(past_end.at("returned").as_number() == 0 &&
                    past_end.at("match_count").as_number() == 6,
                "an offset past the end must return no records but keep match_count");

        // Pagination re-normalizes with raw evidence retained; the filter pass
        // does not, which is what keeps the 42k-row archive affordable.
        require(page.at("records").as_array().front().at("raw").is_object(),
                "returned records must retain their raw evidence");

        // Echoed filters distinguish absent from blank.
        require(baseline.at("filters").at("market").is_null() &&
                    baseline.at("filters").at("code").is_null() &&
                    baseline.at("filters").at("query").as_string().empty() &&
                    baseline.at("filters").at("include_projections").as_bool() == false,
                "echoed filters must report absent market and code as null");

        // Case and whitespace normalization.
        auto messy = synthetic_query();
        messy.group = "  RATING ";
        messy.bucket = " AA-PLUS ";
        messy.sort = "CODE";
        messy.order = "ASC";
        messy.market = "SH";
        require(codes_are(run(messy), {"100001", "100002", "100005"}) &&
                    run(messy).at("filters").at("group").as_string() == "rating",
                "group, bucket, sort, order and market must normalize case and space");

        // Validation, in the order query() checks it.  The paging and
        // cache/timeout ranges are unreachable from the CLI, which range-checks
        // those flags first, so this is their only coverage.
        auto invalid = synthetic_query();
        invalid.group = "bogus";
        require(throws_with([&] { run(invalid); },
                            "bond-reference group must be rating, rate, or category"),
                "an unknown group must be rejected");
        invalid = synthetic_query();
        invalid.bucket = "nosuchbucket";
        require(throws_with([&] { run(invalid); },
                            "unknown bond-reference bucket for selected group: nosuchbucket"),
                "an unknown bucket must be rejected");
        invalid = synthetic_query();
        invalid.sort = "bogus";
        require(throws_with([&] { run(invalid); },
                            "bond-reference sort must be name, code, maturity, rate, or remaining"),
                "an unknown sort must be rejected");
        invalid = synthetic_query();
        invalid.order = "sideways";
        require(throws_with([&] { run(invalid); }, "order must be asc or desc"),
                "an unknown order must be rejected");
        for (const auto& paging : std::vector<tdx::BondReferenceQuery>{
                 [] { auto q = synthetic_query(); q.offset = -1; return q; }(),
                 [] { auto q = synthetic_query(); q.offset = 1000001; return q; }(),
                 [] { auto q = synthetic_query(); q.limit = 0; return q; }(),
                 [] { auto q = synthetic_query(); q.limit = 5001; return q; }()})
            require(throws_with([&] { run(paging); },
                                "bond-reference paging is outside the supported range"),
                    "out-of-range paging must be rejected");
        for (const auto& budget : std::vector<tdx::BondReferenceQuery>{
                 [] { auto q = synthetic_query(); q.cache_ttl_seconds = -1; return q; }(),
                 [] { auto q = synthetic_query(); q.cache_ttl_seconds = 86401; return q; }(),
                 [] { auto q = synthetic_query(); q.timeout_ms = 99; return q; }(),
                 [] { auto q = synthetic_query(); q.timeout_ms = 120001; return q; }()})
            require(throws_with([&] { run(budget); },
                                "bond-reference cache/timeout is outside the supported range"),
                    "out-of-range cache/timeout must be rejected");

        fs::remove_all(synthetic_root);
        std::cout << "bond reference tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bond reference test failed: " << error.what() << '\n';
        return 1;
    }
}
