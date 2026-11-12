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

- Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build --config RelWithDebInfo` (mirrors `.github/workflows/build_cmake.yml`).
- Test: `ctest --test-dir build -C RelWithDebInfo --output-on-failure --no-tests=error` (C++ suite: `tests/tst_slickshot.cpp`).
- Format: clang-format 11 on `src/` (pinned in `.github/workflows/clang-format.yml`).
- Windows installer and release smoke test: `packaging/win-installer/smoke-test.ps1`, run by `.github/workflows/Windows-pack.yml`.

## Conventions

- **Formatting:** `.clang-format` is Mozilla-based with 4-space indents and braces on their own line for classes, structs, enums, and functions. CI checks `src/` with clang-format 11 only.
- **Static analysis:** `.clang-tidy` is in the repo, but no workflow runs it.
- **Qt:** build against Qt 6, the default in `CMakeLists.txt`.
- **Tests:** add cases to `tests/tst_slickshot.cpp`. ctest sets `QT_QPA_PLATFORM=offscreen`, so tests run without a visible screen.
- **Windows check:** test capture and UI changes on Windows before calling them done, since it is the main target.

## Notes

- Keep the inherited Flameshot identifiers (the `Flameshot` class and the `org.flameshot.Flameshot` instance key in `src/main.cpp`) unless the change is specifically about renaming them.
- Upload integrations need their own credentials. Imgur sits behind `ENABLE_IMGUR`, which is off by default. Never commit credentials.
- Follow `DESIGN.md` for UI and `PRODUCT.md` for scope.

## Web search

For any web search, fetching, docs lookup, current info, or browser automation, use the `$use-tinyfish` skill (`C:/Users/Admin/.agents/skills/use-tinyfish/SKILL.md`), not native web search. Go lightest first: search, then fetch, then agent, then browser. If TinyFish is unavailable and you fall back to native search, say so.

## Communication

- Talk to the user in plain, simple language. Say what happened, why it matters, and what to do next.
- Avoid jargon and file names unless the user asks for them.
