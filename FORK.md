# Fork Ledger

Upstream repo: `flameshot-org/flameshot`

Upstream issues scanned: 2026-11-18

## Mirrored Issues

- upstream #4887 → fork #23 (fixed by porting upstream PR #4972)
- upstream #5017 → fork #24 (`Ctrl+IntlBackslash` on Windows 11 ISO keyboards)
- upstream #4960 → fork #25 (fixed by removing redundant null checks in `CaptureToolButton::~CaptureToolButton`)
- upstream #5028 → fork #26 (assigning `Ctrl+Shift+PrintScreen` as a shortcut)
- upstream #4957 → fork #27 (persist tool color individually per tool)
- upstream #4911 → fork #28 (remember 'Use last region' even when capture is cancelled)
- upstream #4913 → fork #29 (pinboard collections management and quick restore)
- upstream #4903 → fork #30 (better 'Open With' default app and custom list)
- upstream #4856 → fork #31 (multiple shortcuts per command)
- upstream #4855 → fork #32 (single-shortcut instant fullscreen or previous-area capture)
- upstream #4854 → fork #33 (better secure pixelation: pixelate only text)
- upstream #4834 → fork #34 (watermark tool, transparency, and rotation)
- upstream #4978 → fork #35 (C2PA provenance metadata in exported screenshots)

## Already Fixed in Fork

- upstream #5049 (`flameshot.ini` 0 bytes / config truncation race): fixed in `791ba09f`
- upstream #5029 (settings General tab vertical stretch): fixed in `791ba09f`
- upstream #4923 (pinned screenshot pinch zoom): fixed in `791ba09f`
- upstream #4906 (`OverlayMessage::updateGeometry` dangling pointer): fixed in `791ba09f`
- upstream #4886 (settings scroll wheel changing dropdowns/spinboxes): fixed in `791ba09f`
- upstream #4901 (copy fails via `Win+Shift+S` / `ms-screenclip` on Windows 11): fixed in `22ab8b7e`
- upstream #4909 (toolbar unreachable at screen edges): fixed by floating toolbar in `buttonhandler.cpp` (`0e8573aa` / `f9205d96`)
- upstream #4934 (Windows binary missing `/guard:cf`): fixed in `c7782a75`
- upstream #4851 (double scaling / Pin offset on Windows 11): fixed in `9a06db4a` (#4907)
- upstream #4921 (Italian `it_CH` localization fallback): fixed in `d3b0d5ea` (#4932)
- upstream #4996 (outdated `README.md` links): fixed in `49dfb928` (#4998)

## Needs Repro (Not Mirrored)

- upstream #5000 (Print Screen in full-screen `mstsc.exe` RDP): SlickShot already uses `WH_KEYBOARD_LL` in `src/core/globalshortcutfilter.cpp` when "Replace Snipping Tool" is enabled; needs live RDP session to verify hook ordering.
- upstream #4868, #4928 (mixed-DPI / rotated multi-monitor distortion): primarily Linux X11/Wayland reports; SlickShot has its own Windows capture/DPI fixes (`791ba09f`, `26075092`).
- upstream #4819 (Windows 10 "Procedure entry point could not be located"): reported against Flameshot's 14.0.0 MSI; SlickShot ships an NSIS installer (`packaging/win-installer/SlickShot.nsi`) verified by `smoke-test.ps1`.

## Not Affected

- **Linux / Wayland / X11 / Flatpak / AppImage-only** (SlickShot targets Windows first): #5062, #5058, #5053, #5051, #5023, #5015, #5007, #5001, #4990, #4989, #4983, #4969, #4966, #4948, #4942, #4938, #4927, #4925, #4920, #4915, #4914, #4896, #4895, #4894, #4883, #4873, #4871, #4869, #4862, #4849, #4848, #4844, #4843, #4840, #4838, #4837, #4833, #4824, #4823, #4820, #4818, #4815, #4812, #4809, #4808.
- **macOS-only** (SlickShot targets Windows first): #5055, #5039, #5005, #4976, #4964, #4953, #4951, #4940, #4930, #4910, #4859, #4850, #4825, #4817, #4814, #4810.
- **Windows MSI-only** (SlickShot uses NSIS in `packaging/win-installer/SlickShot.nsi`): #4955.
- **Closed as expected behavior / duplicate / not planned**: #5045 (intended unload-from-memory behavior), #5018 (empty spam), #5013 (duplicate), #4933 (duplicate), #4926 (Satty courtesy notice), #4880 (intended v14 behavior), #4865 (Imgur docs; Imgur disabled by default), #4852 (expected `Alt+PrtSc` modifier behavior), #4821 (expected CLI final-action behavior), #4805 (closed feature request).
