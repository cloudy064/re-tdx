/* tdx_jsn.c - the JSN resource table format. */
#include "tdx_jsn.h"
#include "tdx_json_write.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
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
    free(doc->row_nodes);
    tdx_jsn_document_init(doc);
}

void tdx_jsn_document_clear(tdx_jsn_document *doc) {
    if (!doc)
        return;
    tdx_json_doc_clear(&doc->json);
    doc->group_count = 0;
    doc->row_count = 0;
}

int tdx_jsn_parse(const uint8_t *data, size_t size, tdx_jsn_document *doc, tdx_error *err) {
    const tdx_json_node *root;
    const tdx_json_node *group;
    size_t index;

    if (!doc) {
        tdx_error_set(err, "JSN parsing needs a document");
        return TDX_ERR;
    }
    tdx_jsn_document_clear(doc);
    if (tdx_json_parse(data, size, &doc->json, err) != TDX_OK)
        return TDX_ERR;
    root = tdx_json_root(&doc->json);
    if (!root || root->type != TDX_JSON_ARRAY) {
        tdx_error_set(err, "a JSN root must be an array of groups");
        goto failed;
    }
    group = tdx_json_first(&doc->json, root);
    for (index = 0; index < root->child_count; ++index,
         group = tdx_json_next(&doc->json, group)) {
        const tdx_json_node *headers;
        const tdx_json_node *data_rows;
        size_t row;
        const tdx_json_node *cells;

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
        if (data_rows->child_count > TDX_JSON_NODES_MAX - doc->row_count) {
            tdx_error_set(err, "JSN row index exceeds the node budget");
            goto failed;
        }
        if (doc->row_count + data_rows->child_count > doc->row_capacity) {
            size_t capacity = doc->row_capacity ? doc->row_capacity : 64;
            size_t *grown;
            while (capacity < doc->row_count + data_rows->child_count)
                capacity *= 2;
            grown = (size_t *)realloc(doc->row_nodes, capacity * sizeof(*grown));
            if (!grown) {
                tdx_error_set(err, "out of memory while indexing JSN rows");
                goto failed;
            }
            doc->row_nodes = grown;
            doc->row_capacity = capacity;
        }
        cells = tdx_json_first(&doc->json, data_rows);
        for (row = 0; row < data_rows->child_count; ++row,
             cells = tdx_json_next(&doc->json, cells)) {
            if (!cells || cells->type != TDX_JSON_ARRAY) {
                tdx_error_set(err, "JSN group %zu row %zu is not an array", index, row);
                goto failed;
            }
            doc->row_nodes[doc->row_count + row] = (size_t)(cells - doc->json.nodes);
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
        doc->groups[doc->group_count].json_index = (size_t)(group - doc->json.nodes);
        doc->groups[doc->group_count].headers_index = (size_t)(headers - doc->json.nodes);
        doc->groups[doc->group_count].first_row = doc->row_count;
        doc->groups[doc->group_count].row_count = data_rows->child_count;
        doc->groups[doc->group_count].column_count = headers->child_count;
        doc->group_count++;
        doc->row_count += data_rows->child_count;
    }
    return TDX_OK;

failed:
    tdx_jsn_document_clear(doc);
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
    if (!doc->json.valid || group->json_index >= doc->json.node_count)
        return NULL;
    return &doc->json.nodes[group->json_index];
}

const char *tdx_jsn_column_name(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                size_t column) {
    const tdx_json_node *node = group_node(doc, group);
    const tdx_json_node *headers;

    if (!node || column >= group->column_count)
        return NULL;
    headers = group->headers_index < doc->json.node_count
                  ? &doc->json.nodes[group->headers_index] : NULL;
    if (!headers)
        return NULL;
    return tdx_json_text(&doc->json, tdx_json_at(&doc->json, headers, column));
}

static const tdx_json_node *cell_node(const tdx_jsn_document *doc, const tdx_jsn_group *group,
                                      size_t row_in_group, size_t column) {
    const tdx_json_node *node = group_node(doc, group);
    const tdx_json_node *cells;

    if (!node || row_in_group >= group->row_count)
        return NULL;
    if (group->first_row > doc->row_count || row_in_group >= doc->row_count - group->first_row)
        return NULL;
    cells = &doc->json.nodes[doc->row_nodes[group->first_row + row_in_group]];
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
        return tdx_format_json_string_n(out, text, node->text_length, err);
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
    if (size == 0)
        return TDX_OK;
    if (!data) {
        tdx_error_set(err, "GBK conversion needs a valid input byte view");
        return TDX_ERR;
    }
    if (size > INT_MAX) {
        tdx_error_set(err, "GBK input is too large for the platform converter");
        return TDX_ERR;
    }
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
    {
        iconv_t converter = iconv_open("UTF-8", "GBK");
        char *input = (char *)(uintptr_t)data;
        char *output;
        size_t input_left = size;
        size_t output_left;
        size_t start = out->len;
        size_t converted;
        if (converter == (iconv_t)-1) {
            tdx_error_set(err, "iconv cannot open the GBK code page");
            return TDX_ERR;
        }
        if (size > (SIZE_MAX - 1) / 3 || tdx_buf_reserve(out, size * 3 + 1, err) != TDX_OK) {
            iconv_close(converter);
            return TDX_ERR;
        }
        output = (char *)out->data + start;
        output_left = out->cap - start;
        converted = iconv(converter, &input, &input_left, &output, &output_left);
        iconv_close(converter);
        if (converted == (size_t)-1 || input_left != 0) {
            tdx_error_set(err, "the payload is not valid GBK (%zu bytes)", size);
            return TDX_ERR;
        }
        out->len = (size_t)(output - (char *)out->data);
        return TDX_OK;
    }
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
