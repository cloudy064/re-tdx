/* tdx_zst_replay.h - incremental replay of a zst_cache tag stream.
 *
 * tdx_zst.c decodes one record at a time; that is not a quote.  The stream is
 * *change-only*: a record carries just the tags whose value moved, so the vast
 * majority of records are one or two fields wide (time, last price, one book
 * level).  A usable historical tick therefore has to be folded forward from the
 * head of the file, and that fold is what this module does.
 *
 * Replay model
 * ------------
 * Each security keeps a tag -> value map.  Applying a record overwrites the
 * tags it carries and leaves the rest untouched; the resulting map is the full
 * book at that instant.  A tag that is present but repeats the previous value
 * is carried, not changed - the two are reported separately because the wire
 * text is not normalised ('17.3900' and '17.390000' are the same price).
 *
 * Field semantics, all calibrated against the four real zst_cache samples with
 * output/zst_field_table.py; "native" below means native/src/data/image_data.cpp,
 * whose recovered TdxW UI labels (TdxW.exe 0xCA1330..0xCA1348) name 1G..1J.
 *
 *   tag            meaning                          evidence
 *   0T             time HHMMSS                       present in every record
 *   04             previous close                    constant for the session
 *   05             open                              first set at the 09:25 auction
 *   06 / 07        high / low                        tracks the day extreme
 *   08             last price                        native: last
 *   09             cumulative trade count            monotone, 0 <= d09 <= dvolume/100
 *   10             cumulative volume, shares         native: cumulative_volume
 *   1A             cumulative turnover, yuan         native: cumulative_amount
 *   1C             P/E ratio                        1C/last constant per security to <5e-4
 *   1E / 1F        limit up / limit down            == round(1.1*04,2) / round(0.9*04,2)
 *   1G / 1H        average bid price / total bid     native + TdxW UI labels
 *   1I / 1J        average ask price / total ask     native + TdxW UI labels
 *   0D             session phase text (S0 O0 B0 T0 C0 E0 A0)
 *   1i..1m         phase-adjacent auxiliary block
 *   20..29/30..39  bid price/volume, levels 1..10 (one tag per level)
 *   40..49/50..59  ask price/volume, levels 1..10
 *   25..29/35..39  levels 6..10: the tags are always present with an EMPTY value
 *   45..49/55..59  on all four samples, so this build never invents levels 6..10
 *   0a..0c 1B 1D 1v 1w Z3 Z4   never non-empty on any sample; kept as raw tags
 *
 * The delta between the L1 client and this one is worth stating: 0x0547 always
 * ships a whole record, so tdx_state.c has to *invent* the delta.  Here the
 * server already ships only the delta, and replay has to *reconstruct* the whole
 * record.  Both directions end at the same tdx_diff_mask style of change report. */
#ifndef TDX_ZST_REPLAY_H
#define TDX_ZST_REPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"
#include "tdx_zst.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Which logical group a changed tag belongs to. */
typedef unsigned tdx_zst_diff_mask;

#define TDX_ZST_DIFF_NEW 0x0001u       /* first record of this security */
#define TDX_ZST_DIFF_TIME 0x0002u      /* 0T */
#define TDX_ZST_DIFF_LAST 0x0004u      /* 08 */
#define TDX_ZST_DIFF_OHLC 0x0008u      /* 04 05 06 07 */
#define TDX_ZST_DIFF_LIMITS 0x0010u    /* 1E 1F */
#define TDX_ZST_DIFF_TRADES 0x0020u    /* 09 */
#define TDX_ZST_DIFF_VOLUME 0x0040u    /* 10 */
#define TDX_ZST_DIFF_AMOUNT 0x0080u    /* 1A (and the derived average price) */
#define TDX_ZST_DIFF_BOOK 0x0100u      /* 20..59 */
#define TDX_ZST_DIFF_AGGREGATE 0x0200u /* 1G 1H 1I 1J */
#define TDX_ZST_DIFF_VALUATION 0x0400u /* 1C */
#define TDX_ZST_DIFF_PHASE 0x0800u     /* 0D 1i..1m */
#define TDX_ZST_DIFF_OTHER 0x1000u     /* a tag this build does not classify */

/* Typed members that the stream has actually delivered at least once. */
#define TDX_ZST_HAVE_PREVIOUS_CLOSE 0x00000001u
#define TDX_ZST_HAVE_OPEN 0x00000002u
#define TDX_ZST_HAVE_HIGH 0x00000004u
#define TDX_ZST_HAVE_LOW 0x00000008u
#define TDX_ZST_HAVE_LAST 0x00000010u
#define TDX_ZST_HAVE_LIMIT_UP 0x00000020u
#define TDX_ZST_HAVE_LIMIT_DOWN 0x00000040u
#define TDX_ZST_HAVE_TRADE_COUNT 0x00000080u
#define TDX_ZST_HAVE_VOLUME 0x00000100u
#define TDX_ZST_HAVE_AMOUNT 0x00000200u
#define TDX_ZST_HAVE_AVERAGE_PRICE 0x00000400u
#define TDX_ZST_HAVE_PE_RATIO 0x00000800u
#define TDX_ZST_HAVE_AVERAGE_BID 0x00001000u
#define TDX_ZST_HAVE_TOTAL_BID 0x00002000u
#define TDX_ZST_HAVE_AVERAGE_ASK 0x00004000u
#define TDX_ZST_HAVE_TOTAL_ASK 0x00008000u
#define TDX_ZST_HAVE_PHASE 0x00010000u
#define TDX_ZST_HAVE_TIME 0x00020000u

typedef struct tdx_zst_book_level {
    double price;
    double volume;
} tdx_zst_book_level;

/* The merged tag map of one security.  Linear lookup on purpose: a real file
 * carries 71 distinct tags, so a scan is cheaper than any index. */
typedef struct tdx_zst_state {
    tdx_zst_field fields[TDX_ZST_MAX_FIELDS];
    size_t count;
    size_t dropped; /* tags that did not fit; zero on every observed file */
} tdx_zst_state;

/* One fully reconstructed instant.  Everything except the identity and the
 * borrowed pointers is a copy, so the typed part stays valid for as long as the
 * caller keeps the struct. */
typedef struct tdx_zst_snapshot {
    char security_key[9];
    /* Raw two leading key digits, not the 0x0547 market id; see tdx_zst.h.  The
     * caller that knows which security it asked for is the authority on the
     * market, so rendering takes the market as an argument. */
    int key_market;
    char code[8];

    size_t record_index; /* index into tdx_zst_document.records */
    size_t update_index; /* how many snapshots this security has produced */

    /* borrowed from the replayer; valid until the next apply on that replayer */
    const tdx_zst_field *fields;
    size_t field_count;
    const char *changed_ids[TDX_ZST_MAX_FIELDS]; /* raw text really differed */
    size_t changed_id_count;

    unsigned present;     /* TDX_ZST_HAVE_* */
    unsigned bid_levels;  /* bit i: bid level i (0-based) has data */
    unsigned ask_levels;  /* bit i: ask level i (0-based) has data */
    size_t dropped_fields; /* tags the map could not hold; zero on real files */

    int time_hhmmss;
    double previous_close;
    double open_price;
    double high_price;
    double low_price;
    double last_price;
    double limit_up;
    double limit_down;
    double trade_count;   /* 09 */
    double volume;        /* 10, shares */
    double amount;        /* 1A, yuan */
    double average_price; /* amount / volume, derived */
    double pe_ratio;      /* 1C */
    double average_bid;   /* 1G */
    double total_bid;     /* 1H */
    double average_ask;   /* 1I */
    double total_ask;     /* 1J */
    tdx_zst_book_level bids[TDX_ZST_LEVELS];
    tdx_zst_book_level asks[TDX_ZST_LEVELS];
    char phase[TDX_ZST_FIELD_VALUE]; /* 0D, trailing padding trimmed */
} tdx_zst_snapshot;

typedef struct tdx_zst_security {
    char security_key[9];
    int key_market;
    char code[8];
    tdx_zst_state state;
    size_t updates;
} tdx_zst_security;

typedef struct tdx_zst_replay {
    tdx_zst_security *securities;
    size_t count;
    size_t capacity;
    size_t applied; /* records folded so far; becomes the snapshot record index */
} tdx_zst_replay;

void tdx_zst_replay_init(tdx_zst_replay *replay);
void tdx_zst_replay_free(tdx_zst_replay *replay);

/* Folds one record forward and writes the resulting full snapshot.  *changed
 * receives the affected logical groups; the snapshot's changed_ids lists the
 * tags whose raw text moved.  Both out parameters may be NULL. */
int tdx_zst_replay_apply(tdx_zst_replay *replay, const tdx_zst_record *record,
                         tdx_zst_snapshot *out, tdx_zst_diff_mask *changed,
                         tdx_error *err);

/* Replays every record of a decoded document in order.  The visitor returns
 * TDX_OK to continue or TDX_ERR to stop; *visited receives how many records
 * were handed to it. */
typedef int (*tdx_zst_visit_fn)(void *context, const tdx_zst_snapshot *snapshot,
                                const tdx_zst_record *record, tdx_zst_diff_mask changed,
                                tdx_error *err);

int tdx_zst_replay_document(const tdx_zst_document *document, tdx_zst_visit_fn visit,
                            void *context, size_t *visited, tdx_error *err);

/* --- building blocks, exposed for direct unit testing ---------------- */

void tdx_zst_state_init(tdx_zst_state *state);
const char *tdx_zst_state_find(const tdx_zst_state *state, const char *id);
/* Overwrites or appends one tag.  *changed (optional) reports whether the raw
 * text differed from the stored value; a NULL state value counts as a change. */
int tdx_zst_state_apply(tdx_zst_state *state, const char *id, const char *value,
                        int *changed, tdx_error *err);

/* The equality this module applies to two wire values: exact text, else exact
 * numbers when both sides parse as numbers, else text with trailing padding
 * trimmed.  Exposed because it is the whole reason the diff is stable. */
int tdx_zst_value_equal(const char *left, const char *right);

void tdx_zst_project(const tdx_zst_state *state, tdx_zst_snapshot *out);

/* Maps a wire tag to its logical group. */
tdx_zst_diff_mask tdx_zst_tag_group(const char *id);

/* Renders a group mask into names, newest-value ordering.  Returns the number of
 * names written, never more than capacity. */
size_t tdx_zst_diff_names(tdx_zst_diff_mask mask, const char **names, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* TDX_ZST_REPLAY_H */
