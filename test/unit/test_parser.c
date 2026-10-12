/* test/unit/test_parser.c - URL parser tests with detailed output */

#include "../../include/url_parser.h"
#include <stdio.h>
#include <string.h>

int test_url_parser(void) {
    printf("🧪 Testing URL parser...\n");
    
    typedef struct {
        const char *input;
        int expected_count;
        const char *expected_first_url;
    } TestCase;
    
    TestCase tests[] = {
        {"https://example.com", 1, "https://example.com"},
        {"http://test.org", 1, "http://test.org"},
        {"www.google.com", 1, "http://www.google.com"},
        {"https://example.com/path?foo=bar", 1, "https://example.com/path?foo=bar"},
        {"https://example.com#anchor", 1, "https://example.com#anchor"},
        {"https://", 0, NULL},
        {"www.", 0, NULL},
        {"Check https://example.com and http://test.org", 2, "https://example.com"},
        {NULL, 0, NULL}
    };
    
    int passed = 0;
    int failed = 0;
    
    for (int i = 0; tests[i].input != NULL; i++) {
        LinkList list = {0};
        int count = get_urls_from_text(tests[i].input, &list);
        
        int test_passed = 1;
        char error_msg[256] = {0};
        
        /* Check count */
        if (count != tests[i].expected_count) {
            test_passed = 0;
            snprintf(error_msg, sizeof(error_msg), 
                     "Expected %d URLs, got %d", 
                     tests[i].expected_count, count);
        }
        
        /* Check first URL if expected */
        if (test_passed && tests[i].expected_first_url != NULL && count > 0) {
            if (strcmp(list.links[0].url, tests[i].expected_first_url) != 0) {
                test_passed = 0;
                snprintf(error_msg, sizeof(error_msg), 
                         "Expected first URL '%s', got '%s'", 
                         tests[i].expected_first_url, list.links[0].url);
            }
        }
        
        if (test_passed) {
            printf("  ✅ '%s' → %d URL(s)\n", tests[i].input, count);
            passed++;
        } else {
            printf("  ❌ '%s' → %s\n", tests[i].input, error_msg);
            if (count > 0) {
                printf("      Found: ");
                for (int j = 0; j < count && j < 5; j++) {
                    printf("\"%s\" ", list.links[j].url);
                }
                if (count > 5) printf("... (%d total)", count);
                printf("\n");
            }
            failed++;
        }
        
        free_url_list(&list);
    }
    
    printf("\n  📊 Parser tests: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
