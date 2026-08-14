#include "relative_valuation_internal.hpp"

#include <algorithm>
#include <array>

namespace tdx::detail::relative_valuation {
namespace {

constexpr std::array<IndexTypeSpec, 5> index_types{{
    {"broad", "1", "broad-index", IndexTypeKind::broad},
    {"industry", "2", "industry-index", IndexTypeKind::industry},
    {"composite", "3", "composite-index", IndexTypeKind::composite},
    {"theme", "4", "theme-index", IndexTypeKind::theme},
    {"scale", "5", "scale-index", IndexTypeKind::scale},
}};

struct MethodAlias { std::string_view alias; MethodKind kind; };
constexpr std::array<MethodSpec, 3> methods{{
    {"pe-ttm", "PETTM", "PE(TTM)", MethodKind::pe_ttm},
    {"pb-mrq", "PBMRQ", "PB(MRQ)", MethodKind::pb_mrq},
    {"ps-ttm", "PSTTM", "PS(TTM)", MethodKind::ps_ttm},
}};
constexpr std::array<MethodAlias, 9> method_aliases{{
    {"pettm", MethodKind::pe_ttm}, {"pe-ttm", MethodKind::pe_ttm},
    {"pe", MethodKind::pe_ttm}, {"pbmrq", MethodKind::pb_mrq},
    {"pb-mrq", MethodKind::pb_mrq}, {"pb", MethodKind::pb_mrq},
    {"psttm", MethodKind::ps_ttm}, {"ps-ttm", MethodKind::ps_ttm},
    {"ps", MethodKind::ps_ttm},
}};

constexpr std::array<BenchmarkSpec, 5> benchmarks{{
    {"000001", "上证指数"}, {"000300", "沪深300"},
    {"000016", "上证50"}, {"000905", "中证500"},
    {"399006", "创业板指"},
}};

template <typename Values, typename Projection>
constexpr bool unique_by(const Values& values, Projection projection) {
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (projection(values[left]) == projection(values[right])) return false;
    return true;
}

static_assert(unique_by(index_types,
    [](const IndexTypeSpec& value) { return std::string_view(value.id); }));
static_assert(unique_by(index_types,
    [](const IndexTypeSpec& value) { return std::string_view(value.code); }));
static_assert(unique_by(methods,
    [](const MethodSpec& value) { return std::string_view(value.code); }));
static_assert(unique_by(method_aliases,
    [](const MethodAlias& value) { return value.alias; }));
static_assert(unique_by(benchmarks,
    [](const BenchmarkSpec& value) { return std::string_view(value.code); }));

}  // namespace

const IndexTypeSpec& index_type_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto found = std::find_if(index_types.begin(), index_types.end(),
        [&](const IndexTypeSpec& spec) {
            return value == spec.id || value == spec.code;
        });
    if (found == index_types.end())
        throw Error("index_type must be broad/1, industry/2, composite/3, theme/4, or scale/5");
    return *found;
}

const MethodSpec& method_spec(std::string_view raw) {
    const auto value = lower_ascii(trim(std::string(raw)));
    const auto alias = std::find_if(method_aliases.begin(), method_aliases.end(),
        [&](const MethodAlias& entry) { return entry.alias == value; });
    if (alias == method_aliases.end())
        throw Error("method must be pe-ttm/PETTM, pb-mrq/PBMRQ, or ps-ttm/PSTTM");
    return *std::find_if(methods.begin(), methods.end(),
        [&](const MethodSpec& spec) { return spec.kind == alias->kind; });
}

const BenchmarkSpec& benchmark_spec(std::string_view raw) {
    const auto value = trim(std::string(raw));
    const auto found = std::find_if(benchmarks.begin(), benchmarks.end(),
        [&](const BenchmarkSpec& spec) { return value == spec.code; });
    if (found == benchmarks.end())
        throw Error("benchmark must be 000001, 000300, 000016, 000905, or 399006");
    return *found;
}

}  // namespace tdx::detail::relative_valuation
