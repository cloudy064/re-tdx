/* tdx_endpoint.c - endpoint parsing and connect.cfg HQHOST discovery.
 *
 * connect.cfg is GBK-encoded INI.  Host labels are only decoded after the
 * address fields have been read, so a malformed label can never invalidate a
 * usable endpoint. */
#include "tdx_endpoint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_internal.h"

#define TDX_CONNECT_CFG_MAX_BYTES (1024u * 1024u)
#define TDX_INI_KEY_MAX 64
#define TDX_INI_VALUE_MAX 256
#define TDX_INI_ENTRY_MAX 512

typedef struct ini_entry {
    char key[TDX_INI_KEY_MAX];
    char value[TDX_INI_VALUE_MAX];
} ini_entry;

typedef struct ini_table {
    ini_entry entries[TDX_INI_ENTRY_MAX];
    size_t count;
} ini_table;

static const char *ini_find(const ini_table *table, const char *key) {
    size_t index;
    for (index = 0; index < table->count; ++index)
        if (strcmp(table->entries[index].key, key) == 0)
            return table->entries[index].value;
    return NULL;
}

/* Copies at most out_size-1 bytes; always terminates and reports truncation. */
static void copy_text(char *out, size_t out_size, const char *source) {
    size_t length;
    if (out_size == 0)
        return;
    if (!source) {
        out[0] = '\0';
        return;
    }
    length = strlen(source);
    if (length >= out_size)
        length = out_size - 1;
    memcpy(out, source, length);
    out[length] = '\0';
}

int tdx_endpoint_parse(const char *text, tdx_endpoint *out, tdx_error *err) {
    char working[TDX_INI_VALUE_MAX];
    char *cursor;
    char *host;
    char *port_text = NULL;
    char *colon;
    unsigned long port = 7709;
    size_t index;

    if (!text || !*text || !out) {
        tdx_error_set(err, "server endpoint cannot be empty");
        return TDX_ERR;
    }
    copy_text(working, sizeof(working), text);
    cursor = tdx_trim(working);
    if (!*cursor) {
        tdx_error_set(err, "server endpoint cannot be empty");
        return TDX_ERR;
    }

    host = cursor;
    if (cursor[0] == '[') {
        char *close = strchr(cursor, ']');
        if (!close) {
            tdx_error_set(err, "invalid endpoint: %s", text);
            return TDX_ERR;
        }
        *close = '\0';
        host = cursor + 1;
        if (close[1] != '\0') {
            if (close[1] != ':') {
                tdx_error_set(err, "invalid endpoint: %s", text);
                return TDX_ERR;
            }
            port_text = close + 2;
        }
    } else {
        colon = strrchr(cursor, ':');
        if (colon && strchr(cursor, ':') == colon) {
            port_text = (char *)(colon + 1);
            *colon = '\0';
        }
    }
    if (!*host) {
        tdx_error_set(err, "invalid endpoint: %s", text);
        return TDX_ERR;
    }
    if (port_text && *port_text) {
        for (index = 0; port_text[index] != '\0'; ++index) {
            if (port_text[index] < '0' || port_text[index] > '9') {
                tdx_error_set(err, "invalid endpoint port: %s", text);
                return TDX_ERR;
            }
        }
        port = strtoul(port_text, NULL, 10);
    }
    if (port < 1 || port > 65535) {
        tdx_error_set(err, "endpoint port out of range: %s", text);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    copy_text(out->host, sizeof(out->host), host);
    out->port = (uint16_t)port;
    return TDX_OK;
}

void tdx_endpoint_address(const tdx_endpoint *endpoint, char *out, size_t out_size) {
    if (!endpoint || !out || out_size == 0)
        return;
    snprintf(out, out_size, "%s:%u", endpoint->host, (unsigned)endpoint->port);
}

/* Reads one key=value pair into the table; ignores blank lines, comments and
 * every section other than the requested one. */
static void ini_add(ini_table *table, const char *key, const char *value) {
    ini_entry *entry;
    if (table->count >= TDX_INI_ENTRY_MAX)
        return;
    entry = &table->entries[table->count];
    copy_text(entry->key, sizeof(entry->key), key);
    copy_text(entry->value, sizeof(entry->value), value);
    table->count++;
}

static void ini_parse_hqhost(const char *text, ini_table *table) {
    const char *cursor = text;
    int in_section = 0;

    while (*cursor) {
        const char *line_end = strchr(cursor, '\n');
        size_t length = line_end ? (size_t)(line_end - cursor) : strlen(cursor);
        char line[TDX_INI_VALUE_MAX];
        char *trimmed;
        char *equals;

        if (length >= sizeof(line))
            length = sizeof(line) - 1;
        memcpy(line, cursor, length);
        line[length] = '\0';
        if (length && line[length - 1] == '\r')
            line[length - 1] = '\0';
        trimmed = tdx_trim(line);

        if (*trimmed == ';' || *trimmed == '#') {
            /* comment */
        } else if (*trimmed == '[') {
            char *close = strrchr(trimmed, ']');
            if (close) {
                *close = '\0';
                in_section = tdx_ascii_casecmp(tdx_trim(trimmed + 1), "hqhost") == 0;
            }
        } else if (in_section && (equals = strchr(trimmed, '=')) != NULL && equals != trimmed) {
            char *key = trimmed;
            char *value = equals + 1;
            *equals = '\0';
            key = tdx_trim(key);
            value = tdx_trim(value);
            if (*key)
                ini_add(table, key, value);
        }
        if (!line_end)
            break;
        cursor = line_end + 1;
    }
}

static int read_whole_file(const char *path, char **out, size_t *out_size,
                           tdx_error *err) {
    FILE *file;
    long size;
    char *buffer;
    size_t read;

    *out = NULL;
    *out_size = 0;
    file = fopen(path, "rb");
    if (!file) {
        tdx_error_set(err, "cannot open %s", path);
        return TDX_ERR;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        tdx_error_set(err, "cannot size %s", path);
        return TDX_ERR;
    }
    size = ftell(file);
    if (size < 0 || (unsigned long)size > TDX_CONNECT_CFG_MAX_BYTES) {
        fclose(file);
        tdx_error_set(err, "%s is not a usable size (%ld bytes)", path, size);
        return TDX_ERR;
    }
    rewind(file);
    buffer = (char *)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        tdx_error_set(err, "out of memory reading %s", path);
        return TDX_ERR;
    }
    read = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(buffer);
        tdx_error_set(err, "short read on %s", path);
        return TDX_ERR;
    }
    buffer[size] = '\0';
    *out = buffer;
    *out_size = (size_t)size;
    return TDX_OK;
}

static void pool_fallback(tdx_endpoint_pool *out) {
    tdx_error ignored;
    memset(out, 0, sizeof(*out));
    out->source[0] = '\0';
    (void)tdx_endpoint_parse("110.41.147.114:7709", &out->items[0], &ignored);
    copy_text(out->items[0].name, sizeof(out->items[0].name), "compiled public fallback");
    out->count = 1;
    out->configured_count = 1;
    out->primary_configured = 0;
    copy_text(out->source, sizeof(out->source), "compiled-default");
}

int tdx_endpoint_pool_load(const char *root, size_t limit, tdx_endpoint_pool *out,
                           tdx_error *err) {
    char path[512];
    char *text = NULL;
    size_t text_size = 0;
    ini_table table;
    const char *count_text;
    const char *primary_text;
    long declared;
    long primary = 0;
    size_t index;
    size_t written = 0;

    if (!out || limit < 1 || limit > TDX_ENDPOINT_POOL_MAX) {
        tdx_error_set(err, "endpoint pool limit must be in 1..%d", TDX_ENDPOINT_POOL_MAX);
        return TDX_ERR;
    }
    memset(out, 0, sizeof(*out));
    if (!root || !*root) {
        pool_fallback(out);
        return TDX_OK;
    }
    snprintf(path, sizeof(path), "%s/connect.cfg", root);
    if (read_whole_file(path, &text, &text_size, err) != TDX_OK) {
        pool_fallback(out);
        return TDX_OK;
    }
    (void)text_size;

    memset(&table, 0, sizeof(table));
    ini_parse_hqhost(text, &table);
    free(text);

    count_text = ini_find(&table, "hostnum");
    if (!count_text) {
        pool_fallback(out);
        return TDX_OK;
    }
    declared = strtol(count_text, NULL, 10);
    if (declared < 1 || declared > 10000) {
        pool_fallback(out);
        return TDX_OK;
    }
    primary_text = ini_find(&table, "primaryhost");
    if (primary_text)
        primary = strtol(primary_text, NULL, 10);
    if (primary < 0 || primary >= declared)
        primary = 0;

    /* First pass: count usable entries so the primary-first rotation can be
     * implemented as a single modular walk. */
    {
        long index_no;
        size_t usable = 0;
        for (index_no = 1; index_no <= declared; ++index_no) {
            char key[TDX_INI_KEY_MAX];
            snprintf(key, sizeof(key), "ipaddress%02ld", index_no);
            if (ini_find(&table, key))
                usable++;
        }
        if (usable == 0) {
            pool_fallback(out);
            return TDX_OK;
        }

        for (index = 0; index < (size_t)declared && written < limit; ++index) {
            long ordinal = ((long)index + primary) % declared;
            long index_no = ordinal + 1;
            char key[TDX_INI_KEY_MAX];
            const char *address;
            const char *port_text;
            const char *label;
            tdx_endpoint endpoint;

            snprintf(key, sizeof(key), "ipaddress%02ld", index_no);
            address = ini_find(&table, key);
            if (!address)
                continue;
            snprintf(key, sizeof(key), "port%02ld", index_no);
            port_text = ini_find(&table, key);
            snprintf(key, sizeof(key), "hostname%02ld", index_no);
            label = ini_find(&table, key);

            {
                char combined[TDX_INI_VALUE_MAX];
                if (port_text && *port_text)
                    snprintf(combined, sizeof(combined), "%s:%s", address, port_text);
                else
                    snprintf(combined, sizeof(combined), "%s:7709", address);
                if (tdx_endpoint_parse(combined, &endpoint, err) != TDX_OK) {
                    /* A single malformed node must not sink the whole pool. */
                    continue;
                }
            }
            if (label && *label) {
                if (tdx_decode_gb18030((const uint8_t *)label, strlen(label),
                                       endpoint.name, sizeof(endpoint.name),
                                       NULL, err) != TDX_OK) {
                    copy_text(endpoint.name, sizeof(endpoint.name), "connect.cfg hqhost");
                }
            } else {
                copy_text(endpoint.name, sizeof(endpoint.name), "connect.cfg hqhost");
            }
            out->items[written++] = endpoint;
        }
    }
    if (written == 0) {
        pool_fallback(out);
        return TDX_OK;
    }
    out->count = written;
    out->configured_count = (size_t)declared;
    out->primary_configured = primary_text != NULL;
    copy_text(out->source, sizeof(out->source), "connect.cfg:hqhost-primary-first");
    return TDX_OK;
}
