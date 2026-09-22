#include "tdx_jsn_json.h"
#include "tdx_json_write.h"
#include <stdlib.h>
#include <string.h>

#define LITERAL(out, err, text) tdx_buf_append(out, text, sizeof(text) - 1, err)
typedef struct column_name {
    const char *original;
    size_t length;
    tdx_buf emitted;
} column_name;
static int equal(const char *left, size_t left_size, const char *right, size_t right_size) {
    return left_size == right_size && (!left_size || memcmp(left, right, left_size) == 0);
}
static int reserved(const char *name, size_t length) {
    return equal(name, length, "type", 4) || equal(name, length, "resource", 8) ||
           equal(name, length, "group", 5) || equal(name, length, "row", 3);
}
static int collision(const column_name *names, size_t count, size_t current, int changed) {
    size_t index;
    const tdx_buf *candidate = &names[current].emitted;
    for (index = 0; index < count; ++index) {
        /* Keep original non-reserved columns available even when they occur later. */
        if (changed && index != current &&
            equal((const char *)candidate->data, candidate->len, names[index].original,
                  names[index].length)) return 1;
        if (index < current && equal((const char *)candidate->data, candidate->len,
                                     (const char *)names[index].emitted.data,
                                     names[index].emitted.len)) return 1;
    }
    return 0;
}
int tdx_jsn_format_row(tdx_buf *out, const tdx_jsn_document *document,
                        size_t group_index, size_t row_index, const char *resource,
                        size_t *renamed, tdx_error *err) {
    const tdx_jsn_group *group;
    const tdx_json_node *header;
    column_name *names = NULL;
    size_t index, changed_count = 0;
    int result = TDX_ERR;
    if (renamed) *renamed = 0;
    if (!out || !document || group_index >= document->group_count) {
        tdx_error_set(err, "rendering a JSN row needs a buffer and valid group");
        return TDX_ERR;
    }
    group = &document->groups[group_index];
    if (row_index >= group->row_count || group->headers_index >= document->json.node_count ||
        group->column_count > SIZE_MAX / sizeof(*names)) {
        tdx_error_set(err, "JSN row or column index is invalid");
        return TDX_ERR;
    }
    names = (column_name *)calloc(group->column_count ? group->column_count : 1, sizeof(*names));
    if (!names) {
        tdx_error_set(err, "out of memory for JSN column names");
        return TDX_ERR;
    }
    header = tdx_json_first(&document->json, &document->json.nodes[group->headers_index]);
    for (index = 0; index < group->column_count; ++index) {
        if (!header || !(names[index].original = tdx_json_text(&document->json, header))) {
            tdx_error_set(err, "JSN column %zu is not a string", index);
            goto done;
        }
        names[index].length = header->text_length;
        header = tdx_json_next(&document->json, header);
    }
    for (index = 0; index < group->column_count; ++index) {
        size_t suffix = 2;
        int changed = reserved(names[index].original, names[index].length);
        if (tdx_buf_append(&names[index].emitted, names[index].original, names[index].length, err) != TDX_OK ||
            (changed && LITERAL(&names[index].emitted, err, "_cell") != TDX_OK)) goto done;
        while (collision(names, group->column_count, index, changed)) {
            changed = 1;
            tdx_buf_clear(&names[index].emitted);
            if (tdx_buf_append(&names[index].emitted, names[index].original, names[index].length, err) != TDX_OK ||
                tdx_buf_append_printf(&names[index].emitted, err, "_cell_%zu", suffix++) != TDX_OK) goto done;
        }
        changed_count += (size_t)changed;
    }
    if (LITERAL(out, err, "{\"type\":\"jsn_row\",\"resource\":") != TDX_OK ||
        tdx_format_json_string(out, resource ? resource : "", err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"group\":%zu,\"row\":%zu", group_index, row_index) != TDX_OK)
        goto done;
    for (index = 0; index < group->column_count; ++index) {
        if (tdx_buf_push(out, ',', err) != TDX_OK ||
            tdx_format_json_string_n(out, (const char *)names[index].emitted.data,
                                      names[index].emitted.len, err) != TDX_OK ||
            tdx_buf_push(out, ':', err) != TDX_OK ||
            tdx_jsn_cell_json(document, group, row_index, index, out, err) != TDX_OK) goto done;
    }
    result = tdx_buf_push(out, '}', err);
    if (result == TDX_OK && renamed) *renamed = changed_count;
done:
    for (index = 0; index < group->column_count; ++index) tdx_buf_free(&names[index].emitted);
    free(names);
    return result;
}
int tdx_jsn_format_summary(tdx_buf *out, const tdx_jsn_summary *summary, tdx_error *err) {
    if (!out || !summary) {
        tdx_error_set(err, "rendering a JSN summary needs a buffer and counters");
        return TDX_ERR;
    }
    if (LITERAL(out, err, "{\"type\":\"jsn_summary\",\"resource\":") != TDX_OK ||
        tdx_format_json_string(out, summary->resource ? summary->resource : "", err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"bytes\":%zu,\"md5\":", summary->bytes) != TDX_OK ||
        tdx_format_json_string(out, summary->md5 ? summary->md5 : "", err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"groups\":%zu,\"rows\":%zu,\"columns_renamed\":%zu,\"endpoint\":",
                              summary->groups, summary->rows, summary->columns_renamed) != TDX_OK ||
        tdx_format_json_string(out, summary->endpoint ? summary->endpoint : "", err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}
