#include "tdx/formula_render_profile.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

using Values = std::map<std::string, std::string, std::less<>>;

std::string lower_key(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

Values other_section(std::string_view text) {
    Values values;
    bool wanted = false;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        auto line = trim(std::string(text.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start)));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            wanted = lower_key(trim(line.substr(1, line.size() - 2))) == "other";
        } else if (wanted && !line.empty() && line.front() != ';' && line.front() != '#') {
            const auto separator = line.find('=');
            if (separator != std::string::npos) {
                auto key = lower_key(trim(line.substr(0, separator)));
                if (!key.empty()) values[key] = trim(line.substr(separator + 1));
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return values;
}

int integer_value(const Values& values, std::string_view key, int fallback) {
    const auto found = values.find(lower_key(std::string(key)));
    if (found == values.end()) return fallback;
    try {
        std::size_t used = 0;
        const auto value = std::stoi(found->second, &used);
        return used == found->second.size() ? value : fallback;
    } catch (...) {
        return fallback;
    }
}

std::string string_value(const Values& values, std::string_view key,
                         std::string fallback) {
    const auto found = values.find(lower_key(std::string(key)));
    return found == values.end() || found->second.empty() ? std::move(fallback)
                                                          : found->second;
}

bool equals_ascii_case_insensitive(const std::string& left, std::string_view right) {
    return lower_key(left) == lower_key(std::string(right));
}

}  // namespace

Json formula_render_environment_document(const fs::path& root) {
    const auto user_ini = root.empty() ? fs::path{} : root / "T0002" / "user.ini";
    Values values;
    bool source_available = false;
    if (!user_ini.empty() && fs::is_regular_file(user_ini)) {
        try {
            values = other_section(decode_gbk(read_bytes(user_ini)));
            source_available = true;
        } catch (...) {
            // Formula evaluation must remain available if a concurrently-written
            // or non-standard user.ini cannot be decoded.
        }
    }

    const int new_font_style = integer_value(values, "NewFontStyle", 0);
    const int elder_style = integer_value(values, "ElderStyle", 0);
    std::string face;
    int configured_height = 0;
    int weight = 400;
    std::string selected_profile;
    if (new_font_style == 1) {
        face = "微软雅黑";
        configured_height = -12;
        selected_profile = "recovered-new-font-style-table";
    } else {
        face = string_value(values, "FONTNAME2", "Arial");
        configured_height = integer_value(values, "FONTSIZE2", 15);
        weight = integer_value(values, "FontWeigth2", 400);
        selected_profile = source_available ? "tdx-user-ini-font-ordinal-2"
                                            : "recovered-legacy-font-table";
    }
    if (face.empty()) face = new_font_style == 1 ? "微软雅黑" : "Arial";
    if (configured_height == 0 || std::abs(configured_height) > 256)
        configured_height = new_font_style == 1 ? -12 : 15;
    if (weight < 0 || weight > 1000) weight = 400;

    // sub_690350 applies the ElderStyle adjustment after loading the ten
    // user-configurable legacy font rows.
    const int logical_height = elder_style == 0 ? configured_height
        : configured_height > 0 ? configured_height + 2 : configured_height - 2;
    const bool microsoft_yahei = equals_ascii_case_insensitive(face, "微软雅黑");
    const int height_adjustment = microsoft_yahei
        ? (std::abs(logical_height) > 12 ? 2 : 1) : 0;
    const int textout_y_adjustment = height_adjustment == 0 ? 0
        : height_adjustment == 1 ? -1 : -3;
    int native_chart_row_height = std::abs(logical_height) + 1;
    if (logical_height > 15)
        native_chart_row_height += 2 * std::abs(logical_height) - 30;
    if (logical_height < 0) native_chart_row_height += 5;
    native_chart_row_height = std::max(16, native_chart_row_height);

    Json keys = Json::object();
    keys["section"] = "Other";
    keys["style"] = "NewFontStyle";
    keys["elder_style"] = "ElderStyle";
    keys["face"] = "FONTNAME2";
    keys["height"] = "FONTSIZE2";
    keys["weight"] = "FontWeigth2";

    Json font = Json::object();
    font["native_font_table_index"] = 1;
    font["user_ini_font_ordinal"] = 2;
    font["selected_profile"] = selected_profile;
    font["source_available"] = source_available;
    font["source_path"] = source_available ? Json(path_utf8(user_ini)) : Json(nullptr);
    font["config_keys"] = std::move(keys);
    font["new_font_style"] = new_font_style;
    font["elder_style"] = elder_style != 0;
    font["face"] = face;
    font["configured_height"] = configured_height;
    font["logical_height"] = logical_height;
    font["logical_height_semantics"] = logical_height < 0
        ? "gdi-character-height" : "gdi-cell-height";
    font["weight"] = weight;
    font["italic"] = false;
    font["underline"] = false;
    font["strikeout"] = false;
    font["charset"] = microsoft_yahei ? 0 : 1;
    font["charset_name"] = microsoft_yahei ? "ANSI_CHARSET" : "DEFAULT_CHARSET";
    font["quality"] = microsoft_yahei ? Json(nullptr) : Json(4);
    font["quality_name"] = microsoft_yahei ? "runtime-dependent"
                                            : "ANTIALIASED_QUALITY";
    font["measurement_height_adjustment"] = height_adjustment;
    font["native_textout_aux_mode"] = height_adjustment;
    font["native_textout_y_adjustment_pixels"] = textout_y_adjustment;
    font["native_textout_y_adjustment_rule"] =
        "aux-mode==0?0:aux-mode==1?-1:-3";
    font["css_pixel_size"] = std::abs(logical_height);
    font["css_mapping"] = "browser-approximation-from-gdi-logical-height";

    // DRAWNUMBER_DIF selects table index 13 when the STYLE series at the
    // first rendered bar is exactly 2.  Unlike index 1, sub_690350 constructs
    // this row from hard-coded values and user.ini does not override it.
    Json sequence_style2_font = Json::object();
    sequence_style2_font["native_font_table_index"] = 13;
    sequence_style2_font["user_ini_font_ordinal"] = Json(nullptr);
    sequence_style2_font["selected_profile"] =
        "recovered-drawnumber-dif-style2-table";
    sequence_style2_font["source_available"] = true;
    sequence_style2_font["source_path"] = Json(nullptr);
    sequence_style2_font["native_source"] = "TdxW.exe!sub_690350";
    sequence_style2_font["style_condition"] =
        "DRAWNUMBER_DIF first-rendered-bar arg1==2";
    sequence_style2_font["face"] = "Arial";
    sequence_style2_font["configured_height"] = 15;
    sequence_style2_font["logical_height"] = 15;
    sequence_style2_font["logical_height_semantics"] = "gdi-cell-height";
    sequence_style2_font["weight"] = 400;
    sequence_style2_font["italic"] = false;
    sequence_style2_font["underline"] = false;
    sequence_style2_font["strikeout"] = false;
    sequence_style2_font["charset"] = 1;
    sequence_style2_font["charset_name"] = "DEFAULT_CHARSET";
    sequence_style2_font["quality"] = 4;
    sequence_style2_font["quality_name"] = "ANTIALIASED_QUALITY";
    sequence_style2_font["measurement_height_adjustment"] = 0;
    sequence_style2_font["native_textout_aux_mode"] = 0;
    sequence_style2_font["native_textout_y_adjustment_pixels"] = 0;
    sequence_style2_font["native_textout_y_adjustment_rule"] =
        "aux-mode==0?0:aux-mode==1?-1:-3";
    sequence_style2_font["css_pixel_size"] = 15;
    sequence_style2_font["css_mapping"] =
        "browser-approximation-from-gdi-logical-height";

    Json conditional_fonts = Json::object();
    conditional_fonts["13"] = std::move(sequence_style2_font);

    Json annotation = Json::object();
    annotation["font"] = std::move(font);
    annotation["conditional_fonts"] = std::move(conditional_fonts);
    annotation["background_mode"] = "transparent";
    annotation["background_mode_value"] = 1;
    annotation["text_encoding"] = "Win32-ANSI";
    annotation["native_chart_row_height_pixels"] = native_chart_row_height;
    annotation["native_chart_row_height_rule"] =
        "max(16,abs(logical-height)+1+(logical-height>15?2*abs(height)-30:0)+(logical-height<0?5:0))";
    annotation["native_chart_row_height_source"] = "TdxW.exe!sub_92A830+0x1EA";

    const bool real_up_k = integer_value(values, "RealUPK", 0) != 0;
    const bool vol_k_use_zt = integer_value(values, "VolKUseZT", 0) != 0;
    const bool bold_zb_line = integer_value(values, "BoldZBLine", 0) != 0;
    Json stick_keys = Json::object();
    stick_keys["section"] = "Other";
    stick_keys["real_up_k"] = "RealUPK";
    stick_keys["vol_k_use_zt"] = "VolKUseZT";
    Json series_sticks = Json::object();
    series_sticks["source_available"] = source_available;
    series_sticks["source_path"] = source_available
        ? Json(path_utf8(user_ini)) : Json(nullptr);
    series_sticks["config_keys"] = std::move(stick_keys);
    series_sticks["real_up_k"] = real_up_k;
    series_sticks["vol_k_use_zt"] = vol_k_use_zt;
    series_sticks["volume_color_rule"] = vol_k_use_zt
        ? "previous-close" : "open-close-with-flat-previous-close-fallback";
    series_sticks["volume_up_fill"] = real_up_k ? "solid" : "hollow";
    series_sticks["volume_down_fill"] = "solid";
    series_sticks["native_up_pen_index"] = 2;
    series_sticks["native_down_pen_index"] = 3;
    series_sticks["bar_body_width_rule"] =
        "spacing>=3?spacing-max(spacing*0.25,2):min(spacing,1);half=floor(body*0.5);pixels=2*half+1";
    series_sticks["native_config_source"] =
        "TdxW.exe!sub_987080/sub_956F40/sub_957030";

    Json line_keys = Json::object();
    line_keys["section"] = "Other";
    line_keys["bold_zb_line"] = "BoldZBLine";
    Json series_lines = Json::object();
    series_lines["source_available"] = source_available;
    series_lines["source_path"] = source_available
        ? Json(path_utf8(user_ini)) : Json(nullptr);
    series_lines["config_keys"] = std::move(line_keys);
    series_lines["bold_zb_line"] = bold_zb_line;
    series_lines["effective_default_width_rule"] =
        "width==1&&BoldZBLine?2:width;skip-if-width<1";
    series_lines["native_source"] = "TdxW.exe!sub_957620";

    Json result = Json::object();
    result["schema"] = "tdx-formula-render-environment-v1";
    result["native_source"] = "TdxW.exe";
    result["annotation"] = std::move(annotation);
    result["series_sticks"] = std::move(series_sticks);
    result["series_lines"] = std::move(series_lines);
    result["pixel_font_equivalent"] = false;
    result["pixel_boundary"] =
        "browser font rasterization is not GDI pixel-identical";
    return result;
}

}  // namespace tdx
