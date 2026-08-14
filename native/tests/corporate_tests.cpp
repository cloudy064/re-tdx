#include "tdx/corporate.hpp"

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void append_u16(tdx::Bytes& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(tdx::Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void append_f32(tdx::Bytes& output, float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(value));
    append_u32(output, raw);
}

void append_code(tdx::Bytes& output, std::uint8_t market, const char* code) {
    output.push_back(market);
    output.insert(output.end(), code, code + 6);
}

tdx::Bytes bytes_from_hex(const std::string& text) {
    auto digit = [](char value) -> std::uint8_t {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        throw std::runtime_error("invalid hex fixture");
    };
    require(text.size() % 2 == 0, "hex fixture length");
    tdx::Bytes result;
    result.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
        result.push_back(static_cast<std::uint8_t>(
            digit(text[index]) * 16 + digit(text[index + 1])));
    return result;
}

}  // namespace

int main() {
    try {
        tdx::Bytes finance;
        append_u16(finance, 1);
        append_code(finance, 0, "000001");
        append_f32(finance, 1940560.0F);
        append_u16(finance, 18);
        append_u16(finance, 1);
        append_u32(finance, 20260425);
        append_u32(finance, 19910403);
        for (int index = 0; index < 30; ++index) {
            float value = 0.0F;
            if (index == 0) value = 1940591.0F;
            if (index == 6) value = 0.67F;
            if (index == 7) value = 70000000.0F;
            if (index == 26) value = 14523000.0F;
            if (index == 28) value = 20.5F;
            append_f32(finance, value);
        }
        require(finance.size() == 145, "finance fixture is not one 143-byte record");
        const auto finance_doc = tdx::parse_finance_batch_payload(finance);
        const auto& finance_row = finance_doc.at("records").as_array().front();
        require(finance_row.at("security_id").as_string() == "SZ000001",
                "finance security identity");
        require(std::abs(finance_row.at("shares").at("circulating").as_number() -
                         19405600000.0) < 1.0, "finance circulating-share units");
        require(std::abs(finance_row.at("per_share").at("eps").as_number() - 0.67) < 0.0001,
                "finance EPS mapping");
        require(std::abs(finance_row.at("income_statement").at("net_profit_yuan").as_number() -
                         14523000000.0) < 1.0, "finance currency units");
        require(finance_row.at("updated_date").as_string() == "2026-04-25",
                "finance update date");

        tdx::Bytes capital;
        append_u16(capital, 1);
        append_code(capital, 0, "000001");
        append_u16(capital, 2);
        append_code(capital, 0, "000001");
        capital.push_back(0);
        append_u32(capital, 20260612);
        capital.push_back(1);
        append_f32(capital, 0.2F);
        append_f32(capital, 8.5F);
        append_f32(capital, 3.0F);
        append_f32(capital, 1.0F);
        append_code(capital, 0, "000001");
        capital.push_back(0);
        append_u32(capital, 20260511);
        capital.push_back(15);
        append_f32(capital, 0.0F);
        append_f32(capital, 0.0F);
        append_f32(capital, 3.5F);
        append_f32(capital, 0.0F);
        require(capital.size() == 69, "capital fixture size");
        const auto capital_doc = tdx::parse_capital_changes_payload(capital);
        require(capital_doc.at("count").as_number() == 2, "capital event count");
        const auto& dividend = capital_doc.at("records").as_array()[0];
        require(dividend.at("category_name").as_string() == "除权除息",
                "capital category mapping");
        require(std::abs(dividend.at("details").at("dividend_per_share_yuan").as_number() -
                         0.02) < 0.0001, "capital per-share dividend conversion");
        require(std::abs(dividend.at("details").at("dividend_per_10_shares_yuan").as_number() -
                         0.2) < 0.0001, "capital per-ten-share dividend mapping");
        require(capital_doc.at("records").as_array()[1].at("category_name").as_string() ==
                    "重整调整", "capital category 15 mapping");

        // First two encrypted records from the native hq_cache/gbbq layout.  The
        // count is reduced to two so the fixture remains self-contained.
        const auto local_gbbq = bytes_from_hex(
            "02000000"
            "631224B0F311C9A953C0BB1806866A39DCBC5C5514C3E1D2000000803F"
            "631224B0F311C9A9B1E786C097F3E81CA0AE101BB36641E14523909745");
        const auto local_capital = tdx::parse_local_gbbq_capital_changes(
            local_gbbq, {"sz:000001"}, true, "fixture/gbbq");
        require(local_capital.at("source_mode").as_string() == "local" &&
                    local_capital.at("source_record_count").as_number() == 2 &&
                    local_capital.at("event_count").as_number() == 2,
                "local GBBQ source metadata");
        const auto& local_records =
            local_capital.at("blocks").as_array()[0].at("records").as_array();
        require(local_records[0].at("date").as_string() == "1990-03-01" &&
                    local_records[0].at("category").as_number() == 1 &&
                    std::abs(local_records[0].at("details")
                                 .at("rights_price_yuan").as_number() - 3.56) < 0.0001,
                "local GBBQ dividend decryption");
        require(local_records[1].at("date").as_string() == "1991-04-03" &&
                    local_records[1].at("category").as_number() == 5 &&
                    std::abs(local_records[1].at("details")
                                 .at("after_circulating_shares").as_number() -
                             26500000.0) < 1.0,
                "local GBBQ share-capital units");
        require(local_records[0].at("record_hex").as_string().rfind(
                    "0030303030303100", 0) == 0,
                "local GBBQ raw record is the decrypted native record");

        bool bad_gbbq_rejected = false;
        try {
            auto truncated = local_gbbq;
            truncated.pop_back();
            (void)tdx::parse_local_gbbq_capital_changes(
                truncated, {"sz:000001"});
        } catch (const tdx::Error&) { bad_gbbq_rejected = true; }
        require(bad_gbbq_rejected, "truncated local GBBQ must be rejected");

        tdx::Bytes limits;
        append_u16(limits, 1);
        limits.push_back(0);
        append_u32(limits, 10);
        append_f32(limits, 1.66F);
        append_f32(limits, 1.36F);
        const auto limits_doc = tdx::parse_special_limits_payload(limits, 7);
        const auto& limit = limits_doc.at("records").as_array().front();
        require(limit.at("index").as_number() == 7, "special-limit index");
        require(limit.at("security_id").as_string() == "SZ000010",
                "special-limit zero-padded code");
        require(std::abs(limit.at("limit_up_price").as_number() - 1.66) < 0.0001,
                "special-limit upper price");
        const auto exact_limit = tdx::find_special_limit_price(limits_doc, "sz", "000010");
        require(exact_limit && std::abs(exact_limit->upper - 1.66) < 0.0001 &&
                std::abs(exact_limit->lower - 1.36) < 0.0001,
                "special-limit lookup by normalized security identity");
        require(!tdx::find_special_limit_price(limits_doc, "sh", "000010"),
                "special-limit lookup must not cross markets");

        bool rejected = false;
        try {
            auto truncated = finance;
            truncated.pop_back();
            (void)tdx::parse_finance_batch_payload(truncated);
        } catch (const tdx::Error&) { rejected = true; }
        require(rejected, "truncated finance payload must be rejected");

        const auto daily = tdx::Json::parse(
            "{\"bars\":["
            "{\"date\":\"2026-06-10\",\"open\":10,\"high\":10,\"low\":10,\"close\":10},"
            "{\"date\":\"2026-06-11\",\"open\":10,\"high\":10,\"low\":10,\"close\":10},"
            "{\"date\":\"2026-06-12\",\"open\":8,\"high\":8,\"low\":8,\"close\":8}]}");
        const auto target = tdx::Json::parse(
            "{\"bars\":["
            "{\"date\":\"2026-06-11\",\"open\":10,\"high\":10,\"low\":10,\"close\":10},"
            "{\"date\":\"2026-06-12\",\"open\":8,\"high\":8,\"low\":8,\"close\":8}]}");
        const auto adjusted = tdx::apply_kline_adjustment(target, daily, capital_doc, "qfq");
        const auto& adjusted_bars = adjusted.at("bars").as_array();
        // 6 月 12 日事件：每 10 股派 0.2、送转 3、配 1 @ 8.5，
        // 事件因子 = (10 - 0.02 + 0.1*8.5) / (10 * 1.4) = 0.7735714286。
        require(std::abs(adjusted_bars[0].at("close").as_number() - 7.735714286) < 0.00001,
                "qfq adjusts prices before the ex-date");
        require(std::abs(adjusted_bars[1].at("close").as_number() - 8.0) < 0.00001,
                "qfq keeps ex-date and later prices on latest basis");
        const auto back = tdx::apply_kline_adjustment(target, daily, capital_doc, "hfq");
        require(std::abs(back.at("bars").as_array()[0].at("close").as_number() - 10.0) < 0.00001,
                "hfq keeps the earliest basis unchanged");
        require(back.at("bars").as_array()[1].at("close").as_number() > 10.0,
                "hfq raises prices after the corporate action");
        const auto fixed_front = tdx::apply_kline_adjustment(
            target, daily, capital_doc, "fixed_qfq", "2026-06-11");
        require(std::abs(fixed_front.at("bars").as_array()[0].at("close").as_number() -
                         10.0) < 0.00001,
                "fixed qfq keeps the anchor date unchanged");
        require(fixed_front.at("bars").as_array()[1].at("close").as_number() > 10.0,
                "fixed qfq moves later bars onto the anchor basis");
        const auto fixed_back = tdx::apply_kline_adjustment(
            target, daily, capital_doc, "fixed_hfq", "2026-06-12");
        require(std::abs(fixed_back.at("bars").as_array()[1].at("close").as_number() -
                         8.0) < 0.00001,
                "fixed hfq keeps the anchor date unchanged");

        require(tdx::normalize_kline_adjustment_mode("raw") == "none" &&
                    tdx::normalize_kline_adjustment_mode("front") == "qfq" &&
                    tdx::normalize_kline_adjustment_mode("back") == "hfq",
                "adjustment aliases normalize before request execution");
        bool invalid_mode_rejected = false;
        try {
            (void)tdx::normalize_kline_adjustment_mode("mystery");
        } catch (const tdx::Error&) { invalid_mode_rejected = true; }
        require(invalid_mode_rejected, "unknown adjustment mode must be rejected");

        const auto provided = tdx::adjust_security_kline_document(
            adjusted, "sz", "000001", "stock", "qfq");
        require(provided.at("adjustment_mode").as_string() == "qfq" &&
                    provided.at("adjustment").at("input_cache").at("status")
                        .as_string() == "provided-document" &&
                    std::abs(provided.at("bars").as_array()[0].at("close")
                        .as_number() - adjusted_bars[0].at("close").as_number()) < 1e-12,
                "already adjusted input is accepted idempotently without another factor");
        const auto fixed_provided = tdx::adjust_security_kline_document(
            fixed_front, "sz", "000001", "stock", "fixed_qfq",
            "20260611");
        require(fixed_provided.at("adjustment").at("input_cache").at("status")
                    .as_string() == "provided-document",
                "fixed adjustment accepts the same normalized anchor idempotently");
        bool fixed_anchor_mismatch_rejected = false;
        try {
            (void)tdx::adjust_security_kline_document(
                fixed_front, "sz", "000001", "stock", "fixed_qfq",
                "2026-06-12");
        } catch (const tdx::Error&) { fixed_anchor_mismatch_rejected = true; }
        require(fixed_anchor_mismatch_rejected,
                "fixed adjustment must reject a different anchor on supplied data");
        bool double_adjustment_rejected = false;
        try {
            (void)tdx::adjust_security_kline_document(
                adjusted, "sz", "000001", "stock", "hfq");
        } catch (const tdx::Error&) { double_adjustment_rejected = true; }
        require(double_adjustment_rejected,
                "a differently adjusted input must not be adjusted twice");

        auto second_provided = provided;
        second_provided["code"] = "600000";
        require(tdx::uniform_kline_adjustment_mode(
                    {provided, second_provided}) == "qfq",
                "multi-security adjustment identity must be uniform");
        auto raw = target;
        raw["adjustment_mode"] = "none";
        bool mixed_rejected = false;
        try {
            (void)tdx::uniform_kline_adjustment_mode({provided, raw});
        } catch (const tdx::Error&) { mixed_rejected = true; }
        require(mixed_rejected,
                "mixed raw and adjusted multi-security inputs must be rejected");
        const auto summary = tdx::summarize_kline_adjustments(
            {provided, second_provided}, "qfq");
        require(summary.at("security_scope").as_string() == "per-security" &&
                    summary.at("input_cache_counts").at("provided-document")
                        .as_number() == 2 &&
                    summary.at("shared_cache").at("maximum_entries")
                        .as_number() == 512 &&
                    summary.at("shared_cache").at("capacity_bypasses")
                        .as_number() == 0,
                "adjustment summary exposes per-security provenance and bounded cache state");

        std::cout << "corporate tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "corporate test failed: " << error.what() << '\n';
        return 1;
    }
}
