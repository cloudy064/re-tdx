/* tdx_blocks_json.c - JSONL rendering for the block families. */
#include "tdx_blocks_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays to a
 * pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define BLK_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

int tdx_blocks_format_block(tdx_buf *out, const tdx_block *block, size_t index,
                            tdx_error *err) {
    if (!out || !block) {
        tdx_error_set(err, "rendering a block needs a buffer and a block");
        return TDX_ERR;
    }
    if (BLK_LITERAL(out, err, "{\"type\":\"block\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"id\":", index) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, block->id, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"family\":") != TDX_OK ||
        tdx_format_json_string(out, block->family, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"code\":") != TDX_OK ||
        tdx_format_json_string(out, block->code, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"name\":") != TDX_OK ||
        tdx_format_json_string(out, block->name, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"source_key\":") != TDX_OK ||
        tdx_format_json_string(out, block->source_key, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"parent_id\":") != TDX_OK)
        return TDX_ERR;
    if (block->parent_id[0]) {
        if (tdx_format_json_string(out, block->parent_id, err) != TDX_OK)
            return TDX_ERR;
    } else if (BLK_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"level\":%d,\"is_leaf\":%s,",
                              block->level, block->is_leaf ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    /* A declared count only the infoharbor file carries, so it can be absent. */
    if (block->declared_count < 0) {
        if (BLK_LITERAL(out, err, "\"declared_count\":null,") != TDX_OK)
            return TDX_ERR;
    } else if (tdx_buf_append_printf(out, err, "\"declared_count\":%d,",
                                     block->declared_count) != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, "\"member_count\":%zu,\"start_date\":",
                              block->member_count) != TDX_OK)
        return TDX_ERR;
    if (block->start_date[0]) {
        if (tdx_format_json_string(out, block->start_date, err) != TDX_OK)
            return TDX_ERR;
    } else if (BLK_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (BLK_LITERAL(out, err, ",\"update_date\":") != TDX_OK)
        return TDX_ERR;
    if (block->update_date[0]) {
        if (tdx_format_json_string(out, block->update_date, err) != TDX_OK)
            return TDX_ERR;
    } else if (BLK_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}

int tdx_blocks_format_member(tdx_buf *out, const tdx_block_member *member, size_t index,
                             tdx_error *err) {
    if (!out || !member) {
        tdx_error_set(err, "rendering a member needs a buffer and a member");
        return TDX_ERR;
    }
    if (BLK_LITERAL(out, err, "{\"type\":\"block_member\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"block_id\":", index) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, member->block_id, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"family\":") != TDX_OK ||
        tdx_format_json_string(out, member->family, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_append_printf(out, err,
                                 ",\"security_id\":\"%s\",\"market_id\":%d,\"code\":\"%s\"}",
                                 member->security_id, member->market_id, member->code);
}

int tdx_blocks_format_assignment(tdx_buf *out, const tdx_block_assignment *assignment,
                                 size_t index, tdx_error *err) {
    if (!out || !assignment) {
        tdx_error_set(err, "rendering an assignment needs a buffer and an assignment");
        return TDX_ERR;
    }
    return tdx_buf_append_printf(out, err,
                                 "{\"type\":\"block_assignment\",\"index\":%zu,"
                                 "\"market_id\":%d,\"code\":\"%s\","
                                 "\"industry_code\":\"%s\",\"research_code\":\"%s\"}",
                                 index, assignment->market_id, assignment->code,
                                 assignment->industry_code, assignment->research_code);
}

int tdx_blocks_format_summary(tdx_buf *out, size_t blocks, size_t members, size_t assignments,
                              const tdx_blocks_load_report *report, const char *root,
                              tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "the block summary needs a buffer");
        return TDX_ERR;
    }
    if (BLK_LITERAL(out, err, "{\"type\":\"block_summary\",\"root\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, root ? root : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"blocks\":%zu,\"members\":%zu,\"assignments\":%zu",
                              blocks, members, assignments) != TDX_OK)
        return TDX_ERR;
    if (report) {
        if (tdx_buf_append_printf(out, err,
                                  ",\"industry_catalog_read\":%s,"
                                  "\"industry_assignments_read\":%s,"
                                  "\"infoharbor_read\":%s,\"catalog_rows_skipped\":%zu,"
                                  "\"member_count_mismatches\":%zu",
                                  report->industry_catalog_read ? "true" : "false",
                                  report->industry_assignments_read ? "true" : "false",
                                  report->infoharbor_read ? "true" : "false",
                                  report->catalog_skipped, report->count_mismatches) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}

int tdx_blocks_format_expanded(tdx_buf *out, const tdx_block_expanded_member *member,
                               size_t index, tdx_error *err) {
    if (!out || !member) {
        tdx_error_set(err, "rendering an expanded member needs a buffer and a member");
        return TDX_ERR;
    }
    if (BLK_LITERAL(out, err, "{\"type\":\"block_expanded_member\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%zu,\"block_id\":", index) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, member->block_id, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"family\":") != TDX_OK ||
        tdx_format_json_string(out, member->family, err) != TDX_OK)
        return TDX_ERR;
    if (BLK_LITERAL(out, err, ",\"membership\":") != TDX_OK ||
        tdx_format_json_string(out, member->membership, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_append_printf(out, err,
                                 ",\"security_id\":\"%s\",\"market_id\":%d,\"code\":\"%s\"}",
                                 member->security_id, member->market_id, member->code);
}
