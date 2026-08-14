#include "tqlex_internal.hpp"

#include "tdx/http.hpp"

#include <set>

namespace tdx {

namespace {

struct PaginationPolicy {
    int start_page;
    int page_size;
    int max_pages;

    void validate() const {
        if (start_page < 0 || page_size < 1 || max_pages < 1 || max_pages > 1000)
            throw Error("TQLEX pagination arguments are invalid");
    }
};

}  // namespace

Json query_tqlex(const std::string& entry, const Json& request,
                 const std::string& base_url, int timeout_ms,
                 const TqlexTransport& transport) {
    if (entry.empty()) throw Error("TQLEX Entry is required");
    if (!request.is_array() && !request.is_object())
        throw Error("TQLEX request must be an array or object");
    const auto url = base_url + (base_url.find('?') == std::string::npos ? "?" : "&") +
                     "Entry=" + url_encode(entry);
    const auto payload_text = request.dump(-1);
    const Bytes payload(payload_text.begin(), payload_text.end());
    Bytes raw;
    if (transport) raw = transport(url, payload, timeout_ms);
    else {
        const auto response = http_post(url, payload,
            {{"Accept", "application/json"}, {"Content-Type", "application/json"}},
            timeout_ms);
        if (response.status < 200 || response.status >= 300)
            throw Error("TQLEX HTTP status " + std::to_string(response.status));
        raw = response.body;
    }
    while (!raw.empty() && raw.back() == 0) raw.pop_back();
    const std::string bytes(reinterpret_cast<const char*>(raw.data()), raw.size());
    std::string text;
    try {
        (void)utf8_to_wide(bytes);
        text = bytes;
    } catch (const Error&) {
        text = decode_gbk(raw);
    }
    const auto result = Json::parse(text);
    const auto& object = detail::tqlex_response_object(result);
    if (const auto* error = detail::tqlex_object_field(object, "error"); error && detail::json_truthy(*error))
        throw Error("TQLEX server returned error: " + error->dump(-1));
    if (const auto* code = detail::tqlex_object_field(object, "ErrorCode")) {
        const int numeric = detail::json_integer(*code, "TQLEX ErrorCode");
        if (numeric) {
            const auto* info = detail::tqlex_object_field(object, "ErrorInfo");
            throw Error("TQLEX server returned ErrorCode " + std::to_string(numeric) +
                        (info ? ": " + info->dump(-1) : ""));
        }
    }
    return result;
}

Json query_tqlex_all_pages(const std::string& entry, const Json& request,
                           int start_page, int page_size, int max_pages,
                           const std::string& base_url, int timeout_ms,
                           const TqlexTransport& transport) {
    const PaginationPolicy policy{start_page, page_size, max_pages};
    policy.validate();
    std::vector<Json> pages;
    std::vector<bool> paged_sets;
    std::set<std::string> signatures;
    for (int offset = 0; offset < max_pages; ++offset) {
        Json current = request;
        set_tqlex_request_value(current, "Page", std::to_string(start_page + offset));
        set_tqlex_request_value(current, "PageSize", std::to_string(page_size));
        auto response = query_tqlex(entry, current, base_url, timeout_ms, transport);
        const auto rows = detail::tqlex_result_set_rows(response);
        const auto& sets = detail::tqlex_result_sets(response);
        if (paged_sets.empty()) {
            paged_sets.reserve(sets.size());
            for (const auto& set : sets) paged_sets.push_back(detail::tqlex_result_content(set).size() ==
                                                              static_cast<std::size_t>(page_size));
        }
        std::string signature;
        for (const auto& set : sets) signature += detail::tqlex_result_content(set).empty()
            ? "[]" : Json(detail::tqlex_result_content(set)).dump(-1);
        if (rows && !signatures.insert(signature).second)
            throw Error("TQLEX server repeated a page; refusing an infinite loop");
        pages.push_back(std::move(response));
        if (rows < static_cast<std::size_t>(page_size)) return detail::merge_tqlex_pages(pages, paged_sets);
    }
    throw Error("TQLEX response did not finish within the page safety limit");
}

}  // namespace tdx
