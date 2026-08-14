#include "tdx/curated_data.hpp"
#include "tdx/curated_data_internal.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace detail {
namespace curated_data {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& hong_kong_names) {
    if (market == 44) market = 2;
    const bool hong_kong = market == 31 || market == 48 || market == 49;
    if (!digits(code, hong_kong ? 5 : 6)) return Json(nullptr);
    auto name = std::string{};
    const auto found = securities.find({market, code});
    if (found != securities.end()) name = found->second.name;
    if (hong_kong) {
        const auto hk = hong_kong_names.find(code);
        if (hk != hong_kong_names.end()) name = hk->second;
    }
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

std::map<std::string, std::string> load_hong_kong_names(const fs::path& root) {
    std::map<std::string, std::string> result;
    const auto path = root / "T0002" / "hq_cache" / "tdxhkag.cfg";
    if (!fs::is_regular_file(path)) return result;
    std::istringstream input(decode_gbk(read_bytes(path)));
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find('|');
        if (first == std::string::npos) continue;
        const auto second = line.find('|', first + 1);
        const auto code = trim(line.substr(0, first));
        const auto name = trim(line.substr(first + 1,
            second == std::string::npos ? std::string::npos : second - first - 1));
        if (digits(code, 5) && !name.empty()) result[code] = name;
    }
    return result;
}

}  // namespace curated_data
}  // namespace detail
}  // namespace tdx
