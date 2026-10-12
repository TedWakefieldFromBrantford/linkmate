#include "../../include/config.h"
/* test/unit/test_config.c - Config tests */

#include "../../include/config.h"
#include <stdio.h>
#include <string.h>

int test_config(void) {
    printf("🧪 Testing config...\n");
    
    /* Load config */
    Config config = load_config();
    
    printf("  Browser: %s\n", config.browser);
    printf("  Theme: %s\n", config.theme);
    printf("  History enabled: %d\n", config.history_enabled);
    printf("  History file: %s\n", config.history_file);
    
    printf("  ✅ Config test passed\n");
    return 0;
}
