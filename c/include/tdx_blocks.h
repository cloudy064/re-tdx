/* tdx_blocks.h - the block families, their hierarchy and their members.
 *
 * Three local files under T0002/hq_cache, all GBK, plus the local security master:
 *
 *   tdxzs3.cfg            the industry catalog: one pipe-separated line of six fields per
 *                         industry - name, code, TYPE, a count, a leaf flag, a source key.  Only
 *                         types 2 and 12 are this family; the file also carries index and other
 *                         rows that are skipped.
 *   tdxhy.cfg             a pipe-separated line per SECURITY: market, code, industry code, and
 *                         a research-industry code.
 *   infoharbor_block.dat  concept, style and index membership.  A header line begins with '#' and
 *                         has seven comma fields; the member lines that follow are
 *                         "<market>#<code>" separated by commas.
 *
 * THE HIERARCHY IS IN THE KEY, NOT IN A PARENT COLUMN.  A source key like T0101 is a child of
 * T01, and its level is half its length: "(len - 1) / 2".  So a parent is found by dropping the
 * last two characters and looking that up - which means a key whose shape is wrong is a bug
 * rather than a missing link, and this refuses one that does not start with T or X.
 *
 * THE INFOHARBOR FILE CHECKS ITSELF.  Each header declares how many members follow, and the
 * reference refuses a block whose parsed count differs.  That check is kept, and the counts are
 * reported either way - a block that declares 88 and carries 87 says something about the file,
 * and refusing to read it would hide that rather than show it. */
#ifndef TDX_BLOCKS_H
#define TDX_BLOCKS_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_BLOCKS_MAX 4096
#define TDX_BLOCKS_MEMBERS_MAX 200000
#define TDX_BLOCKS_TEXT_MAX 128
#define TDX_BLOCKS_ID_MAX 64

/* The family names the reference uses, so both halves of the family agree. */
#define TDX_BLOCKS_FAMILY_INDUSTRY "industry"
#define TDX_BLOCKS_FAMILY_RESEARCH_INDUSTRY "research-industry"
#define TDX_BLOCKS_FAMILY_CONCEPT "concept"
#define TDX_BLOCKS_FAMILY_STYLE "style"
#define TDX_BLOCKS_FAMILY_INDEX "index"

typedef struct tdx_block {
    char id[TDX_BLOCKS_ID_MAX];
    char family[24];
    char code[16];
    char name[TDX_BLOCKS_TEXT_MAX];
    char source_key[24];
    char parent_id[TDX_BLOCKS_ID_MAX];
    int level;
    int is_leaf;
    /* From the infoharbor header; -1 when the file did not declare one. */
    int declared_count;
    size_t member_count;
    char start_date[12];
    char update_date[12];
} tdx_block;

typedef struct tdx_block_member {
    char block_id[TDX_BLOCKS_ID_MAX];
    char family[24];
    int market_id;
    char code[16];
    char security_id[24];
} tdx_block_member;

/* The industry catalog.  Rows whose type is neither 2 nor 12 are skipped and counted; a
 * duplicate source key is an error, because two blocks with one key cannot both own a parent. */
int tdx_blocks_parse_industry_catalog(const char *text, size_t length, tdx_block *out,
                                      size_t capacity, size_t *out_count, size_t *skipped,
                                      tdx_error *err);

/* One security's industry and research-industry assignment. */
typedef struct tdx_block_assignment {
    int market_id;
    char code[16];
    char industry_code[24];
    char research_code[24];
} tdx_block_assignment;

int tdx_blocks_parse_industry_assignments(const char *text, size_t length,
                                          tdx_block_assignment *out, size_t capacity,
                                          size_t *out_count, tdx_error *err);

/* Concept, style and index blocks with their members.  Members are appended, so the caller
 * passes the array and its running count in and gets it back advanced. */
int tdx_blocks_parse_infoharbor(const char *text, size_t length, tdx_block *blocks,
                                size_t block_capacity, size_t *block_count,
                                tdx_block_member *members, size_t member_capacity,
                                size_t *member_count, size_t *count_mismatches,
                                tdx_error *err);

/* The parent key of a source key, and its level.  Exposed because a test can check them against
 * the keys the real file contains rather than only against a block that came out right. */
int tdx_blocks_parent_key(const char *source_key, char *out, size_t capacity);
int tdx_blocks_source_level(const char *source_key, int *out);

/* The family a member's block belongs to, from an infoharbor header prefix.  Returns NULL when
 * the prefix is not one this reader knows. */
const char *tdx_blocks_infoharbor_family(const char *prefix, size_t length);

/* Reads the three files from <root>/T0002/hq_cache.  Each part is optional: a file that is not
 * there leaves that family empty rather than failing, and the caller can see which were read. */
typedef struct tdx_blocks_load_report {
    int industry_catalog_read;
    int industry_assignments_read;
    int infoharbor_read;
    size_t catalog_skipped;
    size_t count_mismatches;
} tdx_blocks_load_report;

int tdx_blocks_load(const char *root, tdx_block *blocks, size_t block_capacity,
                    size_t *block_count, tdx_block_member *members, size_t member_capacity,
                    size_t *member_count, tdx_block_assignment *assignments,
                    size_t assignment_capacity, size_t *assignment_count,
                    tdx_blocks_load_report *report, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_BLOCKS_H */
