#include "formula_context_aggregate_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/market.hpp"
#include "tdx/session_audit.hpp"

#include <cmath>
#include <cstdint>
#include <map>

namespace tdx::formula_context_detail {

void bind_horcalc(Json& context, const std::filesystem::path& root,
                  const BlockData& data, const Json& target,
                  const std::vector<HorcalcBinding>& bindings,
                  int timeout_ms) {
    if (bindings.empty()) return;
    const auto period = lower_ascii(aggregate_text_or(target, "period", "day"));
    if (period != "day")
        throw Error("HORCALC automatic context currently requires daily K-line bars");
    const auto* target_rows = aggregate_optional(target, "bars");
    if (!target_rows || !target_rows->is_array() || target_rows->as_array().empty())
        throw Error("HORCALC requires dated daily K-line bars");

    struct TargetBar {
        int date{};
        std::string key;
        float open{};
        float high{};
        float low{};
        float close{};
        float amount{};
        float volume{};
    };
    std::vector<TargetBar> target_bars;
    target_bars.reserve(target_rows->as_array().size());
    const auto target_number = [&](const Json& row, std::string_view name) {
        const auto* value = aggregate_optional(row, name);
        return static_cast<float>(
            value && value->is_number() ? value->as_number() : 0.0);
    };
    for (const auto& row : target_rows->as_array()) {
        const auto date_text = aggregate_compact_date(
            aggregate_text_or(row, "date"));
        if (date_text.empty()) throw Error("HORCALC requires valid bar dates");
        const auto display_date = aggregate_text_or(row, "date");
        const auto time = aggregate_text_or(row, "time");
        target_bars.push_back(TargetBar{
            std::stoi(date_text), display_date + "|" + time,
            target_number(row, "open"), target_number(row, "high"),
            target_number(row, "low"), target_number(row, "close"),
            target_number(row, "amount"), target_number(row, "volume"),
        });
    }

    const auto catalog = load_aggregate_universe_catalog(root);
    std::vector<AggregateUniverseResolution> resolutions;
    resolutions.reserve(bindings.size());
    for (const auto& binding : bindings) {
        resolutions.push_back(resolve_aggregate_universe(
            root, data, catalog, binding.block_name,
            AggregateMemberPolicy::horcalc));
    }
    std::uint64_t member_file_count = 0;
    auto daily = load_aggregate_daily(root, resolutions, member_file_count);

    bool needs_finance = false;
    bool needs_quote = false;
    for (const auto& binding : bindings) {
        needs_finance = needs_finance ||
            (binding.calculation == 2 && binding.weight != 2);
        needs_quote = needs_quote ||
            (binding.calculation == 2 && binding.weight >= 3);
    }
    std::vector<std::string> securities;
    securities.reserve(daily.size());
    for (const auto& [key, bars] : daily) {
        (void)bars;
        securities.push_back(aggregate_market_name(key.first) + ":" + key.second);
    }
    struct MemberMetadata {
        float circulating{};
        float total{};
        float price{};
    };
    std::map<AggregateSecurityKey, MemberMetadata> metadata;
    for (const auto& [key, bars] : daily)
        metadata[key].price = static_cast<float>(bars.back().close);

    std::uint64_t finance_record_count = 0;
    if (needs_finance && !securities.empty()) {
        const auto finance = fetch_finance_document(
            securities, load_public_quote_endpoints(root).endpoints,
            timeout_ms, 80, false);
        for (const auto& record : finance.at("records").as_array()) {
            const AggregateSecurityKey key{
                aggregate_integer_or(record, "market_id", -1),
                aggregate_text_or(record, "code")};
            const auto found = metadata.find(key);
            if (found == metadata.end()) continue;
            if (const auto* value = aggregate_nested(
                    record, {"shares", "circulating"});
                value && value->is_number()) {
                found->second.circulating =
                    static_cast<float>(value->as_number());
            }
            if (const auto* value = aggregate_nested(
                    record, {"shares", "total"});
                value && value->is_number()) {
                found->second.total = static_cast<float>(value->as_number());
            }
            ++finance_record_count;
        }
    }
    std::uint64_t quote_record_count = 0;
    if (needs_quote && !securities.empty()) {
        const auto quotes = fetch_market_snapshot_document(
            root, securities, timeout_ms, &data);
        for (const auto& record : quotes.at("records").as_array()) {
            const AggregateSecurityKey key{
                aggregate_integer_or(record, "market_id", -1),
                aggregate_text_or(record, "code")};
            const auto found = metadata.find(key);
            if (found == metadata.end()) continue;
            if (const auto* value = aggregate_optional(record, "last_price");
                value && value->is_number() && value->as_number() > 0.00001) {
                found->second.price = static_cast<float>(value->as_number());
            }
            ++quote_record_count;
        }
    }

    const auto raw_target_series = [&](int item) {
        std::vector<float> values(target_bars.size(), 0.0f);
        for (std::size_t index = 0; index < target_bars.size(); ++index) {
            const auto& bar = target_bars[index];
            switch (item) {
                case 100: values[index] = bar.high; break;
                case 101: values[index] = bar.open; break;
                case 102: values[index] = bar.low; break;
                case 103: values[index] = bar.close; break;
                case 104: values[index] = bar.volume; break;
                case 105:
                    if (index && target_bars[index - 1].close > 0.00001f)
                        values[index] = static_cast<float>(
                            bar.close / target_bars[index - 1].close - 1.0f);
                    else if (index)
                        values[index] = values[index - 1];
                    break;
                case 106: values[index] = bar.amount; break;
                default: break;
            }
        }
        return values;
    };
    const auto aligned_member_series = [&](const std::vector<DailyBar>& bars,
                                            int item) {
        std::vector<float> values(target_bars.size(), 0.0f);
        std::size_t source = 0;
        float previous = 0.0f;
        for (std::size_t index = 0; index < target_bars.size(); ++index) {
            while (source < bars.size() && bars[source].date < target_bars[index].date)
                ++source;
            float value = previous;
            if (source < bars.size() && bars[source].date == target_bars[index].date) {
                const auto& bar = bars[source];
                switch (item) {
                    case 100: value = static_cast<float>(bar.high); break;
                    case 101: value = static_cast<float>(bar.open); break;
                    case 102: value = static_cast<float>(bar.low); break;
                    case 103: value = static_cast<float>(bar.close); break;
                    case 104: value = static_cast<float>(bar.volume); break;
                    case 105:
                        if (source && bars[source - 1].close > 0.00001)
                            value = static_cast<float>(
                                bar.close / bars[source - 1].close - 1.0);
                        break;
                    case 106: value = static_cast<float>(bar.amount); break;
                    default: break;
                }
                if (item != 105 && value < 0.00001f) value = previous;
                ++source;
            }
            values[index] = value;
            previous = value;
        }
        return values;
    };

    if (!context.as_object().count("series")) context["series"] = Json::object();
    Json documents = Json::array();
    for (std::size_t resolution_index = 0;
         resolution_index < resolutions.size(); ++resolution_index) {
        const auto& binding = bindings[resolution_index];
        const auto& resolution = resolutions[resolution_index];
        std::vector<float> output(target_bars.size(), 0.0f);
        std::vector<float> denominator(target_bars.size(), 0.0f);
        const auto selected = raw_target_series(binding.item);
        std::uint64_t used_members = 0;
        for (const auto& key : resolution.members) {
            const auto bars = daily.find(key);
            if (bars == daily.end() || bars->second.empty() ||
                bars->second.back().close < 0.00001)
                continue;
            const auto member = aligned_member_series(bars->second, binding.item);
            ++used_members;
            if (binding.calculation == 0) {
                for (std::size_t index = 0; index < output.size(); ++index)
                    output[index] = static_cast<float>(output[index] + member[index]);
            } else if (binding.calculation == 1) {
                for (std::size_t index = 0; index < output.size(); ++index) {
                    if (std::fabs(output[index]) < 0.00001f) output[index] = 1.0f;
                    const auto tolerance =
                        std::fabs(member[index]) * 0.0000001f + 0.00001f;
                    if (selected[index] <= member[index] - tolerance)
                        output[index] = static_cast<float>(output[index] + 1.0f);
                }
            } else {
                const auto found = metadata.find(key);
                const MemberMetadata empty{};
                const auto& values = found == metadata.end() ? empty : found->second;
                float weight = 1.0f;
                if (binding.weight == 0) weight = values.total;
                else if (binding.weight == 1) weight = values.circulating;
                else if (binding.weight == 3)
                    weight = static_cast<float>(values.price * values.circulating);
                else if (binding.weight == 4)
                    weight = static_cast<float>(values.price * values.total);
                for (std::size_t index = 0; index < output.size(); ++index) {
                    if (std::fabs(member[index]) > 0.00001f)
                        denominator[index] = static_cast<float>(
                            denominator[index] + weight);
                    output[index] = static_cast<float>(
                        output[index] + weight * member[index]);
                }
            }
        }
        if (binding.calculation == 2) {
            for (std::size_t index = 0; index < output.size(); ++index)
                if (denominator[index] > 0.00001f)
                    output[index] = static_cast<float>(
                        output[index] / denominator[index]);
        }
        Json points = Json::object();
        for (std::size_t index = 0; index < output.size(); ++index)
            points[target_bars[index].key] = static_cast<double>(output[index]);
        context["series"][binding.name] = std::move(points);

        Json item = Json::object();
        item["binding"] = binding.name;
        item["requested_name"] = binding.block_name;
        item["lookup_name"] = resolution.lookup_name;
        item["found"] = resolution.found;
        item["family"] = resolution.found ? Json(resolution.family) : Json(nullptr);
        item["block_code"] = resolution.found
            ? Json(resolution.block_code) : Json(nullptr);
        item["source"] = resolution.found ? Json(resolution.source) : Json(nullptr);
        item["item"] = binding.item;
        item["calculation"] = binding.calculation;
        item["weight"] = binding.weight;
        item["member_count"] =
            static_cast<std::uint64_t>(resolution.members.size());
        item["member_series_count"] = used_members;
        documents.push_back(std::move(item));
    }
    context["horcalc_resolutions"] = std::move(documents);
    context["horcalc_binding_count"] =
        static_cast<std::uint64_t>(bindings.size());
    context["horcalc_member_file_count"] = member_file_count;
    context["horcalc_member_series_count"] =
        static_cast<std::uint64_t>(daily.size());
    context["horcalc_finance_record_count"] = finance_record_count;
    context["horcalc_quote_record_count"] = quote_record_count;
    context["horcalc_industry_mode"] = catalog.industry_mode;
    context["horcalc_custom_catalog_source"] = catalog.custom.catalog_source;
    context["horcalc_mode"] =
        "tcalc-opcode1245-command8-type7-local-day-date-aligned-horizontal-aggregate";
}

}  // namespace tdx::formula_context_detail
