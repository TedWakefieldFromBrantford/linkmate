/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

/* ============================================================================
 * strset.c - Hash set of strings, used for URL deduplication.
 *
 * Open addressing with linear probing.  See strset.h for the API.
 *
 * Design notes:
 *   - One flat array of char* slots.  No per-bucket allocation, no
 *     linked lists.  Cache-friendly, one malloc for the whole table.
 *   - FNV-1a hash.  Small, fast, well-distributed for short strings.
 *     Not cryptographic and doesn't need to be.
 *   - Power-of-two table size so the modulo is a mask.  Grow when the
 *     load factor exceeds 3/4.  Doubling keeps the mask arithmetic
 *     simple.
 *   - Empty slots are NULL.  We never delete, so there's no need for
 *     a tombstone sentinel.
 *   - Every string is strdup'd on insert.  The set owns its strings;
 *     callers don't have to keep the input buffer alive.
 * ============================================================================*/


 #include "../include/strset.h"
 #include <stdint.h>
 #include <stdlib.h>
 #include <string.h>

 /* ============================================================================
 * FNV-1a
 * ============================================================================*/

/* FNV-1a 64-bit hash.  See http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * The magic numbers are the FNV offset basis and prime.  They're
 * "arbitrary" in the sense that they're constants chosen for good
 * distribution over typical text; don't change them.
 */
static uint64_t fnv1a(const char *s) {
    uint64_t hash = 0xcbf29ce484222325ULL;   /* offset basis */
    while (*s) {
        hash ^= (uint64_t)(unsigned char)*s++;
        hash *= 0x100000001b3ULL;            /* FNV prime */
    }
    return hash;
}

/* ============================================================================
 * Internal structure
 * ============================================================================*/

struct StrSet {
    char   **slots;      /* array of string pointers, or NULL for empty */
    size_t   capacity;   /* number of slots; always a power of two */
    size_t   count;      /* number of entries currently in use */
};

#define INITIAL_CAPACITY 64
#define MAX_LOAD_NUM     3   /* grow when count/capacity > 3/4 */
#define MAX_LOAD_DEN     4

/* ============================================================================
 * Creation and destruction
 * ============================================================================*/

StrSet *strset_create(void) {
    StrSet *set = malloc(sizeof *set);
    if (!set) return NULL;

    set->slots = calloc(INITIAL_CAPACITY, sizeof *set->slots);
    if (!set->slots) {
        free(set);
        return NULL;
    }
    set->capacity = INITIAL_CAPACITY;
    set->count    = 0;
    return set;
}

void strset_free(StrSet *set) {
    if (!set) return;

    for (size_t i = 0; i < set->capacity; i++) {
        free(set->slots[i]);
    }
    free(set->slots);
    free(set);
}

size_t strset_size(const StrSet *set) {
    return set ? set->count : 0;
}

/* ============================================================================
 * Growth
 * ============================================================================*/

/* Reallocate the table at a larger capacity and re-insert all existing
 * entries.  Returns 1 on success, 0 on allocation failure (in which case
 * the original table is unchanged). */
static int grow(StrSet *set) {
    size_t new_cap = set->capacity * 2;
    char **new_slots = calloc(new_cap, sizeof *new_slots);
    if (!new_slots) return 0;

    /* Re-insert every non-empty slot into the new table.  We don't
     * re-hash from the strings themselves; we reuse the string pointer
     * and recompute the slot. */
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < set->capacity; i++) {
        char *s = set->slots[i];
        if (!s) continue;

        size_t slot = (size_t)fnv1a(s) & mask;
        while (new_slots[slot]) {
            slot = (slot + 1) & mask;
        }
        new_slots[slot] = s;
    }

    free(set->slots);
    set->slots    = new_slots;
    set->capacity = new_cap;
    return 1;
}

/* ============================================================================
 * Add
 * ============================================================================*/

int strset_add(StrSet *set, const char *s) {
    if (!set || !s) return 0;

    /* Grow if we're at or above the load factor. */
    if ((set->count + 1) * MAX_LOAD_DEN > set->capacity * MAX_LOAD_NUM) {
        if (!grow(set)) return 0;
    }

    size_t mask = set->capacity - 1;
    size_t slot = (size_t)fnv1a(s) & mask;

    for (;;) {
        char *existing = set->slots[slot];

        if (!existing) {
            /* Found an empty slot.  Copy and insert. */
            char *copy = strdup(s);
            if (!copy) return 0;
            set->slots[slot] = copy;
            set->count++;
            return 1;
        }

        if (strcmp(existing, s) == 0) {
            /* Already present. */
            return 0;
        }

        slot = (slot + 1) & mask;
    }
}
