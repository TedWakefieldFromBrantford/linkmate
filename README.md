markdown

# linkmate

Extract URLs from text and open one in your browser.

## The problem

You're staring at a URL in your terminal. Maybe it's a download link in
a build log, an endpoint in a `curl` response, or a reference in a
README you just cat'd. The URL is right there — you can see it — and
getting it into your browser means selecting it (if the terminal
allows), copying it, switching windows, pasting it, and hoping the
shell didn't wrap it at a bad column.

## The solution

LinkMate - 5 URL(s) found
===================================================
 > https://github.com/tedstuff/linkmate/releases
   https://keepachangelog.com
   https://semver.org
   https://httpbin.org/get
   https://example.com

[up/down] move   [Enter] open   [q] quit



Arrow keys select, Enter opens the highlighted URL in your browser,
`q` quits. The TUI runs on the alternate screen buffer, so your
scrollback is untouched when the tool exits.

Duplicate URLs in the input are silently deduplicated; the same URL
appearing twice (for example, as both an `href` and visible link
text) appears once in the list.


## Install

From source:

git clone https://github.com/tedstuff/linkmate
cd linkmate
make
sudo make install



Requires a C99 compiler and `make`. No external dependencies.

To install somewhere other than `/usr/local`:

make install PREFIX=$HOME/.local



To stage an install for packaging:

make install DESTDIR=/tmp/stage PREFIX=/usr



To uninstall:

sudo make uninstall



## Usage

Four input sources. The first one that applies wins.

    git log --oneline -50 | linkmate    # URLs from recent commit messages
    curl -s <url> | linkmate            # URLs from a web page
    linkmate README.md                  # URLs from a file
    linkmate --pane                     # URLs from the current tmux pane
    linkmate --clip                     # URLs from the system clipboard

If stdout is a terminal, the interactive selector opens. If it isn't
(when piping to another command, or redirecting to a file), linkmate
prints a numbered list and exits:

$ cat README.md | linkmate
✅ Found 3 URL(s):
─────────────────────────────────────

    https://github.com/tedstuff/linkmate

    https://keepachangelog.com/

    https://semver.org/




In the TUI:

- `↑` / `↓` — move selection
- `Enter` — open the selected URL in your browser
- `q` or `Esc` — quit without opening anything

## Environment

- `BROWSER` — program to launch for a URL. Overrides the built-in
  probe for `xdg-open`, `open`, `firefox`, and `chromium`.
- `LINKMATE_BROWSER_LOG` — path to log browser stderr (default:
  `/tmp/lm-browser.log`). Override this if multiple users share a
  machine, or to send browser diagnostics somewhere you'll actually
  look.

## Exit status

- `0` — success (URLs found and listed, or one opened, or user quit)
- `1` — runtime error (couldn't read an input source; file not found)
- `2` — usage error (unknown flag; no input source available)

Note that "no URLs found" is not an error and exits 0.

## What it doesn't do

LinkMate is deliberately small. It extracts URLs and opens one. It
does not:

- **Read your terminal's scrollback.** No portable API exists for
  this. Compose with `tmux capture-pane -p | linkmate` or
  `xclip -o | linkmate` if you need to pull from the terminal
  itself.
- **Maintain a history of visited URLs.**
- **Have a configuration file.** The one thing worth configuring
  (`BROWSER`) is already an environment variable.
- **Do anything but launch one browser per invocation.**

These are decisions, not gaps. A tool that does one thing stays small
enough to understand in an afternoon.

## Documentation

- `man linkmate` — full reference, all options and exit codes
- `docs/DEVELOPMENT.md` — architecture and design decisions, for
  anyone who wants to modify the code

## Building

make # release build
make debug # debug build with sanitizers
make test # run all tests
make dist # create linkmate-$(VERSION).tar.gz
make distclean # remove all build artifacts
make help # list targets and variables



Build variables you can override:

make CC=clang # use a different compiler
make MARCH=native # tune for this CPU (default: x86-64-v3)
make CFLAGS="-O0 -g" # override base flags; warnings stay on
make BUILD_MODE=debug # same as make debug



## License

MIT. See [LICENSE](LICENSE).

## Author

Ted Wakefield — [tedstuff.ca](https://tedstuff.ca)
