/* test_blocks.c - the block families, their hierarchy and their members.
 *
 * THE THREE FILES ARE BUILT HERE rather than captured, because what needs asserting are the
 * cases a capture does not promise to contain: a row of a type this family does not own, a
 * duplicate source key, a key whose shape is wrong, a member before any header, an infoharbor
 * block whose declared count disagrees with what follows.
 *
 * The live check is in output/blocks_verification_evidence.txt: all three real files, 1,159
 * blocks, 79,514 members, 5,663 assignments, and - the part worth trusting - ZERO infoharbor
 * blocks whose declared member count differs from the count this reader produced.  That is the
 * file checking itself more than a thousand times, and a parser that split a member line wrongly
 * would fail it wholesale.
 *
 * The hierarchy is checked against the keys the real file carries as well: 569 parent links, none
 * of which disagrees with an independent "drop the last two characters" computation. */
#include <stdio.h>
#include <string.h>

#include "tdx_blocks.h"
#include "tdx_blocks_json.h"
#include "render_check.h"

static int failures = 0;

#define CHECK(condition, ...)                                                        \
    do {                                                                             \
        if (!(condition)) {                                                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                              \
            printf(__VA_ARGS__);                                                     \
            printf("\n");                                                            \
            failures++;                                                              \
        }                                                                            \
    } while (0)

static tdx_error error;

static const char *text_of(const tdx_buf *buffer) {
    static char scratch[8192];
    size_t copy = buffer->len < sizeof(scratch) - 1 ? buffer->len : sizeof(scratch) - 1;
    if (copy && buffer->data)
        memcpy(scratch, buffer->data, copy);
    scratch[copy] = '\0';
    return scratch;
}

/* The key arithmetic the hierarchy rests on. */
static void test_keys(void) {
    char parent[24];
    int level = 0;

    CHECK(tdx_blocks_parent_key("T010101", parent, sizeof(parent)) &&
              strcmp(parent, "T0101") == 0,
          "T010101's parent is T0101, got %s", parent);
    CHECK(tdx_blocks_parent_key("T0101", parent, sizeof(parent)) &&
              strcmp(parent, "T01") == 0,
          "T0101's parent is T01, got %s", parent);
    CHECK(tdx_blocks_parent_key("T01", parent, sizeof(parent)) && parent[0] == '\0',
          "T01 has no parent, got %s", parent);
    CHECK(tdx_blocks_parent_key("X500102", parent, sizeof(parent)) &&
              strcmp(parent, "X5001") == 0,
          "the research keys follow the same rule, got %s", parent);
    /* THE BOUNDARY IS THREE.  A three-character key has no parent; a four-character one yields
     * a two-character parent that no real block carries, which is harmless but is NOT the same
     * as having none. */
    CHECK(tdx_blocks_parent_key("T01", parent, sizeof(parent)) && parent[0] == '\0',
          "three characters is no parent, got %s", parent);
    CHECK(tdx_blocks_parent_key("T010", parent, sizeof(parent)) &&
              strcmp(parent, "T0") == 0,
          "four characters gives a degenerate two-character parent, got %s", parent);
    CHECK(!tdx_blocks_parent_key(NULL, parent, sizeof(parent)), "a null key is refused");

    /* The level is half the length, and the first character has to be one of the two the format
     * uses - a key of the wrong shape is a bug rather than a shallow hierarchy. */
    CHECK(tdx_blocks_source_level("T01", &level) && level == 1, "T01 is level 1, got %d",
          level);
    CHECK(tdx_blocks_source_level("T0101", &level) && level == 2, "T0101 is level 2, got %d",
          level);
    CHECK(tdx_blocks_source_level("T010101", &level) && level == 3,
          "T010101 is level 3, got %d", level);
    CHECK(tdx_blocks_source_level("X500102", &level) && level == 3, "X500102 is level 3");
    CHECK(!tdx_blocks_source_level("A0101", &level), "a key starting with A is refused");
    CHECK(!tdx_blocks_source_level("", &level), "and an empty one");
    CHECK(!tdx_blocks_source_level(NULL, &level), "and a null one");

    /* The infoharbor families. */
    CHECK(tdx_blocks_infoharbor_family("GN", 2) != NULL &&
              strcmp(tdx_blocks_infoharbor_family("GN", 2), TDX_BLOCKS_FAMILY_CONCEPT) == 0,
          "GN is the concept family");
    CHECK(strcmp(tdx_blocks_infoharbor_family("FG", 2), TDX_BLOCKS_FAMILY_STYLE) == 0,
          "FG is the style family");
    CHECK(strcmp(tdx_blocks_infoharbor_family("ZS", 2), TDX_BLOCKS_FAMILY_INDEX) == 0,
          "ZS is the index family");
    CHECK(tdx_blocks_infoharbor_family("XX", 2) == NULL, "an unknown prefix has no family");
    CHECK(tdx_blocks_infoharbor_family("G", 1) == NULL, "nor does a one-character one");
}

static void test_industry_catalog(void) {
    /* Two rows this family owns (types 2 and 12), one it does not, and a child whose parent
     * comes LATER in the file - which is why the parent is resolved after the whole read. */
    static const char *text =
        "child|880302|2|5|1|T010101\r\n"
        "not-ours|880081|5|2|0|X1\r\n"
        "parent|880301|2|5|0|T0101\r\n"
        "research|880999|12|5|1|X500102\r\n";
    tdx_block blocks[8];
    size_t count = 0;
    size_t skipped = 0;
    size_t index;

    error.message[0] = '\0';
    CHECK(tdx_blocks_parse_industry_catalog(text, strlen(text), blocks, 8, &count, &skipped,
                                            &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == 3, "three of four rows are this family, got %zu", count);
    CHECK(skipped == 1, "and one is skipped and counted, got %zu", skipped);

    /* The child's parent is resolved even though the parent's row came later. */
    for (index = 0; index < count; ++index) {
        CHECK(strcmp(blocks[index].family,
                     strcmp(blocks[index].source_key, "X500102") == 0
                         ? TDX_BLOCKS_FAMILY_RESEARCH_INDUSTRY
                         : TDX_BLOCKS_FAMILY_INDUSTRY) == 0,
              "%s is in the right family, got %s", blocks[index].source_key,
              blocks[index].family);
        CHECK(strcmp(blocks[index].id, "") != 0, "the identity is built");
    }
    CHECK(strcmp(blocks[0].id, "industry:T010101") == 0, "the id is family:key, got %s",
          blocks[0].id);
    CHECK(strcmp(blocks[0].parent_id, "industry:T0101") == 0,
          "the child points at the parent that came later, got %s", blocks[0].parent_id);
    CHECK(strcmp(blocks[1].parent_id, "industry:T01") == 0 ||
              blocks[1].parent_id[0] == '\0',
          "the parent points at T01, which is not in the file, so it has none: %s",
          blocks[1].parent_id);
    CHECK(blocks[0].level == 3 && blocks[0].is_leaf == 1,
          "the child is level 3 and a leaf, got %d and %d", blocks[0].level, blocks[0].is_leaf);
    CHECK(blocks[2].level == 3 && blocks[2].is_leaf == 1,
          "and so is the research one");
    CHECK(blocks[0].declared_count == -1,
          "the catalog carries no declared count, got %d", blocks[0].declared_count);

    /* A duplicate source key would give two blocks one identity. */
    {
        static const char *dup = "a|1|2|1|0|T0101\nb|2|2|1|0|T0101\n";
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_industry_catalog(dup, strlen(dup), blocks, 8, &count, &skipped,
                                                &error) == TDX_ERR,
              "a duplicate source key is refused");
        CHECK(strstr(error.message, "repeats the source key") != NULL, "and says so: %s",
              error.message);
    }
    /* A source key that does not start with T or X. */
    {
        static const char *bad = "a|1|2|1|0|A0101\n";
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_industry_catalog(bad, strlen(bad), blocks, 8, &count, &skipped,
                                                &error) == TDX_ERR,
              "a key of the wrong shape is refused");
        CHECK(strstr(error.message, "start with T or X") != NULL, "and says why: %s",
              error.message);
    }
    /* A line with the wrong field count. */
    {
        static const char *short_line = "a|1|2|1|0\n";
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_industry_catalog(short_line, strlen(short_line), blocks, 8,
                                                &count, &skipped, &error) == TDX_ERR,
              "a five-field line is refused");
        CHECK(strstr(error.message, "expected 6") != NULL, "and says the width: %s",
              error.message);
    }
}

static void test_assignments(void) {
    static const char *text =
        "0|000001|T1001|||X500102\n"
        "1|600519|T010101|||X5001\n";
    tdx_block_assignment rows[8];
    size_t count = 0;

    error.message[0] = '\0';
    CHECK(tdx_blocks_parse_industry_assignments(text, strlen(text), rows, 8, &count,
                                                &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(count == 2, "two rows, got %zu", count);
    CHECK(rows[0].market_id == 0 && strcmp(rows[0].code, "000001") == 0,
          "the market and code, got %d and %s", rows[0].market_id, rows[0].code);
    CHECK(strcmp(rows[0].industry_code, "T1001") == 0 &&
              strcmp(rows[0].research_code, "X500102") == 0,
          "the two industry codes are read from fields 3 and 6: %s / %s",
          rows[0].industry_code, rows[0].research_code);
    {
        static const char *bad = "x|000001|T1001|||X500102\n";
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_industry_assignments(bad, strlen(bad), rows, 8, &count,
                                                    &error) == TDX_ERR,
              "a non-numeric market is refused: %s", error.message);
    }
}

static void test_infoharbor(void) {
    /* Seven header fields, a declared count of THREE, and three members - one of them after a
     * trailing comma, which yields an empty token to skip. */
    static const char *text =
        "#GN_test block,3,880515,20050607,20260512,,\r\n"
        "0#000408,0#000538,1#600519,\r\n"
        "#FG_style block,1,880516,20050607,20260512,,\r\n"
        "0#000001\r\n";
    tdx_block blocks[8];
    tdx_block_member members[16];
    size_t block_count = 0;
    size_t member_count = 0;
    size_t mismatches = 0;

    error.message[0] = '\0';
    CHECK(tdx_blocks_parse_infoharbor(text, strlen(text), blocks, 8, &block_count, members, 16,
                                      &member_count, &mismatches, &error) == TDX_OK,
          "parse: %s", error.message);
    CHECK(block_count == 2, "two blocks, got %zu", block_count);
    CHECK(member_count == 4, "four members, got %zu", member_count);
    CHECK(mismatches == 0, "and both declared counts match, got %zu", mismatches);
    CHECK(strcmp(blocks[0].family, TDX_BLOCKS_FAMILY_CONCEPT) == 0,
          "the first is a concept block, got %s", blocks[0].family);
    CHECK(strcmp(blocks[0].name, "test block") == 0, "named from after the underscore, got %s",
          blocks[0].name);
    CHECK(strcmp(blocks[0].code, "880515") == 0, "with its stable code, got %s",
          blocks[0].code);
    CHECK(strcmp(blocks[0].id, "concept:880515") == 0, "and its identity, got %s",
          blocks[0].id);
    CHECK(strcmp(blocks[0].start_date, "20050607") == 0, "and its start date, got %s",
          blocks[0].start_date);
    CHECK(blocks[0].level == 1 && blocks[0].is_leaf == 1 && blocks[0].parent_id[0] == '\0',
          "an infoharbor block is a level-1 leaf with no parent");
    CHECK(blocks[0].member_count == 3, "the first block holds three, got %zu",
          blocks[0].member_count);
    CHECK(strcmp(blocks[1].family, TDX_BLOCKS_FAMILY_STYLE) == 0,
          "the second is a style block, got %s", blocks[1].family);
    /* Identities and market prefixes. */
    CHECK(strcmp(members[0].block_id, "concept:880515") == 0, "a member names its block");
    CHECK(strcmp(members[0].security_id, "SZ000408") == 0, "and its security, got %s",
          members[0].security_id);
    CHECK(strcmp(members[2].security_id, "SH600519") == 0, "market 1 is Shanghai, got %s",
          members[2].security_id);

    /* A DECLARED COUNT THAT DISAGREES IS COUNTED, NOT REFUSED: a block that declares 88 and
     * carries 87 says something about the file, and refusing would hide it. */
    {
        static const char *wrong = "#GN_x,99,880515,20050607,20260512,,\n0#000408\n";
        tdx_block one[2];
        tdx_block_member one_member[4];
        size_t one_count = 0;
        size_t one_members = 0;
        size_t one_mismatch = 0;
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_infoharbor(wrong, strlen(wrong), one, 2, &one_count, one_member,
                                          4, &one_members, &one_mismatch, &error) == TDX_OK,
              "a count mismatch is not an error: %s", error.message);
        CHECK(one_mismatch == 1, "but it is counted, got %zu", one_mismatch);
        CHECK(one_count == 1 && one_members == 1, "and the block is read anyway");
        CHECK(one[0].declared_count == 99 && one[0].member_count == 1,
              "with both counts visible: %d declared, %zu parsed", one[0].declared_count,
              one[0].member_count);
    }
    /* Members before any header. */
    {
        static const char *stray = "0#000408\n";
        tdx_block one[2];
        tdx_block_member one_member[4];
        size_t one_count = 0;
        size_t one_members = 0;
        size_t one_mismatch = 0;
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_infoharbor(stray, strlen(stray), one, 2, &one_count, one_member,
                                          4, &one_members, &one_mismatch, &error) == TDX_ERR,
              "a member before any header is refused");
        CHECK(strstr(error.message, "before any header") != NULL, "and says so: %s",
              error.message);
    }
    /* An unknown family prefix, and a member without a market. */
    {
        static const char *unknown = "#XX_x,1,1,1,1,,\n0#000408\n";
        static const char *nomarket = "#GN_x,1,1,1,1,,\n000408\n";
        tdx_block one[2];
        tdx_block_member one_member[4];
        size_t one_count = 0;
        size_t one_members = 0;
        size_t one_mismatch = 0;
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_infoharbor(unknown, strlen(unknown), one, 2, &one_count,
                                          one_member, 4, &one_members, &one_mismatch,
                                          &error) == TDX_ERR,
              "an unknown family prefix is refused: %s", error.message);
        error.message[0] = '\0';
        CHECK(tdx_blocks_parse_infoharbor(nomarket, strlen(nomarket), one, 2, &one_count,
                                          one_member, 4, &one_members, &one_mismatch,
                                          &error) == TDX_ERR,
              "a member with no market is refused: %s", error.message);
    }
}

/* THE LIVE DATA MAKES THIS A NO-OP, which is exactly why it needs a constructed case: every
 * assignment in tdxhy.cfg points at a leaf, so no ancestor contributes a member and the union
 * equals the direct set everywhere.  The case below gives the parent members of its own. */
static void test_expansion(void) {
    tdx_block blocks[4];
    tdx_block_member members[8];
    tdx_block_expanded_member out[16];
    size_t out_count = 0;
    size_t index;
    size_t direct = 0;
    size_t expanded = 0;

    memset(blocks, 0, sizeof(blocks));
    memset(members, 0, sizeof(members));
    /* A parent that holds one security of its own, and a child that holds two. */
    snprintf(blocks[0].id, sizeof(blocks[0].id), "industry:T01");
    snprintf(blocks[0].source_key, sizeof(blocks[0].source_key), "T01");
    blocks[0].level = 1;
    snprintf(blocks[1].id, sizeof(blocks[1].id), "industry:T0101");
    snprintf(blocks[1].parent_id, sizeof(blocks[1].parent_id), "industry:T01");
    snprintf(blocks[1].source_key, sizeof(blocks[1].source_key), "T0101");
    blocks[1].level = 2;

    snprintf(members[0].block_id, sizeof(members[0].block_id), "industry:T01");
    snprintf(members[0].security_id, sizeof(members[0].security_id), "SZ000001");
    snprintf(members[0].code, sizeof(members[0].code), "000001");
    snprintf(members[1].block_id, sizeof(members[1].block_id), "industry:T0101");
    snprintf(members[1].security_id, sizeof(members[1].security_id), "SH600519");
    snprintf(members[1].code, sizeof(members[1].code), "600519");
    members[1].market_id = 1;
    snprintf(members[2].block_id, sizeof(members[2].block_id), "industry:T0101");
    snprintf(members[2].security_id, sizeof(members[2].security_id), "SZ000001");
    snprintf(members[2].code, sizeof(members[2].code), "000001");
    /* A second security the PARENT holds and the child does not: this one must be inherited.
     * The one above is held by both, so it must NOT be inherited a second time - the two
     * together are what make the dedup visible. */
    snprintf(members[3].block_id, sizeof(members[3].block_id), "industry:T01");
    snprintf(members[3].security_id, sizeof(members[3].security_id), "SH600000");
    snprintf(members[3].code, sizeof(members[3].code), "600000");
    members[3].market_id = 1;

    error.message[0] = '\0';
    CHECK(tdx_blocks_expand(blocks, 2, members, 4, out, 16, &out_count, &error) == TDX_OK,
          "expand: %s", error.message);
    for (index = 0; index < out_count; ++index) {
        if (strcmp(out[index].membership, TDX_BLOCKS_MEMBERSHIP_DIRECT) == 0)
            direct++;
        else
            expanded++;
    }
    CHECK(direct == 4, "the four direct members are all present, got %zu", direct);
    /* ONE INHERITED, ONE DEDUPED.  The child inherits 600000, which only its parent holds, and
     * does NOT get a second copy of 000001, which it holds itself.  So exactly one expanded
     * entry - and the count is the check on both branches at once. */
    CHECK(expanded == 1, "the child inherits exactly one and dedups the other, got %zu",
          expanded);
    for (index = 0; index < out_count; ++index)
        if (strcmp(out[index].membership, TDX_BLOCKS_MEMBERSHIP_EXPANDED) == 0) {
            CHECK(strcmp(out[index].block_id, "industry:T0101") == 0,
                  "the inherited one belongs to the child, got %s", out[index].block_id);
            CHECK(strcmp(out[index].security_id, "SH600000") == 0,
                  "and is the one the child does not hold, got %s", out[index].security_id);
        }
    /* The parent keeps its own as DIRECT: attribution follows where the security actually is. */
    for (index = 0; index < out_count; ++index)
        if (strcmp(out[index].security_id, "SZ000001") == 0 &&
            strcmp(out[index].block_id, "industry:T01") == 0)
            CHECK(strcmp(out[index].membership, TDX_BLOCKS_MEMBERSHIP_DIRECT) == 0,
                  "the parent's own member stays direct");

    /* A block with no parent contributes nothing beyond its own members. */
    {
        tdx_block orphan[1];
        tdx_block_expanded_member small[4];
        size_t small_count = 0;
        memset(orphan, 0, sizeof(orphan));
        snprintf(orphan[0].id, sizeof(orphan[0].id), "industry:T01");
        CHECK(tdx_blocks_expand(orphan, 1, members, 4, small, 8, &small_count, &error) == TDX_OK,
              "expand: %s", error.message);
        CHECK(small_count == 4, "four direct members and nothing inherited, got %zu",
              small_count);
    }
    /* A CYCLE IS MALFORMED BUT MUST NOT HANG: the walk is capped. */
    {
        tdx_block loop[2];
        tdx_block_expanded_member small[64];
        size_t small_count = 0;
        memset(loop, 0, sizeof(loop));
        snprintf(loop[0].id, sizeof(loop[0].id), "industry:A");
        snprintf(loop[0].parent_id, sizeof(loop[0].parent_id), "industry:B");
        snprintf(loop[1].id, sizeof(loop[1].id), "industry:B");
        snprintf(loop[1].parent_id, sizeof(loop[1].parent_id), "industry:A");
        error.message[0] = '\0';
        CHECK(tdx_blocks_expand(loop, 2, members, 3, small, 64, &small_count,
                                &error) == TDX_OK,
              "a cycle terminates rather than hanging: %s", error.message);
    }
    CHECK(tdx_blocks_expand(NULL, 0, members, 1, out, 16, &out_count, &error) == TDX_ERR,
          "no blocks is refused");
}

static void test_rendering(void) {
    static const char *text =
        "parent|880301|2|5|0|T0101\n"
        "child|880302|2|5|1|T010101\n";
    tdx_block blocks[4];
    tdx_block_member member;
    tdx_block_assignment assignment;
    tdx_blocks_load_report report;
    size_t count = 0;
    size_t skipped = 0;
    tdx_buf line;
    char reason[192];

    error.message[0] = '\0';
    CHECK(tdx_blocks_parse_industry_catalog(text, strlen(text), blocks, 4, &count, &skipped,
                                            &error) == TDX_OK,
          "setup: %s", error.message);
    tdx_buf_init(&line);
    CHECK(tdx_blocks_format_block(&line, &blocks[1], 1, &error) == TDX_OK, "render: %s",
          error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the block line parses: %s\n    %s", reason, text_of(&line));
    CHECK(strstr(text_of(&line), "\"type\":\"block\"") != NULL, "the type");
    /* THE HIERARCHY IS THE OUTPUT: the parent, the level and the leaf flag. */
    CHECK(strstr(text_of(&line), "\"parent_id\":\"industry:T0101\"") != NULL,
          "the parent travels with it: %s", text_of(&line));
    CHECK(strstr(text_of(&line), "\"level\":3") != NULL, "and the level");
    CHECK(strstr(text_of(&line), "\"is_leaf\":true") != NULL, "and the leaf flag");
    /* A count only the infoharbor file carries renders as null, not as zero. */
    CHECK(strstr(text_of(&line), "\"declared_count\":null") != NULL,
          "an absent declared count is null: %s", text_of(&line));

    member.block_id[0] = '\0';
    snprintf(member.block_id, sizeof(member.block_id), "concept:880515");
    snprintf(member.family, sizeof(member.family), "concept");
    member.market_id = 0;
    snprintf(member.code, sizeof(member.code), "000408");
    snprintf(member.security_id, sizeof(member.security_id), "SZ000408");
    tdx_buf_clear(&line);
    CHECK(tdx_blocks_format_member(&line, &member, 0, &error) == TDX_OK, "render: %s",
          error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "the member parses: %s",
          reason);

    memset(&assignment, 0, sizeof(assignment));
    assignment.market_id = 0;
    snprintf(assignment.code, sizeof(assignment.code), "000001");
    snprintf(assignment.industry_code, sizeof(assignment.industry_code), "T1001");
    snprintf(assignment.research_code, sizeof(assignment.research_code), "X500102");
    tdx_buf_clear(&line);
    CHECK(tdx_blocks_format_assignment(&line, &assignment, 0, &error) == TDX_OK,
          "render: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
          "the assignment parses: %s\n    %s", reason, text_of(&line));

    {
        tdx_block_expanded_member expanded;
        memset(&expanded, 0, sizeof(expanded));
        snprintf(expanded.block_id, sizeof(expanded.block_id), "industry:T0101");
        snprintf(expanded.family, sizeof(expanded.family), "industry");
        snprintf(expanded.security_id, sizeof(expanded.security_id), "SZ000001");
        snprintf(expanded.code, sizeof(expanded.code), "000001");
        expanded.membership = TDX_BLOCKS_MEMBERSHIP_EXPANDED;
        tdx_buf_clear(&line);
        CHECK(tdx_blocks_format_expanded(&line, &expanded, 0, &error) == TDX_OK,
              "render: %s", error.message);
        CHECK(render_parses(text_of(&line), reason, sizeof(reason)),
              "the expanded member parses: %s\n    %s", reason, text_of(&line));
        CHECK(strstr(text_of(&line), "\"membership\":\"expanded\"") != NULL,
              "and says where it came from: %s", text_of(&line));
    }

    memset(&report, 0, sizeof(report));
    report.industry_catalog_read = 1;
    report.infoharbor_read = 1;
    report.catalog_skipped = 459;
    report.count_mismatches = 3;
    tdx_buf_clear(&line);
    CHECK(tdx_blocks_format_summary(&line, 1159, 79514, 5663, &report, "C:\\new_tdx",
                                    &error) == TDX_OK,
          "summary: %s", error.message);
    CHECK(render_parses(text_of(&line), reason, sizeof(reason)), "the summary parses: %s",
          reason);
    CHECK(strstr(text_of(&line), "\"member_count_mismatches\":3") != NULL,
          "the self-check result travels with the summary: %s", text_of(&line));
    CHECK(strstr(text_of(&line), "\"industry_assignments_read\":false") != NULL,
          "and a file that was not there is visible: %s", text_of(&line));
    tdx_buf_free(&line);
}

int main(void) {
    test_keys();
    test_industry_catalog();
    test_assignments();
    test_infoharbor();
    test_expansion();
    test_rendering();

    if (failures) {
        printf("%d block check(s) failed\n", failures);
        return 1;
    }
    printf("block checks passed\n");
    return 0;
}
