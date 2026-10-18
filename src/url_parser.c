/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

#include "../include/url_parser.h"
#include "../include/strset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*___________________________________________________________________________
 * URL character whitelist - 256-bit bitmap (4 x 64-bit words)
 *
 *   b7 b6 b5 b4 b3 b2 b1 b0
 *   |__|   |_____________|
 *    row        column
 *
 * Row = high 2 bits (0..3), column = low 6 bits (0..63).
 * Only 128 ASCII values matter, but we index the full 0..255 range
 * safely (non-ASCII will land in rows 2..3 which stay zero).
 *
 * Per RFC 3986 we allow: unreserved (ALNUM - _ . ~), gen-delims
 * (:/?#[]@), and the sub-delims we actually see in the wild
 * (!$&'()*+,;=). We drop the single-quote because it causes more
 * false positives in prose than it's worth.
 *___________________________________________________________________________*/

#define URL_CHAR_MAP_WORDS 4
static uint64_t url_char_map[URL_CHAR_MAP_WORDS];
static int      url_char_map_ready = 0;

static void url_char_map_init(void) {
    static const char allowed[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-._~"              /* unreserved */
        ":/?#[]@"           /* gen-delims */
        "!$&()*+,;=%";      /* sub-delims (no single-quote) + pct */

    memset(url_char_map, 0, sizeof(url_char_map));

    for (const char *cursor = allowed; *cursor; cursor++) {
        unsigned char c  = (unsigned char)*cursor;
        unsigned int  row = (unsigned int)(c >> 6);
        unsigned int  col = (unsigned int)(c & 63);
        url_char_map[row] |= (1ULL << col);
    }
    url_char_map_ready = 1;
}

static inline int is_url_char(unsigned char candidate) {
    unsigned int row = (unsigned int)(candidate >> 6);
    unsigned int col = (unsigned int)(candidate & 63);
    return (int)((url_char_map[row] >> col) & 1ULL);
}

/* Return 1 if s starts with prefix, 0 otherwise. */
static inline int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

int get_urls_from_text(const char *source_text, LinkList *url_container) {
    static const char *search_terms[] = { "https://", "http://", "ftp://", "www." };
    const int total_search_terms = (int)(sizeof(search_terms) / sizeof(search_terms[0]));
    const int is_www_index = 3;

    if (!source_text || !url_container) return 0;
    if (!url_char_map_ready) url_char_map_init();

    url_container->count    = 0;
    url_container->capacity = STARTING_CAPACITY;
    url_container->links    = malloc(sizeof(Link) * url_container->capacity);
    if (!url_container->links) {
        url_container->capacity = 0;
        return 0;
    }

    /* Dedup set.  If allocation fails we fall back to no deduplication
     * rather than failing the whole parse -- duplicates are cosmetic,
     * not a correctness issue for the caller. */
    StrSet *seen = strset_create();

    int urls_found = 0;

    for (int term_index = 0; term_index < total_search_terms; term_index++) {
        const char *scan_spot = source_text;
        const char *hit;
        const size_t term_len = strlen(search_terms[term_index]);
        const int    is_www   = (term_index == is_www_index);

        while ((hit = strstr(scan_spot, search_terms[term_index])) != NULL) {
            const char *url_start = hit;
            const char *url_end   = hit + term_len;

            /* Walk forward while chars are URL-legal */
            while (*url_end && is_url_char((unsigned char)*url_end)) {
                url_end++;
            }

            size_t content_len = (size_t)(url_end - (hit + term_len));

            /* Nothing useful after prefix? skip. */
            if (content_len < 2) {
                scan_spot = url_end;
                continue;
            }

            /* For bare www., require a dot in the host part */
            if (is_www) {
                int has_dot = 0;
                for (const char *p = hit + 4; p < url_end; p++) {
                    if (*p == '.') { has_dot = 1; break; }
                }
                if (!has_dot) {
                    scan_spot = url_end;
                    continue;
                }
            }

            size_t url_len = (size_t)(url_end - url_start);

            /* Build the URL into a local buffer first, so we can check
             * the dedup set before committing a slot in the container. */
            char url_buf[MAX_URL_LENGTH];

            if (is_www) {
                int written = snprintf(url_buf, sizeof url_buf,
                                       "http://%.*s", (int)url_len, url_start);
                if (written < 0 || (size_t)written >= sizeof url_buf) {
                    scan_spot = url_end;
                    continue;
                }
            } else {
                size_t copy_len = url_len;
                if (copy_len >= MAX_URL_LENGTH) {
                    copy_len = MAX_URL_LENGTH - 1;
                }
                memcpy(url_buf, url_start, copy_len);
                url_buf[copy_len] = STOP_BYTE;
            }

            /* Deduplication: skip if we've already seen this exact string.
             * If the set allocation failed, seen is NULL and we skip the
             * check (accepting duplicates rather than dropping URLs). */
            if (seen && !strset_add(seen, url_buf)) {
                scan_spot = url_end;
                continue;
            }

            /* Grow container if needed */
            if (url_container->count >= url_container->capacity) {
                size_t new_capacity = url_container->capacity * 2;
                Link *bigger = realloc(url_container->links,
                                       sizeof(Link) * new_capacity);
                if (!bigger) break;
                url_container->links    = bigger;
                url_container->capacity = new_capacity;
            }

            Link *current_url = &url_container->links[url_container->count];

            memcpy(current_url->url, url_buf, strlen(url_buf) + 1);
            current_url->line_number  = 0;
            current_url->start_column = (int)(url_start - source_text);
            current_url->end_column   = (int)(url_end   - source_text);

            url_container->count++;
            urls_found++;

            scan_spot = url_end;
        }
    }

    strset_free(seen);
    return urls_found;
}
void free_url_list(LinkList *url_container) {
    if (url_container && url_container->links) {
        free(url_container->links);
        url_container->links    = NULL;
        url_container->count    = 0;
        url_container->capacity = 0;
    }
}
