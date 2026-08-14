#include "professional_data_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace tdx {

using namespace professional_data_detail;

Json professional_trading_document(const std::vector<ProfessionalTradingRecord>& records,
                                   std::string kind, std::string security_id,
                                   const std::vector<int>& fields, std::uint32_t from,
                                   std::uint32_t to, std::size_t limit, bool history) {
    if (kind != "stock" && kind != "board" && kind != "market")
        throw Error("trading kind must be stock, board or market");
    std::set<int> selected(fields.begin(), fields.end()), observed;
    for (const auto& row : records) observed.insert(row.id);
    if (selected.empty()) selected = observed;
    Json summaries = Json::array(), rows = Json::array();
    for (const int id : selected) {
        std::size_t count = 0; const ProfessionalTradingRecord* first = nullptr; const ProfessionalTradingRecord* last = nullptr;
        for (const auto& row : records) if (row.id == id && row.date >= from && row.date <= to) {
            if (!first || row.date < first->date) first = &row;
            if (!last || row.date >= last->date) last = &row;
            ++count;
        }
        Json summary = Json::object(); summary["id"] = id;
        const auto name = field_name(kind, id); summary["name"] = name.empty() ? Json(nullptr) : Json(name);
        summary["documented"] = !name.empty(); summary["count"] = static_cast<std::uint64_t>(count);
        summary["first_date"] = first ? Json(date_text(first->date)) : Json(nullptr);
        summary["last_date"] = last ? Json(date_text(last->date)) : Json(nullptr);
        summary["latest_value1"] = last ? number_json(last->value1) : Json(nullptr);
        summary["latest_value2"] = last ? number_json(last->value2) : Json(nullptr);
        summaries.push_back(std::move(summary));
    }
    if (history) {
        std::vector<const ProfessionalTradingRecord*> matched;
        for (const auto& row : records)
            if (selected.count(row.id) && row.date >= from && row.date <= to) matched.push_back(&row);
        std::stable_sort(matched.begin(), matched.end(), [](const auto* left, const auto* right) {
            return std::tie(left->date, left->id) < std::tie(right->date, right->id);
        });
        if (matched.size() > limit)
            matched.erase(matched.begin(), matched.end() - static_cast<std::ptrdiff_t>(limit));
        for (const auto* row : matched) {
            Json value = Json::object(); value["id"] = row->id; value["date_raw"] = static_cast<std::uint64_t>(row->date);
            value["date"] = date_text(row->date); value["value1"] = number_json(row->value1);
            value["value2"] = number_json(row->value2); rows.push_back(std::move(value));
        }
    }
    Json result = Json::object(); result["schema"] = "tdx-professional-trading-v1";
    result["kind"] = kind; result["security_id"] = security_id;
    result["record_count"] = static_cast<std::uint64_t>(records.size());
    result["documented_min_id"] = kind == "board" ? 5 : 1;
    result["documented_max_id"] = kind == "stock" ? 44 : kind == "board" ? 19 : 42;
    result["observed_id_count"] = static_cast<std::uint64_t>(observed.size());
    const bool history_truncated = history && [&] {
        std::size_t matched = 0;
        for (const auto& row : records)
            if (selected.count(row.id) && row.date >= from && row.date <= to) ++matched;
        return matched > limit;
    }();
    result["fields"] = std::move(summaries); result["history"] = std::move(rows);
    result["history_truncated"] = history_truncated;
    return result;
}

Json professional_finance_document(const ProfessionalFinanceData& data, int market_id,
                                   const std::string& code, const std::vector<int>& fields) {
    const auto found = data.records.find({market_id, code});
    Json result = Json::object(); result["schema"] = "tdx-professional-finance-v1";
    result["source"] = data.source; result["report_date_raw"] = static_cast<std::uint64_t>(data.report_date);
    result["report_date"] = date_text(data.report_date);
    result["security_count"] = static_cast<std::uint64_t>(data.records.size());
    result["field_count"] = static_cast<std::uint64_t>(data.field_count);
    result["security_id"] = (market_id == 0 ? "SZ" : market_id == 1 ? "SH" : "BJ") + code;
    result["found"] = found != data.records.end();
    Json values = Json::array();
    if (found != data.records.end()) {
        std::vector<int> selected = fields;
        if (selected.empty()) for (std::size_t id = 1; id <= data.field_count; ++id)
            selected.push_back(static_cast<int>(id));
        for (const int id : selected) {
            if (id < 0 || static_cast<std::size_t>(id) > data.field_count)
                throw Error("FINVALUE field ID is outside the package field range");
            Json row = Json::object(); row["id"] = id;
            row["value"] = id == 0 ? Json(static_cast<std::uint64_t>(data.report_date))
                : number_json(found->second.fields[static_cast<std::size_t>(id - 1)]);
            values.push_back(std::move(row));
        }
    }
    result["fields"] = std::move(values); return result;
}

}  // namespace tdx
