/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

/* ============================================================================
 * VERSION
 * ============================================================================*/

/* Bump on release.  MAJOR.MINOR.PATCH.  We are pre-1.0:
 * the interface can still change between minor versions. */
#define LINKMATE_VERSION "0.2.0"


/* ============================================================================
 * linkmate.c - LinkMate entry point.
 *
 * LinkMate reads text, extracts URLs from it, and offers an interactive
 * selector for launching one in the user's browser.
 *
 * Input sources (in precedence order):
 *   1. --pane   capture text from the current tmux pane
 *   2. --clip   read text from the system clipboard
 *   3. pipe     read from stdin (e.g. `cmd | linkmate`)
 *   4. file     read from a file argument (`linkmate README.md`)
 *
 * If no source applies, we print a short usage message and exit.  We do
 * not guess: the user tells us where the text is.
 *
 * Design notes:
 *   - Input is slurped fully into memory before parsing.  URLs can span
 *     buffer boundaries and one clean scan beats a streaming mess.
 *   - Interactive vs. batch output is decided by isatty(STDOUT_FILENO),
 *     not by argv.  A pipe on stdout says "I'm being consumed by
 *     something else; print a list."  A TTY says "show me the UI."
 *   - Keystrokes are read from /dev/tty when stdin is a pipe.  This is
 *     the same pattern less(1), fzf(1), and vim(1) use, and it is what
 *     makes `cmd | linkmate` interactive.  See open_keyboard_input().
 *   - Terminal state is always restored before exit, even on paths that
 *     don't go through the happy case.  See run_selector().
 *   - Subprocess-based input sources (tmux, clipboard) shell out to
 *     existing tools rather than linking libraries.  This keeps the
 *     build simple and the failure modes observable.
 * ============================================================================*/

#include "../include/browser.h"
#include "../include/terminal.h"
#include "../include/url_parser.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ============================================================================
 * INPUT SOURCES
 * ============================================================================*/

/* ---------------------------------------------------------------------------
 * read_stdin() - Slurp all of stdin into a heap buffer.
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
 * read_file() - Slurp an entire file into a heap buffer.
 *
 * Returns a NUL-terminated string on success, NULL if the file couldn't
 * be opened or was empty.  Caller owns the returned buffer.
 *
 * We use stdio (fopen/fread) rather than open/read because we're reading
 * a regular file, not a device.  Buffering helps and we want the
 * portability that stdio brings.
 * ---------------------------------------------------------------------------*/
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "❌ Cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }

    size_t capacity = 4096;
    size_t used     = 0;
    char  *buffer   = malloc(capacity);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t n;
    while ((n = fread(buffer + used, 1, capacity - used - 1, f)) > 0) {
        used += n;
        if (used + 1 >= capacity) {
            size_t new_capacity = capacity * 2;
            char  *grown        = realloc(buffer, new_capacity);
            if (!grown) {
                free(buffer);
                fclose(f);
                return NULL;
            }
            buffer   = grown;
            capacity = new_capacity;
        }
    }

    fclose(f);

    if (used == 0) {
        free(buffer);
        return NULL;
    }

    buffer[used] = '\0';
    return buffer;
}

/* ---------------------------------------------------------------------------
 * run_and_capture() - Run a subprocess and return its stdout as a string.
 *
 * argv must be NULL-terminated.  Returns a heap buffer on success, NULL
 * on any failure (fork, pipe, exec, or empty output).  Caller owns the
 * buffer.
 *
 * The child's stderr is left attached to ours, so the user sees any
 * error messages from tools like tmux or xclip that fail.
 * ---------------------------------------------------------------------------*/
static char *run_and_capture(char *const argv[]) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child: wire our stdout to the write end of the pipe. */
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) _exit(127);
        close(pipefd[1]);

        execvp(argv[0], argv);
        _exit(127);
    }

    /* Parent: read from the pipe until EOF. */
    close(pipefd[1]);

    size_t capacity = 4096;
    size_t used     = 0;
    char  *buffer   = malloc(capacity);
    if (!buffer) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    ssize_t n;
    while ((n = read(pipefd[0], buffer + used, capacity - used - 1)) > 0) {
        used += (size_t)n;
        if (used + 1 >= capacity) {
            size_t new_capacity = capacity * 2;
            char  *grown        = realloc(buffer, new_capacity);
            if (!grown) {
                free(buffer);
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                return NULL;
            }
            buffer   = grown;
            capacity = new_capacity;
        }
    }

    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    if (used == 0) {
        free(buffer);
        return NULL;
    }

    buffer[used] = '\0';
    return buffer;
}

/* ---------------------------------------------------------------------------
 * capture_tmux_pane() - Grab the visible text of the current tmux pane.
 *
 * Requires being inside a tmux session.  Returns NULL if tmux isn't
 * installed, we're not in a session, or the pane is empty.
 * ---------------------------------------------------------------------------*/
static char *capture_tmux_pane(void) {
    char *tmux_argv[] = { "tmux", "capture-pane", "-p", NULL };
    char *text = run_and_capture(tmux_argv);
    if (!text) {
        fprintf(stderr,
            "❌ --pane: failed to capture from tmux.\n"
            "   Are you inside a tmux session?\n");
    }
    return text;
}

/* ---------------------------------------------------------------------------
 * clipboard_argv() - Pick a clipboard-reading tool appropriate to this
 * session.
 *
 * Prefers Wayland's wl-paste when $WAYLAND_DISPLAY is set, else falls
 * back to X11 tools.  Returns NULL if none is available.  The returned
 * array is static and NULL-terminated.
 * ---------------------------------------------------------------------------*/
static char *const *clipboard_argv(void) {
    static char *wl_paste[] = { "wl-paste", "-n", NULL };
    static char *xclip[]    = { "xclip", "-o", "-selection", "clipboard", NULL };
    static char *xsel[]     = { "xsel", "-b", NULL };

    const char *wayland = getenv("WAYLAND_DISPLAY");
    if (wayland && *wayland) {
        if (access("/usr/bin/wl-paste", X_OK) == 0) return wl_paste;
    }
    if (access("/usr/bin/xclip", X_OK) == 0) return xclip;
    if (access("/usr/bin/xsel",  X_OK) == 0) return xsel;
    return NULL;
}

/* ---------------------------------------------------------------------------
 * capture_clipboard() - Read the system clipboard as text.
 *
 * Returns NULL if no clipboard tool is available or the clipboard is
 * empty.  Prints a specific diagnostic for each case.
 * ---------------------------------------------------------------------------*/
static char *capture_clipboard(void) {
    char *const *cargv = clipboard_argv();
    if (!cargv) {
        fprintf(stderr,
            "❌ --clip: no clipboard tool found.\n"
            "   Install xclip, xsel, or wl-clipboard.\n");
        return NULL;
    }

    char *text = run_and_capture((char *const *)cargv);
    if (!text) {
        fprintf(stderr,
            "❌ --clip: clipboard is empty or unreadable.\n"
            "   Did you copy something first? (Ctrl+Shift+C)\n");
    }
    return text;
}

/* ============================================================================
 * HELP TEXT
 * ============================================================================*/

/* ---------------------------------------------------------------------------
 * usage() - Print the canonical usage message.
 *
 * Written to `out` so it can go to stdout for --help and stderr for
 * errors.  The rule is: usage on error goes to stderr (so it doesn't
 * pollute pipelines); usage on purpose goes to stdout.
 * ---------------------------------------------------------------------------*/
static void usage(FILE *out) {
    fprintf(out,
        "Usage: linkmate [--pane|--clip] [FILE]\n"
        "\n"
        "Extract URLs from text and open one in a browser.\n"
        "\n"
        "Input sources (first match wins):\n"
        "  --pane             capture text from current tmux pane\n"
        "  --clip             read text from system clipboard\n"
        "  (piped stdin)      read from stdin, as in `cmd | linkmate`\n"
        "  <file>             read from a file\n"
        "\n"
        "Options:\n"
        "  -h, --help         display this help and exit\n"
        "  -V, --version      display version information and exit\n"
        "\n"    
        "Output:\n"
        "  If stdout is a terminal, opens an interactive selector.\n"
        "  Otherwise, prints a numbered URL list to stdout.\n"
        "Examples:\n"
        "  tailscale status | linkmate\n"
        "  linkmate --pane\n"
        "  linkmate --clip\n"
        "  linkmate README.md\n"
        "\n"
        "Environment:\n"
        "  BROWSER            program to launch for a URL\n"
        "                     (default: xdg-open, open, firefox, chromium)\n"
        "  LINKMATE_BROWSER_LOG  path to log browser stderr\n"
        "                     (default: /tmp/lm-browser.log)\n"
        "\n"
        "  Full documentation: man linkmate\n");
}

/* ============================================================================
 * OUTPUT
 * ============================================================================*/

/* ---------------------------------------------------------------------------
 * print_url_list() - Non-interactive output.  One URL per line, numbered.
 *
 * This is the path that scripts and pipes see.  No chrome, no escapes.
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

/* ============================================================================
 * INTERACTIVE SELECTOR
 * ============================================================================*/

/* ---------------------------------------------------------------------------
 * open_keyboard_input() - Get an fd suitable for reading keystrokes.
 *
 * If stdin is already a TTY, use it.  Otherwise open /dev/tty so the
 * TUI still works when stdin is busy with piped data.
 *
 * Sets *was_opened to 1 if we opened a new fd (caller must close it),
 * 0 if we're just returning STDIN_FILENO (caller must not close it).
 * Returns -1 on failure.
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
 * run_selector() - Interactive TUI.
 *
 * Full redraw each iteration: simple and correct.  A smarter version
 * would repaint only changed rows; not worth the complexity until we
 * notice flicker on slow terminals.
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

/* ============================================================================
 * ARGUMENT PARSING
 * ============================================================================
*------------------------------------------------------------------------------
* parse_args() - Handle flags that terminate the program.

* This function is deliberately separate from aquire_input() because the flags
* here have a different lifetime:  They are informational and program ends the
* moment one is seen.  They never touch inout, the TUI, or the browser.
*
* aquire_input owns the other class opf flags which identify the source and feed * the rest of the pipline.  MIxing the two in one loop would require scanning 
* argv twice anyway (once to check for early exit flags and once to find the
* source), so I have seperated the concerns for readability.
*
*  On --help or --version:  print and exit 0
*  On unknown flag, print diagnostic and exit 2
*  Otherwise, return normally.
*
------------------------------------------------------------------------------*/

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Informational flags:  Print and exit*/
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout);
            exit(0);
        }
        if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            printf("linkmate %s\n", LINKMATE_VERSION);
            exit(0);
        }

        /* --- Source flags:  known to parse_args, handled in aquire_input. */
        if (strcmp(arg, "--pane") == 0) continue;
        if (strcmp(arg, "--clip") == 0) continue;

        /*  Anything else starting with '-' is uinknown.*/
        if (arg[0] == '-') {
            fprintf(stderr, "linkmate: unknown option '%s'\n", arg);
            fprintf(stderr, "Try 'linkmate --help' for more information.\n");
            exit(2);
        }

        /*  Non flag arg:  it's a file path and not our business here*/
    }
}
/* ============================================================================
 * DISPATCH
 * ============================================================================*/
/* ---------------------------------------------------------------------------
 * acquire_input() - Get input text from wherever the user said it is.
 *
 * Precedence (more explicit wins):
 *   1. --pane or --clip flags (first one found in argv)
 *   2. Piped stdin (isatty false)
 *   3. Non-flag argument (treated as a file path)
 *   4. Nothing -> print usage, return NULL
 *
 * Returns a heap buffer, caller frees.  Returns NULL on failure or when
 * there was nothing to do; in either case a diagnostic has already been
 * printed to stderr.
 * ---------------------------------------------------------------------------*/
static char *acquire_input(int argc, char **argv) {
    /* --- 1. Flags win.  User said exactly what they want. --- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pane") == 0) return capture_tmux_pane();
        if (strcmp(argv[i], "--clip") == 0) return capture_clipboard();
    }

    /* --- 2. Piped stdin.  The standard Unix path. --- */
    if (!isatty(STDIN_FILENO)) {
        return read_stdin();
    }

    /* --- 3. File argument.  Scan argv for a non-flag string. --- */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            return read_file(argv[i]);
        }    
    }

    /* --- 4. Nothing to do. --- */
    usage(stderr);
    return NULL;
}

/* ============================================================================
 * MAIN
 * ============================================================================*/

int main(int argc, char **argv) {
    parse_args(argc, argv);
    char *input_text = acquire_input(argc, argv);
    if (!input_text) return 1;

    LinkList found_urls = {0};
    int      url_count  = get_urls_from_text(input_text, &found_urls);

    if (url_count == 0) {
        print_url_list(&found_urls);        /* prints the "no URLs" message */
    } else if (isatty(STDOUT_FILENO)) {
        run_selector(&found_urls);
    } else {
        print_url_list(&found_urls);
    }

    free(input_text);
    free_url_list(&found_urls);
    return 0;
}
