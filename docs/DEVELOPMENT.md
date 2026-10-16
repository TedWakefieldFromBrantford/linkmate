# LinkMate Development Notes

Running log of design decisions, environment gotchas, and reasoning that
isn't visible from the code alone. Read this before you touch the code.

---

## Current state (as of v0.1.0)

LinkMate is **working end-to-end** on a headless VPS. It:

- Reads piped input from stdin (`echo ... | linkmate`)
- Parses URLs from arbitrary text
- Opens a raw-mode TUI, reading keystrokes from `/dev/tty`
- Navigates with arrow keys, opens with Enter
- Forks a subprocess to launch the URL via `$BROWSER` (or `xdg-open`)
- Restores the terminal cleanly on exit

Tests: 3 green (`raw-mode`, `parser`, `terminal`).

---

## Architecture

stdin (pipe) stdout (terminal)
│ ▲
▼ │
┌────────────────────────────────────────────────────────────┐
│ linkmate.c │
│ acquire_input() → get_urls_from_text() → run_selector() │
└──────────────┬───────────────────────┬─────────────────────┘
│ │
│ │ open_url()
│ ▼
│ ┌───────────────┐
│ │ browser.c │
│ │ fork/exec │
│ └───────────────┘
│
│ read_key()
▼
┌───────────────┐
│ terminal.c │ ← /dev/tty (separate fd, for keystrokes)
│ raw mode │
└───────────────┘

url_parser.c: pure function, text in → LinkList out
text


---

## Design decisions worth remembering

### Why we open `/dev/tty` for keystrokes

When the user pipes input (`echo ... | linkmate`), stdin is a pipe, not a
TTY. `tcsetattr` requires a TTY. And the pipe has no keystrokes anyway —
it's carrying URL text and closes when the writer does.

Solution: open `/dev/tty` to get a **separate fd** to the controlling
terminal, and read keystrokes from there. This is the same pattern
`less(1)`, `fzf(1)`, and `vim(1)` use.

Consequence: `enable_raw_mode()` and `read_key()` take an `fd` parameter.
They do not assume `STDIN_FILENO`.

### Why CR → KEY_ENTER translation lives in `read_key()`

Raw mode disables `ICRNL`, so Enter arrives as byte 13 (CR), not 10 (LF).
`terminal.h` defines `KEY_ENTER` as 10.

We normalize at the boundary (`read_key`) rather than in every caller.
Callers only ever see `KEY_ENTER`, never raw byte 13. This is the
"normalize at the edge" principle — the abstraction doesn't leak.

### Why `\r\n` inside the TUI

Raw mode disables `OPOST` (output post-processing). Without it, `\n`
moves down but does **not** return to column 1. So all `printf`s inside
the raw-mode loop use `\r\n`. Outside the loop (before/after raw mode),
plain `\n` is correct.

If you see "staircase" output in the TUI, you forgot a `\r`.

### Why `disable_raw_mode()` is idempotent

Every exit path from the TUI calls `disable_raw_mode()` — the Enter
branch, the quit branch, and the cleanup after the loop. Making it a
no-op when raw mode isn't active lets us call it unconditionally without
tracking state. A crashed TUI that leaves the terminal in raw mode is
user-hostile; better to over-restore than under-restore.

### Why `open_url()` uses `fork` + `exec` (not `system`)

- `system("xdg-open URL")` passes the URL through a shell. URL is
  untrusted input. Shell injection is trivial (`url; rm -rf ~`).
- `fork` + `exec` avoids the shell entirely. The URL is an argument, not
  a string interpolated into a command line.
- `system` also blocks until the child exits, freezing the TUI while the
  browser is open. Fork lets the parent continue immediately.

### Why `open_url()` calls `setsid()` in the child

The child inherits the parent's controlling terminal. Without detaching
it:
- It receives SIGHUP when the terminal closes.
- It competes with LinkMate for keyboard input.
- Its diagnostic output could corrupt the TUI screen.

`setsid()` puts the child in its own session with no controlling
terminal. It runs independently of LinkMate.

### Why we reap zombies with `SIG_IGN + SA_NOCLDWAIT`

We `fork` browsers but don't `waitpid` them (they may run for hours).
Without intervention, each becomes a zombie until LinkMate exits.
`SIGCHLD = SIG_IGN` with `SA_NOCLDWAIT` tells the kernel to reap them
automatically. This is the modern POSIX-correct form (the old
`signal(SIGCHLD, SIG_IGN)` is historical and less well-defined).

The handler is installed lazily on the first `open_url()` call —
encapsulated in `browser.c` because that's where the `fork` lives.
Cause and cleanup in the same file.

### Why `pick_opener()` checks `$BROWSER` first

Users have preferences. `$BROWSER` is the standard Unix way to express
them. Honor it before falling back to hardcoded probes.

### Why we redirect the browser's stderr to a logfile

Originally we sent it to `/dev/null`. This made launch failures
completely invisible — `xdg-open` would print an error, we'd throw it
away, and the user would see nothing. Now it goes to
`/tmp/lm-browser.log` by default (overridable via `LINKMATE_BROWSER_LOG`).

**Lesson:** `2>/dev/null` in shell and `freopen("/dev/null", "w", stderr)`
in C are the same mistake. Convenient 90% of the time, catastrophic when
the thing you need to know about is happening on that stream.

### Why `url_parser.c` uses a bitmask

`is_url_char()` uses a 256-bit bitmap (4 × 64-bit words) rather than a
chain of `strchr` calls or `isalnum()` + comparisons. It's a single load
and a shift-and-mask. The bitmask also makes the URL character set
explicit and self-documenting in one string literal.

Single-quote (`'`) is deliberately excluded — it's a legal sub-delimiter
per RFC 3986, but it appears in prose far more often than in URLs.
Including it produces more false positives than it's worth.

### Why we use a bitmask but haven't vectorized yet

The `is_url_char` classifier is written in a vectorizable style. The
prefix scan (`https://`, `http://`, etc.) is a `strstr` walk and is the
next candidate for SIMD. See "Next session" below.

---

## Environment gotchas

### Headless VPS / no browser installed

`xdg-open` exists on most systems but fails silently if there's no
browser to hand off to. Symptom: pressing Enter in the TUI does nothing.

Fix for development: set `$BROWSER` to a stub. Example:

```bash
cat > ~/fake-browser <<'EOF'
#!/bin/bash
echo "would open: $1" >> /tmp/fake-browser.log
EOF
chmod +x ~/fake-browser

echo "https://example.com" | BROWSER=$HOME/fake-browser ./linkmate

Note the shell subtlety: BROWSER=x cmd1 | cmd2 sets BROWSER for
cmd1, not cmd2. To set it for the second command of a pipeline:
bash

cmd1 | BROWSER=x cmd2

This bit us once during development. See git log for the fixup commit.
Terminal left in raw mode after a crash

If you Ctrl-C during TUI rendering (shouldn't happen because we disable
ISIG, but if you send SIGKILL), the terminal may be left in raw mode.
Fix:
bash

stty sane
# or
reset

stty sane is less destructive. Get in the habit of typing it if the
terminal "feels wrong."
make test blocks on the raw-mode test

test/unit/test_raw_mode.c is interactive. It waits for keystrokes and
exits on q or ESC. When running make test, be ready to press q.

Run individual tests to avoid this:
bash

make test-parser
make test-terminal
make test-raw        # press q to continue

Style guide

The codebase follows a consistent house style. Match it when adding code.

    Header comment blocks. Every .c and .h file starts with a
    banner explaining purpose and non-obvious design decisions.

    Section separators. /* ---...--- */ between functions. Makes
    files skimmable.

    Comments explain "why," not "what." If the code says i++, a
    comment saying "increment i" is noise. A comment saying "advance to
    the next candidate URL start" is signal.

    Compile with all warnings. -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion -Wsign-conversion. Every cast matters. Every
    (size_t) is deliberate. Don't disable warnings to "just get it to
    build."

    Cast to silence -Wconversion explicitly. Not
    #pragma GCC diagnostic ignored.

    snprintf return values are checked. Not just for correctness —
    it teaches you to think about truncation.

    free() matches malloc(). No leaks. close() matches open().
    No fd leaks. Whoever opens, closes.

    No logging framework. Print to stderr when errors are
    unrecoverable; otherwise return values. Test until happy.

    Comments in the code, not in commit messages. Design intent goes
    in the source, where it lives forever.

Testing strategy

Currently: three unit tests via test/unit/runner.c.

    test/unit/test_raw_mode.c — interactive, blocks on keystrokes

    test/unit/test_parser.c — table of input/expected pairs

    test/unit/test_terminal_functions.c — visual (clears screen)

The test harness is minimal by design. Not a framework; just forward
declares, a run_test helper, and an argv dispatcher.

Future: integration tests in test/integration/ that actually run
linkmate against fixtures. Useful for the SIMD work where correctness
of the vectorized scan has to be verified against the scalar version.
