#include "tdx/common.hpp"
#include "tdx/disclosures.hpp"
#include "tdx/formula_context.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// `market disclosures` rejects conflicting flags while parsing options, before
// it resolves the TDX root, so the option contract is checkable without a TDX
// installation.  Only parse-time rejections belong here.
void require_disclosure_rejection(const std::vector<std::string>& args,
                                  const std::string& expected) {
    try {
        tdx::command_market_disclosures(args);
    } catch (const tdx::Error& error) {
        require(std::string(error.what()) == expected,
                "expected \"" + expected + "\" but got \"" + error.what() + '"');
        return;
    }
    throw std::runtime_error("expected rejection: " + expected);
}

tdx::Bytes listing_date_dbf_fixture() {
    constexpr std::uint16_t header_length = 129;
    constexpr std::uint16_t record_length = 16;
    constexpr std::uint32_t record_count = 3;
    tdx::Bytes data(header_length + record_length * record_count + 1, 0);
    data[0] = 0x03;
    data[1] = 26; data[2] = 8; data[3] = 3;
    data[4] = static_cast<std::uint8_t>(record_count);
    data[8] = static_cast<std::uint8_t>(header_length);
    data[9] = static_cast<std::uint8_t>(header_length >> 8);
    data[10] = static_cast<std::uint8_t>(record_length);
    data[11] = static_cast<std::uint8_t>(record_length >> 8);
    const auto field = [&](std::size_t offset, const std::string& name,
                           std::uint8_t length) {
        std::copy(name.begin(), name.end(), data.begin() +
                  static_cast<std::ptrdiff_t>(offset));
        data[offset + 11] = 'C';
        data[offset + 16] = length;
    };
    field(32, "SC", 1);
    field(64, "GPDM", 6);
    field(96, "SSDATE", 8);
    data[128] = 0x0d;
    const auto record = [&](std::size_t index, char marker, char market,
                            const std::string& code, const std::string& date) {
        const auto offset = header_length + index * record_length;
        std::fill(data.begin() + static_cast<std::ptrdiff_t>(offset),
                  data.begin() + static_cast<std::ptrdiff_t>(offset + record_length), ' ');
        data[offset] = static_cast<std::uint8_t>(marker);
        data[offset + 1] = static_cast<std::uint8_t>(market);
        std::copy(code.begin(), code.end(), data.begin() +
                  static_cast<std::ptrdiff_t>(offset + 2));
        std::copy(date.begin(), date.end(), data.begin() +
                  static_cast<std::ptrdiff_t>(offset + 8));
    };
    record(0, ' ', '0', "300503", "20160119");
    record(1, ' ', '1', "600000", "20260101");
    record(2, '*', '1', "600001", "19990101");
    data.back() = 0x1a;
    return data;
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};
        securities[{0, "300503"}] =
            tdx::Security{0, "SZ", "深圳", "300503", "昊志机电"};
        securities[{1, "600000"}] =
            tdx::Security{1, "SH", "上海", "600000", "浦发银行"};

        auto schedule_rows = tdx::Json::array();
        schedule_rows.push_back(tdx::Json::parse(
            "{\"$SC\":\"0\",\"$ZQDM\":\"000001\",\"bgq\":\"20260630\"," 
            "\"ypldate\":\"20260815\",\"scpldate\":\"20260815\"," 
            "\"date1\":\"20260818\",\"plqk\":\"变更\",\"spldate\":\"\"}"));
        const auto schedule = tdx::normalize_disclosure_schedule_rows(
            schedule_rows, false, securities);
        const auto& pending = schedule.as_array()[0];
        require(pending.at("status").as_string() == "rescheduled" &&
                    pending.at("available_from").is_null() &&
                    pending.at("security").at("name").as_string() == "平安银行" &&
                    pending.at("change_dates").size() == 1,
                "scheduled reports must not become available before actual disclosure");

        auto disclosed_rows = tdx::Json::array();
        disclosed_rows.push_back(tdx::Json::parse(
            "{\"$SC\":\"1\",\"$ZQDM\":\"600000\",\"bgq\":\"20260630\"," 
            "\"ypldate\":\"20260806\",\"scpldate\":\"20260806\"," 
            "\"spldate\":\"20260805\"}"));
        const auto disclosed = tdx::normalize_disclosure_schedule_rows(
            disclosed_rows, false, securities);
        require(disclosed.as_array()[0].at("status").as_string() == "disclosed" &&
                    disclosed.as_array()[0].at("available_from").as_string() == "20260805",
                "actual disclosure must define the point-in-time availability date");

        auto express_rows = tdx::Json::array();
        express_rows.push_back(tdx::Json::parse(
            "{\"$SC\":\"1\",\"$ZQDM\":\"688252\",\"GGRQ\":\"20260806\"," 
            "\"bgq\":\"20260630\",\"jlr1\":\"138020200\",\"jlr3\":\"-9.42\"," 
            "\"MGSY\":\"0.34\"}"));
        const auto express = tdx::normalize_disclosure_express_rows(express_rows, securities);
        require(express.as_array()[0].at("available_from").as_string() == "20260806" &&
                    express.as_array()[0].at("net_profit_yuan").as_number() == 138020200 &&
                    express.as_array()[0].at("basic_eps").as_number() == 0.34,
                "express rows must retain announcement date and financial values");

        auto hk_rows = tdx::Json::array();
        hk_rows.push_back(tdx::Json::parse(
            "{\"$SC\":\"31\",\"$ZQDM\":\"00002\",\"bgq\":\"中报\"," 
            "\"ksrq\":\"20260101\",\"jzri\":\"20260630\"," 
            "\"ypldate\":\"20260806\",\"spldate\":\"20260806\"}"));
        const auto hk = tdx::normalize_disclosure_schedule_rows(hk_rows, true, securities);
        require(hk.as_array()[0].at("security").at("market").as_string() == "hk" &&
                    hk.as_array()[0].at("report_period").as_string() == "20260630" &&
                    hk.as_array()[0].at("report_type").as_string() == "中报",
                "Hong Kong schedules must retain their report interval and market IDs");

        auto announcement_response = tdx::Json::object();
        announcement_response["ResultSets"] = tdx::Json::array();
        auto announcement_set = tdx::Json::object();
        announcement_set["ColName"] =
            "issue_date title tableid rec_id typecode typename url redistime source";
        announcement_set["Content"] = tdx::Json::array();
        const auto add_announcement = [&](const char* date, const char* title,
                                          const char* typecode, const char* typename_value,
                                          const char* url) {
            auto row = tdx::Json::array();
            row.push_back(date); row.push_back(title); row.push_back("table");
            row.push_back(title); row.push_back(typecode); row.push_back(typename_value);
            row.push_back(url); row.push_back(""); row.push_back("exchange");
            announcement_set["Content"].push_back(std::move(row));
        };
        add_announcement("2026-04-21 00:00:00", "样本：2025年年度报告摘要",
                         "010301", "年度报告", "summary.pdf");
        add_announcement("2026-04-21 00:00:00", "样本：2025年年度报告",
                         "010301", "年度报告", "annual.pdf");
        add_announcement("2026-05-01 00:00:00", "样本：2025年年度报告（修订版）",
                         "010301", "年度报告", "annual-revised.pdf");
        add_announcement("2026-04-25 00:00:00", "样本：2026年一季度报告",
                         "010305", "一季度报告", "q1.pdf");
        add_announcement("2025-08-23 00:00:00", "样本：2025年半年度报告",
                         "010303", "半年度报告", "h1.pdf");
        add_announcement("2025-10-25 00:00:00", "样本：2025年三季度报告",
                         "010307", "三季度报告", "q3.pdf");
        add_announcement("2026-03-01 00:00:00", "样本：2025年年度报告业绩说明会",
                         "9901", "其他披露事项", "notice.pdf");
        announcement_response["ResultSets"].push_back(std::move(announcement_set));
        const auto announcements = tdx::normalize_disclosure_announcement_response(
            announcement_response, "sz", "000001", securities);
        require(announcements.at("rows").size() == 4 &&
                    announcements.at("rows").as_array()[1]
                            .at("report_period").as_string() == "20251231" &&
                    announcements.at("rows").as_array()[1]
                            .at("available_from").as_string() == "20260421" &&
                    announcements.at("rows").as_array()[1]
                            .at("announcement_evidence").size() == 3 &&
                    !announcements.at("rows").as_array()[1]
                            .at("selected_announcement").at("summary").as_bool(),
                "announcement backfill must select the earliest full report and retain evidence");

        auto sh_response = tdx::Json::parse(
            "{\"ResultSets\":[{\"ColName\":[\"issue_date\",\"title\",\"typecode\","
            "\"typename\"],\"Content\":["
            "[\"2026-03-31\",\"浦发银行2025年度报告摘要\",\"101\",\"年报\"],"
            "[\"2026-03-31\",\"浦发银行2025年度报告\",\"LSGG\",\"年报\"],"
            "[\"2026-03-31\",\"浦发银行2025年度审计报告\",\"LSGG\",\"年报\"]]}]}");
        const auto sh_announcements = tdx::normalize_disclosure_announcement_response(
            sh_response, "sh", "600000", securities);
        require(sh_announcements.at("rows").size() == 1 &&
                    sh_announcements.at("rows").as_array()[0]
                            .at("report_period").as_string() == "20251231" &&
                    sh_announcements.at("rows").as_array()[0]
                            .at("selected_announcement").at("typecode").as_string() ==
                        "LSGG" &&
                    sh_announcements.at("rows").as_array()[0]
                            .at("announcement_evidence").size() == 2,
                "Shanghai generic LSGG full reports must pair with typed summaries safely");

        const auto watchlist = tdx::parse_tdx_watchlist_securities(
            "1600000\r\n0300503\r\n0BABA\r\n1600000\r\n", securities);
        require(watchlist.size() == 2 && watchlist[0].market == "sz" &&
                    watchlist[0].code == "300503" &&
                    watchlist[1].market == "sh" &&
                    watchlist[1].name == "浦发银行",
                "TDX blocknew watchlists must deduplicate mainland seven-digit entries");

        auto listing_dates = tdx::parse_disclosure_listing_dates_dbf(
            listing_date_dbf_fixture());
        require(listing_dates.at("dbf_updated_date").as_string() == "20260803" &&
                    listing_dates.at("summary").at("listing_date_count").as_number() == 2 &&
                    listing_dates.at("summary").at("deleted_records").as_number() == 1 &&
                    listing_dates.at("records").as_array()[0]
                            .at("security_id").as_string() == "SH600000" &&
                    listing_dates.at("records").as_array()[1]
                            .at("listing_date").as_string() == "20160119",
                "local base.dbf SSDATE records must parse with deletion handling");

        std::vector<tdx::DisclosureBackfillSecurity> batch_securities{
            {"sz", "300503", "昊志机电"}, {"sh", "600000", "浦发银行"}};
        tdx::DisclosureBackfillBatchOptions batch_options;
        batch_options.delay_ms = 0;
        batch_options.retry_count = 1;
        batch_options.retry_delay_ms = 0;
        std::map<std::string, int> fetch_attempts;
        int checkpoint_count = 0, archive_checkpoint_count = 0;
        const auto fake_fetch = [&](const std::string& market, const std::string& code,
                                    int) {
            const auto key = market + code;
            if (++fetch_attempts[key] == 1 && code == "600000")
                throw tdx::Error("synthetic transient failure");
            auto document = tdx::Json::object();
            document["generated_at"] = "2026-08-06T12:00:00+0800";
            document["rows"] = tdx::Json::array();
            auto row = tdx::Json::object();
            row["kind"] = "announcement-report";
            row["region"] = "mainland";
            row["report_period"] = code == "300503" ? "20260331" : "20251231";
            row["available_from"] = code == "300503" ? "20260421" : "20260331";
            row["security"] = tdx::Json::object();
            row["security"]["market"] = market;
            row["security"]["market_id"] = market == "sh" ? 1 : 0;
            row["security"]["code"] = code;
            row["security"]["security_id"] =
                std::string(market == "sh" ? "SH" : "SZ") + code;
            row["security"]["name"] = code == "300503" ? "昊志机电" : "浦发银行";
            row["announcement_evidence"] = tdx::Json::array();
            document["rows"].push_back(std::move(row));
            return document;
        };
        const auto checkpoint = [&](const tdx::Json&, const tdx::Json&,
                                    bool archive_changed) {
            ++checkpoint_count;
            if (archive_changed) ++archive_checkpoint_count;
        };
        const auto batch = tdx::run_disclosure_announcement_backfill_batch(
            batch_securities, nullptr, nullptr, batch_options, fake_fetch, checkpoint);
        require(batch.summary.at("requested").as_number() == 2 &&
                    batch.summary.at("succeeded").as_number() == 2 &&
                    batch.summary.at("network_requests").as_number() == 3 &&
                    batch.archive.at("summary").at("entry_count").as_number() == 2 &&
                    batch.state.at("summary").at("completed").as_number() == 2 &&
                    checkpoint_count == 6 && archive_checkpoint_count == 2,
                "batch backfill must checkpoint before requests, retry failures and merge successes");
        int resumed_fetches = 0;
        const auto resumed = tdx::run_disclosure_announcement_backfill_batch(
            batch_securities, &batch.archive, &batch.state, batch_options,
            [&](const std::string&, const std::string&, int) -> tdx::Json {
                ++resumed_fetches;
                throw tdx::Error("completed securities must not be fetched");
            });
        require(resumed_fetches == 0 &&
                    resumed.summary.at("skipped_completed").as_number() == 2 &&
                    resumed.archive.at("summary").at("entry_count").as_number() == 2,
                "resumed backfill must skip completed securities without changing the archive");
        auto stale_state = batch.state;
        for (auto& entry : stale_state["entries"].as_array())
            entry["completed_unix"] = 0;
        auto ttl_options = batch_options;
        ttl_options.completed_ttl_seconds = 3600;
        int stale_refreshes = 0;
        const auto stale_refreshed = tdx::run_disclosure_announcement_backfill_batch(
            batch_securities, &batch.archive, &stale_state, ttl_options,
            [&](const std::string& market, const std::string& code, int timeout) {
                ++stale_refreshes;
                return fake_fetch(market, code, timeout);
            });
        require(stale_refreshes == 2 &&
                    stale_refreshed.summary.at("succeeded").as_number() == 2 &&
                    stale_refreshed.summary.at("network_requests").as_number() == 2 &&
                    stale_refreshed.archive.at("summary").at("entry_count").as_number() == 2,
                "maintenance TTL must refresh stale completed entries idempotently");

        const auto coverage_archive = tdx::Json::parse(
            "{\"schema\":\"tdx-disclosure-availability-archive-v1\",\"entries\":["
            "{\"market\":\"sz\",\"code\":\"300503\",\"security_id\":\"SZ300503\"," 
            "\"name\":\"昊志机电\",\"report_period\":\"20260331\"," 
            "\"report_available_from\":\"20260421\",\"express_available_from\":null," 
            "\"source_kinds\":[\"announcement-report\"]},"
            "{\"market\":\"sz\",\"code\":\"300503\",\"security_id\":\"SZ300503\"," 
            "\"name\":\"昊志机电\",\"report_period\":\"20251231\"," 
            "\"report_available_from\":null,\"express_available_from\":\"20260301\"," 
            "\"source_kinds\":[\"express\"]},"
            "{\"market\":\"sh\",\"code\":\"600000\",\"security_id\":\"SH600000\"," 
            "\"name\":\"浦发银行\",\"report_period\":\"20260331\"," 
            "\"report_available_from\":null,\"express_available_from\":null," 
            "\"scheduled_disclosure_date\":\"20260430\"," 
            "\"source_kinds\":[\"schedule\"]}]}" );
        auto coverage_securities = batch_securities;
        coverage_securities.push_back({"sh", "513120", "港股创新药ETF"});
        const auto coverage = tdx::audit_disclosure_coverage(
            coverage_securities, coverage_archive, {"20251231", "20260331"}, 4,
            "20260401", &listing_dates);
        require(coverage.at("report_periods").as_array()[0].as_string() == "20260331" &&
                    coverage.at("summary").at("security_period_pairs").as_number() == 3 &&
                    coverage.at("summary").at("pre_listing_pairs").as_number() == 1 &&
                    coverage.at("summary").at("selected_security_period_pairs")
                            .as_number() == 6 &&
                    coverage.at("summary").at("not_applicable_pairs").as_number() == 2 &&
                    coverage.at("summary").at("covered_pairs").as_number() == 1 &&
                    coverage.at("summary").at("gap_pairs").as_number() == 2 &&
                    coverage.at("summary").at("by_status")
                            .at("express_only").as_number() == 1 &&
                    coverage.at("summary").at("by_status")
                            .at("pre_listing").as_number() == 1 &&
                    coverage.at("summary").at("by_status")
                            .at("scheduled_future").as_number() == 1 &&
                    coverage.at("summary").at("pending_pairs").as_number() == 1 &&
                    coverage.at("summary").at("actionable_gap_pairs").as_number() == 1 &&
                    coverage.at("items").size() == 1 &&
                    coverage.at("items").as_array()[0]
                            .at("security_id").as_string() == "SZ300503",
                "coverage audit must exclude pre-listing quarters from actionable gaps");
        const auto latest_coverage = tdx::audit_disclosure_coverage(
            coverage_securities, coverage_archive, {}, 1, "20260401",
            &listing_dates);
        require(latest_coverage.at("report_periods").size() == 1 &&
                    latest_coverage.at("report_periods").as_array()[0].as_string() ==
                        "20260331" &&
                    latest_coverage.at("items").size() == 0 &&
                    latest_coverage.at("summary").at("pending_pairs").as_number() == 1,
                "future scheduled reports must not enter the actionable backfill queue");
        bool invalid_coverage_period_rejected = false;
        try {
            (void)tdx::audit_disclosure_coverage(
                batch_securities, coverage_archive, {"20260101"});
        } catch (const tdx::Error&) {
            invalid_coverage_period_rejected = true;
        }
        require(invalid_coverage_period_rejected,
                "coverage audit must reject dates that are not quarter ends");

        auto observation = tdx::Json::object();
        observation["rows"] = tdx::Json::array();
        observation["rows"].push_back(pending);
        observation["rows"].push_back(express.as_array()[0]);
        auto archive = tdx::merge_disclosure_archive_document(
            observation, nullptr, "2026-08-06T10:00:00+0800");
        require(archive.at("summary").at("entry_count").as_number() == 2 &&
                    archive.at("summary").at("report_available_count").as_number() == 0 &&
                    archive.at("summary").at("express_available_count").as_number() == 1 &&
                    archive.at("summary").at("scheduled_pending_count").as_number() == 1,
                "pending schedules and express announcements must remain distinct");
        bool schedule_metadata_retained = false;
        for (const auto& entry : archive.at("entries").as_array())
            if (entry.at("security_id").as_string() == "SZ000001" &&
                entry.at("scheduled_disclosure_date").as_string() == "20260815" &&
                entry.at("first_scheduled_date").as_string() == "20260815" &&
                entry.at("schedule_status").as_string() == "rescheduled")
                schedule_metadata_retained = true;
        require(schedule_metadata_retained,
                "availability archive must retain current and first scheduled dates");

        auto actual_observation = tdx::Json::object();
        actual_observation["rows"] = tdx::Json::array();
        auto actual_for_ping_an = pending;
        actual_for_ping_an["kind"] = "recent";
        actual_for_ping_an["status"] = "disclosed";
        actual_for_ping_an["available_from"] = "20260817";
        actual_observation["rows"].push_back(actual_for_ping_an);
        archive = tdx::merge_disclosure_archive_document(
            actual_observation, &archive, "2026-08-17T20:00:00+0800");
        require(archive.at("summary").at("entry_count").as_number() == 2 &&
                    archive.at("summary").at("report_available_count").as_number() == 1,
                "actual full-report observations must unlock the report without duplicates");

        auto unchanged = tdx::merge_disclosure_archive_document(
            actual_observation, &archive, "2026-08-18T20:00:00+0800");
        require(unchanged.at("summary").at("entry_count").as_number() == 2 &&
                    unchanged.at("summary").at("revision_count").as_number() == 0,
                "archive merging must be idempotent");

        actual_for_ping_an["available_from"] = "20260818";
        actual_observation["rows"] = tdx::Json::array();
        actual_observation["rows"].push_back(actual_for_ping_an);
        auto revised = tdx::merge_disclosure_archive_document(
            actual_observation, &unchanged, "2026-08-19T20:00:00+0800");
        require(revised.at("summary").at("revision_count").as_number() == 1 &&
                    revised.at("entries").as_array()[1]
                            .at("report_available_from").as_string() == "20260817",
                "changed dates must retain a revision while preserving first availability");
        const auto revision_repeat = tdx::merge_disclosure_archive_document(
            actual_observation, &revised, "2026-08-20T20:00:00+0800");
        require(revision_repeat.at("summary").at("revision_count").as_number() == 1,
                "repeated later observations must not duplicate date revisions");

        auto announcement_observation = tdx::Json::object();
        announcement_observation["rows"] = announcements.at("rows");
        const auto announcement_archive = tdx::merge_disclosure_archive_document(
            announcement_observation, nullptr, "2026-08-06T12:00:00+0800");
        require(announcement_archive.at("summary").at("entry_count").as_number() == 4 &&
                    announcement_archive.at("summary")
                            .at("report_available_count").as_number() == 4 &&
                    announcement_archive.at("entries").as_array()[2]
                            .at("announcement_evidence").size() == 3,
                "full-report announcement rows must merge into the availability archive");

        auto kline = tdx::Json::parse(
            "{\"bars\":["
            "{\"date\":\"2026-04-24\",\"time\":\"\"},"
            "{\"date\":\"2026-04-25\",\"time\":\"\"},"
            "{\"date\":\"2026-08-01\",\"time\":\"\"},"
            "{\"date\":\"2026-08-02\",\"time\":\"\"}]}" );
        auto reports = tdx::Json::parse(
            "[{\"report_period\":\"20260331\",\"available_from\":\"20260424\","
            "\"finance\":{\"43\":11.5},\"finvalue\":{\"183\":22.5}},"
            "{\"report_period\":\"20260630\",\"available_from\":\"20260801\","
            "\"finance\":{\"43\":33.5},\"finvalue\":{\"183\":44.5}}]" );
        const auto point_in_time = tdx::build_point_in_time_finance_series_document(
            kline, reports, {43}, {0, 183});
        const auto& series = point_in_time.at("series");
        require(series.at("FINANCE#43").size() == 3 &&
                    !series.at("FINANCE#43").as_object().count("2026-04-24|") &&
                    series.at("FINANCE#43").at("2026-04-25|").as_number() == 11.5 &&
                    series.at("FINANCE#43").at("2026-08-01|").as_number() == 11.5 &&
                    series.at("FINANCE#43").at("2026-08-02|").as_number() == 33.5 &&
                    series.at("FINVALUE#0").at("2026-08-02|").as_number() == 20260630 &&
                    series.at("FINVALUE#183").at("2026-08-02|").as_number() == 44.5,
                "point-in-time finance must activate on the next bar and select the latest report period");

        require_disclosure_rejection(
            {"--audit-coverage", "--maintain", "--security", "sz300503"},
            "--audit-coverage and --maintain are mutually exclusive");
        require_disclosure_rejection(
            {"--audit-coverage", "--backfill-announcements", "--security",
             "sz300503"},
            "audit and maintenance modes do not use --backfill-announcements");
        require_disclosure_rejection(
            {"--security", "sz300503"},
            "batch inputs require --backfill-announcements");
        require_disclosure_rejection(
            {"--maintain", "--dry-run", "--security", "sz300503"},
            "audit and maintenance modes cannot use --dry-run");
        require_disclosure_rejection(
            {"--audit-as-of", "20260630"},
            "coverage period options require --audit-coverage or --maintain");
        require_disclosure_rejection(
            {"--latest-periods", "8"},
            "coverage period options require --audit-coverage or --maintain");
        require_disclosure_rejection(
            {"--maintenance-refresh-hours", "12"},
            "--maintenance-refresh-hours requires --maintain");
        require_disclosure_rejection(
            {"--backfill-announcements", "--security", "sz300503"},
            "batch announcement backfill requires --archive or --archive-path");
        require_disclosure_rejection(
            {"--backfill-announcements", "--view", "schedule"},
            "--backfill-announcements requires --view all or announcement");

        std::cout << "disclosure tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "disclosure test failed: " << error.what() << '\n';
        return 1;
    }
}
