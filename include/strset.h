#ifndef LINKMATE_STRSET_H
#define LINKMATE_STRSET_H

#include <stddef.h>

/* ============================================================================
 * strset - A hash set of strings.
 *
 * Backed by an open-addressing hash table with linear probing.  Each
 * insertion copies the string (with strdup); the set owns the copy and
 * frees it when the set is freed.  The caller's string does not need to
 * outlive the insertion.
 *
 * Duplicate insertions are idempotent: the set either contains a string
 * or it doesn't.  This is the property we need for URL deduplication --
 * "add if not present" is one operation.
 *
 * Typical usage:
 *
 *     StrSet *seen = strset_create();
 *     if (strset_add(seen, "https://example.com")) {
 *         // first time seen; process it
 *     }
 *     strset_free(seen);
 * ========================================================================= */

 typedef struct StrSet StrSet;

 /* Create an empty set.  Returns NULL on allocation failure.  */
 StrSet *strset_create(void);

 /* Free the set all the strings it owns  */
 void strset_free(StrSet *set);

 /* Add a string to the set.  Returns 1 if new, 0 if not.
 *  On allocation failure, callers can treat as 'Don't process this'. */
 int strset_add(StrSet *set, const char *s);

 /* Return the number of distinct strings in the set.  */
 size_t strset_size(const StrSet *set);

 #endif

