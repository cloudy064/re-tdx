/* blocks: command orchestration. */
#include "cli_commands.h"
#include "tdx_blocks.h"
#include "tdx_blocks_json.h"
#include "tdx_bytes.h"
#include "tdx_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_blocks(const cli_blocks_options *options, tdx_error *err) {
    static tdx_buf line = {0};
    tdx_block *blocks = NULL;
    tdx_block_member *members = NULL;
    tdx_block_assignment *assignments = NULL;
    tdx_blocks_load_report report;
    FILE *stream = NULL;
    size_t block_count = 0;
    size_t member_count = 0;
    size_t assignment_count = 0;
    size_t emitted = 0;
    size_t index;
    int status = TDX_ERR;

    blocks = (tdx_block *)calloc(TDX_BLOCKS_MAX, sizeof(*blocks));
    members = (tdx_block_member *)calloc(TDX_BLOCKS_MEMBERS_MAX, sizeof(*members));
    assignments = (tdx_block_assignment *)calloc(TDX_BLOCKS_MEMBERS_MAX, sizeof(*assignments));
    if (!blocks || !members || !assignments) {
        tdx_error_set(err, "out of memory for the block tables");
        goto done;
    }
    if (tdx_blocks_load(options->common.root, blocks, TDX_BLOCKS_MAX, &block_count, members,
                        TDX_BLOCKS_MEMBERS_MAX, &member_count, assignments,
                        TDX_BLOCKS_MEMBERS_MAX, &assignment_count, &report, err) != TDX_OK)
        goto done;
    if (!options->common.quiet)
        fprintf(stderr,
                "blocks: catalog %s, assignments %s, infoharbor %s, %zu blocks, %zu members, "
                "%zu assignments, %zu declared-count mismatches\n",
                report.industry_catalog_read ? "read" : "absent",
                report.industry_assignments_read ? "read" : "absent",
                report.infoharbor_read ? "read" : "absent", block_count, member_count,
                assignment_count, report.count_mismatches);

    tdx_buf_init(&line);
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < block_count; ++index) {
        if (options->max_records && emitted >= options->max_records)
            break;
        tdx_buf_clear(&line);
        if (tdx_blocks_format_block(&line, &blocks[index], index, err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
            fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the block stream");
            goto close_output;
        }
        emitted++;
    }
    if (options->show_members) {
        for (index = 0; index < member_count; ++index) {
            if (options->max_records && emitted >= options->max_records)
                break;
            tdx_buf_clear(&line);
            if (tdx_blocks_format_member(&line, &members[index], index, err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the member stream");
                goto close_output;
            }
            emitted++;
        }
    }
    if (options->show_assignments) {
        for (index = 0; index < assignment_count; ++index) {
            if (options->max_records && emitted >= options->max_records)
                break;
            tdx_buf_clear(&line);
            if (tdx_blocks_format_assignment(&line, &assignments[index], index, err) != TDX_OK)
                goto close_output;
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the assignment stream");
                goto close_output;
            }
            emitted++;
        }
    }
    if (options->show_expanded) {
        tdx_block_expanded_member *expanded;
        size_t expanded_count = 0;
        expanded = (tdx_block_expanded_member *)calloc(TDX_BLOCKS_MEMBERS_MAX * 4,
                                                       sizeof(*expanded));
        if (!expanded) {
            tdx_error_set(err, "out of memory for the member union");
            goto close_output;
        }
        if (tdx_blocks_expand(blocks, block_count, members, member_count, expanded,
                              TDX_BLOCKS_MEMBERS_MAX * 4, &expanded_count, err) != TDX_OK) {
            free(expanded);
            goto close_output;
        }
        if (!options->common.quiet)
            fprintf(stderr, "blocks: the union of %zu direct members over the hierarchy holds "
                            "%zu\n", member_count, expanded_count);
        for (index = 0; index < expanded_count; ++index) {
            if (options->max_records && emitted >= options->max_records)
                break;
            tdx_buf_clear(&line);
            if (tdx_blocks_format_expanded(&line, &expanded[index], index, err) != TDX_OK) {
                free(expanded);
                goto close_output;
            }
            if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
                fwrite(line.data, 1, line.len, stream) != line.len) {
                tdx_error_set(err, "cannot write the union stream");
                free(expanded);
                goto close_output;
            }
            emitted++;
        }
        free(expanded);
    }
    tdx_buf_clear(&line);
    if (tdx_blocks_format_summary(&line, block_count, member_count, assignment_count, &report,
                                  options->common.root, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK ||
        fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the block summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
done:
    free(blocks);
    free(members);
    free(assignments);
    tdx_buf_free(&line);
    return status;
}
