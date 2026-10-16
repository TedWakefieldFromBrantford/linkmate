/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

/* ============================================================================
 * terminal.h - Raw-mode terminal control.
 *
 * All input-side functions take a file descriptor rather than using
 * STDIN_FILENO implicitly.  This matters because a TUI often needs to
 * read keystrokes from the controlling terminal (/dev/tty) while stdin
 * is busy with piped data (e.g. `tailscale status | linkmate`).
 *
 * Output-side functions always write to stdout; they are only meaningful
 * when stdout is a TTY.
 * ============================================================================*/

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>

/* Key codes - Basic */
#define KEY_UP        1000
#define KEY_DOWN      1001
#define KEY_LEFT      1002
#define KEY_RIGHT     1003
#define KEY_ENTER     10
#define KEY_ESC       27
#define KEY_SPACE     32
#define KEY_Q         113
#define KEY_H         104
#define KEY_CTRL_C    3

/* Key codes - Extended */
#define KEY_HOME      1004
#define KEY_END       1005
#define KEY_INSERT    1006
#define KEY_DELETE    1007
#define KEY_PAGE_UP   1008
#define KEY_PAGE_DOWN 1009

/* Raw-mode control.
 *
 * enable_raw_mode: fd must be a TTY.  Returns 0 on success, -1 on error.
 * disable_raw_mode: safe to call when raw mode was never enabled, or when
 *                   it was already disabled.  Idempotent by design so that
 *                   cleanup paths can call it unconditionally.
 */
int  enable_raw_mode(int fd);
void disable_raw_mode(void);

/* Read one key from fd.  Blocks until a key is available.
 * Translates CR (13) to KEY_ENTER so callers see a single newline value. */
int  read_key(int fd);

/* Output helpers - all write to stdout. */
void clear_screen(void);
void move_cursor(int row, int col);
void get_terminal_size(int *rows, int *cols);
void set_cursor_style(int style);
void hide_cursor(void);
void show_cursor(void);

#endif /* TERMINAL_H */
