# Fork Ledger

Upstream repo: `flameshot-org/flameshot` (`upstream/master`)
Upstream tip SHA: 1c7a704dab00234a7baf0c938b03c657f361858e
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

## Merged Commits

### Ported
- `88a968c85c` → `38c805bf` (Let the text tool handle Ctrl+Backspace and other word deletion keys)

### Skipped
- `75ac1fdbb5` (rename idle close setting) — Non-Windows option and introduces Flameshot branding in UI.
- `1dfa279786` (Translations update from Hosted Weblate) — Upstream Weblate translations for Flameshot strings.
- `1c7a704dab` (docs: fix dead xdg-desktop-portal-hyprland wiki link) — Linux/Hyprland-specific documentation.

## Open PRs

### Ported
PR #5031 @ c18fc2e1ca: Port (Show quit prompt on screens away from global origin)
PR #4973 @ e893ed05f6: Port (Fix configuration window sizing on high DPI displays)
PR #4975 @ c594ca3e39: Port (Fix configuration window after DPI changes on Windows)
PR #4977 @ 60caeb3343: Port (Fix pinned-image zoom flicker and preserve settled quality)
PR #5050 @ 52a062986e: Port (Ignore full-screen last region with logical virtual-desktop check → ee96a6c5)
PR #5022 @ ce1f1eb183: Port (Single-click window selection with mixed-DPI Windows lookup → ee96a6c5)
PR #4994 @ 4575b1ee06: Port (Add pinShadow config option → ee96a6c5)
PR #4980 @ 2c698c8748: Port (Add optional instant capture global shortcut → ee96a6c5)
PR #4972 @ 3890a245e7: Port (Automatic subdirectories in filename patterns → c530b5e6)
PR #4943 @ ed6d2000d8: Port (Text tool drop shadow option → ee96a6c5)
PR #4662 @ eea69ad5b1: Port (Configurable arrow key move/resize step sizes with edge-safe clamping → a7927d01)
PR #3448 @ cad55467d4: Port (Edit pinned screenshot via context menu with monitor targeting → a7927d01)

### Watch
PR #5047 @ f3a35cb7ce: Watch (Clickable save notification to open folder; supersedes #4691, check Windows notification behavior)
PR #5046 @ 55138ffa33: Watch (Add smart region selection; wait for upstream review)
PR #5020 @ da10631440: Watch (White-backed rectangle tool with colored outline; wait for upstream stabilization)
PR #5003 @ 5da07b273f: Watch (Default external application and Alt+Click quick launch; supersedes #2833)
PR #4970 @ 04830a8887: Watch (Resize objects by dragging handles; large cross-cutting PR still evolving)
PR #4965 @ e96a04d41c: Watch (Multi-display capture following cursor; evaluate against fork's Windows capture path)

### Skipped
PR #5061 @ 6e05c70a01: Skip (Docs for Imgur uploads; Imgur is disabled by default in SlickShot)
PR #5059 @ 5cd3859863: Skip (Already ported in fork commit 93989f09)
PR #5056 @ afaea02611: Skip (Already ported in fork commit 726341d4)
PR #5054 @ 1fe3f641ef: Skip (Linux GNOME / VirtualBox clipboard workaround)
PR #5044 @ 70db43b0dc: Skip (Already fixed in fork commit 791ba09f)
PR #5043 @ 10a0f4458c: Skip (GIF recording; out of scope for SlickShot)
PR #5042 @ 982a9480d2: Skip (Already ported in fork commits 18491acf and 5ea20ef4)
PR #5037 @ 920c88e785: Skip (Wayland-only cursor fix)
PR #5030 @ 495335378c: Skip (Wayland/X11-only cross-monitor selection)
PR #5024 @ 1c2d510091: Skip (Linux i3-only fullscreen fix)
PR #5014 @ b9d57bfccb: Skip (Wayland layer-shell only)
PR #5009 @ 38f009f92f: Skip (Wayland portal pixel crop only)
PR #5006 @ 9a2261971a: Skip (GNOME Wayland clipboard fix)
PR #4999 @ 50fe2f9507: Skip (Safe parts already ported in fork commit 516ca047)
PR #4992 @ b900070073: Skip (Superseded by fork's patch hit-testing in 6d558a0d)
PR #4974 @ 9c4713f0a6: Skip (Already ported in fork commits 96dbf758 and e5df6750)
PR #4971 @ b91845e7c4: Skip (RFC emoji stamp tool)
PR #4959 @ 38795dbc23: Skip (Upstream AI-usage disclosure doc)
PR #4905 @ 8184e7222d: Skip (Plugin architecture; out of scope)
PR #4904 @ e38725da9a: Skip (Draft external action plugin platform; out of scope)
PR #4899 @ a159952a9a: Skip (Pixelate rename; fork already has blur/pixelate UX)
PR #4898 @ aa64291e64: Skip (Reorganizes upstream General tab layout; fork General tab is deliberately diverged)
PR #4892 @ 4cd5e41261: Skip (Already fixed in fork commit 791ba09f)
PR #4890 @ 91650e03e0: Skip (Restructures upstream General tab into bordered boxes; fork General tab is deliberately diverged)
PR #4889 @ 1b468f0e92: Skip (User QSS theming; out of scope)
PR #4877 @ 1d14eb5c21: Skip (Wayland KSystemClipboard only)
PR #4875 @ 007597ab7e: Skip (Wayland monitor selection only)
PR #4874 @ b0fe5b0c62: Skip (Wayland multi-monitor only)
PR #4872 @ 09f6bdf63f: Skip (GNOME Wayland mixed-DPI only)
PR #4866 @ b1e1a9c2af: Skip (Linux AppImage autostart only)
PR #4796 @ 95aa41e1e1: Skip (Side panel manual geometry editor; out of scope)
PR #4787 @ 856f1cd25a: Skip (macOS ICC profile only)
PR #4762 @ 075bc2e718: Skip (Windows ARM64 AppVeyor/CI workflow)
PR #4691 @ a31f9c9dec: Skip (Superseded by #5047)
PR #4684 @ 66271f6471: Skip (Upstream CI workflow cleanup)
PR #4682 @ 91a690d0f0: Skip (KDE Plasma Wayland KWin script)
PR #4677 @ ba3a0a4139: Skip (Place icons tool; out of scope)
PR #4657 @ 9d376c06a3: Skip (Zoom lens annotation tool; out of scope)
PR #4638 @ fbf0295bf9: Skip (Wayland clipboard bytes only)
PR #4626 @ d69a10fc88: Skip (Draft macOS-only fix)
PR #4524 @ def8186e7e: Skip (Custom button colors; fork has its own DESIGN.md styling)
PR #4407 @ ce51ac5555: Skip (Tray capture delay; out of scope)
PR #3969 @ 676e1eaaac: Skip (Drop shadow on capture tools; out of scope)
PR #3905 @ af43299a57: Skip (Favorites highlight color; fork has its own styling)
PR #3902 @ ada29fbdcc: Skip (Eyedropper in color picker; fork already has ColorGrabWidget)
PR #3746 @ 2461e72c69: Skip (Circle count contrast ring config; out of scope)
PR #3721 @ a0083e1646: Skip (Ubuntu 24.04 README)
PR #3672 @ a445b4a037: Skip (WebP quality config; out of scope)
PR #3661 @ 79a2498dc1: Skip (Draft plugin framework; out of scope)
PR #3619 @ 7900ff9e1b: Skip (Screenshot history; fork already has its own history)
PR #3608 @ 4789d858f5: Skip (Ubuntu 22.04 instructions)
PR #3607 @ 70fa133fad: Skip (Linux DE cursor hiding)
PR #3550 @ 043045edb9: Skip (Obsolete; fork fixed capture geometry in 26075092)
PR #3439 @ eddbcca807: Skip (Uses file-scope global state for aspectRatio)
PR #3438 @ 7132708e3b: Skip (Old CLI exit status PR)
PR #3430 @ df3c7cda18: Skip (LockPick drawing tool; out of scope)
PR #3427 @ 65f9ef617a: Skip (Superseded by #4994)
PR #3199 @ 66c8587115: Skip (Flat button layout; fork uses its own DESIGN.md toolbar)
PR #3074 @ 9af54b3d73: Skip (Tesseract OCR; out of scope)
PR #2833 @ c8315d02ba: Skip (Superseded by #5003)
PR #2769 @ 116d65ef1f: Skip (WIP stale PR from 2023)
PR #2238 @ f7eac07bd2: Skip (Stale Linux-only cursor compositing PR with high overlap on capture path and build files)
