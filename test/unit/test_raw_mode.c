/* test/unit/test_raw_mode.c - Terminal raw mode tests (linear display) */

#include "../../include/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int test_raw_mode(void) {
    int key;
    int running = 1;
    
    printf("\n\n");
    printf("🧪 Terminal Raw Mode Test\n");
    printf("─────────────────────────────────────\n");
    printf("Press keys to see their codes.\n");
    printf("  Arrow keys, letters, numbers\n");
    printf("  q or ESC to quit\n");
    printf("─────────────────────────────────────\n");
    printf("\n");
    
    if (enable_raw_mode(STDIN_FILENO) == -1) {
        printf("❌ Failed to enter Raw Mode\n");
        return 1;
    }
    
    while (running) {
        printf("\rKey:          ");  /* Clear the line */
        fflush(stdout);
        
        key = read_key(STDIN_FILENO);
        
        /* Move back and overwrite */
        printf("\rKey: %-6d  [ ", key);
        
        switch (key) {
            case KEY_UP:    printf("↑"); break;
            case KEY_DOWN:  printf("↓"); break;
            case KEY_LEFT:  printf("←"); break;
            case KEY_RIGHT: printf("→"); break;
            case KEY_ENTER: printf("↵"); break;
            case KEY_ESC:
                running = 0;
                printf("ESC");
                break;
            case KEY_Q:
                running = 0;
                printf("q");
                break;
            default:
                if (key >= 32 && key <= 126) {
                    printf("%c", key);
                } else {
                    printf("0x%02X", key);
                }
                break;
        }
        printf(" ]\n");
        fflush(stdout);
    }
    
    disable_raw_mode();
    
    printf("\n\n✅ Raw mode test complete\n");
    printf("─────────────────────────────────────\n");
    printf("\n");
    
    return 0;
}
