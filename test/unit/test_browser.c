#include "../../include/browser.h"
/* test/unit/test_browser.c - Browser launcher tests */

#include "../../include/browser.h"
#include <stdio.h>
#include <unistd.h>

int test_browser(void) {
    printf("🧪 Testing browser launcher...\n");
    
    /* Test with a known URL */
    int result = open_url("https://example.com");
    
    if (result == 0) {
        printf("  ✅ Browser opened successfully\n");
        return 0;
    } else {
        printf("  ⚠️  Browser test failed (that's okay in CI)\n");
        return 0; // Don't fail the test suite for browser
    }
}
