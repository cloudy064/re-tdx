#include "formulas_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formulas.hpp"

#include <map>

namespace tdx::formulas_detail {
namespace {

const Profile profile{
    "tdx-2025-11-14",
    "13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5",
    "PE32 TCalc.dll, file timestamp 2025-11-14 16:10:21",
    {
        {0, 0x00133888, 0x002466E8, 222},
        {1, 0x002466F0, 0x002466EC, 107},
        {2, 0x002CAEE0, 0x002DD810, 15},
        {3, 0x002DD818, 0x002DD814, 35},
    },
    {
        {0, 0x00127DB0, 0x00127DAC, 16},
        {1, 0x00128070, 0x00128178, 6},
    },
};

const std::map<std::string, std::string, std::less<>>& recovered_sources() {
    static const auto values = [] {
        std::map<std::string, std::string, std::less<>> result;
        const auto library = load_bundled_formula_library_document();
        for (const auto& formula : library.at("formulas").as_array()) {
            if (!formula.is_object() ||
                !formula.as_object().count("source_text_origin") ||
                formula.at("source_text_origin").as_string() !=
                    "native-recovered")
                continue;
            result.emplace(formula.at("code").as_string(),
                           formula.at("source_text").as_string());
        }
        return result;
    }();
    return values;
}

}  // namespace

const Profile& supported_profile() { return profile; }

std::string recovered_native_source(const std::string& code) {
    const auto& sources = recovered_sources();
    const auto found = sources.find(code);
    return found == sources.end() ? std::string{} : found->second;
}

}  // namespace tdx::formulas_detail
