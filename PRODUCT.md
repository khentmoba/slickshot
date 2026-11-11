# Product

<!-- impeccable:product-schema 1 -->

## Platform

adaptive

## Stack

C++20, Qt 6, CMake, Windows Win32 API, X11/Wayland, macOS CoreGraphics.

## Users

Developers, designers, technical writers, gamers, support engineers, and productivity power users who capture, annotate, and share dozens to hundreds of screenshots daily on Windows and cross-platform desktop setups.

## Product Purpose

SlickShot exists to be a fast, modern, standalone replacement for the default Windows Snipping Tool. It provides instant low-latency screen capture via global hotkeys (`Win+Shift+S`, `PrintScreen`), frictionless in-place annotation, and floating pinned screenshots with zero telemetry and instant clipboard workflows.

## Positioning

Unlike cloud-heavy tools (Loom, Snagit) or clunky legacy tools (Snipping Tool, Greenshot), SlickShot is a lightning-fast, native, offline desktop utility with zero bloat, instant hotkeys, and modern dark-first Windows ergonomics.

## Operating Context

- Invoked globally via hotkeys across multi-monitor, high-DPI setups.
- Freezes screen instantly into a lightweight overlay for marquee selection.
- In-place floating annotation toolbar positioned dynamically beside the crop region.
- Pinned floating screenshots that remain always-on-top for cross-window reference.
- System tray resident with near-instant hotkey response.

## Capabilities and Constraints

- Qt 6 widget rendering, custom paint engines (`QPainter`), and QSS styling.
- Real-time pixel magnifier loupe with hex color inspection.
- Floating pinned screenshot windows with interactive zooming and border toggles.
- In-place markup: arrows, rectangles, circles, counters, highlighter, freehand pencil, text, pixelate/blur, inverted fills.
- High-DPI awareness on Windows 10/11 and multi-monitor layouts.
- Must remain lightweight, sub-second activation, zero UI lag.
- In-app updates: the app downloads the release installer, checks its SHA-256, runs it silently, and relaunches. No MSI, no portable-zip hand-off, and no code-signing requirement.

## Brand Commitments

- Brand: **SlickShot**.
- Visual World: **Black Edition** — deep pitch obsidian (`#000000` / `#202020` / `#2b2b2b`), subtle glass rims (`#ffffff` with `0.16` opacity), pure white high-contrast tool glyphs, and electric azure focus accents (`#0284c7` / `#38bdf8`).
- Complete retirement of legacy Flameshot purple (`#740096` / `#270032`).

## Evidence on Hand

- Core capture engine, magnifier, and annotation tools in `src/`.
- Brand logo: Kinetic Laser Shears on solid black squircle in `assets/icon.svg` and `data/img/app/`.
- Multi-resolution Windows ICO and high-res PNGs in `data/img/app/`.

## Product Principles

1. **Instant Velocity**: Every millisecond between pressing the hotkey and cropping counts. Zero UI friction or delayed animations.
2. **Context-Aware Ergonomics**: Toolbars position themselves intelligently to avoid obscuring the user's selection.
3. **Surgical Precision**: High-contrast pixel rulers, clear magnifier readouts, and clean vector shapes.
4. **Restraint & Polish**: The interface recedes when capturing content; controls are crisp, dark-mode native, and distraction-free.

## Accessibility & Inclusion

- High-contrast selection boundaries legible against both 100% white and 100% black captured content.
- Full keyboard shortcut navigation for all capture, annotation, and export tools.
- Clear numeric indicator badges for numbered annotation steps.
