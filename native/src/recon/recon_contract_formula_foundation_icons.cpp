#include "recon_contract_formula_foundation_internal.hpp"

#include "recon_contract_internal.hpp"

namespace tdx::recon_contract_detail {

void validate_formula_icons_contract(const Json& document, Json& result) {
    const bool resource =
        string_is(member(document, "schema"), "tdx-formula-icon-sprite-v1") &&
        string_is(member(document, "profile"), "tdx-2025-11-14") &&
        string_is(member(document, "resource_type_name"), "RT_BITMAP") &&
        number_is(member(document, "resource_type"), 2.0) &&
        number_is(member(document, "resource_id"), 2060.0);
    add_assertion(result, "native_resource", resource,
                  "tdx-2025-11-14 RT_BITMAP/2060", resource);
    const bool geometry = number_is(member(document, "width"), 1800.0) &&
        number_is(member(document, "height"), 18.0) &&
        number_is(member(document, "cell_width"), 18.0) &&
        number_is(member(document, "cell_height"), 18.0) &&
        number_is(member(document, "cell_count"), 100.0) &&
        number_is(member(document, "official_type_min"), 1.0) &&
        number_is(member(document, "official_type_max"), 51.0);
    add_assertion(result, "sprite_geometry", geometry,
                  "1800x18, 100 cells, official 1..51", geometry);
    const bool endpoints = string_is(member(document, "png_endpoint"),
            "/api/v1/formulas/drawicon-strip.png") &&
        string_is(member(document, "bitmap_endpoint"),
            "/api/v1/formulas/drawicon-strip.bmp") &&
        member(document, "png_bytes") && member(document, "png_bytes")->is_number() &&
        member(document, "png_bytes")->as_number() > 100.0;
    add_assertion(result, "browser_sprite", endpoints, true, endpoints);
}

}  // namespace tdx::recon_contract_detail
