/* tdx_jsn.c - the JSN resource table format. */
#include "tdx_jsn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

void tdx_jsn_document_init(tdx_jsn_document *doc) {
    if (!doc)
        return;
    memset(doc, 0, sizeof(*doc));
    tdx_json_doc_init(&doc->json);
}

void tdx_jsn_document_free(tdx_jsn_document *doc) {
    if (!doc)
        return;
    tdx_json_doc_free(&doc->json);
    free(doc->groups);
    tdx_jsn_document_init(doc);
}

int tdx_jsn_parse(const uint8_t *data, size_t size, tdx_jsn_document *doc, tdx_error *err) {
    const tdx_json_node *root;
    size_t index;

    if (!doc) {
        tdx_error_set(err, "JSN parsing needs a document");
        return TDX_ERR;
    }
    tdx_jsn_document_init(doc);
    if (tdx_json_parse(data, size, &doc->json, err) != TDX_OK)
        return TDX_ERR;
    root = tdx_json_root(&doc->json);
    if (!root || root->type != TDX_JSON_ARRAY) {
        tdx_error_set(err, "a JSN root must be an array of groups");
        goto failed;
    }
    for (index = 0; index < root->child_count; ++index) {
        const tdx_json_node *group = tdx_json_at(&doc->json, root, index);
        const tdx_json_node *headers;
        const tdx_json_node *data_rows;
        size_t row;

        if (!group || group->type != TDX_JSON_OBJECT) {
            tdx_error_set(err, "JSN group %zu is not an object", index);
            goto failed;
        }
        headers = tdx_json_member(&doc->json, group, "colheader");
        data_rows = tdx_json_member(&doc->json, group, "data");
        if (!headers || headers->type != TDX_JSON_ARRAY || !data_rows ||
            data_rows->type != TDX_JSON_ARRAY) {
            tdx_error_set(err, "JSN group %zu lacks colheader/data arrays", index);
            goto failed;
        }
        /* The width invariant the format actually has. */
        for (row = 0; row < data_rows->child_count; ++row) {
            const tdx_json_node *cells = tdx_json_at(&doc->json, data_rows, row);
            if (!cells || cells->type != TDX_JSON_ARRAY) {
                tdx_error_set(err, "JSN group %zu row %zu is not an array", index, row);
                goto failed;
            }
            if (cells->child_count != headers->child_count) {
                tdx_error_set(err,
                              "JSN group %zu row %zu has %zu cells for %zu columns; a row must "
                              "match its header or every cell after the short one would be "
                              "mislabelled",
                              index, row, cells->child_count, headers->child_count);
                goto failed;
            }
        }
        if (doc->group_count == doc->group_capacity) {
            size_t capacity = doc->group_capacity ? doc->group_capacity * 2 : 8;
            tdx_jsn_group *grown =
                (tdx_jsn_group *)realloc(doc->groups, capacity * sizeof(*grown));
            if (!grown) {
                tdx_error_set(err, "out of memory while reading the JSN groups");
                goto failed;
            }
            doc->groups = grown;
            doc->group_capacity = capacity;
        }
        doc->groups[doc->group_count].json_index = index;
        doc->groups[doc->group_count].first_row = doc->row_count;
        doc->groups[doc->group_count].row_count = data_rows->child_count;
        doc->groups[doc->group_count].column_count = headers->child_count;
        doc->group_count++;
        doc->row_count += data_rows->child_count;
    }
    return TDX_OK;

failed:
    tdx_jsn_document_free(doc);
    return TDX_ERR;
}

size_t tdx_jsn_group_count(const tdx_jsn_document *doc) {
    return doc ? doc->group_count : 0;
}

const tdx_jsn_group *tdx_jsn_group_of_row(const tdx_jsn_document *doc, size_t row) {
    size_t index;
    if (!doc)
        return NULL;
    for (index = 0; index < doc->group_count; ++index)
        if (row >= doc->groups[index].first_row &&
            row < doc->groups[index].first_row + doc->groups[index].row_count)
            return &doc->groups[index];
    return NULL;
}

static const tdx_json_node *group_node(const tdx_jsn_document *doc,
                                       const tdx_jsn_group *group) {
    if (!doc || !group)
        return NULL;
    return tdx_json_at(&doc->json, tdx_json_root(&doc->json), group->json_index);
}

const char *tdx_jsn_column_name(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                size_t column) {
    const tdx_json_node *node = group_node(doc, group);
    const tdx_json_node *headers;

    if (!node || column >= group->column_count)
        return NULL;
    headers = tdx_json_member(&doc->json, node, "colheader");
    if (!headers)
        return NULL;
    return tdx_json_text(&doc->json, tdx_json_at(&doc->json, headers, column));
}

static const tdx_json_node *cell_node(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                      size_t row_in_group, size_t column) {
    const tdx_json_node *node = group_node(doc, group);
    const tdx_json_node *rows;
    const tdx_json_node *cells;

    if (!node || row_in_group >= group->row_count)
        return NULL;
    rows = tdx_json_member(&doc->json, node, "data");
    if (!rows)
        return NULL;
    cells = tdx_json_at(&doc->json, rows, row_in_group);
    if (!cells)
        return NULL;
    return tdx_json_at(&doc->json, cells, column);
}

int tdx_jsn_cell_json(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                      size_t row_in_group, size_t column, tdx_buf *out, tdx_error *err) {
    const tdx_json_node *node;
    const char *text;

    if (!doc || !group || !out) {
        tdx_error_set(err, "a JSN cell needs a document, a group and a buffer");
        return TDX_ERR;
    }
    node = cell_node(doc, group, row_in_group, column);
    if (!node)
        return tdx_buf_append(out, "null", 4, err);
    switch (node->type) {
    case TDX_JSON_NULL:
        return tdx_buf_append(out, "null", 4, err);
    case TDX_JSON_BOOL:
        return tdx_buf_append(out, node->boolean ? "true" : "false", node->boolean ? 4 : 5, err);
    case TDX_JSON_NUMBER:
        return tdx_buf_append_printf(out, err, "%.10g", node->number);
    case TDX_JSON_STRING:
        text = tdx_json_text(&doc->json, node);
        if (!text)
            return tdx_buf_append(out, "\"\"", 2, err);
        return tdx_buf_append_printf(out, err, "\"%.*s\"", (int)node->text_length, text);
    default:
        /* A nested container in a cell is not part of this format; emitting it as
         * a string keeps the output well formed instead of inventing a shape. */
        return tdx_buf_append(out, "\"\"", 2, err);
    }
}

int tdx_jsn_cell_view(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                      size_t row_in_group, size_t column, const char **data, size_t *length,
                      int *present, tdx_error *err) {
    const tdx_json_node *node;

    if (data)
        *data = NULL;
    if (length)
        *length = 0;
    if (present)
        *present = 0;
    if (!doc || !group) {
        tdx_error_set(err, "a cell view needs a document and a group");
        return TDX_ERR;
    }
    node = cell_node(doc, group, row_in_group, column);
    if (!node)
        return TDX_OK;
    if (present)
        *present = 1;
    if (node->type != TDX_JSON_STRING)
        return TDX_OK; /* present but not text; the caller formats it */
    if (data)
        *data = tdx_json_text(&doc->json, node);
    if (length)
        *length = node->text_length;
    return TDX_OK;
}

int tdx_jsn_cell_text(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                      size_t row_in_group, size_t column, tdx_buf *out, tdx_error *err) {
    const tdx_json_node *node;

    if (!doc || !group || !out) {
        tdx_error_set(err, "a JSN cell needs a document, a group and a buffer");
        return TDX_ERR;
    }
    node = cell_node(doc, group, row_in_group, column);
    if (!node)
        return TDX_OK;
    if (node->type == TDX_JSON_STRING) {
        const char *text = tdx_json_text(&doc->json, node);
        if (text)
            return tdx_buf_append(out, text, node->text_length, err);
        return TDX_OK;
    }
    if (node->type == TDX_JSON_NUMBER)
        return tdx_buf_append_printf(out, err, "%.10g", node->number);
    if (node->type == TDX_JSON_BOOL)
        return tdx_buf_append(out, node->boolean ? "true" : "false", node->boolean ? 4 : 5, err);
    return TDX_OK;
}

int tdx_jsn_gbk_to_utf8(const uint8_t *data, size_t size, tdx_buf *out, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "GBK conversion needs an output buffer");
        return TDX_ERR;
    }
    if (!data || size == 0)
        return TDX_OK;
#ifdef _WIN32
    {
        /* 936 is the system's GBK.  Using it rather than an embedded table keeps
         * the mapping exact and costs nothing. */
        int wide_length;
        int utf8_length;
        wchar_t *wide;
        char *utf8;

        wide_length = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, (const char *)data,
                                          (int)size, NULL, 0);
        if (wide_length <= 0) {
            tdx_error_set(err, "the payload is not valid GBK (%zu bytes)", size);
            return TDX_ERR;
        }
        wide = (wchar_t *)malloc((size_t)wide_length * sizeof(*wide));
        if (!wide) {
            tdx_error_set(err, "out of memory while converting GBK");
            return TDX_ERR;
        }
        if (MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, (const char *)data, (int)size, wide,
                                wide_length) != wide_length) {
            free(wide);
            tdx_error_set(err, "the GBK conversion failed");
            return TDX_ERR;
        }
        utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, NULL, 0, NULL, NULL);
        if (utf8_length <= 0) {
            free(wide);
            tdx_error_set(err, "the GBK text cannot be expressed as UTF-8");
            return TDX_ERR;
        }
        utf8 = (char *)malloc((size_t)utf8_length);
        if (!utf8) {
            free(wide);
            tdx_error_set(err, "out of memory while converting GBK");
            return TDX_ERR;
        }
        if (WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, utf8, utf8_length, NULL, NULL) !=
            utf8_length) {
            free(utf8);
            free(wide);
            tdx_error_set(err, "the UTF-8 conversion failed");
            return TDX_ERR;
        }
        free(wide);
        {
            int result = tdx_buf_append(out, utf8, (size_t)utf8_length, err);
            free(utf8);
            return result;
        }
    }
#else
    tdx_error_set(err,
                  "GBK conversion needs the platform code page, which this build does not have");
    return TDX_ERR;
#endif
}

int tdx_jsn_remote_path(const char *resource, const char *prefix, char *out, size_t capacity,
                        tdx_error *err) {
    const char *normalised = resource;
    int written;

    if (!resource || !out || capacity == 0) {
        tdx_error_set(err, "a JSN remote path needs a resource and a buffer");
        return TDX_ERR;
    }
    if (!prefix || !*prefix)
        prefix = "bi";
    while (*normalised == '/' || *normalised == '\\')
        normalised++;
    /* A resource that already carries the prefix must not get it twice. */
    if (strncmp(normalised, prefix, strlen(prefix)) == 0 && normalised[strlen(prefix)] == '/')
        written = snprintf(out, capacity, "%s", normalised);
    else
        written = snprintf(out, capacity, "%s/%s", prefix, normalised);
    if (written < 0 || (size_t)written >= capacity) {
        tdx_error_set(err, "the JSN remote path does not fit in %zu bytes", capacity);
        return TDX_ERR;
    }
    return TDX_OK;
}
