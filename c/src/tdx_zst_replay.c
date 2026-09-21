/* tdx_zst_replay.c - fold a change-only zst tag stream into full snapshots. */
#include "tdx_zst_replay.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- value handling ---------------------------------------------------- */

/* A tag value is numeric when the whole trimmed text parses as a finite double.
 * The server is not consistent about precision - the same price arrives as
 * '17.3900' and '17.390000' in adjacent records - so equality has to be decided
 * numerically whenever both sides are numbers. */
static int parse_number(const char *text, double *out) {
    char *end = NULL;
    double value;
    if (!text || !*text)
        return 0;
    value = strtod(text, &end);
    if (end == text || !isfinite(value))
        return 0;
    while (*end == ' ' || *end == '\t')
        end++;
    if (*end != '\0')
        return 0;
    *out = value;
    return 1;
}

/* Text comparison ignores the trailing padding the server adds to 0D. */
static int text_equal(const char *left, const char *right) {
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    while (left_length && (left[left_length - 1] == ' ' || left[left_length - 1] == '\t'))
        left_length--;
    while (right_length && (right[right_length - 1] == ' ' || right[right_length - 1] == '\t'))
        right_length--;
    return left_length == right_length && strncmp(left, right, left_length) == 0;
}

int tdx_zst_value_equal(const char *left, const char *right) {
    double left_number;
    double right_number;
    if (left == right)
        return 1;
    if (!left)
        left = "";
    if (!right)
        right = "";
    if (strcmp(left, right) == 0)
        return 1;
    if (parse_number(left, &left_number) && parse_number(right, &right_number))
        return left_number == right_number;
    return text_equal(left, right);
}

/* --- tag map ----------------------------------------------------------- */

void tdx_zst_state_init(tdx_zst_state *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
}

static const tdx_zst_field *state_slot_const(const tdx_zst_state *state, const char *id) {
    size_t index;
    if (!state || !id || !id[0] || !id[1])
        return NULL;
    for (index = 0; index < state->count; ++index)
        if (state->fields[index].id[0] == id[0] && state->fields[index].id[1] == id[1])
            return &state->fields[index];
    return NULL;
}

static tdx_zst_field *state_slot(tdx_zst_state *state, const char *id) {
    return (tdx_zst_field *)state_slot_const(state, id);
}

const char *tdx_zst_state_find(const tdx_zst_state *state, const char *id) {
    const tdx_zst_field *field = state_slot_const(state, id);
    return field ? field->value : NULL;
}

static void field_store(tdx_zst_field *field, const char *value) {
    size_t length = strlen(value);
    if (length >= TDX_ZST_FIELD_VALUE)
        length = TDX_ZST_FIELD_VALUE - 1;
    memcpy(field->value, value, length);
    field->value[length] = '\0';
}

int tdx_zst_state_apply(tdx_zst_state *state, const char *id, const char *value,
                        int *changed, tdx_error *err) {
    tdx_zst_field *field;
    if (!state || !id || !id[0] || !id[1]) {
        tdx_error_set(err, "zst state needs a two-character tag");
        return TDX_ERR;
    }
    if (!value)
        value = "";
    field = state_slot(state, id);
    if (field) {
        if (changed)
            *changed = !tdx_zst_value_equal(field->value, value);
        field_store(field, value);
        return TDX_OK;
    }
    if (state->count >= TDX_ZST_MAX_FIELDS) {
        /* Keep the snapshot usable rather than failing the whole file; the
         * count is surfaced so a lossy replay is never mistaken for a clean one. */
        state->dropped++;
        if (changed)
            *changed = 1;
        return TDX_OK;
    }
    field = &state->fields[state->count++];
    field->id[0] = id[0];
    field->id[1] = id[1];
    field->id[2] = '\0';
    field_store(field, value);
    if (changed)
        *changed = 1;
    return TDX_OK;
}

/* --- projection -------------------------------------------------------- */

/* Level ladders, one two-character tag per level: 20..29 bid prices, 30..39 bid
 * volumes, 40..49 ask prices and 50..59 ask volumes, level 1 at the low digit.
 * A real file carries 25..29 / 35..39 / 45..49 / 55..59 as present-but-empty
 * placeholders, so levels 6..10 never set a bit. */
static void level_tag(char *out, char family, int level) {
    out[0] = family;
    out[1] = (char)('0' + level);
    out[2] = '\0';
}

static int state_number(const tdx_zst_state *state, const char *id, double *out) {
    return parse_number(tdx_zst_state_find(state, id), out);
}

static void project_ladder(const tdx_zst_state *state, char price_family, char volume_family,
                           tdx_zst_book_level *levels, unsigned *present_mask) {
    int level;
    for (level = 0; level < TDX_ZST_LEVELS; ++level) {
        char tag[3];
        double value;
        int seen = 0;
        level_tag(tag, price_family, level);
        if (state_number(state, tag, &value)) {
            levels[level].price = value;
            seen = 1;
        }
        level_tag(tag, volume_family, level);
        if (state_number(state, tag, &value)) {
            levels[level].volume = value;
            seen = 1;
        }
        if (seen)
            *present_mask |= 1u << level;
    }
}

static void copy_trimmed(char *out, size_t out_size, const char *text) {
    size_t length;
    if (!text)
        text = "";
    length = strlen(text);
    while (length && (text[length - 1] == ' ' || text[length - 1] == '\t'))
        length--;
    if (length >= out_size)
        length = out_size - 1;
    memcpy(out, text, length);
    out[length] = '\0';
}

void tdx_zst_project(const tdx_zst_state *state, tdx_zst_snapshot *out) {
    double value;
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!state)
        return;

    out->dropped_fields = state->dropped;
    if (state_number(state, "0T", &value)) {
        out->time_hhmmss = (int)value;
        out->present |= TDX_ZST_HAVE_TIME;
    }
    if (state_number(state, "04", &value)) {
        out->previous_close = value;
        out->present |= TDX_ZST_HAVE_PREVIOUS_CLOSE;
    }
    if (state_number(state, "05", &value)) {
        out->open_price = value;
        out->present |= TDX_ZST_HAVE_OPEN;
    }
    if (state_number(state, "06", &value)) {
        out->high_price = value;
        out->present |= TDX_ZST_HAVE_HIGH;
    }
    if (state_number(state, "07", &value)) {
        out->low_price = value;
        out->present |= TDX_ZST_HAVE_LOW;
    }
    if (state_number(state, "08", &value)) {
        out->last_price = value;
        out->present |= TDX_ZST_HAVE_LAST;
    }
    if (state_number(state, "1E", &value)) {
        out->limit_up = value;
        out->present |= TDX_ZST_HAVE_LIMIT_UP;
    }
    if (state_number(state, "1F", &value)) {
        out->limit_down = value;
        out->present |= TDX_ZST_HAVE_LIMIT_DOWN;
    }
    if (state_number(state, "09", &value)) {
        out->trade_count = value;
        out->present |= TDX_ZST_HAVE_TRADE_COUNT;
    }
    if (state_number(state, "10", &value)) {
        out->volume = value;
        out->present |= TDX_ZST_HAVE_VOLUME;
    }
    if (state_number(state, "1A", &value)) {
        out->amount = value;
        out->present |= TDX_ZST_HAVE_AMOUNT;
    }
    if ((out->present & (TDX_ZST_HAVE_VOLUME | TDX_ZST_HAVE_AMOUNT)) ==
            (TDX_ZST_HAVE_VOLUME | TDX_ZST_HAVE_AMOUNT) &&
        out->volume != 0.0) {
        out->average_price = out->amount / out->volume;
        out->present |= TDX_ZST_HAVE_AVERAGE_PRICE;
    }
    if (state_number(state, "1C", &value)) {
        out->pe_ratio = value;
        out->present |= TDX_ZST_HAVE_PE_RATIO;
    }
    if (state_number(state, "1G", &value)) {
        out->average_bid = value;
        out->present |= TDX_ZST_HAVE_AVERAGE_BID;
    }
    if (state_number(state, "1H", &value)) {
        out->total_bid = value;
        out->present |= TDX_ZST_HAVE_TOTAL_BID;
    }
    if (state_number(state, "1I", &value)) {
        out->average_ask = value;
        out->present |= TDX_ZST_HAVE_AVERAGE_ASK;
    }
    if (state_number(state, "1J", &value)) {
        out->total_ask = value;
        out->present |= TDX_ZST_HAVE_TOTAL_ASK;
    }
    {
        const char *phase = tdx_zst_state_find(state, "0D");
        if (phase && *phase) {
            copy_trimmed(out->phase, sizeof(out->phase), phase);
            out->present |= TDX_ZST_HAVE_PHASE;
        }
    }
    project_ladder(state, '2', '3', out->bids, &out->bid_levels);
    project_ladder(state, '4', '5', out->asks, &out->ask_levels);
}

/* --- grouping and diffing --------------------------------------------- */

tdx_zst_diff_mask tdx_zst_tag_group(const char *id) {
    if (!id || !id[0] || !id[1])
        return TDX_ZST_DIFF_OTHER;
    if (id[0] == '0') {
        if (id[1] == 'T')
            return TDX_ZST_DIFF_TIME;
        if (id[1] >= '4' && id[1] <= '7')
            return TDX_ZST_DIFF_OHLC;
        if (id[1] == '8')
            return TDX_ZST_DIFF_LAST;
        if (id[1] == '9')
            return TDX_ZST_DIFF_TRADES;
        if (id[1] == 'D')
            return TDX_ZST_DIFF_PHASE;
        return TDX_ZST_DIFF_OTHER;
    }
    if (id[0] == '1') {
        if (id[1] == '0')
            return TDX_ZST_DIFF_VOLUME;
        if (id[1] == 'A')
            return TDX_ZST_DIFF_AMOUNT;
        if (id[1] == 'C')
            return TDX_ZST_DIFF_VALUATION;
        if (id[1] == 'E' || id[1] == 'F')
            return TDX_ZST_DIFF_LIMITS;
        if (id[1] == 'G' || id[1] == 'H' || id[1] == 'I' || id[1] == 'J')
            return TDX_ZST_DIFF_AGGREGATE;
        if (id[1] >= 'i' && id[1] <= 'm')
            return TDX_ZST_DIFF_PHASE;
        return TDX_ZST_DIFF_OTHER;
    }
    if (id[0] >= '2' && id[0] <= '5' && id[1] >= '0' && id[1] <= '9')
        return TDX_ZST_DIFF_BOOK;
    return TDX_ZST_DIFF_OTHER;
}

static int present_changed(unsigned before, unsigned after, unsigned bit) {
    return ((before ^ after) & bit) != 0;
}

static tdx_zst_diff_mask typed_diff(const tdx_zst_snapshot *before,
                                    const tdx_zst_snapshot *after) {
    tdx_zst_diff_mask mask = 0;
    int level;

    if (before->time_hhmmss != after->time_hhmmss ||
        present_changed(before->present, after->present, TDX_ZST_HAVE_TIME))
        mask |= TDX_ZST_DIFF_TIME;
    if (before->last_price != after->last_price ||
        present_changed(before->present, after->present, TDX_ZST_HAVE_LAST))
        mask |= TDX_ZST_DIFF_LAST;
    if (before->previous_close != after->previous_close || before->open_price != after->open_price ||
        before->high_price != after->high_price || before->low_price != after->low_price ||
        present_changed(before->present, after->present,
                        TDX_ZST_HAVE_PREVIOUS_CLOSE | TDX_ZST_HAVE_OPEN | TDX_ZST_HAVE_HIGH |
                            TDX_ZST_HAVE_LOW))
        mask |= TDX_ZST_DIFF_OHLC;
    if (before->limit_up != after->limit_up || before->limit_down != after->limit_down ||
        present_changed(before->present, after->present,
                        TDX_ZST_HAVE_LIMIT_UP | TDX_ZST_HAVE_LIMIT_DOWN))
        mask |= TDX_ZST_DIFF_LIMITS;
    if (before->trade_count != after->trade_count ||
        present_changed(before->present, after->present, TDX_ZST_HAVE_TRADE_COUNT))
        mask |= TDX_ZST_DIFF_TRADES;
    if (before->volume != after->volume ||
        present_changed(before->present, after->present, TDX_ZST_HAVE_VOLUME))
        mask |= TDX_ZST_DIFF_VOLUME;
    if (before->amount != after->amount || before->average_price != after->average_price ||
        present_changed(before->present, after->present,
                        TDX_ZST_HAVE_AMOUNT | TDX_ZST_HAVE_AVERAGE_PRICE))
        mask |= TDX_ZST_DIFF_AMOUNT;
    if (before->bid_levels != after->bid_levels || before->ask_levels != after->ask_levels)
        mask |= TDX_ZST_DIFF_BOOK;
    else
        for (level = 0; level < TDX_ZST_LEVELS; ++level)
            if (before->bids[level].price != after->bids[level].price ||
                before->bids[level].volume != after->bids[level].volume ||
                before->asks[level].price != after->asks[level].price ||
                before->asks[level].volume != after->asks[level].volume) {
                mask |= TDX_ZST_DIFF_BOOK;
                break;
            }
    if (before->average_bid != after->average_bid || before->total_bid != after->total_bid ||
        before->average_ask != after->average_ask || before->total_ask != after->total_ask ||
        present_changed(before->present, after->present,
                        TDX_ZST_HAVE_AVERAGE_BID | TDX_ZST_HAVE_TOTAL_BID |
                            TDX_ZST_HAVE_AVERAGE_ASK | TDX_ZST_HAVE_TOTAL_ASK))
        mask |= TDX_ZST_DIFF_AGGREGATE;
    if (before->pe_ratio != after->pe_ratio ||
        present_changed(before->present, after->present, TDX_ZST_HAVE_PE_RATIO))
        mask |= TDX_ZST_DIFF_VALUATION;
    if (strcmp(before->phase, after->phase) != 0 ||
        present_changed(before->present, after->present, TDX_ZST_HAVE_PHASE))
        mask |= TDX_ZST_DIFF_PHASE;
    return mask;
}

size_t tdx_zst_diff_names(tdx_zst_diff_mask mask, const char **names, size_t capacity) {
    static const struct {
        tdx_zst_diff_mask bit;
        const char *name;
    } table[] = {
        {TDX_ZST_DIFF_NEW, "new"},
        {TDX_ZST_DIFF_TIME, "time"},
        {TDX_ZST_DIFF_LAST, "last"},
        {TDX_ZST_DIFF_OHLC, "ohlc"},
        {TDX_ZST_DIFF_LIMITS, "limits"},
        {TDX_ZST_DIFF_TRADES, "trades"},
        {TDX_ZST_DIFF_VOLUME, "volume"},
        {TDX_ZST_DIFF_AMOUNT, "amount"},
        {TDX_ZST_DIFF_BOOK, "book"},
        {TDX_ZST_DIFF_AGGREGATE, "aggregate"},
        {TDX_ZST_DIFF_VALUATION, "valuation"},
        {TDX_ZST_DIFF_PHASE, "phase"},
        {TDX_ZST_DIFF_OTHER, "other"},
    };
    size_t index;
    size_t written = 0;
    if (!names)
        return 0;
    for (index = 0; index < sizeof(table) / sizeof(table[0]); ++index) {
        if (!(mask & table[index].bit))
            continue;
        if (written >= capacity)
            break;
        names[written++] = table[index].name;
    }
    return written;
}

/* --- replay ------------------------------------------------------------ */

void tdx_zst_replay_init(tdx_zst_replay *replay) {
    if (!replay)
        return;
    memset(replay, 0, sizeof(*replay));
}

void tdx_zst_replay_free(tdx_zst_replay *replay) {
    if (!replay)
        return;
    free(replay->securities);
    memset(replay, 0, sizeof(*replay));
}

static void security_identity(tdx_zst_security *security) {
    size_t index;
    security->key_market =
        (security->security_key[0] - '0') * 10 + (security->security_key[1] - '0');
    for (index = 0; index < 6; ++index)
        security->code[index] = security->security_key[2 + index];
    security->code[6] = '\0';
}

static int security_lookup(tdx_zst_replay *replay, const char *key,
                           tdx_zst_security **out, int *created, tdx_error *err) {
    size_t index;
    tdx_zst_security *grown;
    for (index = 0; index < replay->count; ++index) {
        if (strcmp(replay->securities[index].security_key, key) == 0) {
            *out = &replay->securities[index];
            *created = 0;
            return TDX_OK;
        }
    }
    if (replay->count == replay->capacity) {
        size_t wanted = replay->capacity ? replay->capacity * 2 : 4;
        grown = (tdx_zst_security *)realloc(replay->securities, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the replay to %zu securities", wanted);
            return TDX_ERR;
        }
        replay->securities = grown;
        replay->capacity = wanted;
    }
    *out = &replay->securities[replay->count++];
    memset(*out, 0, sizeof(**out));
    snprintf((*out)->security_key, sizeof((*out)->security_key), "%s", key);
    security_identity(*out);
    *created = 1;
    return TDX_OK;
}

int tdx_zst_replay_apply(tdx_zst_replay *replay, const tdx_zst_record *record,
                         tdx_zst_snapshot *out, tdx_zst_diff_mask *changed,
                         tdx_error *err) {
    tdx_zst_security *security = NULL;
    tdx_zst_snapshot before;
    const char *changed_ids[TDX_ZST_MAX_FIELDS];
    size_t changed_count = 0;
    tdx_zst_diff_mask raw_other = 0;
    tdx_zst_diff_mask mask;
    size_t index;
    int created = 0;

    if (!replay || !record || !out) {
        tdx_error_set(err, "zst replay needs a replayer, a record and an output snapshot");
        return TDX_ERR;
    }
    if (security_lookup(replay, record->security_key, &security, &created, err) != TDX_OK)
        return TDX_ERR;

    tdx_zst_project(&security->state, &before);
    for (index = 0; index < record->field_count; ++index) {
        const tdx_zst_field *field = &record->fields[index];
        int raw_changed = 0;
        if (tdx_zst_state_apply(&security->state, field->id, field->value, &raw_changed,
                                err) != TDX_OK)
            return TDX_ERR;
        if (!raw_changed)
            continue;
        if (tdx_zst_tag_group(field->id) == TDX_ZST_DIFF_OTHER)
            raw_other |= TDX_ZST_DIFF_OTHER;
        if (changed_count < TDX_ZST_MAX_FIELDS) {
            tdx_zst_field *slot = state_slot(&security->state, field->id);
            changed_ids[changed_count++] = slot ? slot->id : field->id;
        }
    }

    tdx_zst_project(&security->state, out);
    mask = typed_diff(&before, out) | raw_other;
    if (created)
        mask |= TDX_ZST_DIFF_NEW;

    snprintf(out->security_key, sizeof(out->security_key), "%s", security->security_key);
    out->key_market = security->key_market;
    snprintf(out->code, sizeof(out->code), "%s", security->code);
    out->record_index = replay->applied++;
    out->update_index = security->updates++;
    out->fields = security->state.fields;
    out->field_count = security->state.count;
    out->changed_id_count = changed_count;
    for (index = 0; index < changed_count; ++index)
        out->changed_ids[index] = changed_ids[index];
    if (changed)
        *changed = mask;
    return TDX_OK;
}

int tdx_zst_replay_document(const tdx_zst_document *document, tdx_zst_visit_fn visit,
                            void *context, size_t *visited, tdx_error *err) {
    tdx_zst_replay replay;
    tdx_zst_snapshot snapshot;
    size_t index;
    size_t seen = 0;
    int result = TDX_OK;

    if (!document || !visit) {
        tdx_error_set(err, "zst replay needs a document and a visitor");
        return TDX_ERR;
    }
    tdx_zst_replay_init(&replay);
    for (index = 0; index < document->count; ++index) {
        const tdx_zst_record *record = &document->records[index];
        tdx_zst_diff_mask changed = 0;
        if (tdx_zst_replay_apply(&replay, record, &snapshot, &changed, err) != TDX_OK) {
            result = TDX_ERR;
            break;
        }
        seen++;
        if (visit(context, &snapshot, record, changed, err) != TDX_OK) {
            result = TDX_ERR;
            break;
        }
    }
    tdx_zst_replay_free(&replay);
    if (visited)
        *visited = seen;
    return result;
}
