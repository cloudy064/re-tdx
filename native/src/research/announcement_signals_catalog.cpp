#include "tdx/announcement_signals_internal.hpp"

namespace tdx {
namespace detail {
namespace announcement_signals {

const ResourceSpec* find_resource(std::string_view view) {
    for (std::size_t i = 0; i < resource_count; ++i)
        if (resources[i].view == view) return &resources[i];
    return nullptr;
}

Json catalog_rows() {
    Json records = Json::array();
    for (std::size_t i = 0; i < resource_count; ++i) {
        const auto& spec = resources[i];
        Json row = Json::object();
        row["view"] = spec.view;
        row["label"] = spec.label;
        row["resource"] = spec.resource;
        row["fields"] = spec.fields;
        records.push_back(std::move(row));
    }
    Json history = Json::object();
    history["view"] = "history";
    history["label"] = "个股公告精选历史";
    history["resource"] = "ggjx/<market-id><code>.jsn";
    history["fields"] = "date,title,pdf-url,direction,type,pre/post-3d-return";
    records.push_back(std::move(history));
    Json security = Json::object();
    security["view"] = "security";
    security["label"] = "单票公告信号汇总";
    security["resource"] = "two current lists + optional ggjx history";
    security["fields"] = "selected/risk current matches plus history";
    records.push_back(std::move(security));
    return records;
}

}  // namespace announcement_signals
}  // namespace detail
}  // namespace tdx
