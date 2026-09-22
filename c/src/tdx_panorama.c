/* tdx_panorama.c - the market panorama. */
#include "tdx_panorama.h"

#include <stdio.h>
#include <string.h>


static void lower_ascii(char *text) {
    size_t index;
    for (index = 0; text[index]; ++index)
        if (text[index] >= 'A' && text[index] <= 'Z')
            text[index] = (char)(text[index] - 'A' + 'a');
}

const tdx_panorama_view *tdx_panorama_find(const char *id) {
    size_t index;
    char scratch[64];

    if (!id || !*id)
        return NULL;
    snprintf(scratch, sizeof(scratch), "%s", id);
    lower_ascii(scratch);
    for (index = 0; index < tdx_panorama_view_count; ++index) {
        char candidate[64];
        snprintf(candidate, sizeof(candidate), "%s", tdx_panorama_views[index].id);
        lower_ascii(candidate);
        if (strcmp(candidate, scratch) == 0)
            return &tdx_panorama_views[index];
    }
    return NULL;
}

static int valid_code(const tdx_bond_text *text) {
    size_t index;
    if (!text->present || !text->data || text->length != 6)
        return 0;
    for (index = 0; index < 6; ++index)
        if (text->data[index] < '0' || text->data[index] > '9')
            return 0;
    return 1;
}

int tdx_panorama_project(const tdx_panorama_view *view, const tdx_jsn_document *doc,
                         const tdx_jsn_group *group, tdx_panorama_row *out, size_t capacity,
                         size_t *out_count, size_t *skipped_count, tdx_error *err) {
    size_t stored = 0;
    size_t skipped = 0;
    size_t row;

    if (out_count)
        *out_count = 0;
    if (skipped_count)
        *skipped_count = 0;
    if (!view || !doc || !group || !out) {
        tdx_error_set(err, "projecting a panorama view needs a view, a document, a group and "
                           "an output");
        return TDX_ERR;
    }
    if (view->field_count > TDX_PANORAMA_FIELDS_MAX) {
        tdx_error_set(err, "view %s declares %zu fields, more than this reader holds",
                      view->id, view->field_count);
        return TDX_ERR;
    }
    for (row = 0; row < group->row_count; ++row) {
        tdx_panorama_row *item;
        tdx_bond_text code;
        tdx_bond_text market;
        int market_id = -1;
        char prefix[16];
        size_t index;

        if (stored >= capacity) {
            tdx_error_set(err, "the resource holds more than %zu rows", capacity);
            return TDX_ERR;
        }
        code = tdx_bonds_cell_text(doc, group, row, view->code_field);
        market = tdx_bonds_cell_text(doc, group, row, view->market_field);
        /* A row that cannot name its security is skipped and counted rather than failing the
         * whole resource. */
        if (!valid_code(&code) ||
            tdx_bonds_market_id(market.data, market.length, &market_id, NULL) != TDX_OK) {
            skipped++;
            continue;
        }
        if (tdx_bonds_market_prefix(market_id, prefix, sizeof(prefix), err) != TDX_OK)
            return TDX_ERR;
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        item->market_id = market_id;
        snprintf(item->security_id, sizeof(item->security_id), "%s%.*s", prefix,
                 (int)code.length, code.data);
        snprintf(item->code, sizeof(item->code), "%.*s", (int)code.length, code.data);
        for (index = 0; index < view->field_count; ++index) {
            tdx_bond_text value = tdx_bonds_cell_text(doc, group, row, view->fields[index].column);
            item->values[index] = value;
            item->present[index] = (unsigned char)(value.present && value.data != NULL);
        }
        stored++;
    }
    if (out_count)
        *out_count = stored;
    if (skipped_count)
        *skipped_count = skipped;
    return TDX_OK;
}
