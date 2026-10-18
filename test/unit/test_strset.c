/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

/* test/unit/test_strset.c - StrSet (hash set of strings) tests */

#include "../../include/strset.h"
#include <stdio.h>
#include <string.h>

int test_strset(void) {
    printf("Testing StrSet...\n");

    int failed = 0;

    /* --- Test 1: basic add and duplicate --- */
    {
        StrSet *s = strset_create();
        if (!s) { printf("  FAIL: strset_create returned NULL\n"); return 1; }

        int r1 = strset_add(s, "alpha");
        int r2 = strset_add(s, "beta");
        int r3 = strset_add(s, "alpha");   /* duplicate */

        if (r1 != 1) { printf("  FAIL: first add of alpha returned %d\n", r1); failed++; }
        if (r2 != 1) { printf("  FAIL: add of beta returned %d\n", r2); failed++; }
        if (r3 != 0) { printf("  FAIL: duplicate add of alpha returned %d\n", r3); failed++; }
        if (strset_size(s) != 2) { printf("  FAIL: size should be 2, got %zu\n", strset_size(s)); failed++; }

        strset_free(s);
        if (!failed) printf("  PASS: basic add and duplicate\n");
    }

    /* --- Test 2: many inserts, forcing growth --- */
    {
        StrSet *s = strset_create();
        if (!s) return 1;

        char buf[32];
        int  all_new = 1;
        for (int i = 0; i < 1000; i++) {
            snprintf(buf, sizeof buf, "url-%d", i);
            if (strset_add(s, buf) != 1) {
                printf("  FAIL: unique insert %d returned 0\n", i);
                all_new = 0;
                failed++;
                break;
            }
        }
        if (all_new && strset_size(s) != 1000) {
            printf("  FAIL: size should be 1000, got %zu\n", strset_size(s));
            failed++;
        }

        /* Re-add all; nothing should be new. */
        int all_old = 1;
        for (int i = 0; i < 1000; i++) {
            snprintf(buf, sizeof buf, "url-%d", i);
            if (strset_add(s, buf) != 0) {
                printf("  FAIL: duplicate insert %d returned 1\n", i);
                all_old = 0;
                failed++;
                break;
            }
        }
        if (all_old && strset_size(s) != 1000) {
            printf("  FAIL: size changed on re-add, got %zu\n", strset_size(s));
            failed++;
        }

        strset_free(s);
        if (all_new && all_old) printf("  PASS: 1000 inserts with growth\n");
    }

    /* --- Test 3: NULL and empty string --- */
    {
        StrSet *s = strset_create();
        if (!s) return 1;

        if (strset_add(s, NULL) != 0)  { printf("  FAIL: NULL should not add\n"); failed++; }
        if (strset_add(s, "") != 1)    { printf("  FAIL: empty string should add\n"); failed++; }
        if (strset_add(s, "") != 0)    { printf("  FAIL: duplicate empty should not add\n"); failed++; }
        if (strset_size(s) != 1)       { printf("  FAIL: size should be 1, got %zu\n", strset_size(s)); failed++; }

        strset_free(s);
        if (!failed) printf("  PASS: NULL and empty-string handling\n");
    }

    printf("\n  StrSet: %s\n", failed ? "FAILED" : "all tests passed");
    return failed > 0 ? 1 : 0;
}
