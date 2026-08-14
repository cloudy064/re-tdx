#include "level2_sdk_security_classifier.hpp"

#include "tdx/common.hpp"

#include <string>

namespace tdx::level2_detail {
namespace {

int code_number(const std::string& code) {
    int result = 0;
    for (const auto value : code)
        result = result * 10 + (value - '0');
    return result;
}

int classify_market_zero(const std::string& code) {
    switch (code[0]) {
    case '0':
        if (code[1] == '0')
            return code[2] == '2' || code[2] == '3' || code[2] == '4'
                ? 8 : 0;
        if (code[1] == '3' || code[1] == '8') return 1;
        return 10;
    case '1':
        switch (code[1]) {
        case '0':
        case '9': return 2;
        case '1':
        case '4': return 3;
        case '2':
            if (code[2] == '1' && code[3] >= '5') return 3;
            return code[2] == '0' ? 3 : 4;
        case '3': return code[2] == '1' ? 5 : 3;
        case '5':
        case '6':
        case '7':
        case '8': return 6;
        default: return 10;
        }
    case '2':
        if (code[1] == '3' || code[1] == '8') return 1;
        return code[1] == '0' ? 7 : 10;
    case '3':
        if (code[1] == '0') return 9;
        return code[1] == '8' ? 1 : 10;
    case '5':
        if (code[1] == '0' || code[1] == '1' || code[1] == '2')
            return 3;
        if (code[1] == '6' || code[1] == '7' || code[1] == '8' ||
            code[1] == '9')
            return 2;
        return 10;
    default: return 10;
    }
}

int classify_market_one(const std::string& code) {
    switch (code[0]) {
    case '0': return code_number(code) < 1000 ? 20 : 13;
    case '1':
        if (code[1] != '1' || code[2] == '2' || code[2] == '4' ||
            code[2] == '5')
            return 14;
        return 15;
    case '2':
        if (code[1] == '0' || code[1] == '1' || code[1] == '2')
            return 16;
        return code[1] == '3' ? 13 : 14;
    case '5':
        if (code[1] == '8' && (code[2] == '0' || code[2] == '2'))
            return 12;
        return 17;
    case '6':
        if (code[1] == '8' && (code[2] == '8' || code[2] == '9'))
            return 19;
        return 11;
    case '7': return code[1] == '5' || code[1] == '7' ? 13 : 20;
    case '9':
        return code[1] == '0' && code[2] == '0' ? 18 : 20;
    default: return 20;
    }
}

int classify_market_two(const std::string& code) {
    if (code[0] == '4') return code[1] == '3' ? 21 : 24;
    if (code[0] == '8') {
        if (code[1] == '3' || code[1] == '7') return 21;
        if (code[1] == '1') return 22;
        if (code[1] == '2' && code[2] != '0') return 23;
        return 24;
    }
    if (code[0] == '9' && code[1] == '2' && code[2] == '0')
        return 21;
    return 24;
}

bool sub_594680_predicate(int security_class_raw) {
    switch (security_class_raw) {
    case 2:
    case 3:
    case 4:
    case 5:
    case 13:
    case 14:
    case 15:
    case 16:
    case 22:
    case 23: return true;
    default: return false;
    }
}

}  // namespace

Level2SdkSecurityClassification classify_level2_sdk_security(
    std::uint16_t market_id, const std::string& code,
    std::string_view contract_label) {
    const std::string label(contract_label);
    if (market_id > 2)
        throw Error(label + " market must be 0, 1, or 2");
    if (code.size() != 6)
        throw Error(label + " code must contain exactly 6 digits");
    for (const auto value : code) {
        if (value < '0' || value > '9')
            throw Error(label + " code must contain exactly 6 digits");
    }

    const auto security_class_raw = market_id == 0
        ? classify_market_zero(code)
        : market_id == 1 ? classify_market_one(code)
                         : classify_market_two(code);
    return {security_class_raw,
            sub_594680_predicate(security_class_raw)};
}

}  // namespace tdx::level2_detail
