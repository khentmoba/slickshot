# SlickShot

## Project Goal

SlickShot's primary goal is to be a fast, modern replacement for the default Windows Snipping Tool.

Every feature, interaction, and performance optimization should be evaluated against this objective:
- **Instant activation**: Low-latency screen capture via global hotkeys (PrintScreen, Win+Shift+S) and system tray integration.
- **In-place annotation**: Fast, precise markup tools (arrows, rectangles, highlighter, blur/pixelate, text, and numbered steps).
- **Frictionless workflow**: Quick copy to clipboard, auto-save options, and floating pinned screenshots.
- **Windows-first ergonomics**: Robust multi-monitor and high-DPI support with clean native feel.

## Navigation

- Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build --config RelWithDebInfo` (mirrors `.github/workflows/build_cmake.yml`).
- Test: `ctest --test-dir build -C RelWithDebInfo --output-on-failure --no-tests=error` (C++ suite: `tests/tst_slickshot.cpp`).
- Format: clang-format 11 on `src/` (pinned in `.github/workflows/clang-format.yml`).
- Windows installer and release smoke test: `packaging/win-installer/smoke-test.ps1`, run by `.github/workflows/Windows-pack.yml`.
