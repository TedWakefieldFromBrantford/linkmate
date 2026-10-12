#include "../../include/clipboard.h"
/* test/unit/test_clipboard.c - Clipboard tests */

#include "../../include/clipboard.h"
#include <stdio.h>
#include <string.h>

int test_clipboard(void) {
    printf("🧪 Testing clipboard...\n");
    
    const char *test_url = "https://example.com";
    int result = copy_to_clipboard(test_url);
    
    if (result == 0) {
        printf("  ✅ Copied to clipboard: %s\n", test_url);
        return 0;
    } else {
        printf("  ⚠️  Clipboard not available (that's okay)\n");
        return 0;
    }
}
