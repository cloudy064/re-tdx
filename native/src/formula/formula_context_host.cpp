#include "formula_context_host_internal.hpp"

#include "formula_context_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/external_series.hpp"
#include "tdx/external_signals.hpp"
#include "tdx/finance_eligibility.hpp"
#include "tdx/local_signals.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_status.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::formula_context_detail {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key, std::string fallback = {}) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string() : std::move(fallback);
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

} // namespace

int formula_security_type(const std::string& market_text, const std::string& code,
                  const Json* kline_document) {
    if (kline_document) {
        if (const auto* index = optional(*kline_document, "index_mode");
            index && index->is_bool() && index->as_bool()) return 0;
    }
    auto market = lower_ascii(market_text);
    if (market == "2") market = "bj";
    else if (market == "1") market = "sh";
    else if (market == "0") market = "sz";
    if (market == "bj") return 2;
    if (market == "sz" && (starts_with(code, "300") || starts_with(code, "301"))) return 3;
    if (market == "sh" && (starts_with(code, "688") || starts_with(code, "689"))) return 4;
    if ((market == "sz" && starts_with(code, "200")) ||
        (market == "sh" && starts_with(code, "900"))) return 5;
    if ((market == "sh" && (starts_with(code, "01") || starts_with(code, "10") ||
                             starts_with(code, "11") || starts_with(code, "12"))) ||
        (market == "sz" && (starts_with(code, "10") || starts_with(code, "11") ||
                             starts_with(code, "12")))) return 6;
    if ((market == "sh" && starts_with(code, "5")) ||
        (market == "sz" && (starts_with(code, "15") || starts_with(code, "16") ||
                             starts_with(code, "18")))) return 7;
    if ((market == "sh" && starts_with(code, "6")) ||
        (market == "sz" && (starts_with(code, "0") || starts_with(code, "3")))) return 1;
    return market == "sh" || market == "sz" ? 9 : 10;
}

namespace {

std::string compact_date(const std::string& value) {
    std::string result;
    for (const unsigned char ch : value)
        if (std::isdigit(ch)) result.push_back(static_cast<char>(ch));
    return result.size() >= 8 ? result.substr(0, 8) : std::string{};
}

int local_machine_date() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 +
           local.tm_mday;
}

int latest_kline_date(const Json& kline_document) {
    const auto* bars = optional(kline_document, "bars");
    if (!bars || !bars->is_array())
        throw Error("formula host-calendar context requires dated K-line bars");
    std::string latest;
    for (const auto& bar : bars->as_array())
        latest = std::max(latest, compact_date(text_or(bar, "date")));
    if (latest.empty())
        throw Error("formula host-calendar context requires dated K-line bars");
    return std::stoi(latest);
}

struct LocalDaySnapshot {
    std::filesystem::path path;
    bool exists = false;
    std::uint64_t record_count = 0;
    int last_date = 0;
    bool synthetic_current = false;
    std::uint64_t effective_count = 0;
};

LocalDaySnapshot local_day_snapshot(const std::filesystem::path& root,
                                    int market_id, const std::string& code,
                                    int current_trading_date) {
    LocalDaySnapshot result;
    if (market_id >= 0 && market_id <= 2) {
        const std::string prefix = market_id == 0 ? "sz" : market_id == 1 ? "sh" : "bj";
        result.path = root / "vipdoc" / prefix / "lday" /
                      (prefix + code + ".day");
    } else {
        result.path = root / "vipdoc" / "ds" / "lday" /
                      (std::to_string(market_id) + "#" + code + ".day");
    }
    result.exists = std::filesystem::is_regular_file(result.path);
    if (result.exists) {
        const auto bytes = read_bytes(result.path);
        result.record_count = static_cast<std::uint64_t>(bytes.size() / 32);
        if (result.record_count) {
            const auto offset = static_cast<std::size_t>(result.record_count - 1) * 32;
            result.last_date = static_cast<int>(read_u32_le(bytes.data() + offset));
        }
    }
    result.synthetic_current = market_id <= 2 &&
                               result.last_date < current_trading_date;
    result.effective_count = result.record_count +
                             (result.synthetic_current ? 1u : 0u);
    return result;
}

} // namespace

void bind_host_formula_context(
    Json& context, Json& symbols, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const FormulaContextPlan& plan, int status_market_id,
    int formula_security_type_value, const Json* kline_document,
    const BlockData* block_data, int timeout_ms) {
    (void)market;
    (void)formula_security_type_value;
    const auto& dependencies = plan.dependencies;
    const auto& external_series_bindings = plan.external_series_bindings;
    const auto& local_signal_bindings = plan.local_signal_bindings;
    const bool contract_multiplier_context = plan.contract_multiplier_context;
    const bool host_calendar_context = plan.host_calendar_context;
    const bool security_status_context = plan.security_status_context;
    std::optional<ExpansionInstrument> expansion_instrument;
    if (status_market_id > 2 &&
        (dependencies.count("ISQHQQCODE") || contract_multiplier_context))
        expansion_instrument = cached_expansion_security_record(
            status_market_id, code, timeout_ms);
    if (contract_multiplier_context) {
        int value = 0;
        if (expansion_instrument &&
            tcalc_is_futures_or_option_category(expansion_instrument->category)) {
            const auto low_word = static_cast<std::uint16_t>(
                expansion_instrument->contract_multiplier & 0xFFFFu);
            value = low_word <= 0x7FFFu
                ? static_cast<int>(low_word)
                : static_cast<int>(low_word) - 0x10000;
        }
        symbols["MULTIPLIER"] = static_cast<double>(value);
        context["contract_multiplier"] = value;
        context["contract_multiplier_mode"] =
            "tcalc-opcode1252-type105-offset42-signed-int16-broadcast";
        if (expansion_instrument) {
            context["contract_multiplier_raw"] = static_cast<std::uint64_t>(
                expansion_instrument->contract_multiplier);
            context["contract_multiplier_category"] = static_cast<std::uint64_t>(
                expansion_instrument->category);
            context["contract_multiplier_source"] =
                tcalc_is_futures_or_option_category(expansion_instrument->category)
                    ? "tdx-7727-0x23f5-offset56-u32-low-word"
                    : "tcalc-type105-zero-for-non-derivative-category";
        } else {
            context["contract_multiplier_raw"] = Json(nullptr);
            context["contract_multiplier_category"] = Json(nullptr);
            context["contract_multiplier_source"] =
                "tcalc-type105-zero-for-standard-equity-market";
        }
    }
    if (host_calendar_context) {
        const int current_trading_date = latest_kline_date(*kline_document);
        const int machine_date = local_machine_date();
        Json values = Json::object();
        if (dependencies.count("ISJYDATE")) {
            const double value = current_trading_date == machine_date ? 1.0 : 0.0;
            symbols["ISJYDATE"] = value;
            values["ISJYDATE"] = value;
        }
        if (dependencies.count("LOCALDAYNUM")) {
            const auto snapshot = local_day_snapshot(
                root, status_market_id, code, current_trading_date);
            const double value = static_cast<double>(snapshot.effective_count);
            symbols["LOCALDAYNUM"] = value;
            values["LOCALDAYNUM"] = value;
            context["host_calendar_local_day_file"] = path_utf8(snapshot.path);
            context["host_calendar_local_day_file_exists"] = snapshot.exists;
            context["host_calendar_local_day_record_count"] = snapshot.record_count;
            context["host_calendar_local_day_last_date"] = snapshot.last_date;
            context["host_calendar_local_day_synthetic_current"] =
                snapshot.synthetic_current;
            context["host_calendar_local_day_effective_count"] =
                snapshot.effective_count;
        }
        context["host_calendar_mode"] =
            "tdxw-types122-168-exact-host-field-reconstruction";
        context["host_calendar_values"] = std::move(values);
        context["host_calendar_machine_date"] = machine_date;
        context["host_calendar_current_trading_date"] = current_trading_date;
        context["host_calendar_current_trading_date_source"] =
            "latest-request-kline-date-proxy-for-tdxw-global-trading-date";
    }
    if (dependencies.count("EXTERNVALUE") || dependencies.count("EXTERNSTR")) {
        const auto catalog = load_external_signal_catalog(root);
        Json values = Json::object();
        Json texts = Json::object();
        std::size_t matched = 0;
        for (const auto& record : catalog.records) {
            const auto* selected = find_external_signal(
                catalog, record.system_namespace, status_market_id, code,
                record.external_id);
            if (selected != &record) continue;
            const auto key = std::string(record.system_namespace ? "1#" : "0#") +
                             std::to_string(record.external_id);
            values[key] = static_cast<double>(record.value);
            texts[key] = trim(record.text);
            ++matched;
        }
        context["formula_external_values"] = std::move(values);
        context["formula_external_text_values"] = std::move(texts);
        context["formula_external_signals_ready"] = true;
        context["formula_external_signal_mode"] =
            "tdxw-type37-extern-user-system-file-first-match";
        context["formula_external_signal_record_format"] =
            "market|code|external_id|text|float_value";
        context["formula_external_signal_namespace_selector"] =
            "0=user; low-byte-nonzero=system";
        context["formula_external_signal_user_path"] =
            path_utf8(catalog.user_path);
        context["formula_external_signal_system_path"] =
            path_utf8(catalog.system_path);
        context["formula_external_signal_user_exists"] = catalog.user_exists;
        context["formula_external_signal_system_exists"] = catalog.system_exists;
        context["formula_external_signal_loaded_record_count"] =
            static_cast<std::uint64_t>(catalog.records.size());
        context["formula_external_signal_matched_binding_count"] =
            static_cast<std::uint64_t>(matched);
        context["formula_external_signal_missing_numeric"] = 0.0;
        context["formula_external_signal_missing_text"] = " ";
    }
    if (!external_series_bindings.empty()) {
        if (!kline_document)
            throw Error("EXTDATA_USER formula context requires a K-line document");
        if (!context.as_object().count("series")) context["series"] = Json::object();
        std::map<int, ExternalSeriesSelection> datasets;
        int first_bar_date = 99991231, last_bar_date = 0;
        for (const auto& bar : kline_document->at("bars").as_array()) {
            const auto value = compact_date(text_or(bar, "date"));
            if (value.empty())
                throw Error("EXTDATA_USER formula context requires dated K-line bars");
            const int date = std::stoi(value);
            first_bar_date = std::min(first_bar_date, date);
            last_bar_date = std::max(last_bar_date, date);
        }
        if (last_bar_date == 0)
            throw Error("EXTDATA_USER formula context requires non-empty K-line bars");
        Json metadata = Json::array();
        for (const auto& binding : external_series_bindings) {
            const auto parts = split(binding, '#');
            if (parts.size() != 3) continue;
            const int dataset_id = std::stoi(parts[1]);
            const int mode = std::stoi(parts[2]);
            auto found = datasets.find(dataset_id);
            if (found == datasets.end())
                found = datasets.emplace(
                    dataset_id,
                    load_external_series(root, dataset_id, status_market_id,
                                         code, 30000, first_bar_date,
                                         last_bar_date)).first;
            const auto aligned = align_external_series_to_kline(
                *kline_document, found->second.points, mode);
            Json item = Json::object();
            item["binding"] = binding;
            item["dataset_id"] = dataset_id;
            item["mode"] = mode;
            item["index_path"] = path_utf8(found->second.index_path);
            item["data_path"] = path_utf8(found->second.data_path);
            item["index_exists"] = found->second.index_exists;
            item["data_exists"] = found->second.data_exists;
            item["security_found"] = found->second.security_found;
            item["source_point_count"] =
                static_cast<std::uint64_t>(found->second.points.size());
            item["range_start_date"] = first_bar_date;
            item["range_end_date"] = last_bar_date;
            item["range_matched_point_count"] =
                found->second.range_matched_point_count;
            item["aligned_finite_point_count"] =
                static_cast<std::uint64_t>(aligned.size());
            item["host_limit_truncated"] = found->second.host_limit_truncated;
            metadata.push_back(std::move(item));
            context["series"][binding] = aligned;
        }
        context["formula_external_series_ready"] = true;
        context["formula_external_series_mode"] =
            "tdxw-selector38-exact-date-time-alignment";
        context["formula_external_series_missing_modes"] =
            "1=forward-fill;2=zero;3=backward-fill;other=DRAWNULL";
        context["formula_external_series_host_point_limit"] = 30000;
        context["formula_external_series"] = std::move(metadata);
    }
    if (!local_signal_bindings.empty()) {
        if (!kline_document)
            throw Error("local signal formula context requires a K-line document");
        if (status_market_id < 0)
            throw Error("local signal formula context has an invalid market");
        if (!context.as_object().count("series")) context["series"] = Json::object();
        int first_bar_date = 99991231, last_bar_date = 0;
        for (const auto& bar : kline_document->at("bars").as_array()) {
            const auto value = compact_date(text_or(bar, "date"));
            if (value.empty())
                throw Error("local signal formula context requires dated K-line bars");
            const int date = std::stoi(value);
            first_bar_date = std::min(first_bar_date, date);
            last_bar_date = std::max(last_bar_date, date);
        }
        if (last_bar_date == 0)
            throw Error("local signal formula context requires non-empty K-line bars");
        Json metadata = Json::array();
        for (const auto& binding : local_signal_bindings) {
            const auto parts = split(binding, '#');
            if (parts.size() != 3) continue;
            const auto signal_namespace = parts[0] == "SIGNALS_SYS"
                ? LocalSignalNamespace::system
                : LocalSignalNamespace::user;
            const int signal_id = std::stoi(parts[1]);
            const int mode = std::stoi(parts[2]);
            const auto selection = load_local_signal_series(
                root, signal_namespace, signal_id, status_market_id, code,
                30000, first_bar_date, last_bar_date);
            const auto aligned = align_local_signal_to_kline(
                *kline_document, selection.points, mode);
            Json item = Json::object();
            item["binding"] = binding;
            item["namespace"] = parts[0] == "SIGNALS_SYS" ? "system" : "user";
            item["signal_id"] = signal_id;
            item["mode"] = mode;
            item["path"] = path_utf8(selection.path);
            item["exists"] = selection.exists;
            item["source_record_count"] = selection.source_record_count;
            item["matched_record_count"] = selection.matched_record_count;
            item["returned_point_count"] =
                static_cast<std::uint64_t>(selection.points.size());
            item["aligned_finite_point_count"] =
                static_cast<std::uint64_t>(aligned.size());
            item["host_limit_truncated"] = selection.host_limit_truncated;
            metadata.push_back(std::move(item));
            context["series"][binding] = aligned;
        }
        context["formula_local_signals_ready"] = true;
        context["formula_local_signal_mode"] =
            "tdxw-selector34-system-text-selector36-user-binary-date-alignment";
        context["formula_local_signal_missing_modes"] =
            "1=forward-fill;2=zero;other=DRAWNULL";
        context["formula_local_signal_host_point_limit"] = 30000;
        context["formula_local_signals"] = std::move(metadata);
    }
    if (security_status_context) {
        std::string security_name;
        int security_category = 0;
        if (kline_document) {
            security_name = text_or(*kline_document, "name");
            if (const auto* category = optional(*kline_document, "security_category");
                category && category->is_number())
                security_category = static_cast<int>(category->as_number());
        }
        if (status_market_id <= 2) {
            if (!block_data)
                throw Error("formula security-status context requires the local TNF security master");
            const auto found = block_data->securities.find({status_market_id, code});
            if (found == block_data->securities.end())
                throw Error("selected security is absent from the local TNF security master");
            if (security_name.empty()) security_name = found->second.name;
            if (!security_category) security_category = found->second.tdx_category;
        } else if (dependencies.count("ISQHQQCODE") && !security_category) {
            if (!expansion_instrument)
                expansion_instrument = cached_expansion_security_record(
                    status_market_id, code, timeout_ms);
            security_category = expansion_instrument->category;
        }

        Json values = Json::object();
        if (dependencies.count("IST0CODE")) {
            bool value = false;
            if (status_market_id <= 2) {
                const auto resource = cached_finance_eligibility_resource(root);
                value = tcalc_is_t0_security(*resource, status_market_id, code);
                context["security_status_spblock_source"] = resource->source_path;
                context["security_status_t0_fund_count"] =
                    static_cast<std::uint64_t>(resource->t0_funds.size());
                context["security_status_beijing_convertible_bond_count"] =
                    static_cast<std::uint64_t>(
                        resource->beijing_convertible_bonds.size());
            }
            symbols["IST0CODE"] = value ? 1.0 : 0.0;
            values["IST0CODE"] = value ? 1.0 : 0.0;
        }
        if (dependencies.count("ISSTCODE")) {
            const bool value = tcalc_is_st_security(
                status_market_id, code, security_name);
            symbols["ISSTCODE"] = value ? 1.0 : 0.0;
            values["ISSTCODE"] = value ? 1.0 : 0.0;
        }
        if (dependencies.count("ISQUITCODE")) {
            bool value = false;
            if (status_market_id <= 2) {
                const auto resource = cached_security_quit_resource(root);
                value = tcalc_is_quit_security(*resource, status_market_id, code);
                context["security_status_quit_source"] = resource->source_path;
                context["security_status_quit_effective_date"] =
                    resource->effective_date;
                context["security_status_quit_active_count"] =
                    static_cast<std::uint64_t>(resource->active.size());
            }
            symbols["ISQUITCODE"] = value ? 1.0 : 0.0;
            values["ISQUITCODE"] = value ? 1.0 : 0.0;
        }
        if (dependencies.count("ISQHQQCODE")) {
            const bool value = tcalc_is_futures_or_option_category(
                security_category);
            symbols["ISQHQQCODE"] = value ? 1.0 : 0.0;
            values["ISQHQQCODE"] = value ? 1.0 : 0.0;
            context["security_status_category"] = security_category;
            context["security_status_category_source"] = status_market_id <= 2
                ? "local-tnf-offset-282"
                : "tdx-7727-0x23f0-0x23f5-category";
        }
        context["security_status_mode"] =
            "tdxw-types167-120-105-exact-host-field-reconstruction";
        context["security_status_values"] = std::move(values);
    }
}

} // namespace tdx::formula_context_detail
