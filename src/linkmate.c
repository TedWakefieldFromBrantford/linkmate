/* ============================================================================
 * linkmate.c - LinkMate entry point.
 *
 * Modes of operation:
 *   1. Piped stdin + TTY stdout  -> interactive TUI selector
 *   2. Piped stdin + non-TTY     -> plain text list (script-friendly)
 *   3. No stdin, no args         -> usage hint
 *   4. No stdin, file arg        -> (not yet implemented)
 *
 * Design notes:
 *   - Input is read entirely into memory before parsing.  URLs can span
 *     buffer boundaries and we want one clean scan, not a streaming mess.
 *   - Interactive vs. batch is decided by isatty(STDOUT_FILENO), not argv.
 *     The pipe tells us what the user wants more reliably than a flag.
 *   - When stdin is a pipe (the common case: `cat x | linkmate`), keystrokes
 *     are read from the controlling terminal via /dev/tty.  This is the same
 *     pattern less(1), fzf(1), and vim(1) use.
 *   - Terminal state is always restored before exit, even on quit paths
 *     that don't go through the happy path.  See run_selector().
 * ============================================================================*/

#include "../include/browser.h"
#include "../include/terminal.h"
#include "../include/url_parser.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Read all of stdin into a heap buffer.
 *
 * Returns a NUL-terminated string on success, NULL if stdin was empty or
 * allocation failed.  Caller owns the returned buffer.
 * ---------------------------------------------------------------------------*/
static char *read_stdin(void) {
    size_t capacity = 4096;
    size_t used     = 0;
    char  *buffer   = malloc(capacity);
    if (!buffer) return NULL;

    int character;
    while ((character = getchar()) != EOF) {
        if (used + 1 >= capacity) {
            size_t new_capacity = capacity * 2;
            char  *grown        = realloc(buffer, new_capacity);
            if (!grown) {
                free(buffer);
                return NULL;
            }
            buffer   = grown;
            capacity = new_capacity;
        }
        buffer[used++] = (char)character;
    }

    if (used == 0) {
        free(buffer);
        return NULL;
    }

    buffer[used] = '\0';
    return buffer;
}

/* ---------------------------------------------------------------------------
 * Non-interactive output.  One URL per line, numbered, no chrome.
 * This is the path that scripts and pipes see.
 * ---------------------------------------------------------------------------*/
static void print_url_list(const LinkList *urls) {
    if (urls->count == 0) {
        printf("😴 No URLs found in input.\n");
        return;
    }

    printf("\n✅ Found %zu URL(s):\n", urls->count);
    printf("─────────────────────────────────────\n");
    for (size_t i = 0; i < urls->count; i++) {
        printf("  %zu. %s\n", i + 1, urls->links[i].url);
    }
}

/* ---------------------------------------------------------------------------
 * Open a file descriptor suitable for reading keystrokes.
 *
 * If stdin is already a TTY, use it.  Otherwise open /dev/tty so that the
 * TUI still works when stdin is busy with piped data.  Caller is
 * responsible for closing the returned fd if it is not STDIN_FILENO.
 *
 * Returns -1 on failure.  On failure, *was_opened is untouched.
 * ---------------------------------------------------------------------------*/
static int open_keyboard_input(int *was_opened) {
    if (isatty(STDIN_FILENO)) {
        *was_opened = 0;
        return STDIN_FILENO;
    }

    int fd = open("/dev/tty", O_RDONLY);
    if (fd < 0) return -1;

    *was_opened = 1;
    return fd;
}

/* ---------------------------------------------------------------------------
 * Interactive TUI selector.
 *
 * Full redraw each iteration: simple and correct.  A smarter version would
 * repaint only the two changed rows; not worth the complexity until we
 * actually notice flicker on slow terminals.
 *
 * Returns 1 if the user opened a URL, 0 if they quit.
 * ---------------------------------------------------------------------------*/
static int run_selector(LinkList *urls) {
    if (!urls || urls->count == 0) return 0;

    int was_opened = 0;
    int input_fd   = open_keyboard_input(&was_opened);
    if (input_fd < 0) {
        fprintf(stderr, "❌ No terminal available for interactive mode\n");
        return 0;
    }

    if (enable_raw_mode(input_fd) == -1) {
        fprintf(stderr, "❌ Failed to enter raw mode: %s\n", strerror(errno));
        if (was_opened) close(input_fd);
        return 0;
    }
    hide_cursor();

    int selected = 0;
    int opened   = 0;
    int running  = 1;

    while (running) {
        clear_screen();
        move_cursor(1, 1);

        printf("🧑‍🤝‍🧑 LinkMate — %zu URL(s) found\r\n", urls->count);
        printf("═══════════════════════════════════════════════════\r\n");

        for (size_t i = 0; i < urls->count; i++) {
            if ((int)i == selected) {
                printf("\033[7m ▶ %s\033[0m\r\n", urls->links[i].url);
            } else {
                printf("   %s\r\n", urls->links[i].url);
            }
        }

        printf("\r\n");
        printf("[↑/↓] move   [Enter] open   [q] quit\r\n");
        fflush(stdout);

        int key = read_key(input_fd);
        {
             FILE *f = fopen("/tmp/lm.log", "a");
             if (f) { fprintf(f, "key=%d KEY_ENTER=%d match=%d\n",
                         key, KEY_ENTER, key == KEY_ENTER);
                       fclose(f); }
        }

        switch (key) {
            case KEY_UP:
                if (selected > 0) selected--;
                break;

            case KEY_DOWN:
                if (selected + 1 < (int)urls->count) selected++;
                break;

            case KEY_ENTER:
                /* Leave raw mode BEFORE opening so the browser is not
                 * competing with us for the TTY. */
                disable_raw_mode();
                show_cursor();
                clear_screen();
                printf("🌐 Opening: %s\n", urls->links[selected].url);
                open_url(urls->links[selected].url);
                opened  = 1;
                running = 0;
                break;

            case KEY_Q:
            case KEY_ESC:
                running = 0;
                break;

            default:
                /* Unknown key: ignore.  No beep, no fuss. */
                break;
        }
    }

    /* Always restore the terminal, no matter how the loop exited.
     * disable_raw_mode() is idempotent, so calling it here even if
     * the Enter branch already called it is fine. */
    disable_raw_mode();
    show_cursor();
    clear_screen();

    if (was_opened) close(input_fd);
    return opened;
}

/* ---------------------------------------------------------------------------
 * Acquire input text.  Handles the three "where do bytes come from" cases.
 *
 * Returns:
 *   On success: heap buffer, caller frees.
 *   On usage-error (nothing to do): NULL, message already printed.
 * ---------------------------------------------------------------------------*/
static char *acquire_input(int argc, char **argv) {
    if (!isatty(STDIN_FILENO)) {
        return read_stdin();
    }

    if (argc > 1) {
        fprintf(stderr, "📁 File input not implemented yet: %s\n", argv[1]);
        return NULL;
    }

    fprintf(stderr,
            "🤯 No input! Pipe text to me:\n"
            "   echo 'Check https://example.com' | ./linkmate\n");
    return NULL;
}

/* --------------------------------------------------------------------------- */
int main(int argc, char **argv) {
    char *input_text = acquire_input(argc, argv);
    if (!input_text) {
        return 1;
    }

    LinkList found_urls = {0};
    int      url_count  = get_urls_from_text(input_text, &found_urls);

    if (url_count == 0) {
        print_url_list(&found_urls);
    } else if (isatty(STDOUT_FILENO)) {
        run_selector(&found_urls);
    } else {
        print_url_list(&found_urls);
    }

    free(input_text);
    free_url_list(&found_urls);
    return 0;
}
