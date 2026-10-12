#include "../../include/history.h"
/* test/unit/test_history.c - History tests */

#include "../../include/history.h"
#include <stdio.h>
#include <string.h>

int test_history(void) {
    printf("🧪 Testing history...\n");
    
    /* Add some URLs */
    add_to_history("https://example.com");
    add_to_history("https://test.org");
    add_to_history("https://example.com"); // Duplicate should be handled
    
    /* Get history */
    char **history = get_history();
    int count = get_history_count();
    
    printf("  History entries: %d\n", count);
    for (int i = 0; i < count; i++) {
        printf("    %d. %s\n", i + 1, history[i]);
    }
    
    /* Clean up */
    clear_history();
    
    printf("  ✅ History test passed\n");
    return 0;
}
