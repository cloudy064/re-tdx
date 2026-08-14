#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <map>

namespace fs = std::filesystem;

namespace tdx {
using namespace disclosure_detail;

fs::path default_disclosure_archive_path(const fs::path& tdx_root) {
    return tdx_root / "T0002" / "tdx-tool" / "disclosure-availability.json";
}

Json merge_disclosure_archive_document(const Json& disclosure_document,
                                        const Json* existing_archive,
                                        std::string observed_at) {
    if (!disclosure_document.is_object() ||
        !disclosure_document.as_object().count("rows") ||
        !disclosure_document.at("rows").is_array())
        throw Error("disclosure archive input needs a rows array");
    if (observed_at.empty()) observed_at = now_text();

    std::map<std::string, Json> entries;
    if (existing_archive) {
        if (!existing_archive->is_object() ||
            !existing_archive->as_object().count("schema") ||
            !existing_archive->at("schema").is_string() ||
            existing_archive->at("schema").as_string() !=
                "tdx-disclosure-availability-archive-v1" ||
            !existing_archive->as_object().count("entries") ||
            !existing_archive->at("entries").is_array())
            throw Error("existing disclosure archive has an unsupported schema");
        for (const auto& entry : existing_archive->at("entries").as_array()) {
            if (!entry.is_object()) continue;
            const auto security_id = text_value(entry, "security_id");
            const auto report_period = text_value(entry, "report_period");
            if (!security_id.empty() && digits(report_period, 8))
                entries[security_id + "|" + report_period] = entry;
        }
    }

    const auto add_unique = [](Json& values, const std::string& value) {
        if (!values.is_array()) values = Json::array();
        for (const auto& item : values.as_array())
            if (item.is_string() && item.as_string() == value) return;
        values.push_back(value);
    };
    const auto revise_date = [&](Json& entry, const char* field_name,
                                 const std::string& value,
                                 const std::string& source_kind) {
        if (!digits(value, 8)) return;
        auto& field_value = entry[field_name];
        if (field_value.is_string() && field_value.as_string() == value) return;
        if (field_value.is_string() && digits(field_value.as_string(), 8)) {
            if (!entry.as_object().count("revisions")) entry["revisions"] = Json::array();
            const auto previous = field_value.as_string();
            const auto selected = std::min(previous, value);
            for (const auto& existing : entry.at("revisions").as_array()) {
                if (text_value(existing, "field") == field_name &&
                    text_value(existing, "observed") == value &&
                    text_value(existing, "source_kind") == source_kind)
                    return;
            }
            Json revision = Json::object();
            revision["field"] = field_name;
            revision["previous"] = previous;
            revision["observed"] = value;
            revision["current"] = selected;
            revision["observed_at"] = observed_at;
            revision["source_kind"] = source_kind;
            entry["revisions"].push_back(std::move(revision));
            field_value = selected;
            return;
        }
        field_value = value;
    };
    const auto revise_schedule_date = [&](Json& entry, const std::string& value) {
        if (!digits(value, 8)) return;
        auto& current = entry["scheduled_disclosure_date"];
        if (current.is_string() && current.as_string() == value) return;
        if (current.is_string() && digits(current.as_string(), 8)) {
            if (!entry.as_object().count("schedule_revisions") ||
                !entry.at("schedule_revisions").is_array())
                entry["schedule_revisions"] = Json::array();
            const auto previous = current.as_string();
            bool known = false;
            for (const auto& revision : entry.at("schedule_revisions").as_array())
                if (text_value(revision, "previous") == previous &&
                    text_value(revision, "observed") == value) known = true;
            if (!known) {
                Json revision = Json::object();
                revision["previous"] = previous;
                revision["observed"] = value;
                revision["observed_at"] = observed_at;
                entry["schedule_revisions"].push_back(std::move(revision));
            }
        }
        current = value;
    };
    const auto merge_announcement_evidence = [](Json& entry, const Json& row) {
        const auto found = row.as_object().find("announcement_evidence");
        if (found == row.as_object().end() || !found->second.is_array()) return;
        if (!entry.as_object().count("announcement_evidence") ||
            !entry.at("announcement_evidence").is_array())
            entry["announcement_evidence"] = Json::array();
        std::set<std::string> known;
        for (const auto& item : entry.at("announcement_evidence").as_array())
            known.insert(item.dump(-1));
        for (const auto& item : found->second.as_array())
            if (known.insert(item.dump(-1)).second)
                entry["announcement_evidence"].push_back(item);
    };

    std::uint64_t observed_rows = 0;
    for (const auto& row : disclosure_document.at("rows").as_array()) {
        if (!row.is_object() || !row.as_object().count("security") ||
            !row.at("security").is_object()) continue;
        const auto& security = row.at("security");
        const auto market = lower_ascii(text_value(security, "market"));
        const auto code = text_value(security, "code");
        const auto security_id = text_value(security, "security_id");
        const auto report_period = text_value(row, "report_period");
        const auto kind = text_value(row, "kind");
        if ((market != "sz" && market != "sh" && market != "bj") ||
            code.empty() || security_id.empty() || !digits(report_period, 8) ||
            (kind != "schedule" && kind != "recent" && kind != "express" &&
             kind != "announcement-report"))
            continue;
        ++observed_rows;
        const auto key = security_id + "|" + report_period;
        auto found = entries.find(key);
        if (found == entries.end()) {
            Json entry = Json::object();
            entry["market"] = market;
            entry["code"] = code;
            entry["security_id"] = security_id;
            entry["name"] = text_value(security, "name");
            entry["report_period"] = report_period;
            entry["report_available_from"] = Json(nullptr);
            entry["express_available_from"] = Json(nullptr);
            entry["scheduled_disclosure_date"] = Json(nullptr);
            entry["first_scheduled_date"] = Json(nullptr);
            entry["schedule_status"] = Json(nullptr);
            entry["schedule_change_dates"] = Json::array();
            entry["schedule_revisions"] = Json::array();
            entry["source_kinds"] = Json::array();
            entry["first_observed_at"] = observed_at;
            entry["last_observed_at"] = observed_at;
            entry["revisions"] = Json::array();
            found = entries.emplace(key, std::move(entry)).first;
        }
        auto& entry = found->second;
        entry["last_observed_at"] = observed_at;
        const auto name = text_value(security, "name");
        if (!name.empty()) entry["name"] = name;
        add_unique(entry["source_kinds"], kind);
        if (kind == "announcement-report") merge_announcement_evidence(entry, row);
        if (kind == "schedule") {
            revise_schedule_date(entry, text_value(row, "scheduled_disclosure_date"));
            const auto first_scheduled = text_value(row, "first_scheduled_date");
            if (digits(first_scheduled, 8) &&
                (!entry.as_object().count("first_scheduled_date") ||
                 !entry.at("first_scheduled_date").is_string()))
                entry["first_scheduled_date"] = first_scheduled;
            const auto status = text_value(row, "status");
            if (!status.empty()) entry["schedule_status"] = status;
            if (row.as_object().count("change_dates") &&
                row.at("change_dates").is_array()) {
                if (!entry.as_object().count("schedule_change_dates") ||
                    !entry.at("schedule_change_dates").is_array())
                    entry["schedule_change_dates"] = Json::array();
                for (const auto& change : row.at("change_dates").as_array()) {
                    const auto date = scalar_text(change);
                    if (digits(date, 8)) add_unique(entry["schedule_change_dates"], date);
                }
            }
        }
        const auto available = text_value(row, "available_from");
        if (kind == "express")
            revise_date(entry, "express_available_from", available, kind);
        else
            revise_date(entry, "report_available_from", available, kind);
    }

    Json sorted = Json::array();
    std::uint64_t report_available = 0, express_available = 0, revision_count = 0,
                  scheduled_pending = 0;
    for (auto& [key, entry] : entries) {
        (void)key;
        if (entry.at("report_available_from").is_string()) ++report_available;
        if (entry.at("express_available_from").is_string()) ++express_available;
        if (!entry.at("report_available_from").is_string() &&
            entry.as_object().count("scheduled_disclosure_date") &&
            entry.at("scheduled_disclosure_date").is_string()) ++scheduled_pending;
        if (entry.as_object().count("revisions") && entry.at("revisions").is_array())
            revision_count += static_cast<std::uint64_t>(entry.at("revisions").size());
        sorted.push_back(std::move(entry));
    }
    Json result = Json::object();
    result["schema"] = "tdx-disclosure-availability-archive-v1";
    result["updated_at"] = observed_at;
    result["entries"] = std::move(sorted);
    Json summary = Json::object();
    summary["entry_count"] = static_cast<std::uint64_t>(entries.size());
    summary["observed_row_count"] = observed_rows;
    summary["report_available_count"] = report_available;
    summary["express_available_count"] = express_available;
    summary["scheduled_pending_count"] = scheduled_pending;
    summary["revision_count"] = revision_count;
    result["summary"] = std::move(summary);
    result["semantics"] =
        "report_available_from keeps the earliest confirmed actual full-report "
        "disclosure date; express_available_from never unlocks a professional-finance package";
    result["value_revision_boundary"] =
        "availability timing is point-in-time, but historical gpcw values come from the "
        "currently distributed official package and may include later restatements";
    return result;
}

}  // namespace tdx
