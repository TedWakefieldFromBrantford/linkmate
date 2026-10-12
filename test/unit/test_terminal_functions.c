/* test/unit/test_terminal_functions.c - Terminal function tests */

#include "../../include/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int test_terminal_functions(void) {
    printf("🧪 Testing terminal functions...\n");
    
    int rows, cols;
    get_terminal_size(&rows, &cols);
    printf("  Terminal size: %dx%d\n", rows, cols);
    
    /* Test clearing and cursor movement */
    clear_screen();
    move_cursor(5, 10);
    printf("Cursor moved to (5,10)");
    fflush(stdout);
    
    usleep(500000); /* Wait half a second */
    
    clear_screen();
    printf("  ✅ Terminal functions test passed\n");
    return 0;
}
