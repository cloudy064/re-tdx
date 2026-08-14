#include "formula_context_professional_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/professional_data.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key,
                    std::string fallback = {}) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string()
                                       : std::move(fallback);
}

int formula_market_id(std::string market) {
    market = lower_ascii(std::move(market));
    if (market == "sz" || market == "0") return 0;
    if (market == "sh" || market == "1") return 1;
    if (market == "bj" || market == "2") return 2;
    try {
        std::size_t used = 0;
        const int value = std::stoi(market, &used);
        return used == market.size() ? value : -1;
    } catch (...) {
        return -1;
    }
}

void bind_formula_scalar(Json& context, const std::string& name,
                         const std::optional<double>& value) {
    if (!context.as_object().count("formula_scalar_bindings"))
        context["formula_scalar_bindings"] = Json::object();
    context["formula_scalar_bindings"][name] =
        value ? Json(*value) : Json(nullptr);
}
std::vector<std::uint32_t> kline_dates(const Json& document) {
    const auto* rows = optional(document, "bars");
    if (!rows || !rows->is_array()) throw Error("professional formula context needs K-line bars");
    std::vector<std::uint32_t> result; result.reserve(rows->size());
    for (const auto& row : rows->as_array()) {
        const auto text = text_or(row, "date");
        std::string digits;
        for (const char ch : text) if (ch >= '0' && ch <= '9') digits.push_back(ch);
        if (digits.size() != 8) throw Error("professional formula context found an invalid K-line date");
        result.push_back(static_cast<std::uint32_t>(std::stoul(digits)));
    }
    return result;
}

void bind_professional_series(Json& context, const Json& target,
                              const std::string& market, const std::string& code,
                              const std::vector<TradingBinding>& stock,
                              const std::vector<TradingBinding>& aggregate,
                              int timeout_ms) {
    if (stock.empty() && aggregate.empty()) return;
    const auto dates = kline_dates(target);
    std::vector<ProfessionalTradingRecord> stock_records, market_records;
    if (!stock.empty())
        stock_records = fetch_professional_stock_trading_data(market, code, {}, timeout_ms, false);
    if (!aggregate.empty())
        market_records = fetch_professional_market_trading_data({}, timeout_ms, false);
    Json* series = nullptr;
    const auto found = context.as_object().find("series");
    if (found == context.as_object().end()) context["series"] = Json::object();
    series = &context["series"];
    const auto bind = [&](const TradingBinding& binding,
                          const std::vector<ProfessionalTradingRecord>& records) {
        const auto values = professional_trading_series(records, binding.id, binding.field,
                                                        binding.type, dates);
        Json points = Json::object();
        const auto& rows = target.at("bars").as_array();
        for (std::size_t index = 0; index < rows.size(); ++index) {
            if (!values[index]) continue;
            const auto date = text_or(rows[index], "date"), time = text_or(rows[index], "time");
            points[date + "|" + time] = *values[index];
        }
        (*series)[binding.name] = std::move(points);
    };
    for (const auto& binding : stock) bind(binding, stock_records);
    for (const auto& binding : aggregate) bind(binding, market_records);
}

}  // namespace

void bind_professional_series_context(
    Json& context, const Json& target,
    const std::string& market, const std::string& code,
    const std::vector<TradingBinding>& stock,
    const std::vector<TradingBinding>& board,
    const std::vector<TradingBinding>& aggregate,
    const std::set<int>& finance_bindings,
    const Json& finance_values,
    bool point_in_time_finance,
    int security_type,
    int timeout_ms) {
    bind_professional_series(
        context, target, market, code, stock, aggregate, timeout_ms);
    if (!board.empty()) {
        if (!context.as_object().count("series"))
            context["series"] = Json::object();
        if (security_type == 0) {
            try {
                const auto records = fetch_professional_stock_trading_data(
                    market, code, {}, timeout_ms, false);
                const auto dates = kline_dates(target);
                const auto& rows = target.at("bars").as_array();
                for (const auto& binding : board) {
                    const auto values = professional_trading_series(
                        records, binding.id, binding.field, binding.type, dates);
                    Json points = Json::object();
                    for (std::size_t index = 0; index < rows.size(); ++index) {
                        if (!values[index]) continue;
                        points[text_or(rows[index], "date") + "|" +
                               text_or(rows[index], "time")] = *values[index];
                    }
                    context["series"][binding.name] = std::move(points);
                }
            } catch (...) {
                for (const auto& binding : board)
                    context["series"][binding.name] = Json::object();
            }
        } else {
            for (const auto& binding : board)
                context["series"][binding.name] = Json::object();
        }
    }
    for (const int id : finance_bindings) {
        if (point_in_time_finance) continue;
        const auto key = std::to_string(id);
        if (security_type != 0 && finance_values.as_object().count(key))
            continue;
        if (!context.as_object().count("series"))
            context["series"] = Json::object();
        context["series"]["FINVALUE#" + key] = Json::object();
    }
}

void bind_professional_one_points(
    Json& context, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const std::vector<OnePointBinding>& finance,
    const std::vector<OnePointBinding>& stock,
    const std::vector<OnePointBinding>& board,
    const std::vector<OnePointBinding>& aggregate,
    const std::vector<OnePointBinding>& local,
    int timeout_ms,
    const std::optional<ProfessionalBoardTarget>& board_target) {
    if (!finance.empty()) {
        std::vector<int> fields;
        for (const auto& binding : finance) fields.push_back(binding.id);
        std::sort(fields.begin(), fields.end());
        fields.erase(std::unique(fields.begin(), fields.end()), fields.end());
        const int market_id = formula_market_id(market);
        const auto cache = root / "T0002" / "tdx-tool" /
                           "professional-finance-cache";
        using FinancePoints = std::map<
            int, std::vector<std::pair<std::uint32_t, std::optional<double>>>>;
        const auto append_document = [](const Json& document, FinancePoints& values) {
          if (const auto* periods = optional(document, "periods"); periods && periods->is_array()) {
            for (const auto& period : periods->as_array()) {
                const auto* date = optional(period, "report_date_raw");
                const auto* rows = optional(period, "fields");
                if (!date || !date->is_number() || !rows || !rows->is_array()) continue;
                for (const auto& row : rows->as_array()) {
                    const auto* id = optional(row, "id");
                    const auto* value = optional(row, "value");
                    if (!id || !id->is_number() || !value) continue;
                    values[static_cast<int>(id->as_number())].push_back({
                        static_cast<std::uint32_t>(date->as_number()),
                        value->is_number() ? std::optional<double>(value->as_number()) :
                                             std::nullopt});
                }
            }
          }
        };
        FinancePoints explicit_values, relative_values;
        bool needs_explicit = false, needs_relative = false;
        std::uint32_t explicit_from = 99999999, explicit_to = 0;
        std::size_t relative_periods = 1;
        for (const auto& binding : finance) {
            if (binding.year > 0 && binding.mmdd > 0) {
                needs_explicit = true;
                int year = binding.year;
                if (year < 1900) year += year <= 90 ? 2000 : 1900;
                const int date = year * 10000 + binding.mmdd;
                const int mmdd = date % 10000;
                const int lower = mmdd < 331 ? (year - 1) * 10000 + 1231 :
                                  mmdd < 630 ? year * 10000 + 331 :
                                  mmdd < 930 ? year * 10000 + 630 :
                                  mmdd < 1231 ? year * 10000 + 930 :
                                  year * 10000 + 1231;
                explicit_from = std::min(explicit_from,
                    static_cast<std::uint32_t>(lower));
                explicit_to = std::max(explicit_to,
                    static_cast<std::uint32_t>(date));
            } else {
                needs_relative = true;
                std::size_t wanted = 1;
                if (binding.year > 0)
                    wanted = static_cast<std::size_t>(binding.year) * 4 + 1;
                else if (binding.mmdd > 0 && binding.mmdd <= 300)
                    wanted = static_cast<std::size_t>(binding.mmdd) + 1;
                else if (binding.mmdd > 300)
                    wanted = 5;
                relative_periods = std::max(relative_periods,
                    std::min<std::size_t>(80, wanted));
            }
        }
        std::uint64_t loaded_periods = 0;
        if (needs_explicit) {
            const auto document = fetch_professional_finance_series_document(
                market_id, code, fields, explicit_from, explicit_to, 80,
                cache, timeout_ms, false);
            append_document(document, explicit_values);
            loaded_periods += static_cast<std::uint64_t>(
                document.at("period_count").as_number());
        }
        if (needs_relative) {
            const auto document = fetch_professional_finance_series_document(
                market_id, code, fields, 0, 99999999, relative_periods,
                cache, timeout_ms, false);
            append_document(document, relative_values);
            loaded_periods += static_cast<std::uint64_t>(
                document.at("period_count").as_number());
        }
        for (const auto& binding : finance) {
            const auto& values = binding.year > 0 && binding.mmdd > 0
                ? explicit_values : relative_values;
            const auto found = values.find(binding.id);
            const std::vector<std::pair<std::uint32_t, std::optional<double>>> empty;
            bind_formula_scalar(context, binding.name,
                professional_finance_one(found == values.end() ? empty : found->second,
                                         binding.year, binding.mmdd));
        }
        context["finone_mode"] =
            "tcalc-type172-quarter-year-mmdd-single-point-official-gpcw";
        context["finone_period_count"] = loaded_periods;
        context["finone_relative_period_limit"] =
            static_cast<std::uint64_t>(relative_periods);
        context["finone_cache"] = path_utf8(cache);
    }

    const auto bind_trading = [&](const std::vector<OnePointBinding>& bindings,
                                  const std::vector<ProfessionalTradingRecord>& records) {
        for (const auto& binding : bindings)
            bind_formula_scalar(context, binding.name,
                professional_trading_one(records, binding.id, binding.field,
                                         binding.year, binding.mmdd));
    };
    if (!stock.empty()) {
        const auto records = fetch_professional_stock_trading_data(
            market, code, {}, timeout_ms, false);
        bind_trading(stock, records);
        context["gpjyone_mode"] =
            "tdxw-type175-exact-date-or-reverse-ordinal-official-tdxgp";
        context["gpjyone_record_count"] = static_cast<std::uint64_t>(records.size());
    }
    if (!aggregate.empty()) {
        const auto records = fetch_professional_market_trading_data({}, timeout_ms, false);
        bind_trading(aggregate, records);
        context["scjyone_mode"] =
            "tdxw-type175-sh999999-exact-date-or-reverse-ordinal-official-tdxgp";
        context["scjyone_record_count"] = static_cast<std::uint64_t>(records.size());
    }
    if (!board.empty()) {
        if (!board_target)
            throw Error("BKJYONE requires a resolved board target");
        const auto records = fetch_professional_stock_trading_data(
            board_target->market, board_target->code, {}, timeout_ms, false);
        bind_trading(board, records);
        context["bkjyone_mode"] = board_target->mode +
            ";tdxw-type175-exact-date-or-reverse-ordinal-official-tdxgp";
        context["bkjyone_target_market"] = board_target->market;
        context["bkjyone_target_code"] = board_target->code;
        context["bkjyone_record_count"] = static_cast<std::uint64_t>(records.size());
    }

    if (!local.empty()) {
        const int market_id = formula_market_id(market);
        const std::string prefix = market_id == 0 ? "sz" :
                                   market_id == 1 ? "sh" :
                                   market_id == 2 ? "bj" : std::string{};
        const auto path = root / "T0002" / "hq_cache" /
                          ("gp" + prefix + "one.dat");
        Bytes bytes;
        if (!prefix.empty() && std::filesystem::is_regular_file(path))
            bytes = read_bytes(path);
        std::uint32_t numeric_code = 0;
        try { numeric_code = static_cast<std::uint32_t>(std::stoul(code)); }
        catch (...) { /* Host lookup cannot match a non-numeric six-character code. */ }
        std::size_t matched = 0;
        for (const auto& binding : local) {
            std::optional<double> value = 0.0;
            for (std::size_t offset = 0; offset + 10 <= bytes.size(); offset += 10) {
                const auto field = static_cast<std::int16_t>(read_u16_le(bytes.data() + offset + 4));
                if (read_u32_le(bytes.data() + offset) != numeric_code ||
                    field != binding.id) continue;
                value = static_cast<double>(read_f32_le(bytes.data() + offset + 6));
                ++matched;
                break;
            }
            // TdxW type 170 zero-initializes the destination and returns
            // success even when the local file or matching record is absent.
            bind_formula_scalar(context, binding.name, value);
        }
        context["gponedat_mode"] =
            "tdxw-type170-local-10-byte-record-zero-when-absent";
        context["gponedat_source"] = path_utf8(path);
        context["gponedat_source_exists"] = !bytes.empty();
        context["gponedat_record_count"] =
            static_cast<std::uint64_t>(bytes.size() / 10);
        context["gponedat_match_count"] = static_cast<std::uint64_t>(matched);
    }
}

}  // namespace tdx::formula_context_detail
