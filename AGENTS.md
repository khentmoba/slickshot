# SlickShot

## Project Goal

SlickShot's primary goal is to be a fast, modern replacement for the default Windows Snipping Tool.

Every feature, interaction, and performance optimization should be evaluated against this objective:
- **Instant activation**: Low-latency screen capture via global hotkeys (PrintScreen, Win+Shift+S) and system tray integration.
- **In-place annotation**: Fast, precise markup tools (arrows, rectangles, highlighter, blur/pixelate, text, and numbered steps).
- **Frictionless workflow**: Quick copy to clipboard, auto-save options, and floating pinned screenshots.
- **Windows-first ergonomics**: Robust multi-monitor and high-DPI support with clean native feel.

## Stack and layout

- **Stack:** C++ with Qt 6, built with CMake 3.22 or newer. SlickShot is a standalone fork of [Flameshot](https://github.com/flameshot-org/flameshot) (see `README.md`). Windows is the first target. The Linux and macOS packaging under `packaging/` is inherited.
- `src/main.cpp`, `src/windows-cli.cpp`: entry points.
- `src/widgets/capture/`: the capture overlay (selection, magnifier, toolbar buttons).
- `src/tools/`: annotation tools such as arrow and circle.
- `src/config/`: settings UI and config resolution.
- `src/core/`: capture requests, the daemon, and global shortcut filtering.
- `src/utils/`: screen grabbing, saving, history, update checks, and shared helpers.
- `src/cli/`: command-line parsing.
- `tests/tst_slickshot.cpp`: the Qt Test suite that ctest runs.
- `packaging/win-installer/`: the NSIS installer and release smoke test.
- `DESIGN.md` covers how each UI piece looks. `PRODUCT.md` covers who SlickShot is for and what is in scope.

## Navigation

- Build (Windows): `cmake --preset windows && cmake --build --preset windows` (or `cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3/msvc2022_64" -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build --config RelWithDebInfo`).
- Test: `ctest --preset windows` (or `ctest --test-dir build -C RelWithDebInfo --output-on-failure --no-tests=error`). C++ suite: `tests/tst_slickshot.cpp` (also runs on Ubuntu 24.04 CI with a Qt 6.4 API floor).
- Format & Lint: CI checks `src/` and `tests/` with `clang-format 11` (`.github/workflows/clang-format.yml`), changed `.cpp` files with `clang-tidy` (`.clang-tidy`), workflows with `actionlint`, and SVGs/`graphics.qrc` with `python3 scripts/build_logo_assets.py --check`.
- GitHub & Git: Repo is `khentmoba/slickshot` (default branch `master`; `upstream` is `flameshot-org/flameshot`). Pass `-R khentmoba/slickshot` to `gh`, use `git fetch upstream --no-tags`, and set `GIT_EDITOR=true` for non-interactive `git rebase --continue`.
- Release & Packaging: See `docs/Releasing.md` for SlickShot's tag-driven release flow and `packaging/win-installer/smoke-test.ps1` (run by `.github/workflows/Windows-pack.yml`).
- Code Review: Read `CODING_STANDARDS.md` during review for recurring Qt6, Windows HiDPI, and fork-specific invariants.

## Conventions

- **Formatting:** `.clang-format` is Mozilla-based with 4-space indents and braces on their own line for classes, structs, enums, and functions. CI checks both `src/` and `tests/` with clang-format 11.
- **Static analysis:** `.clang-tidy` holds a curated Qt-safe check set run on changed `.cpp` files in Linux CI.
- **Qt:** build against Qt 6, the default in `CMakeLists.txt`. Keep tests and cross-platform code compatible with Ubuntu 24.04's Qt 6.4 API floor.
- **Tests:** add cases to `tests/tst_slickshot.cpp`. ctest sets `QT_QPA_PLATFORM=offscreen`, so tests run without a visible screen.
- **Verification:** agents verify headless (`ctest` with `QT_QPA_PLATFORM=offscreen` and CI `Windows-pack.yml` smoke tests). Live-screen capture and UI checks are handoff items for the user unless explicitly requested.

## Notes

- Keep the inherited Flameshot identifiers (the `Flameshot` class and the `org.flameshot.Flameshot` instance key in `src/main.cpp`) unless the change is specifically about renaming them.
- Upload integrations need their own credentials. Imgur sits behind `ENABLE_IMGUR`, which is off by default. Never commit credentials.
- Follow `DESIGN.md` for UI and `PRODUCT.md` for scope.

## Web search

For web search, docs lookup, or page fetching, use the `$use-tinyfish` skill instead of native web search (lightest first: search, fetch, agent, browser; note if falling back). For GitHub repos, PRs, issues, and diffs, use `gh` and `git` directly instead of web scraping.

## Communication

- Talk to the user in plain, simple language. Say what happened, why it matters, and what to do next.
- Avoid jargon and file names unless the user asks for them.
