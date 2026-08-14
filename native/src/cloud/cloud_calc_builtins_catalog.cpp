#include "cloud_calc_builtins_internal.hpp"

#include <iomanip>
#include <sstream>

namespace tdx::cloud_calc_detail {
namespace {

constexpr std::array<Builtin, 36> kBuiltins{{
    {100, 2, "$B_GetDateDiff_d$",      0x10001170, true},
    {101, 2, "$B_GetDateDiff_yd_y$",   0x100011C0, true},
    {102, 2, "$B_GetDateDiff_yd_d$",   0x100011C0, true},
    {103, 2, "$B_GetDateDiff_yd1_y$",  0x100012D0, true},
    {104, 2, "$B_GetDateDiff_yd1_d$",  0x100012D0, true},
    {105, 3, "$B_GetYearTime$",        0x100013E0, true},
    {106, 3, "$B_GetYearTime2$",       0x10001410, true},
    {107, 2, "$B_GetDay_Before$",      0x10001490, true},
    {108, 2, "$B_GetDay_After$",       0x100014C0, true},
    {109, 1, "$B_GetDayofWeek$",       0x100015A0, true},
    {110, 3, "$B_CalcRemainTime$",     0x100015E0, true},
    {111, 2, "$B_CalcRemainTime2$",    0x10001620, true},
    {112, 4, "$B_CalcAITime_All$",     0x10001810, true},
    {113, 3, "$B_CalcAITime$",         0x100016B0, true},
    {114, 3, "$B_CalcAITime2$",        0x10001750, true},
    {115, 3, "$B_CalcNDTime$",         0x10001850, true},
    {116, 2, "$B_GetPayCount$",        0x10001910, true},
    {117, 2, "$B_GetNextPayTime$",     0x10001990, true},
    {118, 9, "$B_CalcAI_All$",         0x10001B00, true},
    {119, 4, "$B_CalcAI$",             0x10001A00, true},
    {120, 3, "$B_CalcAI_CI_E$",        0x10001A30, true},
    {121, 4, "$B_CalcAI_Z_E$",         0x10001A50, true},
    {122,10, "$B_CalcPV_All$",         0x10001D80, true},
    {123, 5, "$B_CalcPV$",             0x10001B60, true},
    {124, 6, "$B_CalcPV_Fixed$",       0x10001C30, true},
    {125, 5, "$B_CalcPV_E$",           0x10001CD0, true},
    {126, 5, "$B_CalcPV_CI$",          0x10001CF0, true},
    {127, 3, "$B_CalcPV_Z_E$",         0x10001D60, true},
    {128, 2, "$B_CalcCV$",             0x10001E60, true},
    {129,10, "$B_CalcYTM_All$",        0x10002420, true},
    {130, 5, "$B_CalcYTM$",            0x10001E80, true},
    {131, 6, "$B_CalcYTM_Fixed$",      0x10002190, true},
    {132, 5, "$B_CalcYTM_E$",          0x10002370, true},
    {133, 5, "$B_CalcYTM_CI$",         0x10002390, true},
    {134, 3, "$B_CalcYTM_Z_E$",        0x10002400, true},
    {1000,0, "$SF_CurrDate$",          0x10089B80, true},
}};

constexpr bool unique_builtins() {
    for (std::size_t left = 0; left < kBuiltins.size(); ++left)
        for (std::size_t right = left + 1; right < kBuiltins.size(); ++right)
            if (kBuiltins[left].id == kBuiltins[right].id ||
                std::string_view(kBuiltins[left].name) == kBuiltins[right].name)
                return false;
    return true;
}
static_assert(unique_builtins(), "cloud-calc builtin ids and names must be unique");

}  // namespace

const std::array<Builtin, 36>& builtin_registry() {
    return kBuiltins;
}

const Builtin* find_builtin(std::string_view name) {
    for (const auto& builtin : kBuiltins)
        if (name == builtin.name) return &builtin;
    return nullptr;
}

Json builtin_catalog(const std::map<std::string, std::uint64_t, std::less<>>& usage) {
    Json values = Json::array();
    for (const auto& builtin : kBuiltins) {
        Json item = Json::object();
        item["id"] = builtin.id;
        item["argc"] = builtin.argc;
        item["name"] = builtin.name;
        std::ostringstream address;
        address << "0x" << std::uppercase << std::hex << builtin.handler;
        item["handler"] = address.str();
        item["implemented"] = builtin.implemented;
        item["implementation_basis"] = "recovered-handler-port";
        const auto found = usage.find(builtin.name);
        item["audited_config_calls"] = found == usage.end() ? 0 : found->second;
        item["used_by_audited_configs"] = found != usage.end();
        values.push_back(std::move(item));
    }
    return values;
}

}  // namespace tdx::cloud_calc_detail

