# Changelog

##  [Unreleased]

### Added
- Section 1 man page ("linkmate.1")

## [0.2.0] - 2026-10-16




### Added
- `--help` and `-h` flags
- `--version` and `-V` flags

### Changed
- Unknown flags now exit 2 with a two-line diagnostic
- `usage()` includes Options block and man-page pointer

## [Unreleased]

### Added
- Section 1 man page (`linkmate.1`)
- URL deduplication via a hash set; the same URL appearing multiple
  times in the input appears once in the output

### Fixed
- TUI: header row was being scrolled off-screen on terminals where
  the frame height matched the terminal height exactly
- TUI: `cursor_home()` emitted an invalid escape sequence (`\0333[H`)
  that corrupted the terminal's output
- TUI: viewport scrolling pinned at the top of the list, preventing
  selection from moving past the visible window
