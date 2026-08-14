#include "tdx/cloud_calc.hpp"

#include "cloud_calc_config_internal.hpp"
#include "cloud_calc_host_internal.hpp"
#include "cloud_calc_service_internal.hpp"
#include "tdx/common.hpp"

#include <map>
#include <set>

namespace fs = std::filesystem;

namespace tdx {

using namespace cloud_calc_detail;
using namespace cloud_calc_service_detail;
Json generate_cloud_calc_template_request(const fs::path &root, const std::string &cfg_name) {
    if (cfg_name.empty())
        throw Error("cloud-calc template cfg is required");
    require_safe_cfg_request_name(cfg_name);
    const auto cfg = resolve_cfg(root, cfg_name);
    const auto config = parse_config(cfg);
    struct FieldInfo {
        std::string code;
        std::string name;
        std::string datatype;
        std::set<std::string, std::less<>> units;
        std::set<std::string, std::less<>> required_by;
        std::set<std::string, std::less<>> reasons;
    };
    struct HostInfo : FieldInfo {
        std::string syscol;
        std::string security_suffix;
        std::string resolver;
    };
    std::map<std::string, FieldInfo, std::less<>> inputs;
    std::map<std::string, FieldInfo, std::less<>> derived;
    std::map<std::string, HostInfo, std::less<>> hosts;
    std::set<std::string, std::less<>> calculated_codes;
    std::set<std::string, std::less<>> resources;
    std::uint64_t calculation_count = 0;

    const auto resolver_for = [](std::string_view syscol) {
        if (syscol == "$BONDAI")
            return std::string("row-bond-actual-365");
        if (group_syscol(syscol))
            return std::string("row-group-members-plus-public-l1");
        if (syscol == "$TDXHY" || syscol == "$TDXHYCODE")
            return std::string("local-tdx-industry-hierarchy");
        if (syscol == "$PE")
            return std::string("public-finance-0x0010-plus-l1");
        if (finance_syscol(syscol))
            return std::string("public-finance-0x0010");
        if (speed_syscol(syscol))
            return std::string("public-l1-rise-speed-0x053e");
        if (depth_syscol(syscol))
            return std::string("public-l1-depth-0x0547");
        if (quote_syscol(syscol))
            return std::string("public-l1-0x054c");
        return std::string("unresolved-host");
    };
    const auto set_json = [](const std::set<std::string, std::less<>> &values) {
        Json result = Json::array();
        for (const auto &value : values)
            result.push_back(value);
        return result;
    };
    const auto field_json = [&](const FieldInfo &field) {
        Json result = Json::object();
        result["code"] = field.code;
        if (!field.name.empty())
            result["name"] = field.name;
        result["datatype"] = field.datatype.empty() ? "unknown" : field.datatype;
        result["units"] = set_json(field.units);
        result["required_by"] = set_json(field.required_by);
        result["reasons"] = set_json(field.reasons);
        return result;
    };

    for (const auto &unit : config.units) {
        if (!unit.file.empty())
            resources.insert(unit.file);
        std::map<std::string, const Column *, std::less<>> columns;
        std::set<std::string, std::less<>> calculated;
        for (const auto &column : unit.columns) {
            columns[column.code] = &column;
            if (!column.calc.empty()) {
                calculated.insert(column.code);
                calculated_codes.insert(column.code);
                ++calculation_count;
            }
        }
        const auto merge_field = [&](std::map<std::string, FieldInfo, std::less<>> &fields,
                                     const std::string &code, const Column *column,
                                     const std::string &required_by,
                                     const std::string &reason) -> FieldInfo & {
            auto &field = fields[code];
            field.code = code;
            if (column) {
                if (field.name.empty())
                    field.name = column->name;
                if (field.datatype.empty())
                    field.datatype = column->datatype;
            }
            field.units.insert(unit.id);
            if (!required_by.empty())
                field.required_by.insert(required_by);
            if (!reason.empty())
                field.reasons.insert(reason);
            return field;
        };
        const auto add_input = [&](const std::string &code, const std::string &required_by,
                                   const std::string &reason) {
            const auto found = columns.find(code);
            merge_field(inputs, code, found == columns.end() ? nullptr : found->second, required_by,
                        reason);
        };
        const auto add_identity = [&](const std::string &suffix, const std::string &required_by) {
            add_input("$ZQDM" + suffix, required_by, "referenced-security-code");
            add_input("$SC" + suffix, required_by, "referenced-security-market");
        };
        for (const auto &column : unit.columns) {
            if (column.calc.empty())
                continue;
            for (const auto &ref : column.refs) {
                double literal = 0.0;
                if (parse_double(ref, literal) || calculated.count(ref))
                    continue;
                const auto found = columns.find(ref);
                const auto *referenced = found == columns.end() ? nullptr : found->second;
                std::string syscol;
                if (referenced) {
                    syscol = referenced->syscol;
                    if (syscol.empty() && recognized_host_syscol(referenced->code))
                        syscol = referenced->code;
                } else if (recognized_host_syscol(ref))
                    syscol = ref;
                if (!syscol.empty()) {
                    auto &host = hosts[ref];
                    host.code = ref;
                    if (referenced && host.name.empty())
                        host.name = referenced->name;
                    if (referenced && host.datatype.empty())
                        host.datatype = referenced->datatype;
                    host.units.insert(unit.id);
                    host.required_by.insert(column.code);
                    host.reasons.insert("resolved-by-native-host-context");
                    host.syscol = syscol;
                    host.security_suffix = !referenced || referenced->refzqdm.empty()
                                               ? unit.refunit
                                               : referenced->refzqdm;
                    host.resolver = resolver_for(syscol);
                    if (syscol == "$BONDAI") {
                        add_input("MZ", column.code, "bond-face-value");
                        add_input("SGFXRQ", column.code, "previous-coupon-date");
                        add_input("XGFXRQ", column.code, "next-coupon-date");
                        add_input("SYFXLLXL", column.code, "remaining-coupon-rates");
                    } else if (group_syscol(syscol)) {
                        add_input("$S_ZQDM", column.code, "group-member-security-list");
                    } else {
                        add_identity(host.security_suffix, column.code);
                    }
                    continue;
                }
                if (ref == "DQLL2") {
                    auto &field =
                        merge_field(derived, ref, nullptr, column.code, "first-rate-from-SYFXLLXL");
                    if (field.datatype.empty())
                        field.datatype = "F";
                    add_input("SYFXLLXL", column.code, "remaining-coupon-rates");
                    continue;
                }
                add_input(ref, column.code,
                          referenced ? "cfg-source-column" : "external-formula-input");
            }
        }
    }

    Json input_fields = Json::array();
    Json row_template = Json::object();
    for (const auto &[code, field] : inputs) {
        input_fields.push_back(field_json(field));
        row_template[code] = Json();
    }
    Json host_fields = Json::array();
    for (const auto &[code, host] : hosts) {
        auto item = field_json(host);
        item["system_column"] = host.syscol;
        item["resolver"] = host.resolver;
        item["security_suffix"] = host.security_suffix;
        Json identity = Json::array();
        if (!group_syscol(host.syscol) && host.syscol != "$BONDAI") {
            identity.push_back("$SC" + host.security_suffix);
            identity.push_back("$ZQDM" + host.security_suffix);
        }
        item["identity_fields"] = std::move(identity);
        host_fields.push_back(std::move(item));
    }
    Json derived_fields = Json::array();
    for (const auto &[code, field] : derived)
        derived_fields.push_back(field_json(field));
    Json source_resources = Json::array();
    for (const auto &resource : resources)
        source_resources.push_back(resource);
    Json calculated = Json::array();
    for (const auto &code : calculated_codes)
        calculated.push_back(code);
    Json counts = Json::object();
    counts["input_fields"] = static_cast<std::uint64_t>(inputs.size());
    counts["host_fields"] = static_cast<std::uint64_t>(hosts.size());
    counts["derived_fields"] = static_cast<std::uint64_t>(derived.size());
    counts["calculated_fields"] = calculation_count;
    counts["units"] = static_cast<std::uint64_t>(config.units.size());
    Json result = Json::object();
    result["schema"] = template_schema;
    result["execution_mode"] = "native-cpp-offline";
    result["dll_loaded"] = false;
    result["cfg_name"] = cfg.filename().u8string();
    result["row_template"] = std::move(row_template);
    result["input_fields"] = std::move(input_fields);
    result["host_fields"] = std::move(host_fields);
    result["derived_fields"] = std::move(derived_fields);
    result["calculated_fields"] = std::move(calculated);
    result["source_resources"] = std::move(source_resources);
    result["counts"] = std::move(counts);
    return result;
}

} // namespace tdx
