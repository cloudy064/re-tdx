/* tdx_state.c - open addressing last-known-value table. */
#include "tdx_state.h"

#include <stdlib.h>
#include <string.h>

static int key_equal(const tdx_code *left, const tdx_code *right) {
    return left->market_id == right->market_id &&
           memcmp(left->code, right->code, 6) == 0;
}

static uint64_t hash_key(int market_id, const char *code) {
    /* FNV-1a over the seven key bytes; the seed keeps market 0 distinct from a
     * code that happens to hash identically. */
    uint64_t hash = 1469598103934665603ull;
    size_t index;
    hash ^= (uint64_t)(uint32_t)market_id;
    hash *= 1099511628211ull;
    for (index = 0; index < 6; ++index) {
        hash ^= (uint64_t)(unsigned char)code[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

static int level_equal(const tdx_level *left, const tdx_level *right) {
    return left->price == right->price && left->volume_hand == right->volume_hand;
}

tdx_diff_mask tdx_depth_diff(const tdx_depth *previous, const tdx_depth *current) {
    tdx_diff_mask mask = 0;
    size_t level;

    if (!previous || !current)
        return TDX_DIFF_NEW;
    if (previous->last != current->last)
        mask |= TDX_DIFF_LAST;
    if (previous->previous != current->previous || previous->open != current->open ||
        previous->high != current->high || previous->low != current->low)
        mask |= TDX_DIFF_OHLC;
    if (previous->amount != current->amount)
        mask |= TDX_DIFF_AMOUNT;
    if (previous->total_hand != current->total_hand ||
        previous->current_hand != current->current_hand)
        mask |= TDX_DIFF_VOLUME;
    if (previous->inside != current->inside || previous->outside != current->outside ||
        previous->auction_imbalance_hand != current->auction_imbalance_hand ||
        previous->open_amount != current->open_amount)
        mask |= TDX_DIFF_DISHES;
    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        if (!level_equal(&previous->buys[level], &current->buys[level]) ||
            !level_equal(&previous->sells[level], &current->sells[level])) {
            mask |= TDX_DIFF_BOOK;
            break;
        }
    }
    if (previous->status != current->status ||
        previous->update_time != current->update_time ||
        previous->active != current->active ||
        previous->unknown_after_outer != current->unknown_after_outer ||
        previous->tail_size != current->tail_size)
        mask |= TDX_DIFF_STATUS;
    return mask;
}

size_t tdx_diff_names(tdx_diff_mask mask, const char **names, size_t capacity) {
    static const struct {
        tdx_diff_mask bit;
        const char *name;
    } table[] = {
        {TDX_DIFF_NEW, "new"},        {TDX_DIFF_LAST, "last"},
        {TDX_DIFF_OHLC, "ohlc"},      {TDX_DIFF_AMOUNT, "amount"},
        {TDX_DIFF_VOLUME, "volume"},  {TDX_DIFF_DISHES, "dishes"},
        {TDX_DIFF_BOOK, "book"},      {TDX_DIFF_STATUS, "status"},
    };
    size_t written = 0;
    size_t index;
    if (!names)
        return 0;
    for (index = 0; index < sizeof(table) / sizeof(table[0]); ++index) {
        if ((mask & table[index].bit) == 0)
            continue;
        if (written >= capacity)
            break;
        names[written++] = table[index].name;
    }
    return written;
}

int tdx_state_init(tdx_state *state, size_t expected, tdx_error *err) {
    size_t buckets = 64;

    if (!state) {
        tdx_error_set(err, "state table is null");
        return TDX_ERR;
    }
    /* Keep the load factor at or below 0.5 so probing stays short. */
    while (buckets < expected * 2) {
        if (buckets > (SIZE_MAX / 2)) {
            tdx_error_set(err, "state table is too large");
            return TDX_ERR;
        }
        buckets *= 2;
    }
    state->entries = (tdx_state_entry *)calloc(buckets, sizeof(*state->entries));
    if (!state->entries) {
        tdx_error_set(err, "out of memory for %zu state buckets", buckets);
        state->bucket_count = 0;
        state->count = 0;
        return TDX_ERR;
    }
    state->bucket_count = buckets;
    state->count = 0;
    return TDX_OK;
}

void tdx_state_free(tdx_state *state) {
    if (!state)
        return;
    free(state->entries);
    state->entries = NULL;
    state->bucket_count = 0;
    state->count = 0;
}

size_t tdx_state_count(const tdx_state *state) {
    return state ? state->count : 0;
}

static tdx_state_entry *find_slot(tdx_state *state, const tdx_code *code,
                                  int *found) {
    const size_t mask = state->bucket_count - 1;
    size_t index = (size_t)(hash_key(code->market_id, code->code) & mask);
    for (;;) {
        tdx_state_entry *entry = &state->entries[index];
        if (!entry->used) {
            *found = 0;
            return entry;
        }
        if (key_equal(&entry->security, code)) {
            *found = 1;
            return entry;
        }
        index = (index + 1) & mask;
    }
}

static int state_grow(tdx_state *state, tdx_error *err) {
    tdx_state_entry *old_entries = state->entries;
    const size_t old_buckets = state->bucket_count;
    const size_t new_buckets = old_buckets * 2;
    size_t index;

    state->entries = (tdx_state_entry *)calloc(new_buckets, sizeof(*state->entries));
    if (!state->entries) {
        state->entries = old_entries;
        tdx_error_set(err, "out of memory growing the state table to %zu buckets",
                      new_buckets);
        return TDX_ERR;
    }
    state->bucket_count = new_buckets;
    for (index = 0; index < old_buckets; ++index) {
        if (!old_entries[index].used)
            continue;
        {
            int found;
            tdx_state_entry *slot =
                find_slot(state, &old_entries[index].security, &found);
            *slot = old_entries[index];
        }
    }
    free(old_entries);
    return TDX_OK;
}

int tdx_state_apply(tdx_state *state, const tdx_depth *depth, tdx_diff_mask *mask,
                    const tdx_state_entry **entry, tdx_error *err) {
    int found = 0;
    tdx_state_entry *slot;

    if (mask)
        *mask = 0;
    if (entry)
        *entry = NULL;
    if (!state || !state->entries || !depth) {
        tdx_error_set(err, "state table is not initialised");
        return TDX_ERR;
    }
    if (state->count * 10 >= state->bucket_count * 5) {
        if (state_grow(state, err) != TDX_OK)
            return TDX_ERR;
    }
    slot = find_slot(state, &depth->security, &found);
    if (!found) {
        memset(slot, 0, sizeof(*slot));
        slot->security = depth->security;
        slot->depth = *depth;
        slot->used = 1;
        slot->observations = 1;
        slot->updates = 1;
        state->count++;
        if (mask)
            *mask = TDX_DIFF_NEW;
    } else {
        const tdx_diff_mask delta = tdx_depth_diff(&slot->depth, depth);
        if (delta != 0)
            slot->updates++;
        slot->observations++;
        slot->depth = *depth;
        if (mask)
            *mask = delta;
    }
    if (entry)
        *entry = slot;
    return TDX_OK;
}

const tdx_state_entry *tdx_state_find(const tdx_state *state, const tdx_code *code) {
    size_t index;
    size_t mask;
    if (!state || !state->entries || !code)
        return NULL;
    mask = state->bucket_count - 1;
    index = (size_t)(hash_key(code->market_id, code->code) & mask);
    for (;;) {
        const tdx_state_entry *entry = &state->entries[index];
        if (!entry->used)
            return NULL;
        if (key_equal(&entry->security, code))
            return entry;
        index = (index + 1) & mask;
    }
}
