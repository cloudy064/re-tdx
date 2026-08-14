#include "institution_analysis_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::institution_analysis_detail {

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

int bounded(const std::string& text, const std::string& name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace tdx::institution_analysis_detail

namespace tdx {

using namespace institution_analysis_detail;

Json compose_institution_analysis_document(
    const InstitutionAnalysisQuery& options,
    const Json& source_documents,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto view_id = lower_ascii(trim(options.view));
    if (view_id == "catalog") return catalog_document();
    if (options.offset < 0 || options.offset > 1'000'000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be supplied together");
    if (!options.code.empty() && !six_digits(options.code))
        throw Error("code must contain six digits");
    if (!options.market.empty()) (void)market_id(options.market);

    Json sections = Json::array();
    if (view_id == "security") {
        if (options.code.empty()) throw Error("security view requires market and code");
        for (const auto& view : views())
            sections.push_back(section_document(
                view, document_for(source_documents, view.resource), options, securities));
    } else {
        const auto* view = find_view(view_id);
        if (!view) throw Error("unknown institution-analysis view: " + options.view);
        sections.push_back(section_document(
            *view, document_for(source_documents, view->resource), options, securities));
    }

    std::uint64_t matched = 0, returned = 0;
    Json sources = Json::array();
    for (const auto& section : sections.as_array()) {
        matched += static_cast<std::uint64_t>(section.at("matched").as_number());
        returned += static_cast<std::uint64_t>(section.at("returned").as_number());
        sources.push_back(section.at("source"));
    }
    Json counts = Json::object();
    counts["sections"] = static_cast<std::uint64_t>(sections.size());
    counts["matched"] = matched;
    counts["returned"] = returned;
    Json result = Json::object();
    result["schema"] = "tdx-institution-analysis-native-v1";
    result["generated_at"] = now_text();
    result["view"] = view_id;
    result["market"] = options.market.empty()
        ? Json(nullptr) : Json(market_name(market_id(options.market)));
    result["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    result["query"] = options.query;
    result["counts"] = std::move(counts);
    result["sections"] = std::move(sections);
    result["sources"] = std::move(sources);
    result["upstream_health"] = jsn_sources_health(result.at("sources"));
    result["semantics"] =
        "Institution master tables describe the latest available disclosure period; per-security history and top holders remain in market institution. Fund-exclusive rows deliberately retain one row per fund-management-company/security pair. Huijin/CSF market values depend on the client host total-market-cap syscol and are not fabricated; disclosed ratios and their sum check remain available. Development-bank holdings preserve the source shareholder-rank and holding-detail text without guessing numeric values from prose.";
    if (view_id == "stake-building")
        result["semantics"] =
            "TZCG108 is the client 被举牌 disclosure view. Share counts are source base shares, ratios are percentage points, and period return reproduces (price2/price1-1)*100. Whether the holder may continue increasing and whether it is insurance capital remain source labels; this view is distinct from the general ownership increase ledger.";
    return result;
}

}  // namespace tdx
