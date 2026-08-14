#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tdx {
struct ShapeMatchQuery;
}

namespace tdx::shape_match_detail {

inline constexpr std::size_t kHeaderSize = 4;
inline constexpr std::size_t kRecordSize = 94195;
inline constexpr std::size_t kMaximumPointCount = 2000;
inline constexpr std::size_t kBarOffset = 127;
inline constexpr std::size_t kBarSize = 35;
inline constexpr std::size_t kDrawingOffset = 70131;
inline constexpr std::size_t kDrawingPointSize = 12;
inline constexpr std::size_t kThresholdOffset = 94131;
inline constexpr std::size_t kActiveOffset = 94135;
inline constexpr std::size_t kWeightOffset = 94155;

struct ShapeBar {
    int year{};
    int month{};
    int day{};
    int hour{};
    int minute{};
    int second{};
    float open{};
    float close{};
    float volume{};
};

struct DrawingPoint {
    std::int32_t x{};
    std::int32_t y{};
    float value{};
};

struct ShapeTemplate {
    std::size_t record_index{};
    std::uint32_t id{};
    std::string name;
    std::uint16_t template_kind{};
    std::int16_t period_code{};
    std::int32_t point_count{};
    std::int16_t market_id{};
    std::string code;
    std::string security_name;
    std::vector<ShapeBar> bars;
    std::vector<DrawingPoint> drawing_points;
    std::int32_t threshold_percent{};
    std::array<bool, 4> active{};
    std::array<float, 4> weights{};
};

struct ShapeLibrary {
    std::filesystem::path path;
    bool available{};
    std::uintmax_t byte_size{};
    std::int32_t scope_code{};
    std::vector<ShapeTemplate> templates;
};

struct CandidateBar {
    float open{};
    float close{};
    float volume{};
};

ShapeLibrary parse_shape_library(const std::filesystem::path& path);
std::string scope_name(std::int32_t code);
std::string template_kind_name(std::uint16_t code);
std::string period_name(std::int16_t code);
std::string market_name(std::int16_t market_id);
Json template_document(const ShapeTemplate& value);

Json score_template(const ShapeTemplate& shape,
                    const std::vector<CandidateBar>& candidate);
std::vector<CandidateBar> candidate_bars_from_document(const Json& document);
Json scan_shape_matches(const std::filesystem::path& root,
                        const ShapeMatchQuery& query,
                        const ShapeTemplate& shape,
                        std::int32_t scope_code);

}  // namespace tdx::shape_match_detail
