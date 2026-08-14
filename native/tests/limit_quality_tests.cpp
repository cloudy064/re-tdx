#include "tdx/limit_quality.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        const auto ranking = tdx::Json::parse(
            "{\"generated_at\":\"2026/8/5 12:00:00\",\"records\":["
            "{\"security_id\":\"SZ000001\",\"code\":\"000001\","
            "\"last_price\":10,\"open_price\":9.5,\"opening_rush\":5.56,"
            "\"seal_amount_yuan\":1000000,\"is_sealed\":true},"
            "{\"security_id\":\"SH600000\",\"code\":\"600000\","
            "\"last_price\":12,\"open_price\":11,\"seal_amount_yuan\":200000}]}" );
        const auto depth = tdx::Json::parse(
            "{\"records\":["
            "{\"security_id\":\"SZ000001\",\"last_price\":10,"
            "\"bid1_amount_yuan\":1000000,"
            "\"buy_levels\":[{\"price\":10,\"volume_hand\":1000}],"
            "\"sell_levels\":[{\"price\":0,\"volume_hand\":0}]},"
            "{\"security_id\":\"SH600000\",\"last_price\":11.8,"
            "\"bid1_amount_yuan\":100000,"
            "\"buy_levels\":[{\"price\":11.7,\"volume_hand\":100}],"
            "\"sell_levels\":[{\"price\":11.8,\"volume_hand\":10}]}]}" );
        const auto stats = tdx::Json::parse(
            "{\"records\":["
            "{\"security_id\":\"SZ000001\","
            "\"stat\":{\"free_float_shares\":1000000},"
            "\"stat2\":{\"stats_date\":\"20260804\","
            "\"seal_amount_yuan\":500000,\"prev_seal_amount_yuan\":200000}},"
            "{\"security_id\":\"SH600000\","
            "\"stat\":{\"free_float_shares\":2000000},"
            "\"stat2\":{\"stats_date\":\"20260805\","
            "\"seal_amount_yuan\":100000,\"prev_seal_amount_yuan\":50000}}]}" );
        const auto auction = tdx::Json::parse(
            "{\"server_trade_date\":\"2026-08-05\",\"records\":["
            "{\"security_id\":\"SZ000001\",\"summary\":{\"opening\":{"
            "\"last_sample_price\":9,\"last_sample_matched_volume_hand\":200,"
            "\"last_sample_matched_amount_yuan\":180000,"
            "\"last_sample_unmatched_signed_hand\":300,"
            "\"last_sample_unmatched_volume_hand\":300,"
            "\"last_sample_unmatched_direction_raw\":1,"
            "\"unmatched_direction_flips\":2}}}]}" );
        const auto document = tdx::compose_limit_quality_document(
            ranking, depth, stats, auction, 10);
        const auto& rows = document.at("records").as_array();
        const auto& first = rows[0].at("quality");
        require(first.at("depth_status").as_string() == "sealed",
                "live depth should confirm a sealed candidate");
        require(std::abs(first.at("seal_free_float_pct").as_number() - 10.0) < 1e-9,
                "seal/free-float ratio is incorrect");
        require(first.at("stats_alignment").as_string() == "previous-resource-day" &&
                    std::abs(first.at("seal_to_previous").as_number() - 2.0) < 1e-9 &&
                    std::abs(first.at("seal_decay_pct").as_number() + 100.0) < 1e-9,
                "previous-resource-day seal strengthening is incorrect");
        require(std::abs(first.at("auction").at("recomputed_opening_rush_pct").as_number() -
                         5.5555555556) < 1e-6,
                "opening-rush recomputation is incorrect");
        const auto& second = rows[1].at("quality");
        require(second.at("depth_status").as_string() == "ranking-stale-or-unsealed",
                "changed order book should not remain marked sealed");
        require(second.at("stats_alignment").as_string() == "same-day" &&
                    second.at("statistics_matches_depth").as_bool(),
                "same-day statistics should compare against current depth");
        require(document.at("summary").at("depth_sealed").as_number() == 1 &&
                    document.at("summary").at("statistics_matched").as_number() == 1,
                "quality summary counts are incorrect");
        std::cout << "limit-quality tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "limit-quality test failed: " << error.what() << '\n';
        return 1;
    }
}
