#include "tdx/announcement_signals.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000921"}] = {0, "SZ", "深圳", "000921", "海信家电"};

        auto raw = tdx::Json::array();
        auto row = tdx::Json::object();
        row["$ZQDM"] = "000921";
        row["$SC"] = "0";
        row["date"] = "20260807";
        row["title"] = "2025年度A股权益分派实施公告TXT:http://example.test/a.PDF";
        row["dk"] = "利好";
        row["zf1"] = "-2.04";
        row["zf2"] = "5.54";
        row["gglx"] = "权益分派预案及实施";
        raw.push_back(row);
        auto selected = tdx::normalize_announcement_signal_rows(raw, "selected", securities);
        require(selected.size() == 1, "selected row count");
        const auto& signal = selected.as_array().front();
        require(signal.at("security").at("name").as_string() == "海信家电",
                "security name resolved");
        require(signal.at("date").as_string() == "2026-08-07", "date normalized");
        require(signal.at("title").as_string() == "2025年度A股权益分派实施公告",
                "TXT suffix removed from title");
        require(signal.at("pdf_url").as_string() == "http://example.test/a.PDF",
                "PDF URL extracted");
        require(signal.at("direction").as_string() == "bullish", "direction normalized");
        require(std::abs(signal.at("recent_3d_return_pct").as_number() + 2.04) < 1e-9,
                "main 3-day return normalized");
        require(signal.at("pre_3d_return_pct").is_null(),
                "main return not mislabeled as pre-event return");

        auto history_raw = tdx::Json::array();
        auto history_row = tdx::Json::object();
        history_row["date"] = "20260722";
        history_row["title"] = "历史公告TXT:http://example.test/b.PDF";
        history_row["DK"] = "利空";
        history_row["zf1"] = "1.908099688473520200";
        history_row["zf2"] = "";
        history_row["gglx"] = "风险提示";
        history_raw.push_back(history_row);
        auto history = tdx::normalize_announcement_history_rows(
            history_raw, 0, "000921", securities);
        require(history.size() == 1, "history row count");
        const auto& event = history.as_array().front();
        require(event.at("direction").as_string() == "bearish",
                "uppercase DK normalized");
        require(std::abs(event.at("pre_3d_return_pct").as_number() -
                         1.9080996884735202) < 1e-9,
                "history pre-event return normalized");
        require(event.at("post_3d_return_pct").is_null(),
                "blank post-event return retained as null");
        require(event.at("recent_3d_return_pct").is_null(),
                "history return not mislabeled as recent return");

        selected.push_back(event);
        tdx::sort_announcement_signal_rows(selected, "date", "desc");
        require(selected.as_array().front().at("record_kind").as_string() == "selected",
                "date sort descending");

        bool rejected = false;
        try { tdx::sort_announcement_signal_rows(selected, "bogus", "desc"); }
        catch (const tdx::Error&) { rejected = true; }
        require(rejected, "unknown sort rejected");

        std::cout << "announcement-signals tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "announcement-signals test failed: " << error.what() << '\n';
        return 1;
    }
}
