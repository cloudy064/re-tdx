#include "hk_finance_internal.hpp"
#include "tdx/time.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::hk_finance_detail {
namespace {

struct NativeField {
    std::size_t index;
    std::string_view key;
    std::string_view destination;
    std::string_view conversion;
    double scale;
    std::string_view semantic_status;
    std::string_view semantic_key;
    std::string_view label_zh;
    std::string_view unit;
};

constexpr std::array<NativeField, 16> kNativeFields{{
    {1, "security_152", "security+152", "atof/10000", 0.0001,
     "verified", "shares.h_10k", "H股", "ten_thousand_shares"},
    {2, "security_124", "security+124", "atol", 1.0,
     "verified", "report_date", "财务日期", "yyyymmdd"},
    {3, "security_196", "security+196", "atof", 1.0,
     "verified", "income_statement.revenue_10k", "营业收入",
     "ten_thousand_reporting_currency"},
    {4, "security_240", "security+240", "atof/100", 0.01,
     "verified", "per_share.dividend", "每股股息",
     "reporting_currency_per_share"},
    {5, "security_156", "security+156", "atof/100", 0.01,
     "verified", "per_share.earnings", "每股收益",
     "reporting_currency_per_share"},
    {6, "security_160", "security+160", "atof", 1.0,
     "verified", "balance_sheet.total_assets_10k", "总资产",
     "ten_thousand_reporting_currency"},
    {7, "security_192", "security+192", "atof", 1.0,
     "verified", "balance_sheet.net_assets_10k", "净资产",
     "ten_thousand_reporting_currency"},
    {8, "security_236", "security+236", "atof", 1.0,
     "verified", "income_statement.net_profit_10k", "净利润",
     "ten_thousand_reporting_currency"},
    {9, "security_244", "security+244", "atof", 1.0,
     "verified", "per_share.net_assets", "每股净资产",
     "reporting_currency_per_share"},
    {10, "companion_37", "companion+37", "string[10]", 1.0,
     "verified", "classification_code", "分类码", "code"},
    {11, "companion_56", "companion+56", "atof", 1.0,
     "verified", "valuation.pe_ttm", "市盈率(TTM)", "ratio"},
    {12, "security_128", "security+128", "atof-to-int", 1.0,
     "verified", "listing_date", "上市日期", "yyyymmdd"},
    {13, "security_132", "security+132", "atof/10000", 0.0001,
     "verified", "shares.total_10k", "总股本", "ten_thousand_shares"},
    {14, "security_184", "security+184", "atof", 1.0,
     "verified", "balance_sheet.minority_interest_10k", "少数股权",
     "ten_thousand_reporting_currency"},
    {15, "security_352", "security+352", "atof-to-byte", 1.0,
     "verified-role-enum-unresolved", "currency_adjustment.native_code",
     "币种折算码", "native_code"},
    {16, "companion_52", "companion+52", "atof", 1.0,
     "verified", "valuation.pe_static", "市盈率(静)", "ratio"},
}};

bool digits(std::string_view value, std::size_t count) {
    return value.size() == count &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string generated_at() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

void validate_query(HkFinanceQuery& query) {
    query.code = trim(query.code);
    query.query = lower_ascii(trim(query.query));
    query.classification = trim(query.classification);
    query.report_from = trim(query.report_from);
    query.report_to = trim(query.report_to);
    query.sort = lower_ascii(trim(query.sort));
    query.order = lower_ascii(trim(query.order));
    if (!query.code.empty() && !digits(query.code, 5))
        throw Error("code must contain exactly five digits");
    if (!query.report_from.empty() && !digits(query.report_from, 8))
        throw Error("from must be YYYYMMDD");
    if (!query.report_to.empty() && !digits(query.report_to, 8))
        throw Error("to must be YYYYMMDD");
    if (!query.report_from.empty() && !query.report_to.empty() &&
        query.report_from > query.report_to)
        throw Error("from must not be after to");
    if (query.sort != "code" && query.sort != "report_date" &&
        query.sort != "listing_date" && query.sort != "classification")
        throw Error("sort must be code, report_date, listing_date, or classification");
    if (query.order != "asc" && query.order != "desc")
        throw Error("order must be asc or desc");
    if (query.offset < 0 || query.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (query.limit < 1 || query.limit > 2000)
        throw Error("limit must be in 1..2000");
}

bool matches(const FinanceRecord& record, const HkFinanceQuery& query) {
    if (!query.code.empty() && record.raw[0] != query.code) return false;
    if (!query.classification.empty() &&
        record.raw[10] != query.classification) return false;
    if (!query.report_from.empty() &&
        (record.raw[2].empty() || record.raw[2] < query.report_from)) return false;
    if (!query.report_to.empty() &&
        (record.raw[2].empty() || record.raw[2] > query.report_to)) return false;
    if (query.query.empty()) return true;
    std::string haystack;
    for (const auto& value : record.raw) {
        haystack += value;
        haystack.push_back(' ');
    }
    return lower_ascii(haystack).find(query.query) != std::string::npos;
}

std::string sort_value(const FinanceRecord& record, std::string_view sort) {
    if (sort == "report_date") return record.raw[2];
    if (sort == "listing_date") return record.raw[12];
    if (sort == "classification") return record.raw[10];
    return record.raw[0];
}

Json nullable_text(const std::string& value) {
    return value.empty() ? Json(nullptr) : Json(value);
}

Json decoded_value(const FinanceRecord& record, const NativeField& field) {
    if (field.index == 10) return nullable_text(record.raw[field.index]);
    const auto& value = record.numeric[field.index];
    return value ? Json(*value * field.scale) : Json(nullptr);
}

Json numeric_value(const FinanceRecord& record, std::size_t index,
                   double scale = 1.0) {
    const auto& value = record.numeric[index];
    return value ? Json(*value * scale) : Json(nullptr);
}

Json finance_json(const FinanceRecord& record) {
    Json shares = Json::object();
    shares["total_10k"] = numeric_value(record, 13, 0.0001);
    shares["h_10k"] = numeric_value(record, 1, 0.0001);

    Json balance_sheet = Json::object();
    balance_sheet["total_assets_10k"] = numeric_value(record, 6);
    balance_sheet["net_assets_10k"] = numeric_value(record, 7);
    balance_sheet["minority_interest_10k"] = numeric_value(record, 14);

    Json income_statement = Json::object();
    income_statement["revenue_10k"] = numeric_value(record, 3);
    income_statement["net_profit_10k"] = numeric_value(record, 8);

    Json per_share = Json::object();
    per_share["dividend"] = numeric_value(record, 4, 0.01);
    per_share["earnings"] = numeric_value(record, 5, 0.01);
    per_share["net_assets"] = numeric_value(record, 9);

    Json valuation = Json::object();
    valuation["pe_ttm"] = numeric_value(record, 11);
    valuation["pe_static"] = numeric_value(record, 16);

    Json currency_adjustment = Json::object();
    currency_adjustment["native_code"] = numeric_value(record, 15);
    currency_adjustment["conversion_required"] = record.numeric[15]
        ? Json(*record.numeric[15] != 0.0) : Json(nullptr);

    Json result = Json::object();
    result["report_date"] = nullable_text(record.raw[2]);
    result["listing_date"] = nullable_text(record.raw[12]);
    result["classification_code"] = nullable_text(record.raw[10]);
    result["shares"] = std::move(shares);
    result["balance_sheet"] = std::move(balance_sheet);
    result["income_statement"] = std::move(income_statement);
    result["per_share"] = std::move(per_share);
    result["valuation"] = std::move(valuation);
    result["currency_adjustment"] = std::move(currency_adjustment);
    return result;
}

Json record_json(const FinanceRecord& record) {
    Json values = Json::object();
    for (const auto& field : kNativeFields)
        values[std::string(field.key)] = decoded_value(record, field);
    Json raw = Json::array();
    for (const auto& field : record.raw)
        raw.push_back(field.empty() ? Json(nullptr) : Json(field));

    Json result = Json::object();
    result["code"] = record.raw[0];
    result["report_date"] = nullable_text(record.raw[2]);
    result["listing_date"] = nullable_text(record.raw[12]);
    result["classification_code"] = nullable_text(record.raw[10]);
    result["finance"] = finance_json(record);
    result["native_values"] = std::move(values);
    result["raw_fields"] = std::move(raw);
    return result;
}

Json native_schema_json() {
    Json fields = Json::array();
    for (const auto& field : kNativeFields) {
        Json item = Json::object();
        item["source_index"] = static_cast<std::uint64_t>(field.index);
        item["key"] = std::string(field.key);
        item["destination"] = std::string(field.destination);
        item["conversion"] = std::string(field.conversion);
        item["semantic_status"] = std::string(field.semantic_status);
        item["semantic_key"] = field.semantic_key.empty()
            ? Json(nullptr) : Json(std::string(field.semantic_key));
        item["label_zh"] = field.label_zh.empty()
            ? Json(nullptr) : Json(std::string(field.label_zh));
        item["unit"] = field.unit.empty()
            ? Json(nullptr) : Json(std::string(field.unit));
        fields.push_back(std::move(item));
    }
    Json result = Json::object();
    result["loader"] = "TdxW!sub_51F310";
    result["downloader"] = "TdxW!sub_640690";
    result["column_count"] = 17;
    result["code_source_index"] = 0;
    result["empty_field_native_substitution"] = 0;
    result["fields"] = std::move(fields);
    result["interpretation_status"] =
        "16-of-16 source-field roles recovered; source index 15 currency-"
        "adjustment enum labels remain unresolved";
    return result;
}

Json source_json(const fs::path& source, std::size_t row_count) {
    std::error_code error;
    const auto size = fs::file_size(source, error);
    Json result = Json::object();
    result["file"] = source.filename().string();
    result["path"] = path_utf8(source);
    result["size"] = error ? Json(nullptr) : Json(static_cast<std::uint64_t>(size));
    result["row_count"] = static_cast<std::uint64_t>(row_count);
    result["endpoint"] = "local-file:" + path_utf8(source);
    result["encrypted"] = true;
    result["cipher"] = "Blowfish-ECB/native-little-endian-words";
    return result;
}

}  // namespace

Json build_document(const std::vector<FinanceRecord>& records,
                    const fs::path& source,
                    HkFinanceQuery query) {
    validate_query(query);
    std::vector<const FinanceRecord*> matched;
    matched.reserve(records.size());
    for (const auto& record : records)
        if (matches(record, query)) matched.push_back(&record);
    std::stable_sort(matched.begin(), matched.end(), [&](const auto* left,
                                                         const auto* right) {
        const auto left_value = sort_value(*left, query.sort);
        const auto right_value = sort_value(*right, query.sort);
        if (left_value != right_value)
            return query.order == "asc"
                ? left_value < right_value : left_value > right_value;
        return left->raw[0] < right->raw[0];
    });

    std::string earliest_report;
    std::string latest_report;
    std::map<std::string, std::size_t> classifications;
    for (const auto* record : matched) {
        const auto& report = record->raw[2];
        if (!report.empty()) {
            if (earliest_report.empty() || report < earliest_report)
                earliest_report = report;
            if (latest_report.empty() || report > latest_report)
                latest_report = report;
        }
        if (!record->raw[10].empty()) ++classifications[record->raw[10]];
    }
    Json by_classification = Json::object();
    for (const auto& [name, count] : classifications)
        by_classification[name] = static_cast<std::uint64_t>(count);
    std::array<std::size_t, 17> missing{};
    for (const auto& record : records)
        for (std::size_t index = 0; index < record.raw.size(); ++index)
            if (record.raw[index].empty()) ++missing[index];
    Json missing_json = Json::array();
    for (const auto count : missing)
        missing_json.push_back(static_cast<std::uint64_t>(count));

    const auto begin = std::min<std::size_t>(
        static_cast<std::size_t>(query.offset), matched.size());
    const auto end = std::min<std::size_t>(
        begin + static_cast<std::size_t>(query.limit), matched.size());
    Json rows = Json::array();
    for (std::size_t index = begin; index < end; ++index)
        rows.push_back(record_json(*matched[index]));

    Json summary = Json::object();
    summary["source_records"] = static_cast<std::uint64_t>(records.size());
    summary["earliest_report_date"] = nullable_text(earliest_report);
    summary["latest_report_date"] = nullable_text(latest_report);
    summary["by_classification"] = std::move(by_classification);
    summary["source_missing_by_index"] = std::move(missing_json);

    Json transport = Json::object();
    transport["kind"] = "local-encrypted-file";
    transport["network_requests"] = 0;

    Json result = Json::object();
    result["schema"] = "tdx-market-hk-finance-native-v1";
    result["generated_at"] = generated_at();
    result["source_mode"] = "local";
    result["mode"] = query.code.empty() ? "catalog" : "security";
    result["availability"] = matched.empty() ? "empty" : "local";
    result["sort"] = query.sort;
    result["order"] = query.order;
    result["offset"] = query.offset;
    result["match_count"] = static_cast<std::uint64_t>(matched.size());
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["summary"] = std::move(summary);
    result["rows"] = std::move(rows);
    result["source"] = source_json(source, records.size());
    result["native_schema"] = native_schema_json();
    result["transport"] = std::move(transport);
    result["semantics"] =
        "Current HK local finance cache decoded with the exact TdxW loader. "
        "Verified HK summary fields are projected under finance while every native "
        "destination and raw source field remains available; the currency-adjustment "
        "role is recovered without inventing names for its native enum values.";
    return result;
}

}  // namespace tdx::hk_finance_detail

namespace tdx {

Json load_local_hk_finance(const fs::path& root,
                           const HkFinanceQuery& query) {
    const auto records = hk_finance_detail::load_records(root);
    return hk_finance_detail::build_document(
        *records, root / "T0002" / "hq_cache" / "hkcwdata.dat", query);
}

}  // namespace tdx
