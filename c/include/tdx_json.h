/* tdx_json.h - a bounded JSON parser for the JSN resources.
 *
 * The JSN resources are GBK-encoded JSON, so reading them needs a JSON parser in
 * C.  This one is deliberately small and strict rather than general:
 *
 *   * it builds a DOM in two arenas (nodes and text), so freeing is one call;
 *   * strings are unescaped into the text arena ONCE, and nodes store offsets into
 *     that arena rather than pointers - the arena grows with realloc as a document
 *     is parsed, so a stored pointer would be left dangling by the next string;
 *   * a member's NAME and its VALUE are separate fields, because an object member
 *     that is itself a string needs both and one field cannot hold two;
 *   * it enforces a nesting limit, because recursive descent on hostile input is a
 *     stack overflow waiting to happen;
 *   * it rejects trailing data after the top-level value, rejects leading zeros,
 *     and does not accept JSON5 extensions.
 *
 * Numbers are kept as doubles: nothing here needs more precision, and the JSN
 * payloads are names, codes and prices. */
#ifndef TDX_JSON_H
#define TDX_JSON_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_JSON_DEPTH_MAX 64
#define TDX_JSON_INPUT_MAX (64u * 1024u * 1024u)
#define TDX_JSON_NODES_MAX (1024u * 1024u)
#define TDX_JSON_TEXT_MAX (64u * 1024u * 1024u)
#define TDX_JSON_NODE_NONE ((size_t)-1)
#define TDX_JSON_TEXT_NONE ((size_t)-1)

typedef enum tdx_json_type {
    TDX_JSON_NULL = 0,
    TDX_JSON_BOOL,
    TDX_JSON_NUMBER,
    TDX_JSON_STRING,
    TDX_JSON_ARRAY,
    TDX_JSON_OBJECT
} tdx_json_type;

typedef struct tdx_json_node {
    tdx_json_type type;
    int boolean;
    double number;
    /* The string value, as an offset into the document's text arena. */
    size_t text_offset;
    size_t text_length;
    /* The member name, when this node is a member of an object. */
    size_t key_offset;
    size_t key_length;
    /* Children are a LINKED LIST, not a contiguous range: a child that is itself a
     * container allocates its own children in between, so a range would name
     * grandchildren.  That is a bug this parser actually had, and the JSN shape -
     * an object whose members are arrays of arrays - is exactly what triggers it. */
    size_t first_child;
    size_t next_sibling;
    size_t child_count;
} tdx_json_node;

typedef struct tdx_json_doc {
    tdx_json_node *nodes;
    size_t node_count;
    size_t node_capacity;
    char *text;
    size_t text_used;
    size_t text_capacity;
    size_t root;
    int valid;
} tdx_json_doc;

void tdx_json_doc_init(tdx_json_doc *doc);
/* Documents must be initialized (or {0}) before use. clear preserves allocations;
 * free releases them. Both invalidate all borrowed nodes and text views. */
void tdx_json_doc_clear(tdx_json_doc *doc);
void tdx_json_doc_free(tdx_json_doc *doc);

/* Parses one complete value within size, without requiring a terminator.
 * Reuses an initialized document; failure leaves it empty. Input must not
 * alias document storage. Explicit limits bound input, node and text storage. */
int tdx_json_parse(const uint8_t *data, size_t size, tdx_json_doc *doc, tdx_error *err);

const tdx_json_node *tdx_json_root(const tdx_json_doc *doc);
/* Child accessors return NULL for a wrong type or an out-of-range index, so a
 * caller can walk a document without checking every step. */
const tdx_json_node *tdx_json_at(const tdx_json_doc *doc, const tdx_json_node *node,
                                 size_t index);
/* Linear traversal of siblings without repeatedly scanning from child zero. */
const tdx_json_node *tdx_json_first(const tdx_json_doc *doc, const tdx_json_node *node);
const tdx_json_node *tdx_json_next(const tdx_json_doc *doc, const tdx_json_node *node);
const tdx_json_node *tdx_json_member(const tdx_json_doc *doc, const tdx_json_node *object,
                                     const char *key);
/* The node's string value, or NULL.  The pointer is valid until the document is
 * freed or parsed into again. */
const char *tdx_json_text(const tdx_json_doc *doc, const tdx_json_node *node);
/* The node's member name, or NULL. */
const char *tdx_json_key(const tdx_json_doc *doc, const tdx_json_node *node);
/* The node's value as an integer, when it is a number or a numeric string.  The
 * JSN payloads carry numbers as strings, so both are accepted. */
int tdx_json_integer(const tdx_json_doc *doc, const tdx_json_node *node, long *out);

#ifdef __cplusplus
}
#endif

#endif /* TDX_JSON_H */
