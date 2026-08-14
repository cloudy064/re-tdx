#include "tpool_internal.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace tdx::tpool_detail {

namespace {

bool enabled(const Json& raw, std::string_view key) {
    return json_integer_text(raw, key, 0) != 0;
}

struct RetentionUnit {
    int type_id;
    std::string_view name;
    std::int64_t seconds;
};

constexpr std::array<RetentionUnit, 4> kRetentionUnits{{
    {0, "days", 24 * 60 * 60},
    {1, "hours", 60 * 60},
    {2, "minutes", 60},
    {3, "seconds", 1},
}};

const RetentionUnit* retention_unit(int type_id) {
    for (const auto& unit : kRetentionUnits) {
        if (unit.type_id == type_id) return &unit;
    }
    return nullptr;
}

void append_retention_policy(
    Json& value, bool policy_enabled, int count, int type_id) {
    const auto* unit = retention_unit(type_id);
    const bool effective = policy_enabled && count > 0 && unit != nullptr;
    value["enabled"] = policy_enabled;
    value["effective"] = effective;
    value["count"] = count;
    value["type_id"] = type_id;
    value["retention_count"] = count;
    value["retention_unit_id"] = type_id;
    value["retention_unit"] = unit ? Json(std::string(unit->name)) : Json(nullptr);
    value["retention_seconds"] = effective
        ? Json(static_cast<std::int64_t>(count) * unit->seconds)
        : Json(nullptr);
    value["supported_unit"] = unit != nullptr;
    value["record_scope"] = "stored-pool-history";
    value["record_size_bytes"] = 85;
    value["expiry_condition"] = "record_age_seconds >= retention_seconds";
    value["trigger_scope"] = "periodic-history-maintenance";
    value["parameter_semantics"] =
        "TPool runtime: ndelnum is the retention interval; ndeltype maps 0=days, 1=hours, 2=minutes, 3=seconds";
}

Json action(std::string kind) {
    Json result = Json::object();
    result["kind"] = std::move(kind);
    result["original_host_side_effect"] = true;
    result["executed"] = false;
    return result;
}

void append_action(Json& actions, Json value) {
    actions.push_back(std::move(value));
}

Json string_array(std::initializer_list<std::string_view> values) {
    Json result = Json::array();
    for (const auto value : values) result.push_back(std::string(value));
    return result;
}

Json layout_field(
    std::string_view name, int offset, int size, std::string_view type) {
    Json result = Json::object();
    result["name"] = std::string(name);
    result["offset"] = offset;
    result["size"] = size;
    result["type"] = std::string(type);
    return result;
}

Json tip_payload_layout() {
    Json result = Json::array();
    result.push_back(layout_field("pool_name", 0, 51, "ansi-z"));
    result.push_back(layout_field("cell_display_name", 51, 51, "ansi-z"));
    result.push_back(layout_field("stock_state", 102, 85, "tpool-stock-state"));
    return result;
}

Json tip_stock_state_fields() {
    Json result = Json::array();
    result.push_back(layout_field("market", 0, 2, "int16"));
    result.push_back(layout_field("code", 2, 6, "ansi-code"));
    result.push_back(layout_field("indate", 25, 4, "yyyymmdd-int32"));
    result.push_back(layout_field("intime", 29, 4, "hhmmss-int32"));
    result.push_back(layout_field("inprice", 33, 4, "float32"));
    result.push_back(layout_field("income", 49, 4, "float32"));
    result.push_back(layout_field("now", 53, 4, "float32"));
    result.push_back(layout_field("rise", 57, 4, "float32"));
    result.push_back(layout_field("volume", 61, 4, "int32"));
    result.push_back(layout_field("maxrate", 65, 4, "float32"));
    result.push_back(layout_field("maxperiod", 69, 4, "int32"));
    result.push_back(layout_field("maxprice", 73, 4, "float32"));
    result.push_back(layout_field("maxtime", 77, 4, "hhmmss-int32"));
    result.push_back(layout_field("idaynum", 81, 4, "int32"));
    return result;
}

void append_tip_semantics(Json& value) {
    value["ui_owner"] = "TPool.dll";
    value["host_callback"] = false;
    value["window_behavior"] = "topmost-timed-popup";
    value["batch_record_size_bytes"] = 187;
    value["payload_layout"] = tip_payload_layout();
    value["stock_state_fields"] = tip_stock_state_fields();
    value["default_stay_seconds"] = 20;
    value["stay_seconds_config"] = "tpool/config.ini [NEWTIP] StaySec";
    value["deduplicate_scope"] = "TPool tip-window record vector";
}

Json block_callback_arguments() {
    Json result = Json::array();
    const auto append = [&result](
                            std::string_view name, std::string_view value) {
        Json row = Json::object();
        row["name"] = std::string(name);
        row["value"] = std::string(value);
        result.push_back(std::move(row));
    };
    append("block_name", "configured-name");
    append("replace_index", "-1");
    append("security_buffer", "pointer");
    append("security_buffer_bytes", "security_count * 7");
    append("operation_id", "88");
    append("reserved", "0");
    append("lparam", "0");
    return result;
}

void append_block_callback_semantics(Json& value) {
    value["callback_registration"] =
        "TPool_RegisterCallBack argument 2 (dword_10067498)";
    value["callback_argument_count"] = 7;
    value["callback_operation_id"] = 88;
    value["callback_arguments"] = block_callback_arguments();
    value["security_record_size_bytes"] = 7;
    value["security_record_layout"] = Json::array();
    value["security_record_layout"].push_back(
        layout_field("market", 0, 1, "uint8"));
    value["security_record_layout"].push_back(
        layout_field("code", 1, 6, "fixed-ansi-code"));
    value["source_record_stride_bytes"] = 85;
    value["target_path_template"] = "blocknew/<configured-name>.blk";
}

}  // namespace

Json tpool_action_policy_document(const Json& raw,
                                  const std::string& cell_id) {
    const bool delete_enabled = enabled(raw, "bdel");
    const bool aim_pool_enabled = enabled(raw, "baimpool");
    const bool sound_enabled = enabled(raw, "bsound");
    const bool tip_enabled = enabled(raw, "btip");
    const bool save_block_enabled = enabled(raw, "bsavetoblock");
    const bool clear_block_enabled = enabled(raw, "bclearblock");
    const bool save_history_enabled = enabled(raw, "bsavehis");
    const int retention_count = json_integer_text(raw, "ndelnum", 0);
    const int retention_type = json_integer_text(raw, "ndeltype", 0);
    const int sound_type = json_integer_text(raw, "nsoundtype", 0);
    const auto sound_file = json_text(raw, "soundfile");
    const auto block_file = json_text(raw, "blockfile");
    const bool sound_ready = sound_type == 0 || !sound_file.empty();

    Json actions = Json::array();
    if (delete_enabled) {
        auto value = action("delete");
        append_retention_policy(value, delete_enabled, retention_count, retention_type);
        append_action(actions, std::move(value));
    }
    if (aim_pool_enabled) {
        auto value = action("record-entry-log");
        value["effective"] = true;
        value["trigger_scope"] = "cell-entry";
        value["storage_scope"] = "daily-cell-entry-log";
        value["format"] = "xml";
        value["path_template"] = "tpool/<pool>/<cell>/<YYYYMMDD>.log";
        value["record_element"] = "stk";
        value["deduplicate_by"] = string_array({"market", "code"});
        value["record_fields"] =
            string_array({"market", "code", "indate", "intime", "inprice"});
        value["existing_record_behavior"] = "replace-then-append";
        value["parameter_semantics"] =
            "TPool sub_10018380 writes matched 85-byte records into a per-pool, per-cell daily XML log";
        append_action(actions, std::move(value));
    }
    if (sound_enabled) {
        auto value = action("sound");
        value["effective"] = aim_pool_enabled && sound_ready;
        value["requires_aim_pool_enabled"] = true;
        value["trigger_scope"] = "cell-entry";
        value["sound_type_id"] = sound_type;
        value["mode"] = sound_type == 0 ? "default" : "custom";
        value["configured_file"] = sound_file.empty() ? Json(nullptr) : Json(sound_file);
        value["effective_file"] = sound_type == 0
            ? Json("sound\\default.wav")
            : (sound_file.empty() ? Json(nullptr) : Json(sound_file));
        value["ready"] = sound_ready;
        append_action(actions, std::move(value));
    }
    if (tip_enabled) {
        auto value = action("tip");
        value["effective"] = aim_pool_enabled;
        value["requires_aim_pool_enabled"] = true;
        value["trigger_scope"] = "cell-entry";
        append_tip_semantics(value);
        append_action(actions, std::move(value));
    }
    if (save_block_enabled) {
        auto value = action("save-block");
        value["effective"] = aim_pool_enabled && !block_file.empty();
        value["requires_aim_pool_enabled"] = true;
        value["trigger_scope"] = "cell-entry";
        value["block_file"] = block_file.empty() ? Json(nullptr) : Json(block_file);
        value["clear_before_save"] = clear_block_enabled;
        value["target_present"] = !block_file.empty();
        append_block_callback_semantics(value);
        append_action(actions, std::move(value));
    }
    if (save_history_enabled) {
        auto value = action("save-history");
        value["effective"] = aim_pool_enabled;
        value["requires_aim_pool_enabled"] = true;
        value["trigger_scope"] = "pool-state-serialization";
        value["storage_scope"] = "daily-cell-history-snapshot";
        value["format"] = "xml";
        value["path_template"] = "tpool/<pool>/<cell>/<YYYYMMDD>.dat";
        value["record_element"] = "stk";
        value["record_fields"] = string_array({
            "market", "code", "indate", "intime", "inprice", "income",
            "now", "rise", "volume", "maxrate", "maxperiod", "maxtime",
            "maxprice", "idaynum"});
        append_action(actions, std::move(value));
    }

    Json result = Json::object();
    result["cell_id"] = cell_id;
    result["present"] = true;
    result["raw"] = raw;
    result["delete"] = Json::object();
    append_retention_policy(
        result["delete"], delete_enabled, retention_count, retention_type);
    result["aim_pool_enabled"] = aim_pool_enabled;
    result["aim_pool_semantics"] =
        "baimpool enables a per-pool, per-cell YYYYMMDD.log entry record via sub_10018380";
    result["aim_pool"] = Json::object();
    result["aim_pool"]["enabled"] = aim_pool_enabled;
    result["aim_pool"]["kind"] = "record-entry-log";
    result["aim_pool"]["path_template"] =
        "tpool/<pool>/<cell>/<YYYYMMDD>.log";
    result["aim_pool"]["deduplicate_by"] = string_array({"market", "code"});
    result["aim_pool"]["record_fields"] =
        string_array({"market", "code", "indate", "intime", "inprice"});
    result["sound"] = Json::object();
    result["sound"]["enabled"] = sound_enabled;
    result["sound"]["type_id"] = sound_type;
    result["sound"]["mode"] = sound_type == 0 ? "default" : "custom";
    result["sound"]["file"] = sound_file.empty() ? Json(nullptr) : Json(sound_file);
    result["sound"]["ready"] = sound_ready;
    result["sound"]["effective"] =
        sound_enabled && aim_pool_enabled && sound_ready;
    result["tip_enabled"] = tip_enabled;
    result["tip_effective"] = tip_enabled && aim_pool_enabled;
    result["tip"] = Json::object();
    result["tip"]["enabled"] = tip_enabled;
    result["tip"]["effective"] = tip_enabled && aim_pool_enabled;
    append_tip_semantics(result["tip"]);
    result["block"] = Json::object();
    result["block"]["save_enabled"] = save_block_enabled;
    result["block"]["clear_enabled"] = clear_block_enabled;
    result["block"]["file"] = block_file.empty() ? Json(nullptr) : Json(block_file);
    result["block"]["effective"] =
        save_block_enabled && aim_pool_enabled && !block_file.empty();
    result["block"]["clear_is_save_option"] = true;
    append_block_callback_semantics(result["block"]);
    result["history_enabled"] = save_history_enabled;
    result["history_effective"] = save_history_enabled && aim_pool_enabled;
    result["history_trigger_scope"] = "pool-state-serialization";
    result["history_path_template"] =
        "tpool/<pool>/<cell>/<YYYYMMDD>.dat";
    result["configured_action_count"] =
        static_cast<std::uint64_t>(actions.size());
    result["actions"] = std::move(actions);
    result["execution_mode"] = "planned-only";
    result["evidence"] =
        "TPool.dll 325-byte psatt layout; sub_1001E610 maps ndeltype 0/1/2/3 to stored-history expiry and routes baimpool records through sub_10018380 daily XML entry logs; all file, sound, UI and block effects remain planned-only";
    return result;
}

Json tpool_action_plans_document(
    const Json& inspection,
    std::string_view cell_id,
    const std::set<std::string, std::less<>>& securities,
    std::string_view trigger) {
    Json result = Json::array();
    if (securities.empty()) return result;
    const auto* policies = optional(inspection, "action_policies");
    if (!policies || !policies->is_array()) return result;
    for (const auto& policy : policies->as_array()) {
        if (json_text(policy, "cell_id") != cell_id) continue;
        const auto* actions = optional(policy, "actions");
        if (!actions || !actions->is_array()) continue;
        for (const auto& configured : actions->as_array()) {
            const auto* effective = optional(configured, "effective");
            if (effective && effective->is_bool() && !effective->as_bool()) continue;
            const auto trigger_scope = json_text(configured, "trigger_scope");
            if (!trigger_scope.empty() && trigger_scope != "cell-entry") continue;
            Json planned = configured;
            planned["cell_id"] = std::string(cell_id);
            planned["trigger"] = std::string(trigger);
            planned["security_count"] =
                static_cast<std::uint64_t>(securities.size());
            const auto kind = json_text(planned, "kind");
            if (kind == "tip") {
                planned["payload_bytes"] =
                    static_cast<std::uint64_t>(securities.size() * 187);
            } else if (kind == "save-block") {
                planned["security_buffer_bytes"] =
                    static_cast<std::uint64_t>(securities.size() * 7);
            }
            planned["securities"] = string_set_json(securities);
            planned["execution_mode"] = "planned-only";
            planned["executed"] = false;
            result.push_back(std::move(planned));
        }
    }
    return result;
}

}  // namespace tdx::tpool_detail
