/* tdx_blocks.c - the block families, their hierarchy and their members. */
#include "tdx_blocks.h"
#include "tdx_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_jsn.h"

/* --- small helpers ---------------------------------------------------- */

/* One text line at a time, tolerating CRLF and a missing final newline. */
typedef struct line_cursor {
    const char *text;
    size_t length;
    size_t position;
} line_cursor;

static int next_line(line_cursor *cursor, const char **out, size_t *out_length) {
    size_t start;
    size_t stop;
    if (cursor->position >= cursor->length)
        return 0;
    start = cursor->position;
    stop = start;
    while (stop < cursor->length && cursor->text[stop] != '\n')
        stop++;
    cursor->position = stop + 1;
    /* Trim a trailing CR and any surrounding spaces. */
    while (stop > start && (cursor->text[stop - 1] == '\r' || cursor->text[stop - 1] == ' ' ||
                            cursor->text[stop - 1] == '\t'))
        stop--;
    while (start < stop && (cursor->text[start] == ' ' || cursor->text[start] == '\t'))
        start++;
    *out = cursor->text + start;
    *out_length = stop - start;
    return 1;
}

/* Copies a field into a fixed buffer.  Returns 0 when it does not fit, which for a code or a
 * key means the line cannot be used. */
static int copy_field(const char *text, size_t length, char *out, size_t capacity) {
    if (length >= capacity) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, text, length);
    out[length] = '\0';
    return 1;
}

/* Splits on one separator, up to capacity fields; returns how many were found (which may exceed
 * capacity, in which case the extra ones are dropped). */
/* One row width for every caller: a 2D array parameter needs a fixed second dimension, so
 * 64-wide rows would not compile against 128-wide ones. */
static size_t split_fields(const char *text, size_t length, char separator, char fields[][128],
                           size_t field_capacity, size_t capacity) {
    size_t count = 0;
    size_t start = 0;
    size_t index;
    for (index = 0; index <= length; ++index) {
        if (index == length || text[index] == separator) {
            if (count < capacity) {
                size_t piece = index - start;
                if (piece >= field_capacity)
                    piece = field_capacity - 1;
                memcpy(fields[count], text + start, piece);
                fields[count][piece] = '\0';
            }
            count++;
            start = index + 1;
        }
    }
    return count;
}

static int parse_int(const char *text, int *out) {
    int value = 0;
    size_t index = 0;
    if (!text[0])
        return 0;
    while (text[index]) {
        if (text[index] < '0' || text[index] > '9')
            return 0;
        value = value * 10 + (text[index] - '0');
        if (value > 100000000)
            return 0;
        index++;
    }
    *out = value;
    return 1;
}

/* Copies a text field into a fixed buffer, refusing one that does not fit.  Truncating would
 * produce a code or a date that looks right and is not. */
static int checked_copy(const char *text, char *out, size_t capacity, const char *what,
                        size_t line_number, tdx_error *err) {
    size_t length = strlen(text);
    if (length >= capacity) {
        tdx_error_set(err, "line %zu has a %zu-character %s, longer than the %zu this reader "
                           "holds", line_number, length, what, capacity - 1);
        return TDX_ERR;
    }
    memcpy(out, text, length + 1);
    return TDX_OK;
}

int tdx_blocks_parent_key(const char *source_key, char *out, size_t capacity) {
    size_t length;
    if (!source_key || !out)
        return 0;
    length = strlen(source_key);
    if (length <= 3) {
        out[0] = '\0';
        return 1; /* no parent, which is a legitimate answer */
    }
    return copy_field(source_key, length - 2, out, capacity);
}

int tdx_blocks_source_level(const char *source_key, int *out) {
    size_t length;
    if (!source_key || !out)
        return 0;
    length = strlen(source_key);
    /* The reference refuses a key that does not begin with one of the two letters the format
     * uses, so a key of the wrong shape is a bug rather than a shallow hierarchy. */
    if (length == 0 || (source_key[0] != 'T' && source_key[0] != 'X'))
        return 0;
    *out = (int)((length - 1) / 2);
    return 1;
}

const char *tdx_blocks_infoharbor_family(const char *prefix, size_t length) {
    if (!prefix || length != 2)
        return NULL;
    if (prefix[0] == 'G' && prefix[1] == 'N')
        return TDX_BLOCKS_FAMILY_CONCEPT;
    if (prefix[0] == 'F' && prefix[1] == 'G')
        return TDX_BLOCKS_FAMILY_STYLE;
    if (prefix[0] == 'Z' && prefix[1] == 'S')
        return TDX_BLOCKS_FAMILY_INDEX;
    return NULL;
}

/* --- the industry catalog --------------------------------------------- */

int tdx_blocks_parse_industry_catalog(const char *text, size_t length, tdx_block *out,
                                      size_t capacity, size_t *out_count, size_t *skipped,
                                      tdx_error *err) {
    line_cursor cursor;
    size_t stored = 0;
    size_t dropped = 0;
    size_t line_number = 0;
    int status = TDX_OK;

    if (out_count)
        *out_count = 0;
    if (skipped)
        *skipped = 0;
    if (!text || !out) {
        tdx_error_set(err, "parsing the industry catalog needs text and an output");
        return TDX_ERR;
    }
    cursor.text = text;
    cursor.length = length;
    cursor.position = 0;
    while (1) {
        const char *line;
        size_t line_length;
        char fields[6][128];
        size_t count;
        int leaf = 0;
        int level = 0;
        char parent_key[24];

        if (!next_line(&cursor, &line, &line_length))
            break;
        line_number++;
        if (line_length == 0)
            continue;
        count = split_fields(line, line_length, '|', fields, 128, 6);
        if (count != 6) {
            tdx_error_set(err, "tdxzs3.cfg line %zu has %zu fields, expected 6", line_number,
                          count);
            return TDX_ERR;
        }
        /* Only types 2 and 12 are this family; the file also carries index and other rows. */
        if (strcmp(fields[2], "2") != 0 && strcmp(fields[2], "12") != 0) {
            dropped++;
            continue;
        }
        if (stored >= capacity) {
            tdx_error_set(err, "the catalog holds more than %zu blocks", capacity);
            return TDX_ERR;
        }
        if (!parse_int(fields[4], &leaf)) {
            tdx_error_set(err, "tdxzs3.cfg line %zu has a non-numeric leaf flag: %s",
                          line_number, fields[4]);
            return TDX_ERR;
        }
        if (!tdx_blocks_source_level(fields[5], &level)) {
            tdx_error_set(err, "tdxzs3.cfg line %zu has an industry source key that does not "
                               "start with T or X: %s", line_number, fields[5]);
            return TDX_ERR;
        }
        /* A duplicate key would give two blocks the same identity and both the same parent,
         * which no later step could untangle. */
        {
            size_t index;
            for (index = 0; index < stored; ++index)
                if (strcmp(out[index].source_key, fields[5]) == 0) {
                    tdx_error_set(err, "tdxzs3.cfg line %zu repeats the source key %s",
                                  line_number, fields[5]);
                    return TDX_ERR;
                }
        }
        memset(&out[stored], 0, sizeof(out[stored]));
        snprintf(out[stored].family, sizeof(out[stored].family), "%s",
                 strcmp(fields[2], "2") == 0 ? TDX_BLOCKS_FAMILY_INDUSTRY
                                             : TDX_BLOCKS_FAMILY_RESEARCH_INDUSTRY);
        snprintf(out[stored].name, sizeof(out[stored].name), "%s", fields[0]);
        if (checked_copy(fields[1], out[stored].code, sizeof(out[stored].code), "code",
                         line_number, err) != TDX_OK)
            return TDX_ERR;
        if (checked_copy(fields[5], out[stored].source_key,
                         sizeof(out[stored].source_key), "source key", line_number,
                         err) != TDX_OK)
            return TDX_ERR;
        snprintf(out[stored].id, sizeof(out[stored].id), "%s:%s", out[stored].family,
                 out[stored].source_key);
        out[stored].is_leaf = leaf != 0;
        out[stored].level = level;
        out[stored].declared_count = -1;
        tdx_blocks_parent_key(fields[5], parent_key, sizeof(parent_key));
        out[stored].parent_id[0] = '\0';
        stored++;
        /* The parent is resolved after the whole file is read, since it may come later. */
        (void)parent_key;
    }
    /* Second pass: resolve each block's parent from the complete key set. */
    {
        size_t index;
        for (index = 0; index < stored; ++index) {
            char parent_key[24];
            size_t other;
            tdx_blocks_parent_key(out[index].source_key, parent_key, sizeof(parent_key));
            if (!parent_key[0])
                continue;
            for (other = 0; other < stored; ++other)
                if (strcmp(out[other].source_key, parent_key) == 0) {
                    snprintf(out[index].parent_id, sizeof(out[index].parent_id), "%s",
                             out[other].id);
                    break;
                }
        }
    }
    if (out_count)
        *out_count = stored;
    if (skipped)
        *skipped = dropped;
    return status;
}

/* --- the per-security assignments ------------------------------------- */

int tdx_blocks_parse_industry_assignments(const char *text, size_t length,
                                          tdx_block_assignment *out, size_t capacity,
                                          size_t *out_count, tdx_error *err) {
    line_cursor cursor;
    size_t stored = 0;
    size_t line_number = 0;

    if (out_count)
        *out_count = 0;
    if (!text || !out) {
        tdx_error_set(err, "parsing the industry assignments needs text and an output");
        return TDX_ERR;
    }
    cursor.text = text;
    cursor.length = length;
    cursor.position = 0;
    while (1) {
        const char *line;
        size_t line_length;
        char fields[6][128];
        size_t count;
        int market_id = 0;

        if (!next_line(&cursor, &line, &line_length))
            break;
        line_number++;
        if (line_length == 0)
            continue;
        count = split_fields(line, line_length, '|', fields, 64, 6);
        if (count != 6) {
            tdx_error_set(err, "tdxhy.cfg line %zu has %zu fields, expected 6", line_number,
                          count);
            return TDX_ERR;
        }
        if (!parse_int(fields[0], &market_id)) {
            tdx_error_set(err, "tdxhy.cfg line %zu has a non-numeric market: %s", line_number,
                          fields[0]);
            return TDX_ERR;
        }
        if (stored >= capacity) {
            tdx_error_set(err, "the assignments hold more than %zu rows", capacity);
            return TDX_ERR;
        }
        memset(&out[stored], 0, sizeof(out[stored]));
        out[stored].market_id = market_id;
        if (checked_copy(fields[1], out[stored].code, sizeof(out[stored].code), "code",
                         line_number, err) != TDX_OK ||
            checked_copy(fields[2], out[stored].industry_code,
                         sizeof(out[stored].industry_code), "industry code", line_number,
                         err) != TDX_OK ||
            checked_copy(fields[5], out[stored].research_code,
                         sizeof(out[stored].research_code), "research code", line_number,
                         err) != TDX_OK)
            return TDX_ERR;
        stored++;
    }
    if (out_count)
        *out_count = stored;
    return TDX_OK;
}

/* --- concept, style and index ----------------------------------------- */

int tdx_blocks_parse_infoharbor(const char *text, size_t length, tdx_block *blocks,
                                size_t block_capacity, size_t *block_count,
                                tdx_block_member *members, size_t member_capacity,
                                size_t *member_count, size_t *count_mismatches,
                                tdx_error *err) {
    line_cursor cursor;
    size_t stored_blocks = 0;
    size_t stored_members = 0;
    size_t mismatches = 0;
    size_t line_number = 0;
    tdx_block *current = NULL;

    if (block_count)
        *block_count = 0;
    if (member_count)
        *member_count = 0;
    if (count_mismatches)
        *count_mismatches = 0;
    if (!text || !blocks || !members) {
        tdx_error_set(err, "parsing the infoharbor file needs text, blocks and members");
        return TDX_ERR;
    }
    cursor.text = text;
    cursor.length = length;
    cursor.position = 0;
    while (1) {
        const char *line;
        size_t line_length;
        if (!next_line(&cursor, &line, &line_length))
            break;
        line_number++;
        if (line_length == 0)
            continue;
        if (line[0] == '#') {
            /* A header: seven comma fields, the first being "<prefix>_<name>". */
            char fields[7][128];
            size_t count = split_fields(line + 1, line_length - 1, ',', fields, 128, 7);
            const char *underscore;
            const char *family;
            const char *stable;
            int declared = 0;
            char prefix[8];
            if (count != 7) {
                tdx_error_set(err, "infoharbor line %zu has %zu fields, expected 7",
                              line_number, count);
                return TDX_ERR;
            }
            underscore = strchr(fields[0], '_');
            if (!underscore) {
                tdx_error_set(err, "infoharbor line %zu has no family prefix: %s", line_number,
                              fields[0]);
                return TDX_ERR;
            }
            memset(prefix, 0, sizeof(prefix));
            {
                size_t prefix_length = (size_t)(underscore - fields[0]);
                if (prefix_length != 2) {
                    tdx_error_set(err, "infoharbor line %zu has a %zu-character family prefix",
                                  line_number, prefix_length);
                    return TDX_ERR;
                }
                memcpy(prefix, fields[0], 2);
            }
            family = tdx_blocks_infoharbor_family(prefix, 2);
            if (!family) {
                tdx_error_set(err, "infoharbor line %zu names an unknown family: %s",
                              line_number, prefix);
                return TDX_ERR;
            }
            if (!parse_int(fields[1], &declared)) {
                tdx_error_set(err, "infoharbor line %zu declares a non-numeric count: %s",
                              line_number, fields[1]);
                return TDX_ERR;
            }
            if (stored_blocks >= block_capacity) {
                tdx_error_set(err, "the infoharbor file holds more than %zu blocks",
                              block_capacity);
                return TDX_ERR;
            }
            /* A block with no stable code is identified by its name. */
            stable = fields[2][0] ? fields[2] : underscore + 1;
            memset(&blocks[stored_blocks], 0, sizeof(blocks[stored_blocks]));
            snprintf(blocks[stored_blocks].family, sizeof(blocks[stored_blocks].family), "%s",
                     family);
            snprintf(blocks[stored_blocks].name, sizeof(blocks[stored_blocks].name), "%s",
                     underscore + 1);
            if (checked_copy(stable, blocks[stored_blocks].code,
                             sizeof(blocks[stored_blocks].code), "code", line_number,
                             err) != TDX_OK)
                return TDX_ERR;
            snprintf(blocks[stored_blocks].id, sizeof(blocks[stored_blocks].id), "%s:%s",
                     family, stable);
            if (checked_copy(fields[3], blocks[stored_blocks].start_date,
                             sizeof(blocks[stored_blocks].start_date), "start date",
                             line_number, err) != TDX_OK ||
                checked_copy(fields[4], blocks[stored_blocks].update_date,
                             sizeof(blocks[stored_blocks].update_date), "update date",
                             line_number, err) != TDX_OK)
                return TDX_ERR;
            blocks[stored_blocks].level = 1;
            blocks[stored_blocks].is_leaf = 1;
            blocks[stored_blocks].parent_id[0] = '\0';
            blocks[stored_blocks].declared_count = declared;
            current = &blocks[stored_blocks];
            stored_blocks++;
            continue;
        }
        if (!current) {
            tdx_error_set(err, "infoharbor line %zu has members before any header",
                          line_number);
            return TDX_ERR;
        }
        /* Members: "<market>#<code>", comma separated, with a trailing comma that yields an
         * empty token to skip. */
        {
            size_t start = 0;
            size_t index;
            for (index = 0; index <= line_length; ++index) {
                if (index == line_length || line[index] == ',') {
                    size_t token = index - start;
                    if (token > 0) {
                        const char *hash = NULL;
                        size_t scan;
                        int market_id = 0;
                        for (scan = start; scan < index; ++scan)
                            if (line[scan] == '#') {
                                hash = line + scan;
                                break;
                            }
                        if (!hash) {
                            tdx_error_set(err, "infoharbor line %zu has a member with no "
                                               "market: %.*s", line_number, (int)token,
                                          line + start);
                            return TDX_ERR;
                        }
                        {
                            char market_text[16];
                            size_t market_length = (size_t)(hash - (line + start));
                            if (!copy_field(line + start, market_length, market_text,
                                            sizeof(market_text)) ||
                                !parse_int(market_text, &market_id)) {
                                tdx_error_set(err, "infoharbor line %zu has a non-numeric "
                                                   "market: %s", line_number, market_text);
                                return TDX_ERR;
                            }
                        }
                        if (stored_members >= member_capacity) {
                            tdx_error_set(err, "the infoharbor file holds more than %zu members",
                                          member_capacity);
                            return TDX_ERR;
                        }
                        memset(&members[stored_members], 0, sizeof(members[stored_members]));
                        snprintf(members[stored_members].block_id,
                                 sizeof(members[stored_members].block_id), "%s", current->id);
                        snprintf(members[stored_members].family,
                                 sizeof(members[stored_members].family), "%s", current->family);
                        members[stored_members].market_id = market_id;
                        {
                            size_t code_start = (size_t)(hash - line) + 1;
                            size_t code_length = index - code_start;
                            if (!copy_field(line + code_start, code_length,
                                            members[stored_members].code,
                                            sizeof(members[stored_members].code))) {
                                tdx_error_set(err, "infoharbor line %zu has a %zu-character "
                                                   "code", line_number, code_length);
                                return TDX_ERR;
                            }
                        }
                        snprintf(members[stored_members].security_id,
                                 sizeof(members[stored_members].security_id), "%s%s",
                                 market_id == 0 ? "SZ" : market_id == 1 ? "SH" : "BJ",
                                 members[stored_members].code);
                        current->member_count++;
                        stored_members++;
                    }
                    start = index + 1;
                }
            }
        }
    }
    /* The file's own check, reported rather than enforced: a block that declares 88 members and
     * carries 87 says something about the file. */
    {
        size_t index;
        for (index = 0; index < stored_blocks; ++index)
            if (blocks[index].declared_count >= 0 &&
                (size_t)blocks[index].declared_count != blocks[index].member_count)
                mismatches++;
    }
    if (block_count)
        *block_count = stored_blocks;
    if (member_count)
        *member_count = stored_members;
    if (count_mismatches)
        *count_mismatches = mismatches;
    return TDX_OK;
}

/* --- reading the files ------------------------------------------------ */


int tdx_blocks_load(const char *root, tdx_block *blocks, size_t block_capacity,
                    size_t *block_count, tdx_block_member *members, size_t member_capacity,
                    size_t *member_count, tdx_block_assignment *assignments,
                    size_t assignment_capacity, size_t *assignment_count,
                    tdx_blocks_load_report *report, tdx_error *err) {
    tdx_buf raw;
    tdx_buf utf8;
    char path[TDX_BLOCKS_TEXT_MAX * 4];
    size_t catalog_blocks = 0;
    size_t harbor_blocks = 0;
    size_t total_members = 0;
    size_t total_blocks = 0;
    int status = TDX_ERR;
    int read;

    if (block_count)
        *block_count = 0;
    if (member_count)
        *member_count = 0;
    if (assignment_count)
        *assignment_count = 0;
    if (report)
        memset(report, 0, sizeof(*report));
    if (!root || !blocks || !members) {
        tdx_error_set(err, "loading the blocks needs a root, blocks and members");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&utf8);

    /* The industry catalog. */
    if (snprintf(path, sizeof(path), "%s/T0002/hq_cache/tdxzs3.cfg", root) >= (int)sizeof(path)) {
        tdx_error_set(err, "block data path is too long");
        goto done;
    }
    read = tdx_cache_read(path, &raw, err);
    if (read < 0)
        goto done;
    if (read > 0) {
        if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
            goto done;
        if (tdx_blocks_parse_industry_catalog((const char *)utf8.data, utf8.len, blocks,
                                              block_capacity, &catalog_blocks,
                                              report ? &report->catalog_skipped : NULL,
                                              err) != TDX_OK)
            goto done;
        total_blocks = catalog_blocks;
        if (report)
            report->industry_catalog_read = 1;
    }
    tdx_buf_clear(&raw);
    tdx_buf_clear(&utf8);

    /* The per-security assignments. */
    if (snprintf(path, sizeof(path), "%s/T0002/hq_cache/tdxhy.cfg", root) >= (int)sizeof(path)) {
        tdx_error_set(err, "block data path is too long");
        goto done;
    }
    read = tdx_cache_read(path, &raw, err);
    if (read < 0)
        goto done;
    if (read > 0 && assignments) {
        if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
            goto done;
        if (tdx_blocks_parse_industry_assignments((const char *)utf8.data, utf8.len,
                                                  assignments, assignment_capacity,
                                                  assignment_count, err) != TDX_OK)
            goto done;
        if (report)
            report->industry_assignments_read = 1;
    }
    tdx_buf_clear(&raw);
    tdx_buf_clear(&utf8);

    /* The concept, style and index blocks. */
    if (snprintf(path, sizeof(path), "%s/T0002/hq_cache/infoharbor_block.dat", root) >= (int)sizeof(path)) {
        tdx_error_set(err, "block data path is too long");
        goto done;
    }
    read = tdx_cache_read(path, &raw, err);
    if (read < 0)
        goto done;
    if (read > 0) {
        if (tdx_jsn_gbk_to_utf8(raw.data, raw.len, &utf8, err) != TDX_OK)
            goto done;
        if (tdx_blocks_parse_infoharbor((const char *)utf8.data, utf8.len,
                                        blocks + total_blocks, block_capacity - total_blocks,
                                        &harbor_blocks, members, member_capacity,
                                        &total_members,
                                        report ? &report->count_mismatches : NULL,
                                        err) != TDX_OK)
            goto done;
        total_blocks += harbor_blocks;
        if (report)
            report->infoharbor_read = 1;
    }
    if (block_count)
        *block_count = total_blocks;
    if (member_count)
        *member_count = total_members;
    status = TDX_OK;

done:
    tdx_buf_free(&raw);
    tdx_buf_free(&utf8);
    return status;
}

/* --- the member union -------------------------------------------------- */

/* Finds a block by id, or NULL. */
static const tdx_block *find_block(const tdx_block *blocks, size_t count, const char *id) {
    size_t index;
    for (index = 0; index < count; ++index)
        if (strcmp(blocks[index].id, id) == 0)
            return &blocks[index];
    return NULL;
}

/* Whether a security is already among a block's emitted members. */
static int already_present(const tdx_block_expanded_member *out, size_t count,
                           const char *block_id, const char *security_id) {
    size_t index;
    for (index = 0; index < count; ++index)
        if (strcmp(out[index].block_id, block_id) == 0 &&
            strcmp(out[index].security_id, security_id) == 0)
            return 1;
    return 0;
}

int tdx_blocks_expand(const tdx_block *blocks, size_t block_count,
                      const tdx_block_member *members, size_t member_count,
                      tdx_block_expanded_member *out, size_t capacity, size_t *out_count,
                      tdx_error *err) {
    size_t stored = 0;
    size_t index;

    if (out_count)
        *out_count = 0;
    if (!blocks || !members || !out) {
        tdx_error_set(err, "expanding the members needs blocks, members and an output");
        return TDX_ERR;
    }
    /* Direct members first, so a security that is both its own and an ancestor's keeps the
     * direct attribution. */
    for (index = 0; index < member_count; ++index) {
        tdx_block_expanded_member *item;
        if (stored >= capacity) {
            tdx_error_set(err, "the union holds more than %zu members", capacity);
            return TDX_ERR;
        }
        item = &out[stored];
        memset(item, 0, sizeof(*item));
        snprintf(item->block_id, sizeof(item->block_id), "%s", members[index].block_id);
        snprintf(item->family, sizeof(item->family), "%s", members[index].family);
        item->market_id = members[index].market_id;
        snprintf(item->code, sizeof(item->code), "%s", members[index].code);
        snprintf(item->security_id, sizeof(item->security_id), "%s",
                 members[index].security_id);
        item->membership = TDX_BLOCKS_MEMBERSHIP_DIRECT;
        stored++;
    }
    /* Then each block's ancestors, walking the parent chain to its end. */
    for (index = 0; index < block_count; ++index) {
        const tdx_block *ancestor = blocks[index].parent_id[0]
                                        ? find_block(blocks, block_count, blocks[index].parent_id)
                                        : NULL;
        size_t depth = 0;
        while (ancestor && depth < 64) {
            size_t member;
            for (member = 0; member < member_count; ++member) {
                tdx_block_expanded_member *item;
                if (strcmp(members[member].block_id, ancestor->id) != 0)
                    continue;
                if (already_present(out, stored, blocks[index].id, members[member].security_id))
                    continue;
                if (stored >= capacity) {
                    tdx_error_set(err, "the union holds more than %zu members", capacity);
                    return TDX_ERR;
                }
                item = &out[stored];
                memset(item, 0, sizeof(*item));
                snprintf(item->block_id, sizeof(item->block_id), "%s", blocks[index].id);
                snprintf(item->family, sizeof(item->family), "%s", blocks[index].family);
                item->market_id = members[member].market_id;
                snprintf(item->code, sizeof(item->code), "%s", members[member].code);
                snprintf(item->security_id, sizeof(item->security_id), "%s",
                         members[member].security_id);
                item->membership = TDX_BLOCKS_MEMBERSHIP_EXPANDED;
                stored++;
            }
            /* The next ancestor up.  A cycle would be a malformed file; the depth cap stops it
             * rather than looping forever. */
            ancestor = ancestor->parent_id[0]
                           ? find_block(blocks, block_count, ancestor->parent_id)
                           : NULL;
            depth++;
        }
    }
    if (out_count)
        *out_count = stored;
    return TDX_OK;
}
