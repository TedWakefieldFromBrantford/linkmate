# LinkMate Development Notes

Running log of design decisions, environment gotchas, and reasoning that
isn't visible from the code alone.  Read this before you touch the code.

---

## Current state (as of v0.2.0)

LinkMate is **working end-to-end** on both a headless VPS and a local
MX Linux desktop.

Input modes:

- `cmd | linkmate` — pipe text in (stdin)
- `linkmate file.txt` — read a file
- `linkmate --pane` — capture from current tmux pane
- `linkmate --clip` — read from system clipboard

Common pipeline:

1. `parse_args()` handles terminal flags (`-h`, `-V`); may exit.
2. `acquire_input()` picks a source (flags → pipe → file → usage)
3. `get_urls_from_text()` parses URLs
4. If stdout is a TTY: `run_selector()` opens the TUI
   Else: `print_url_list()` prints a numbered list
5. Enter in the TUI calls `open_url()` → fork/exec → browser

Tests: 3 green (`raw-mode`, `parser`, `terminal`).

Documentation: man page (`man/man1/linkmate.1`), `--help`/`--version`
flags, `CHANGELOG.md`.

---

## Architecture

    INPUT SOURCES
      stdin (pipe) ──┐
      file argument ─┤
      tmux capture ──┼──► acquire_input()
      clipboard tool ┘         │
                               ▼
                      get_urls_from_text()
                               │
                               ▼
                           LinkList
                               │
                  isatty(stdout)? ──┤
                               │
              yes ─────────────┼───────────── no
                               │
                               ▼
                       run_selector       print_url_list
                       (/dev/tty,
                        raw mode)
                               │ Enter
                               ▼
                           open_url()
                           fork / exec

Files:

| File | Responsibility |
|------|----------------|
| `src/linkmate.c` | dispatch, arg parsing, input sources, TUI loop, main |
| `src/terminal.c` | raw mode, key reading, ANSI escapes |
| `src/url_parser.c` | bitmask classifier, prefix scan |
| `src/browser.c` | `open_url()` via fork/exec |
| `src/config.c` | (stub) config file parsing |
| `src/clipboard.c` | (stub) clipboard helpers |
| `src/history.c` | (stub) history persistence |
| `src/utils.c` | (stub) misc helpers |

---

## Design decisions worth remembering

### The input dispatch tree

`acquire_input()` is a four-branch tree.  Order matters:

1. **Flags first** (`--pane`, `--clip`).  User said what they want.
2. **Pipe second** (`!isatty(STDIN_FILENO)`).  Standard Unix path.
3. **File third** (non-flag argv element).  Explicit but less than a
   flag.
4. **Usage last.**  Nothing to do.

**More explicit wins.**  A flag beats a pipe; a pipe beats a file; a
file beats nothing.  This gives predictable behavior and matches what
users expect from other Unix tools.

Consequence: `cmd | linkmate file.txt` uses the pipe, not the file.
Documented in the man page.

### Why `parse_args()` and `acquire_input()` are separate

Both walk argv, but they handle **different lifetimes of flags**:

- `parse_args()` owns **terminal** flags: `-h`, `--help`, `-V`,
  `--version`.  Their only job is to print something and exit.  They
  never touch input, the TUI, or the browser.
- `acquire_input()` owns **source** flags: `--pane`, `--clip`.  They
  choose where text comes from and hand off to the rest of the
  pipeline.

Merging them into one loop would require scanning argv twice anyway
(once for early-exit flags, once for source selection) and would widen
`acquire_input()`'s responsibility past "get me text."  Two functions
with one job each is cleaner.

**Precedence:** `parse_args()` runs first.  If it exits, `acquire_input()`
never runs.  Otherwise `acquire_input()` sees all argv and picks a
source.  Informational flags always beat source flags.

### Exit codes

- `0` — success (help/version printed; URLs listed; URL opened; user
  quit the selector)
- `1` — runtime failure (couldn't read an input source; file not
  found)
- `2` — usage error (unknown flag; no input source available)

`--help` and `--version` write to **stdout**, not stderr, so they can
be piped (`linkmate --help | less`).  Usage-on-error goes to stderr.
This is the standard convention.

### Why we open `/dev/tty` for keystrokes

When the user pipes input (`echo ... | linkmate`), stdin is a pipe, not
a TTY.  `tcsetattr` requires a TTY.  And the pipe has no keystrokes
anyway — it's carrying URL text and closes when the writer does.

Solution: open `/dev/tty` to get a **separate fd** to the controlling
terminal, and read keystrokes from there.  Same pattern as `less`,
`fzf`, `vim`.

Consequence: `enable_raw_mode()` and `read_key()` take an `fd`
parameter.  They do not assume `STDIN_FILENO`.

### Why CR → KEY_ENTER translation lives in `read_key()`

Raw mode disables `ICRNL`, so Enter arrives as byte 13 (CR), not 10
(LF).  `terminal.h` defines `KEY_ENTER` as 10.

We normalize at the boundary (`read_key`) rather than in every caller.
Callers only ever see `KEY_ENTER`.  This is the "normalize at the edge"
principle — the abstraction doesn't leak.

### Why `\r\n` inside the TUI

Raw mode disables `OPOST` (output post-processing).  Without it, `\n`
moves down but does **not** return to column 1.  So all `printf`s inside
the raw-mode loop use `\r\n`.  Outside the loop (before/after raw mode),
plain `\n` is correct.

If you see "staircase" output in the TUI, you forgot a `\r`.

### Why `disable_raw_mode()` is idempotent

Every exit path from the TUI calls `disable_raw_mode()` — the Enter
branch, the quit branch, and the cleanup after the loop.  Making it a
no-op when raw mode isn't active lets us call it unconditionally
without tracking state.  A crashed TUI that leaves the terminal in raw
mode is user-hostile; better to over-restore than under-restore.

### Why `open_url()` uses `fork` + `exec` (not `system`)

- `system("xdg-open URL")` passes the URL through a shell.  URL is
  untrusted input.  Shell injection is trivial (`url; rm -rf ~`).
- `fork` + `exec` avoids the shell entirely.  The URL is an argument,
  not a string interpolated into a command line.
- `system` blocks until the child exits, freezing the TUI.  Fork lets
  the parent continue immediately.

### Why `open_url()` calls `setsid()` in the child

The child inherits the parent's controlling terminal.  Without
detaching:

- It receives SIGHUP when the terminal closes.
- It competes with LinkMate for keyboard input.
- Its diagnostic output could corrupt the TUI screen.

`setsid()` puts the child in its own session with no controlling
terminal.  It runs independently of LinkMate.

### Why we reap zombies with `SIG_IGN + SA_NOCLDWAIT`

We `fork` browsers but don't `waitpid` them (they may run for hours).
Without intervention, each becomes a zombie until LinkMate exits.
`SIGCHLD = SIG_IGN` with `SA_NOCLDWAIT` tells the kernel to reap them
automatically.  This is the modern POSIX-correct form (the old
`signal(SIGCHLD, SIG_IGN)` is historical and less well-defined).

Installed lazily on first `open_url()` call — encapsulated in
`browser.c` because that's where the `fork` lives.  Cause and cleanup
in the same file.

### Why browser stderr goes to a logfile, not `/dev/null`

Originally we sent it to `/dev/null`.  This made launch failures
completely invisible — `xdg-open` would print an error, we'd throw it
away, and the user would see nothing.  Now it goes to
`/tmp/lm-browser.log` (overridable via `LINKMATE_BROWSER_LOG`).

**Lesson:** `2>/dev/null` in shell and `freopen("/dev/null", "w",
stderr)` in C are the same mistake.  Convenient 90% of the time,
catastrophic when the thing you need to know about is happening on that
stream.

### Why `url_parser.c` uses a bitmask

`is_url_char()` uses a 256-bit bitmap (4 × 64-bit words) rather than a
chain of `strchr` calls or `isalnum()` + comparisons.  It's a single
load and a shift-and-mask.  The bitmask also makes the URL character
set explicit and self-documenting in one string literal.

Single-quote (`'`) is deliberately excluded — it's a legal sub-delimiter
per RFC 3986, but appears in prose far more often than in URLs.
Including it produces more false positives than it's worth.

### Why we shell out to `tmux` and `xclip` instead of linking libraries

For `--pane`, we could link against a tmux client library.  For
`--clip`, we could implement X11 selection handling ourselves (or link
`libX11`).  Both are wrong-sized solutions:

- **Libraries add build dependencies.**  Users would need dev packages
  to build LinkMate.  Shelling out to `xclip` just needs `xclip` at
  runtime, and only if you use `--clip`.
- **Libraries add complexity.**  X11 selection handling is ~100 lines
  of protocol code and doesn't work on Wayland.  `xclip` and `wl-paste`
  already solve this, robustly.
- **Subprocesses are observable.**  If `--pane` fails, run
  `tmux capture-pane -p` by hand and see what happens.  If a library
  fails, you're debugging with `gdb`.

The `run_and_capture()` helper is the whole subprocess interface — 60
lines, no dependencies, works with any tool that writes text to stdout.

### Why "scour the terminal" is not a feature

Terminal emulators have no portable API for reading scrollback.  Each
one that *does* have an API (kitty, wezterm, tmux) has a different one.
Users don't know which terminal they're in — even the developer of this
project had to check.

Solution: don't build it.  Users compose:

    tmux capture-pane -p | linkmate     # if in tmux
    xclip -o | linkmate                 # after Ctrl+Shift+C

`--pane` and `--clip` wrap those compositions in a flag so the workflow
is fast, without pretending LinkMate can reach into arbitrary terminals.

**The manual step is a feature, not a bug.**

### Why `-march=x86-64-v3` is the default (not `native`)

`-march=native` compiles for the exact CPU of the build machine.  On a
distributed binary, this crashes with SIGILL on older CPUs.  `x86-64-v3`
is a fixed feature set (AVX2, BMI, FMA) supported by essentially every
CPU made since ~2013.

Override for personal builds with `make MARCH=native`.  Override for
maximum compatibility with `make MARCH=x86-64-v2`.

**Default to the lowest common denominator; let users opt into maximum
performance.**

### Why the URL parser deduplicates

The same URL often appears multiple times in HTML input — once as an
`href` and once as visible link text, or as a repeated reference in a
sidebar.  Duplicates in the selector are user-hostile: they make the
list look longer than it is and force arrow-key presses that do
nothing useful.

Deduplication uses a `StrSet` (see `src/strset.c`): an open-addressing
hash table with FNV-1a hashing and linear probing.  Every extracted
URL is checked against the set before being added to the result list.
If it's already present, the URL is silently skipped.

The design tradeoff:
- **Open addressing** over chaining: single allocation, cache-friendly,
  no per-entry `malloc`.
- **FNV-1a** over a cryptographic hash: fast for short strings, good
  distribution, no dependency on OpenSSL or libsodium.
- **`strdup` on insert**: the set owns its strings, so the caller's
  input buffer can be freed after parsing without dangling pointers.

Deduplication is best-effort: if `strset_create()` fails (out of
memory), the parser proceeds without it, accepting duplicates rather
than dropping URLs.  For a tool whose output is "open this URL in a
browser," a duplicate is a cosmetic annoyance; a dropped URL is a
missing result.

Exact byte comparison only: `https://example.com` and
`https://example.com/` are treated as distinct.  Normalizing URLs
(trailing slashes, host case, port defaults) is a rabbit hole that
doesn't pay off for this use case — a user who sees both variants
in the list can pick whichever opens.


---

## Environment gotchas

### Headless VPS / no browser installed

`xdg-open` exists on most systems but fails silently if there's no
browser to hand off to.  Symptom: pressing Enter in the TUI does
nothing.

Fix for development: set `$BROWSER` to a stub.  Example:

    cat > ~/fake-browser <<'EOF'
    #!/bin/bash
    echo "would open: $1" >> /tmp/fake-browser.log
    EOF
    chmod +x ~/fake-browser

    echo "https://example.com" | BROWSER=$HOME/fake-browser ./linkmate

### Pipeline env-var gotcha

`BROWSER=x cmd1 | cmd2` sets `BROWSER` for `cmd1`, not `cmd2`.  In a
pipeline, put the assignment on the **right** side, or use `env`:

    echo "https://example.com" | BROWSER=$HOME/fake-browser ./linkmate
    echo "https://example.com" | env BROWSER=$HOME/fake-browser ./linkmate

This bit us during v0.1.0 debugging.  Shell parsing rules, not LinkMate
behavior — but easy to forget.

### Two-machine workflow (VPS + MX Linux)

Development happens on two machines: a headless VPS (Debian, tmux, no
browser) and a local MX Linux 23.6 box (X11, has a real browser).

**Source sync:** `rsync` for now, until a git remote is set up.

**The gotcha:** `rsync` copies *files*, not *git history*.  If you edit
`src/linkmate.c` on MX and rsync only the file to VPS, the VPS's working
tree has the code but its git repo doesn't have the commit.  You end up
with two divergent histories on the two machines.

**This bit us during v0.2.0.**  The `--help`/`--version` commit was made
on the VPS, then the man page commit was made on MX, and neither machine
had both commits until we manually reconciled.

**Fix going forward:** set up a single canonical git remote (GitHub,
Codeberg, or a bare repo on one of the machines).  Both machines
`push`/`pull` from it.  `rsync` then becomes a tool for deploying built
artifacts, not for sharing source.

### The `man` / `mandoc` distinction

`man` on Debian/Ubuntu/MX uses `groff` under the hood.  `mandoc` is a
separate program: a stricter, faster, BSD-built man-page renderer that
catches roff errors `groff` silently tolerates.

Useful commands:

    man ./man/man1/linkmate.1        # preview with groff (what users see)
    mandoc man/man1/linkmate.1       # preview with mandoc
    mandoc -Tlint man/man1/linkmate.1 # lint only, silent if clean

---

## Git history lessons

- The first working commit is the one where the TUI selector opens and
  Enter launches a browser.  Everything before that was scaffolding.

- The `/dev/tty` refactor is documented in the commit that introduces
  `open_keyboard_input()`.  Look for it if the design feels off.

- The CR → KEY_ENTER fix is one line in `terminal.c`'s `read_key()`.  If
  you ever see "Enter does nothing," check that line first.

- `rsync` is not a substitute for a git remote.  See "Two-machine
  workflow" above.

---

## Current limitations / TODOs

- `src/config.c`, `src/clipboard.c`, `src/history.c`, `src/utils.c` are
  empty stubs.  No plan to fill them in the near term — the current
  feature set is the point of the tool, not a starting point for more.

- `src/linkmate.c`: `acquire_input()` returns `NULL` for both usage
  errors and empty input; `main()` can't distinguish.  Deferred.

- `src/linkmate.c`: `parse_args()` does not support `--` (end of
  options) or combined short flags (`-hV`).  Both deliberate for now.

- `src/browser.c`: `pick_opener()` only checks `/usr/bin/`, not the
  full `$PATH`.  Fine for Linux desktops; may misfire in odd
  environments.

- `src/browser.c`: `$BROWSER` values with arguments (e.g.
  `firefox --private-window`) aren't split — the whole string is
  passed as the program name.  Rare in practice.

- No SIGINT/SIGTERM handler for terminal restoration on crash.  Only
  matters if `ISIG` gets re-enabled; raw mode disables it, so we're
  fine today.

- `test/data/big_file.txt` is 0 bytes.  Needed for the SIMD benchmark
  (feature C, below).

---

## Next session — pick one

**Distribution pass (in progress).**  Remaining items:

1. ~~`--help` / `--version` flags~~ — done (v0.2.0)
2. ~~Man page (`man/man1/linkmate.1`)~~ — done
3. `LICENSE` file + SPDX headers in source files
4. `Makefile` with `install` / `uninstall` targets, respecting
   `PREFIX`, `DESTDIR`, `MANDIR`
5. `make dist` target producing a versioned tarball
6. `README.md` polished (README draft exists in old `docs/next`,
   needs cleanup)
7. Git remote setup (GitHub/Codeberg) to end the rsync dance

**Feature work (deferred):**

- **SIMD benchmark** — vectorize the prefix scan in `url_parser.c`.
  Use SSE2/AVX2 via `<immintrin.h>`, dispatch at runtime via
  `__builtin_cpu_supports("avx2")`.  **This is the interesting one.**
  Pre-req: generate `test/data/big_file.txt`.

**Recommended next:** item 4 (Makefile install/uninstall).  It's the
biggest remaining piece before a shippable tarball.

**Considered and rejected (do not re-litigate):**

- **OSC 52 clipboard.**  The URL was already in the user's clipboard
  or terminal.  Copying it back is a party trick, not a workflow.
- **`~/.config/linkmate/linkmate.conf`.**  The only real need
  (`BROWSER=` prefix on headless VPS) is already solved by `$BROWSER`.
  Everything else a config could configure is unbuilt and unplanned.
  A tool this small does not need a config file.

  These are not "not yet" — they are **no**.  If someone asks for
  them later, the answer is "compose with `xclip`/`tmux` and set
  `$BROWSER`," not "let's add a config file."  The value of this tool
  is that it's small and does one thing well.  Cruft erodes that.

---

## How I work

- Retype code rather than paste.  Concepts stick better when typed.
- Complete files or complete functions, in house style, with the kind
  of annotation you'd write in a code review.
- Explain the "why" behind design decisions, not just the "what."
- When hitting a bug, walk through the diagnosis; don't just hand over
  the fix.

  ### Why the tool stays small

Every feature has a cost that outlives the feature's usefulness: code to
maintain, docs to update, edge cases to test, and — most expensively —
surface area that future decisions must not break.  A tool with a config
file must forever honor that config file.  A tool with a clipboard
integration must forever route through the clipboard API.

LinkMate's value is that it does one thing (extract URLs, launch one)
and does it without asking the user to configure it, remember its
options, or learn its conventions.  New features are judged against
that: does this make LinkMate *more* of a "small tool that just works,"
or less?

Two candidates were considered and rejected:

- **OSC 52 clipboard.**  The URL was already in the user's clipboard
  or terminal.  Copying it back does not solve a problem they have.
- **Config file.**  The only real use case (`BROWSER=` prefix on a
  headless VPS) is already solvable via `$BROWSER`.  Everything else
  would be configuring features that don't exist.

Both are the kind of feature that seems harmless and is not.
"Small" is not the absence of effort — it's a decision that must be
made repeatedly, against the pull of every plausible-sounding feature.


# To do:

SIGWINCH handler for mid-sesh window resizing (?)

