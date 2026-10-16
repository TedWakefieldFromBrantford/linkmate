/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

/* ============================================================================
 * browser.c - Launch URLs in the user's preferred browser.
 *
 * Design notes:
 *   - Never uses system(3): the URL is untrusted input and would otherwise
 *     be subject to shell injection.
 *   - Uses fork(2) + execvp(3) so the browser detaches cleanly from our TTY
 *     and survives our exit.
 *   - Installs a one-time SIGCHLD handler so that launched browsers don't
 *     linger as zombies in our process table.
 * ============================================================================*/

#include "../include/browser.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * One-time SIGCHLD setup.
 *
 * Normally a forked child that exits before the parent calls waitpid()
 * becomes a zombie until the parent exits.  Since we deliberately do NOT
 * wait for browsers (they may stay open for hours), we tell the kernel
 * to reap our children automatically.
 *
 * SIG_IGN + SA_NOCLDWAIT is the modern, POSIX-correct way to do this.
 * (The old signal(SIGCHLD, SIG_IGN) is historical and less well-defined.)
 * ---------------------------------------------------------------------------*/
static void ensure_children_reaped(void) {
    static int installed = 0;
    if (installed) return;

    struct sigaction sa = {
        .sa_handler = SIG_IGN,
        .sa_flags   = SA_NOCLDWAIT,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    installed = 1;
}

/* ---------------------------------------------------------------------------
 * Choose a browser launcher.
 *
 * Priority:
 *   1. $BROWSER (respect the user's explicit preference)
 *   2. First available well-known launcher in /usr/bin
 *   3. Fall back to "xdg-open" and let execvp() fail loudly if it's missing
 *
 * We deliberately do NOT walk $PATH here: Linux desktop launchers live in
 * /usr/bin, and probing that one directory keeps this simple and fast.
 * ---------------------------------------------------------------------------*/
static const char *pick_opener(void) {
    const char *env = getenv("BROWSER");
    if (env && *env) return env;

    static const char *candidates[] = {
        "xdg-open",
        "open",
        "firefox",
        "chromium",
        NULL,
    };

    for (const char **cursor = candidates; *cursor; cursor++) {
        char path[256];
        int  written = snprintf(path, sizeof path, "/usr/bin/%s", *cursor);
        if (written < 0 || (size_t)written >= sizeof path) continue;
        if (access(path, X_OK) == 0) return *cursor;
    }

    return "xdg-open";
}

/* ---------------------------------------------------------------------------
 * open_url() - Launch a URL in the browser.
 *
 * Returns 0 on successful fork, -1 on error (empty URL or fork failure).
 *
 * The child process is deliberately detached:
 *   - setsid(2) puts it in its own session, so it won't get our SIGHUP
 *     when the terminal closes, and won't fight us for the TTY.
 *   - stdin/stdout/stderr are redirected to /dev/null so the browser's
 *     diagnostic chatter can't corrupt our TUI's screen state.
 * ---------------------------------------------------------------------------*/
int open_url(const char *url) {
    if (!url || !*url) return -1;

    ensure_children_reaped();

    const char *opener = pick_opener();

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        /* --- child --- */
        setsid();

        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        execlp(opener, opener, url, (char *)NULL);

        /* If execlp returns, it failed.  _exit (not exit) so we don't
         * flush the parent's stdio buffers twice.  127 is the shell's
         * conventional "command not found" code. */
        _exit(127);
    }

    /* --- parent --- */
    return 0;
}
