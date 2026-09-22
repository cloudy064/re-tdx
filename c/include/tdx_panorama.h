/* tdx_panorama.h - the market panorama: ten typed projections of GX resources.
 *
 * Each view is one JSN resource plus a list of (output name, source column) pairs, so the same
 * row data becomes a named, typed document instead of a bag of codes.  The registry itself is
 * GENERATED from the reference's table - see tdx_panorama_views.c - because a hundred field
 * mappings transcribed by hand is exactly the work that produced three rounds of mistakes
 * earlier in this project.
 *
 * THE CATALOG IS A VIEW TOO, and the default one: asking for nothing returns the registry, so a
 * caller can discover what is available without a resource fetch at all.
 *
 * A view's rows are keyed by the two fields its spec names - $SC/$ZQDM for most of them, and
 * $SC1/$ZQDM1 for the one that describes something other than the security itself.  Those two
 * names travel in the registry rather than being assumed, because assuming them would break
 * that view silently. */
#ifndef TDX_PANORAMA_H
#define TDX_PANORAMA_H

#include <stddef.h>

#include "tdx_bonds.h"
#include "tdx_error.h"
#include "tdx_jsn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_PANORAMA_ROWS_MAX 8192
#define TDX_PANORAMA_TEXT_MAX 256
/* The widest view the registry holds declares eighteen fields. */
#define TDX_PANORAMA_FIELDS_MAX 24

typedef struct tdx_panorama_field {
    const char *name;
    const char *column;
} tdx_panorama_field;

typedef struct tdx_panorama_view {
    const char *id;
    const char *label; /* UTF-8 bytes */
    const char *resource;
    const char *market_field;
    const char *code_field;
    size_t field_count;
    const tdx_panorama_field *fields;
} tdx_panorama_view;

extern const tdx_panorama_view tdx_panorama_views[];
extern const size_t tdx_panorama_view_count;

/* Finds a view by id, or NULL.  The id is compared case-insensitively. */
const tdx_panorama_view *tdx_panorama_find(const char *id);

/* One projected row: a security and the view's fields, in the view's own order.
 *
 * A field is marked present only when its cell HOLDS CHARACTERS.  A column the resource does
 * not carry and a cell that is empty both count as no value and render as null - the convention
 * the rest of this project follows, so a caller never has to tell an empty string from a missing
 * one. */
typedef struct tdx_panorama_row {
    int market_id;
    char security_id[24];
    char code[16];
    /* One presence flag and one value per field, in the registry's order.  The widest view in
     * the registry has EIGHTEEN fields, so sixteen - the count that looked generous - would
     * have refused it.  The extractor reports each view's width, which is how that was seen
     * before it became a runtime refusal. */
    unsigned char present[TDX_PANORAMA_FIELDS_MAX];
    tdx_bond_text values[TDX_PANORAMA_FIELDS_MAX];
} tdx_panorama_row;

/* Projects a fetched resource through a view.  Rows whose key columns are missing or malformed
 * are skipped and counted; the reference throws, which would cost a caller the whole resource
 * over one row. */
int tdx_panorama_project(const tdx_panorama_view *view, const tdx_jsn_document *doc,
                         const tdx_jsn_group *group, tdx_panorama_row *out, size_t capacity,
                         size_t *out_count, size_t *skipped_count, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_PANORAMA_H */
