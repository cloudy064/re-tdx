/* tdx_json.c - a bounded JSON parser for the JSN resources. */
#include "tdx_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *start;
    const char *cursor;
    const char *end;
    tdx_json_doc *doc;
    tdx_error *err;
    unsigned depth;
} parser_state;

void tdx_json_doc_init(tdx_json_doc *doc) {
    if (!doc)
        return;
    memset(doc, 0, sizeof(*doc));
    doc->root = TDX_JSON_NODE_NONE;
}

void tdx_json_doc_free(tdx_json_doc *doc) {
    if (!doc)
        return;
    free(doc->nodes);
    free(doc->text);
    tdx_json_doc_init(doc);
}

static int fail(parser_state *state, const char *message) {
    /* The offset is from the start of the input, which is what makes a failure
     * reproducible rather than just visible. */
    tdx_error_set(state->err, "JSON at offset %zu: %s",
                  (size_t)(state->cursor - state->start), message);
    return TDX_ERR;
}

/* --- storage ---------------------------------------------------------- */

static tdx_json_node *push_node(parser_state *state, tdx_json_type type) {
    tdx_json_doc *doc = state->doc;
    tdx_json_node *node;

    if (doc->node_count == doc->node_capacity) {
        size_t capacity = doc->node_capacity ? doc->node_capacity * 2 : 64;
        tdx_json_node *grown =
            (tdx_json_node *)realloc(doc->nodes, capacity * sizeof(*grown));
        if (!grown) {
            tdx_error_set(state->err, "out of memory while parsing JSON (nodes)");
            return NULL;
        }
        doc->nodes = grown;
        doc->node_capacity = capacity;
    }
    node = &doc->nodes[doc->node_count];
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->first_child = TDX_JSON_NODE_NONE;
    node->next_sibling = TDX_JSON_NODE_NONE;
    node->text_offset = TDX_JSON_TEXT_NONE;
    node->key_offset = TDX_JSON_TEXT_NONE;
    doc->node_count++;
    return node;
}

/* Appends bytes to the text arena with NO terminator, so a string assembled from
 * several pieces stays contiguous.  The arena may move, so nothing here is handed
 * back as a pointer. */
static int append_bytes(parser_state *state, const char *bytes, size_t size) {
    tdx_json_doc *doc = state->doc;

    if (size == 0)
        return TDX_OK;
    if (doc->text_used + size > doc->text_capacity) {
        size_t capacity = doc->text_capacity ? doc->text_capacity : 256;
        char *grown;
        while (capacity < doc->text_used + size)
            capacity *= 2;
        grown = (char *)realloc(doc->text, capacity);
        if (!grown) {
            tdx_error_set(state->err, "out of memory while parsing JSON (text)");
            return TDX_ERR;
        }
        doc->text = grown;
        doc->text_capacity = capacity;
    }
    memcpy(doc->text + doc->text_used, bytes, size);
    doc->text_used += size;
    return TDX_OK;
}

/* --- lexical helpers -------------------------------------------------- */

static void skip_space(parser_state *state) {
    while (state->cursor < state->end) {
        char ch = *state->cursor;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            state->cursor++;
        else
            break;
    }
}

static int literal(parser_state *state, const char *word, size_t length) {
    if ((size_t)(state->end - state->cursor) < length || memcmp(state->cursor, word, length) != 0)
        return 0;
    state->cursor += length;
    return 1;
}

static int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

/* Reads one string into the arena and reports its offset and length.  The pieces
 * of a string with escapes are appended back to back, so the result is contiguous
 * and a single NUL is written after it. */
static int parse_string(parser_state *state, size_t *out_offset, size_t *out_length) {
    size_t start = state->doc->text_used;
    size_t written = 0;

    if (state->cursor >= state->end || *state->cursor != '"')
        return fail(state, "a string must start with a quote");
    state->cursor++;
    for (;;) {
        unsigned char ch;
        char bytes[4];
        size_t count = 0;

        if (state->cursor >= state->end)
            return fail(state, "the string is not terminated");
        ch = (unsigned char)*state->cursor++;
        if (ch == '"')
            break;
        if (ch < 0x20) {
            state->cursor--;
            return fail(state, "a control character is not allowed unescaped");
        }
        if (ch != '\\') {
            bytes[0] = (char)ch;
            count = 1;
        } else {
            if (state->cursor >= state->end)
                return fail(state, "the escape is not terminated");
            ch = (unsigned char)*state->cursor++;
            switch (ch) {
            case '"':
            case '\\':
            case '/':
                bytes[0] = (char)ch;
                count = 1;
                break;
            case 'b':
                bytes[0] = '\b';
                count = 1;
                break;
            case 'f':
                bytes[0] = '\f';
                count = 1;
                break;
            case 'n':
                bytes[0] = '\n';
                count = 1;
                break;
            case 'r':
                bytes[0] = '\r';
                count = 1;
                break;
            case 't':
                bytes[0] = '\t';
                count = 1;
                break;
            case 'u': {
                unsigned code = 0;
                int index;
                for (index = 0; index < 4; ++index) {
                    int digit;
                    if (state->cursor >= state->end)
                        return fail(state, "a \\u escape is short");
                    digit = hex_digit(*state->cursor++);
                    if (digit < 0)
                        return fail(state, "a \\u escape has a non-hex digit");
                    code = (code << 4) | (unsigned)digit;
                }
                /* A high surrogate is joined with its low half when it is there;
                 * a lone surrogate becomes U+FFFD rather than invalid UTF-8. */
                if (code >= 0xD800 && code <= 0xDBFF &&
                    (size_t)(state->end - state->cursor) >= 6 && state->cursor[0] == '\\' &&
                    state->cursor[1] == 'u') {
                    unsigned low = 0;
                    int ok = 1;
                    for (index = 0; index < 4; ++index) {
                        int digit = hex_digit(state->cursor[2 + index]);
                        if (digit < 0) {
                            ok = 0;
                            break;
                        }
                        low = (low << 4) | (unsigned)digit;
                    }
                    if (ok && low >= 0xDC00 && low <= 0xDFFF) {
                        state->cursor += 6;
                        code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                    }
                }
                if (code >= 0xD800 && code <= 0xDFFF)
                    code = 0xFFFDu;
                if (code < 0x80) {
                    bytes[0] = (char)code;
                    count = 1;
                } else if (code < 0x800) {
                    bytes[0] = (char)(0xC0 | (code >> 6));
                    bytes[1] = (char)(0x80 | (code & 0x3F));
                    count = 2;
                } else if (code < 0x10000) {
                    bytes[0] = (char)(0xE0 | (code >> 12));
                    bytes[1] = (char)(0x80 | ((code >> 6) & 0x3F));
                    bytes[2] = (char)(0x80 | (code & 0x3F));
                    count = 3;
                } else {
                    bytes[0] = (char)(0xF0 | (code >> 18));
                    bytes[1] = (char)(0x80 | ((code >> 12) & 0x3F));
                    bytes[2] = (char)(0x80 | ((code >> 6) & 0x3F));
                    bytes[3] = (char)(0x80 | (code & 0x3F));
                    count = 4;
                }
                break;
            }
            default:
                state->cursor--;
                return fail(state, "an unknown escape sequence");
            }
        }
        if (append_bytes(state, bytes, count) != TDX_OK)
            return TDX_ERR;
        written += count;
    }
    /* One terminator for the whole string, so a caller that wants a C string has
     * one and the reported length stays the byte count. */
    if (append_bytes(state, "", 1) != TDX_OK)
        return TDX_ERR;
    *out_offset = start;
    *out_length = written;
    return TDX_OK;
}

static int parse_number(parser_state *state, tdx_json_node *node) {
    const char *start = state->cursor;
    char *stop = NULL;

    if (state->cursor < state->end && (*state->cursor == '-' || *state->cursor == '+'))
        state->cursor++;
    if (state->cursor >= state->end || *state->cursor < '0' || *state->cursor > '9')
        return fail(state, "a number must start with a digit");
    /* Leading zeros are not in the grammar; accepting them would mean accepting
     * data no other parser would. */
    if (*state->cursor == '0' && state->cursor + 1 < state->end && state->cursor[1] >= '0' &&
        state->cursor[1] <= '9')
        return fail(state, "a number must not have a leading zero");
    while (state->cursor < state->end) {
        char ch = *state->cursor;
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == 'e' || ch == 'E' || ch == '+' ||
            ch == '-')
            state->cursor++;
        else
            break;
    }
    node->number = strtod(start, &stop);
    if (stop != state->cursor)
        return fail(state, "a number is malformed");
    return TDX_OK;
}

static int parse_value(parser_state *state, size_t *out_index);

/* Parses the children of a container whose node index is already known. */
static int parse_container(parser_state *state, size_t index, int is_object) {
    tdx_json_doc *doc = state->doc;
    size_t last_child = TDX_JSON_NODE_NONE;
    size_t count = 0;
    char closer = is_object ? '}' : ']';

    if (++state->depth > TDX_JSON_DEPTH_MAX) {
        state->depth--;
        return fail(state, "the document nests deeper than the limit");
    }
    state->cursor++;
    skip_space(state);
    if (state->cursor < state->end && *state->cursor == closer) {
        state->cursor++;
        state->depth--;
        return TDX_OK;
    }
    for (;;) {
        size_t child = TDX_JSON_NODE_NONE;
        size_t key_offset = TDX_JSON_TEXT_NONE;
        size_t key_length = 0;

        skip_space(state);
        if (is_object) {
            if (parse_string(state, &key_offset, &key_length) != TDX_OK)
                return TDX_ERR;
            skip_space(state);
            if (state->cursor >= state->end || *state->cursor != ':')
                return fail(state, "an object member needs a colon");
            state->cursor++;
        }
        if (parse_value(state, &child) != TDX_OK)
            return TDX_ERR;
        if (is_object) {
            /* The name goes on the VALUE's node.  Both are offsets into an arena
             * that may move, so neither can dangle. */
            doc->nodes[child].key_offset = key_offset;
            doc->nodes[child].key_length = key_length;
        }
        /* Link the child after the previous one.  A container child has already
         * claimed the nodes after itself for its own children, so the siblings are
         * NOT adjacent and a range would be wrong. */
        if (last_child == TDX_JSON_NODE_NONE)
            doc->nodes[index].first_child = child;
        else
            doc->nodes[last_child].next_sibling = child;
        last_child = child;
        count++;

        skip_space(state);
        if (state->cursor < state->end && *state->cursor == ',') {
            state->cursor++;
            continue;
        }
        if (state->cursor < state->end && *state->cursor == closer) {
            state->cursor++;
            break;
        }
        return fail(state, is_object ? "an object member must be followed by , or }"
                                     : "an array element must be followed by , or ]");
    }
    doc->nodes[index].child_count = count;
    state->depth--;
    return TDX_OK;
}

static int parse_value(parser_state *state, size_t *out_index) {
    tdx_json_doc *doc = state->doc;
    size_t index;
    tdx_json_node *node;
    char ch;

    skip_space(state);
    if (state->cursor >= state->end)
        return fail(state, "a value is missing");
    ch = *state->cursor;
    index = doc->node_count;
    if (ch == '{' || ch == '[') {
        if (!push_node(state, ch == '{' ? TDX_JSON_OBJECT : TDX_JSON_ARRAY))
            return TDX_ERR;
        if (parse_container(state, index, ch == '{') != TDX_OK)
            return TDX_ERR;
    } else if (ch == '"') {
        size_t offset = TDX_JSON_TEXT_NONE;
        size_t length = 0;
        if (!push_node(state, TDX_JSON_STRING))
            return TDX_ERR;
        if (parse_string(state, &offset, &length) != TDX_OK)
            return TDX_ERR;
        node = &doc->nodes[index];
        node->text_offset = offset;
        node->text_length = length;
    } else if (literal(state, "true", 4)) {
        node = push_node(state, TDX_JSON_BOOL);
        if (!node)
            return TDX_ERR;
        node->boolean = 1;
    } else if (literal(state, "false", 5)) {
        node = push_node(state, TDX_JSON_BOOL);
        if (!node)
            return TDX_ERR;
        node->boolean = 0;
    } else if (literal(state, "null", 4)) {
        if (!push_node(state, TDX_JSON_NULL))
            return TDX_ERR;
    } else if (ch == '-' || ch == '+' || (ch >= '0' && ch <= '9')) {
        node = push_node(state, TDX_JSON_NUMBER);
        if (!node)
            return TDX_ERR;
        if (parse_number(state, node) != TDX_OK)
            return TDX_ERR;
    } else {
        return fail(state, "an unexpected character starts a value");
    }
    *out_index = index;
    return TDX_OK;
}

int tdx_json_parse(const uint8_t *data, size_t size, tdx_json_doc *doc, tdx_error *err) {
    parser_state state;
    size_t root = TDX_JSON_NODE_NONE;

    if (!doc) {
        tdx_error_set(err, "JSON parsing needs a document");
        return TDX_ERR;
    }
    tdx_json_doc_init(doc);
    if (!data) {
        tdx_error_set(err, "JSON parsing needs data");
        return TDX_ERR;
    }
    state.start = (const char *)data;
    state.cursor = (const char *)data;
    state.end = (const char *)data + size;
    state.doc = doc;
    state.err = err;
    state.depth = 0;
    if (parse_value(&state, &root) != TDX_OK) {
        tdx_json_doc_free(doc);
        return TDX_ERR;
    }
    skip_space(&state);
    if (state.cursor != state.end) {
        tdx_error_set(err, "JSON has %zu bytes of trailing data after the value",
                      (size_t)(state.end - state.cursor));
        tdx_json_doc_free(doc);
        return TDX_ERR;
    }
    doc->root = root;
    doc->valid = 1;
    return TDX_OK;
}

/* --- access ----------------------------------------------------------- */

const tdx_json_node *tdx_json_root(const tdx_json_doc *doc) {
    if (!doc || !doc->valid || doc->root == TDX_JSON_NODE_NONE)
        return NULL;
    return &doc->nodes[doc->root];
}

const tdx_json_node *tdx_json_at(const tdx_json_doc *doc, const tdx_json_node *node,
                                 size_t index) {
    size_t cursor;
    size_t seen;

    if (!doc || !node)
        return NULL;
    if (node->type != TDX_JSON_ARRAY && node->type != TDX_JSON_OBJECT)
        return NULL;
    if (index >= node->child_count || node->first_child == TDX_JSON_NODE_NONE)
        return NULL;
    cursor = node->first_child;
    for (seen = 0; seen < index; ++seen) {
        cursor = doc->nodes[cursor].next_sibling;
        if (cursor == TDX_JSON_NODE_NONE)
            return NULL;
    }
    return &doc->nodes[cursor];
}

const tdx_json_node *tdx_json_member(const tdx_json_doc *doc, const tdx_json_node *object,
                                     const char *key) {
    size_t index;
    size_t length;

    if (!doc || !object || !key)
        return NULL;
    if (object->type != TDX_JSON_OBJECT)
        return NULL;
    length = strlen(key);
    for (index = 0; index < object->child_count; ++index) {
        const tdx_json_node *child = tdx_json_at(doc, object, index);
        if (child && child->key_offset != TDX_JSON_TEXT_NONE && child->key_length == length &&
            memcmp(doc->text + child->key_offset, key, length) == 0)
            return child;
    }
    return NULL;
}

const char *tdx_json_text(const tdx_json_doc *doc, const tdx_json_node *node) {
    if (!doc || !node || node->text_offset == TDX_JSON_TEXT_NONE)
        return NULL;
    return doc->text + node->text_offset;
}

const char *tdx_json_key(const tdx_json_doc *doc, const tdx_json_node *node) {
    if (!doc || !node || node->key_offset == TDX_JSON_TEXT_NONE)
        return NULL;
    return doc->text + node->key_offset;
}

int tdx_json_integer(const tdx_json_doc *doc, const tdx_json_node *node, long *out) {
    if (!node || !out)
        return TDX_ERR;
    if (node->type == TDX_JSON_NUMBER) {
        *out = (long)node->number;
        return TDX_OK;
    }
    /* The JSN payloads carry numbers as strings, so a numeric string counts. */
    if (node->type == TDX_JSON_STRING && doc) {
        const char *text = tdx_json_text(doc, node);
        char *stop = NULL;
        long value;
        if (!text)
            return TDX_ERR;
        value = strtol(text, &stop, 10);
        if (stop && stop != text && *stop == '\0') {
            *out = value;
            return TDX_OK;
        }
    }
    return TDX_ERR;
}
