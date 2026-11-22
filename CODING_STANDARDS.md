# SlickShot Coding Standards

Read during code review. Mechanical style and bug checks live in `.clang-format`, `.clang-tidy`, `scripts/build_logo_assets.py --check`, and CI workflows. This file covers recurring judgement calls for SlickShot's Qt 6 / Windows fork.

## Qt Lifetime & UI

- **No `WA_DeleteOnClose` on stack dialogs:** Never set `Qt::WA_DeleteOnClose` on a stack-allocated `QDialog` run with `exec()` — closing it causes a double-free.
- **No pre-init `QCoreApplication` calls:** Do not call `QCoreApplication` / `QGuiApplication` APIs (including `QSettings` paths that depend on `qApp`) before the application object is initialized.
- **Light and dark mode QSS:** Avoid dark-only hardcoded colors in stylesheets; verify contrast works in both light and dark themes.
- **Capture toolbar reality:** `CaptureToolButton` floats directly over the dimmed capture scrim (there is no painted toolbar background) and is not checkable. Do not rely on `:checked` stylesheet states or a solid toolbar backdrop.

## Windows & HiDPI

- **Fractional DPR parity:** Verify drawing, hit-testing, and coordinate math at fractional Windows scale factors (`1.25`, `1.5`, `1.75`), not only `1.0` and `2.0`.
- **Physical vs. logical coordinates:** Keep `QPixmap::size()` / Win32 physical pixels separate from `QScreen::geometry()` / Qt logical coordinates; convert explicitly via `devicePixelRatio()`.
- **Cross-platform path handling (Qt 6.4 floor):** `QDir::fromNativeSeparators()` is a no-op on Linux (`\` is a valid filename char there). Normalize backslashes explicitly when Windows paths are tested or processed on Ubuntu 24.04 CI, and stay within Qt 6.4 APIs.
- **Keep `WH_KEYBOARD_LL` non-blocking:** Never run `QSettings`, disk I/O, or blocking calls inside the low-level Windows keyboard hook; cache settings on the main thread.
- **Symmetric registry writes:** Pair every `HKLM` / `HKCU` write with matching cleanup/uninstall logic across the same hives.
- **No UAC TOCTOU:** Re-verify downloaded installer SHA-256 hashes immediately before launching an elevated UAC process.

## Fork & Packaging Invariants

- **Preserve SlickShot-only entry points:** When porting upstream Flameshot changes, verify they do not bypass `ms-screenclip:` / `/clip` handling in `src/main.cpp` or the single-monitor fast path in `ScreenGrabber::grabEntireDesktop()`.
- **NSIS install tree hygiene:** `cmake --install` stages `include/` and `lib/` from bundled `KDSingleApplication`. When modifying `packaging/win-installer/SlickShot.nsi`, ensure development headers and static libs are not shipped to end users.
