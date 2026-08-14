#include "tpool_internal.hpp"

#include "tdx/common.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace tdx::tpool_detail {
namespace {

constexpr std::string_view kEntryHeader =
    "市场|代码|名称|进入日期|进入时间|进入价";
constexpr std::string_view kSnapshotHeader =
    "市场|代码|名称|进入日期|进入时间|进入价|最高收益率|最高周期|最高日期|最高价格";

int integer_value(const Json& record, std::string_view key) {
    const auto* value = optional(record, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : 0;
}

double number_value(const Json& record, std::string_view key) {
    const auto* value = optional(record, key);
    return value && value->is_number() ? value->as_number() : 0.0;
}

std::string text_value(const Json& record, std::string_view key) {
    const auto* value = optional(record, key);
    return value && value->is_string() ? value->as_string() : "";
}

void write_date(std::ostringstream& output, int value) {
    output << std::setfill('0') << std::setw(4) << value / 10000
           << std::setw(2) << value % 10000 / 100
           << std::setw(2) << value % 100;
}

void write_time(std::ostringstream& output, int value) {
    output << std::setfill('0') << std::setw(2) << value / 10000
           << ':' << std::setw(2) << value % 10000 / 100;
}

void write_month_day(std::ostringstream& output, int value) {
    output << std::setfill('0') << std::setw(2) << value % 10000 / 100
           << '/' << std::setw(2) << value % 100;
}

void render_record(std::ostringstream& output, const Json& record,
                   std::string_view kind, const TpoolSecurityNames& names,
                   std::size_t& resolved_names) {
    const auto* complete = optional(record, "complete");
    if (!complete || !complete->is_bool() || !complete->as_bool())
        throw Error("TPool native text export requires complete history records");
    const int market = integer_value(record, "market_id");
    const auto code = text_value(record, "code");
    const auto found = names.find({market, code});
    const auto name = found == names.end() || found->second.empty()
        ? text_value(record, "name") : found->second;
    if (!name.empty()) ++resolved_names;

    output << "\r\n" << market << '|' << code << '|' << name << '|';
    write_date(output, integer_value(record, "entry_date"));
    output << '|';
    write_time(output, integer_value(record, "entry_time"));
    output << '|' << std::fixed << std::setprecision(2)
           << number_value(record, "entry_price");
    if (kind == "daily-snapshot") {
        output << '|' << number_value(record, "maximum_rise_pct") << "%|"
               << integer_value(record, "maximum_period") << '|';
        const auto month_day = text_value(record, "maximum_date_month_day");
        if (!month_day.empty()) output << month_day;
        else write_month_day(output, integer_value(record, "maximum_date"));
        output << '|' << number_value(record, "maximum_price");
    }
}

void render_file(std::ostringstream& output, const Json& file,
                 std::string_view kind, const TpoolSecurityNames& names,
                 std::size_t& rows, std::size_t& resolved_names) {
    const auto& records = file.at("records");
    if (!records.is_array()) throw Error("TPool history records must be an array");
    for (const auto& record : records.as_array()) {
        render_record(output, record, kind, names, resolved_names);
        ++rows;
    }
}

}  // namespace

TpoolNativeTextExport render_tpool_history_native_text(
    const Json& history, const TpoolSecurityNames& names) {
    TpoolNativeTextExport result;
    const auto schema = text_value(history, "schema");
    const Json* files = nullptr;
    if (schema == "tdx-tpool-history-file-v1") {
        result.kind = text_value(history, "kind");
    } else if (schema == "tdx-tpool-history-catalog-v1") {
        files = optional(history, "files");
        if (!files || !files->is_array() || files->size() == 0)
            throw Error("TPool native text export requires at least one history file");
        result.kind = text_value(files->as_array().front(), "kind");
    } else {
        throw Error("TPool native text export requires a history file or catalog document");
    }
    if (result.kind != "daily-entry-log" && result.kind != "daily-snapshot")
        throw Error("TPool native text export has an unknown history kind");

    std::ostringstream output;
    output << (result.kind == "daily-snapshot" ? kSnapshotHeader : kEntryHeader);
    if (files) {
        for (const auto& file : files->as_array()) {
            if (text_value(file, "kind") != result.kind)
                throw Error("TPool native text export cannot mix .dat and .log files");
            render_file(output, file, result.kind, names,
                        result.row_count, result.resolved_name_count);
        }
    } else {
        render_file(output, history, result.kind, names,
                    result.row_count, result.resolved_name_count);
    }
    result.text_utf8 = output.str();
    return result;
}

}  // namespace tdx::tpool_detail
