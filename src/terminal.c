/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

/* ============================================================================
 * terminal.c - Raw-mode terminal control.
 *
 * See terminal.h for the rationale behind the fd-based API.
 *
 * Design notes:
 *   - We remember which fd we entered raw mode on so that disable_raw_mode()
 *     restores the *same* terminal, not necessarily stdin.
 *   - disable_raw_mode() is idempotent: calling it twice, or calling it
 *     without enable_raw_mode() having succeeded, is a no-op.  Callers can
 *     therefore put it in cleanup paths without tracking state.
 *   - read_key() normalizes CR to KEY_ENTER because ICRNL is disabled in
 *     raw mode; without this, Enter arrives as byte 13 and callers must
 *     special-case it.  Normalizing at the boundary is the right layer.
 * ============================================================================*/

#include "../include/terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

/* Raw-mode state.  Set by enable_raw_mode(), cleared by disable_raw_mode(). */
static struct termios original_termios;
static int            raw_mode_fd      = -1;

/* ---------------------------------------------------------------------------
 * enable_raw_mode() - Put fd into raw mode.
 *
 * fd must be a TTY.  Returns 0 on success, -1 on error.
 * ---------------------------------------------------------------------------*/
int enable_raw_mode(int fd) {
    struct termios raw;

    if (tcgetattr(fd, &original_termios) == -1) return -1;

    raw = original_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | INLCR | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &raw) == -1) return -1;

    raw_mode_fd = fd;
    return 0;
}

/* ---------------------------------------------------------------------------
 * disable_raw_mode() - Restore the terminal state captured at enable time.
 *
 * Safe to call at any time.  No-op if raw mode was never enabled.
 * ---------------------------------------------------------------------------*/
void disable_raw_mode(void) {
    if (raw_mode_fd < 0) return;
    tcsetattr(raw_mode_fd, TCSAFLUSH, &original_termios);
    raw_mode_fd = -1;
}

/* ---------------------------------------------------------------------------
 * read_key() - Block until one key is available on fd, return its code.
 *
 * Returns 0 on read error or EOF (a caller can treat this as "nothing
 * happened"); callers that need to distinguish should read errno.
 * ---------------------------------------------------------------------------*/
int read_key(int fd) {
    char    buffer[4];
    ssize_t n;

    n = read(fd, buffer, sizeof buffer);
    if (n <= 0) return 0;

    /* Single byte: the common case.  Normalize CR -> KEY_ENTER here so
     * callers never have to know about ICRNL. */
    if (n == 1) {
        if (buffer[0] == '\r') return KEY_ENTER;
        return (unsigned char)buffer[0];
    }

    /* Escape sequence: ESC [ X */
    if (n == 3 && buffer[0] == '\033' && buffer[1] == '[') {
        switch (buffer[2]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case '2': return KEY_INSERT;
            case '3': return KEY_DELETE;
            case '5': return KEY_PAGE_UP;
            case '6': return KEY_PAGE_DOWN;
        }
    }

    /* ESC [ 1 ~  (Home on some terminals) */
    if (n == 4 && buffer[0] == '\033' && buffer[1] == '[' &&
        buffer[2] == '1' && buffer[3] == '~') {
        return KEY_HOME;
    }
    /* ESC [ 4 ~  (End on some terminals) */
    if (n == 4 && buffer[0] == '\033' && buffer[1] == '[' &&
        buffer[2] == '4' && buffer[3] == '~') {
        return KEY_END;
    }

    return 0;
}

/*---------------------------------------------------------------------------
* Alternate screen buffer.
*
----------------------------------------------------------------------------*/

void enter_alternate_screen(void) {
     /* Enter the alternate screen, reset any inherited scroll region,
     * and home the cursor.  The scroll-region reset is essential:
     * some terminals retain a scroll region across the alternate-screen
     * switch, in which case \033[H would land at the top of the stale
     * region rather than the top of the screen. */
    printf("\033>"); 
    printf("\033[?1049h");
    printf("\033[H");
    fflush(stdout);
}
void leave_alternate_screen(void) {
    printf("\033[?1049l");
    fflush(stdout);
}
void drain_input(int fd) {
    /* Discard any pending input on the terminal.  Otherwise a stale
     * byte from before the TUI started (shell-buffered newline, mouse
     * event, previous program's leftover) can be read as the user's
     * first keypress and prematurely exit the selector. */
    tcflush(fd, TCIFLUSH);
}

/*---------------------------------------------------------------------------
* In-place redraw primitives.
* 
-----------------------------------------------------------------------------*/

void cursor_home(void) {
    printf("\033[1;1H");
    fflush(stdout);
}

void clear_to_end(void) {
    printf("\033[J");
    fflush(stdout);
}


/* ---------------------------------------------------------------------------
 * Output helpers.
 * ---------------------------------------------------------------------------*/
void clear_screen(void) {
    printf("\033[2J");
    printf("\033[H");
    fflush(stdout);
}

void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

void get_terminal_size(int *rows, int *cols) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        *rows = w.ws_row;
        *cols = w.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

void set_cursor_style(int style) {
    printf("\033[%d q", style);
    fflush(stdout);
}

void hide_cursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

void show_cursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}
