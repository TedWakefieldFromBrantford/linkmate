/* test/unit/runner.c - Test harness (active tests only) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations - ONLY active tests */
int test_raw_mode(void);
int test_strset(void);
int test_url_parser(void);
int test_terminal_functions(void);

/* Test runner helper */
static int run_test(const char *name, int (*test_func)(void)) {
    printf("📝 Running test: %s...\n", name);
    fflush(stdout);
    
    int result = test_func();
    
    if (result == 0) {
        printf("  ✅ %s passed\n\n", name);
        return 1;
    } else {
        printf("  ❌ %s failed\n\n", name);
        return 0;
    }
}

/* Run all tests */
static int run_all_tests(void) {
    int passed = 0;
    int total = 0;
    
    printf("\n🧪 LinkMate Test Suite\n");
    printf("═══════════════════════════════════════\n\n");
    
    passed += run_test("Raw Mode", test_raw_mode);
    total++;
    
    passed += run_test("URL Parser", test_url_parser);
    total++;
    
    passed += run_test("Terminal Functions", test_terminal_functions);
    total++;
    
    /* Placeholder tests - commented out until implemented */
    /* 
    passed += run_test("Browser", test_browser);
    total++;
    passed += run_test("Clipboard", test_clipboard);
    total++;
    passed += run_test("History", test_history);
    total++;
    passed += run_test("Config", test_config);
    total++;
    */
    
    printf("─────────────────────────────────────\n");
    printf("✅ Passed: %d\n", passed);
    printf("❌ Failed: %d\n", total - passed);
    printf("📊 Total: %d\n", total);
    printf("\n");
    
    return (passed == total) ? 0 : 1;
}

/* Print help */
static void print_help(void) {
    printf("LinkMate Test Suite\n\n");
    printf("Usage: %s [OPTIONS] [TEST]\n\n", "runner");
    printf("Options:\n");
    printf("  -h, --help     Show this help\n");
    printf("  -l, --list     List available tests\n");
    printf("\n");
    printf("Tests (active):\n");
    printf("  raw-mode       Test terminal raw mode\n");
    printf("  parser         Test URL parser\n");
    printf("  terminal       Test terminal functions\n");
    printf("  all            Run all tests (default)\n");
    printf("\n");
    printf("Tests (coming soon):\n");
    printf("  browser        Browser launcher (not implemented)\n");
    printf("  clipboard      Clipboard (not implemented)\n");
    printf("  history        History (not implemented)\n");
    printf("  config         Config (not implemented)\n");
    printf("\n");
    printf("Example: %s raw-mode\n", "runner");
}

/* List available tests */
static void list_tests(void) {
    printf("Active tests:\n");
    printf("  raw-mode\n");
    printf("  parser\n");
    printf("  terminal\n");
    printf("  all\n");
    printf("\n");
    printf("Coming soon:\n");
    printf("  browser\n");
    printf("  clipboard\n");
    printf("  history\n");
    printf("  config\n");
}

/* Main test runner */
int main(int argc, char **argv) {
    if (argc == 1) {
        return run_all_tests();
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_tests();
            return 0;
        }
        if (strcmp(argv[i], "strset") == 0) {
            return test_strset();
        }
        if (strcmp(argv[i], "raw-mode") == 0) {
            return test_raw_mode();
        }
        if (strcmp(argv[i], "parser") == 0) {
            return test_url_parser();
        }
        if (strcmp(argv[i], "terminal") == 0) {
            return test_terminal_functions();
        }
        if (strcmp(argv[i], "all") == 0) {
            return run_all_tests();
        }
        
        fprintf(stderr, "Unknown test: %s\n", argv[i]);
        print_help();
        return 1;
    }
    
    return run_all_tests();
}
