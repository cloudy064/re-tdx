#include "tdx/shape_match.hpp"

#include "shape_match_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct CandidateDocument {
    Json document;
    std::string source;
    bool network_used{};
    std::string local_error;
};

CandidateDocument load_candidate(const fs::path& root,
                                 const ShapeMatchQuery& query,
                                 const shape_match_detail::ShapeTemplate& shape) {
    if (!query.candidate_path.empty()) {
        CandidateDocument result;
        result.document = Json::parse(read_text_utf8(query.candidate_path));
        result.source = "candidate-json";
        return result;
    }
    if (query.market.empty() || query.code.empty())
        throw Error("score view requires --candidate or --market and --code");
    std::string period = query.period.empty()
        ? shape_match_detail::period_name(shape.period_code) : query.period;
    if (period == "unknown" || period == "intraday-custom" ||
        period == "seconds" || period == "seconds-custom")
        throw Error("template period is not available through the public K-line loader; pass --period explicitly");
    const auto required = std::max(1, shape.point_count);
    const int page_size = std::min(800, required);
    const int pages = (required + page_size - 1) / page_size;
    auto local = [&] {
        return load_local_kline_document(
            root, query.market, query.code, query.kind, period,
            pages, page_size, 0, "all");
    };
    auto network = [&] {
        return fetch_kline_document(
            query.market, query.code, query.kind, period,
            pages, page_size, 0, "all", query.timeout_ms, root, {});
    };
    if (query.source == "local") return {local(), "local", false, {}};
    if (query.source == "network") return {network(), "network", true, {}};
    CandidateDocument result;
    try {
        result.document = local();
        result.source = "local";
    } catch (const std::exception& error) {
        result.local_error = error.what();
        result.document = network();
        result.source = "network-after-local-miss";
        result.network_used = true;
    }
    return result;
}

const shape_match_detail::ShapeTemplate& select_template(
    const shape_match_detail::ShapeLibrary& library,
    const ShapeMatchQuery& query) {
    if (query.template_index && !query.template_name.empty())
        throw Error("use either --template or --template-index, not both");
    if (query.template_index) {
        if (*query.template_index >= library.templates.size())
            throw Error("template index is outside the shape library");
        return library.templates[*query.template_index];
    }
    if (query.template_name.empty())
        throw Error("score view requires --template or --template-index");
    const shape_match_detail::ShapeTemplate* selected = nullptr;
    for (const auto& shape : library.templates) {
        if (shape.name != query.template_name) continue;
        if (selected) throw Error("shape library contains duplicate template names");
        selected = &shape;
    }
    if (!selected) throw Error("shape template was not found");
    return *selected;
}

}  // namespace

Json load_shape_match(const fs::path& root, const ShapeMatchQuery& input) {
    ShapeMatchQuery query = input;
    query.view = lower_ascii(trim(query.view));
    query.source = lower_ascii(trim(query.source));
    query.period = lower_ascii(trim(query.period));
    query.kind = lower_ascii(trim(query.kind));
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    if (query.view != "library" && query.view != "score" &&
        query.view != "scan")
        throw Error("view must be library, score or scan");
    if (query.source != "auto" && query.source != "local" &&
        query.source != "network")
        throw Error("source must be auto, local or network");
    if (query.kind != "auto" && query.kind != "stock" && query.kind != "index")
        throw Error("kind must be auto, stock or index");
    const auto library_path = query.library_path.empty()
        ? root / "T0002" / "shapematch.dat" : query.library_path;
    const auto library = shape_match_detail::parse_shape_library(library_path);

    Json document = Json::object();
    document["schema"] = "tdx-shape-match-native-v1";
    document["view"] = query.view;
    document["native_cpp"] = true;
    document["library_path"] = path_utf8(library.path);
    document["library_available"] = library.available;
    document["library_byte_size"] =
        static_cast<std::uint64_t>(library.byte_size);
    document["record_size"] =
        static_cast<std::uint64_t>(shape_match_detail::kRecordSize);
    document["scope_code"] = library.available
        ? Json(library.scope_code) : Json(nullptr);
    document["scope"] = library.available
        ? Json(shape_match_detail::scope_name(library.scope_code)) : Json(nullptr);
    Json templates = Json::array();
    std::size_t security_templates = 0;
    std::size_t drawing_templates = 0;
    for (const auto& shape : library.templates) {
        if (shape.template_kind == 0) ++security_templates;
        if (shape.template_kind == 1) ++drawing_templates;
        templates.push_back(shape_match_detail::template_document(shape));
    }
    Json summary = Json::object();
    summary["template_count"] =
        static_cast<std::uint64_t>(library.templates.size());
    summary["security_template_count"] =
        static_cast<std::uint64_t>(security_templates);
    summary["hand_drawn_template_count"] =
        static_cast<std::uint64_t>(drawing_templates);
    document["summary"] = std::move(summary);
    document["templates"] = std::move(templates);
    document["network_used"] = false;
    document["evidence"] =
        "TDXDeep.dll CShapeMatch/CShapeSet, shapematch.dat 4 + N*94195, sub_100152B0 fixed vectors";

    if (query.view == "score") {
        if (!library.available)
            throw Error("shape library is absent: " + path_utf8(library.path));
        const auto& shape = select_template(library, query);
        auto candidate = load_candidate(root, query, shape);
        const auto bars = shape_match_detail::candidate_bars_from_document(
            candidate.document);
        document["selected_template"] =
            shape_match_detail::template_document(shape);
        document["score"] = shape_match_detail::score_template(shape, bars);
        Json source = Json::object();
        source["mode"] = candidate.source;
        source["market"] = query.market;
        source["code"] = query.code;
        source["period"] = query.period.empty()
            ? shape_match_detail::period_name(shape.period_code) : query.period;
        source["bar_count"] = static_cast<std::uint64_t>(bars.size());
        source["path"] = query.candidate_path.empty()
            ? Json(nullptr) : Json(path_utf8(query.candidate_path));
        if (!candidate.local_error.empty())
            source["local_error"] = candidate.local_error;
        document["candidate"] = std::move(source);
        document["network_used"] = candidate.network_used;
    }
    if (query.view == "scan") {
        if (!library.available)
            throw Error("shape library is absent: " + path_utf8(library.path));
        const auto& shape = select_template(library, query);
        auto scan = shape_match_detail::scan_shape_matches(
            root, query, shape, library.scope_code);
        document["selected_template"] =
            shape_match_detail::template_document(shape);
        document["scan"] = std::move(scan);
        document["network_used"] =
            document.at("scan").at("network_requests").as_number() > 0;
    }
    return document;
}

}  // namespace tdx
