#!/usr/bin/env python3
"""
SlickShot Black Background Logo & Asset Generator
Symbol: Modern aerodynamic precision shears with an optical aperture core on solid black.
Generates:
1. Master scalable SVG (Clean, bold, non-letter screen capture shears on solid black squircle)
2. Monochrome Tray SVG & Mask SVG (Solid white glyph with aperture for system tray / menubar)
3. High-resolution PNGs at all required resolutions (1024, 128, 48, 36)
4. Multi-resolution Windows .ico (16, 24, 32, 48, 64, 128, 256)
"""

import os
import subprocess
from PIL import Image

# 1. Master Application Icon SVG (Solid Black Squircle Base with White Shears)
APP_ICON_SVG = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <!-- Clean Minimal Black Squircle Base with Subtle Glass Rim -->
  <rect x="32" y="32" width="448" height="448" rx="112" fill="#000000" stroke="#ffffff" stroke-opacity="0.16" stroke-width="2"/>

  <!-- Two Crossing Kinetic Precision Shears -->
  <g fill="#ffffff">
    <!-- Blade 1: Top-Right to Bottom-Left -->
    <path d="M 364 120
             C 380 136 380 162 364 178
             L 178 364
             C 162 380 136 380 120 364
             C 104 348 104 322 120 306
             L 306 120
             C 322 104 348 104 364 120 Z"/>

    <!-- Blade 2: Top-Left to Bottom-Right -->
    <path d="M 148 120
             C 164 104 190 104 206 120
             L 392 306
             C 408 322 408 348 392 364
             C 376 380 350 380 334 364
             L 148 178
             C 132 162 132 136 148 120 Z"/>
  </g>

  <!-- Center Pivot / Optical Aperture Core -->
  <circle cx="256" cy="256" r="44" fill="#000000"/>
  <circle cx="256" cy="256" r="24" fill="#ffffff"/>
</svg>'''

# 2. Monochrome Tray Icon SVG (Solid white with aperture ring for system tray)
TRAY_MONOCHROME_SVG = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <clipPath id="tray-clip" clipPathUnits="userSpaceOnUse">
      <path clip-rule="evenodd" fill-rule="evenodd" d="M 0 0 H 512 V 512 H 0 Z M 256 208 A 48 48 0 1 0 256 304 A 48 48 0 1 0 256 208 Z"/>
    </clipPath>
  </defs>
  <g fill="#ffffff" clip-path="url(#tray-clip)">
    <path d="M 384 96 C 404 116 404 148 384 168 L 168 384 C 148 404 116 404 96 384 C 76 364 76 332 96 312 L 312 96 C 332 76 364 76 384 96 Z"/>
    <path d="M 128 96 C 148 76 180 76 200 96 L 416 312 C 436 332 436 364 416 384 C 396 404 364 404 344 384 L 128 168 C 108 148 108 116 128 96 Z"/>
  </g>
  <circle cx="256" cy="256" r="24" fill="#ffffff"/>
</svg>'''

# 3. macOS Mask SVG (Black glyph stencil for template image)
TRAY_MASK_SVG = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <clipPath id="mask-clip" clipPathUnits="userSpaceOnUse">
      <path clip-rule="evenodd" fill-rule="evenodd" d="M 0 0 H 512 V 512 H 0 Z M 256 208 A 48 48 0 1 0 256 304 A 48 48 0 1 0 256 208 Z"/>
    </clipPath>
  </defs>
  <g fill="#000000" clip-path="url(#mask-clip)">
    <path d="M 384 96 C 404 116 404 148 384 168 L 168 384 C 148 404 116 404 96 384 C 76 364 76 332 96 312 L 312 96 C 332 76 364 76 384 96 Z"/>
    <path d="M 128 96 C 148 76 180 76 200 96 L 416 312 C 436 332 436 364 416 384 C 396 404 364 404 344 384 L 128 168 C 108 148 108 116 128 96 Z"/>
  </g>
  <circle cx="256" cy="256" r="24" fill="#000000"/>
</svg>'''

def render_svg_to_png(svg_content, out_png_path, size=1024):
    """Render an SVG string to a PNG using headless Chrome."""
    temp_svg = out_png_path + ".temp.svg"
    temp_html = out_png_path + ".temp.html"
    with open(temp_svg, "w", encoding="utf-8") as f:
        f.write(svg_content)

    html_content = f'''<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * {{ margin: 0; padding: 0; box-sizing: border-box; }}
  html, body {{
    width: {size}px;
    height: {size}px;
    background: transparent;
    overflow: hidden;
  }}
  img {{
    width: {size}px;
    height: {size}px;
    display: block;
  }}
</style>
</head>
<body>
  <img src="file:///{os.path.abspath(temp_svg).replace(os.sep, '/')}" />
</body>
</html>'''

    with open(temp_html, "w", encoding="utf-8") as f:
        f.write(html_content)

    chrome = os.environ.get("CHROME_BIN") or os.environ.get("CHROME_PATH")
    if not chrome:
        candidates = [
            r"C:\Program Files\Google\Chrome\Application\chrome.exe",
            r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
            "/usr/bin/google-chrome",
            "/usr/bin/chromium-browser",
            "/usr/bin/chromium",
        ]
        for c in candidates:
            if os.path.exists(c):
                chrome = c
                break
    if not chrome:
        chrome = "chrome"
    abs_out = os.path.abspath(out_png_path)
    abs_html = "file:///" + os.path.abspath(temp_html).replace(os.sep, "/")

    cmd = [
        chrome,
        "--headless=new",
        "--disable-gpu",
        "--hide-scrollbars",
        "--force-device-scale-factor=1",
        f"--screenshot={abs_out}",
        f"--window-size={size},{size}",
        "--default-background-color=00000000",
        abs_html
    ]

    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    if os.path.exists(temp_svg):
        os.remove(temp_svg)
    if os.path.exists(temp_html):
        os.remove(temp_html)

def main():
    print("Step 1: Writing vector SVG source files...")
    # 1. Update assets/icon.svg (for README and branding)
    with open("assets/icon.svg", "w", encoding="utf-8") as f:
        f.write(APP_ICON_SVG)
    print("  -> assets/icon.svg written")

    # 2. Update data/img/app SVGs
    for svg_name in ["flameshot.svg", "org.flameshot.Flameshot.svg", "slickshot.svg"]:
        with open(f"data/img/app/{svg_name}", "w", encoding="utf-8") as f:
            f.write(APP_ICON_SVG)
    with open("data/img/app/flameshot.monochrome.svg", "w", encoding="utf-8") as f:
        f.write(TRAY_MONOCHROME_SVG)
    with open("data/img/app/slickshot.monochrome.svg", "w", encoding="utf-8") as f:
        f.write(TRAY_MONOCHROME_SVG)
    for svg_name in ["flameshot.mask.svg", "slickshot.mask.svg"]:
        with open(f"data/img/app/{svg_name}", "w", encoding="utf-8") as f:
            f.write(TRAY_MASK_SVG)
    print("  -> data/img/app/ SVGs written")

    # 3. Update data/img/hicolor scalable SVGs
    for svg_p in [
        "data/img/hicolor/scalable/apps/flameshot.svg",
        "data/img/hicolor/scalable/apps/org.flameshot.Flameshot.svg",
    ]:
        with open(svg_p, "w", encoding="utf-8") as f:
            f.write(APP_ICON_SVG)
    print("  -> data/img/hicolor/scalable/apps/ SVGs written")

    print("\nStep 2: Rendering Master 1024x1024 App Icon PNG...")
    master_png = "data/img/app/org.flameshot.Flameshot-1024.png"
    render_svg_to_png(APP_ICON_SVG, master_png, size=1024)
    print(f"  -> {master_png} rendered (1024x1024)")

    master_im = Image.open(master_png)

    # 4. Generate all required PNG sizes
    png_targets = [
        ("data/img/app/flameshot.png", 128),
        ("data/img/app/org.flameshot.Flameshot.png", 128),
        ("data/img/app/slickshot.png", 128),
        ("data/img/hicolor/128x128/apps/flameshot.png", 128),
        ("data/img/hicolor/128x128/apps/org.flameshot.Flameshot.png", 128),
        ("data/img/hicolor/48x48/apps/flameshot.png", 48),
        ("data/img/hicolor/48x48/apps/org.flameshot.Flameshot.png", 48),
    ]

    for path, sz in png_targets:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        resized = master_im.resize((sz, sz), Image.Resampling.LANCZOS)
        resized.save(path, "PNG")
        print(f"  -> {path} ({sz}x{sz}) saved")

    print("\nStep 3: Rendering Tray Monochrome PNGs...")
    mono_1024 = "data/img/app/flameshot.monochrome-1024.png"
    render_svg_to_png(TRAY_MONOCHROME_SVG, mono_1024, size=1024)
    mono_im = Image.open(mono_1024)

    mono_im.resize((48, 48), Image.Resampling.LANCZOS).save("data/img/app/flameshot.monochrome.png", "PNG")
    mono_im.resize((48, 48), Image.Resampling.LANCZOS).save("data/img/app/slickshot.monochrome.png", "PNG")
    print("  -> Tray monochrome PNGs (48x48) saved")

    print("\nStep 4: Rendering macOS Tray Mask PNG...")
    mask_temp = "data/img/app/flameshot.mask.temp.png"
    render_svg_to_png(TRAY_MASK_SVG, mask_temp, size=1024)
    mask_im = Image.open(mask_temp)
    mask_im.resize((36, 36), Image.Resampling.LANCZOS).save("data/img/app/flameshot.mask.png", "PNG")
    mask_im.resize((36, 36), Image.Resampling.LANCZOS).save("data/img/app/slickshot.mask.png", "PNG")
    if os.path.exists(mask_temp):
        os.remove(mask_temp)
    print("  -> macOS Tray Mask PNGs (36x36) saved")

    print("\nStep 5: Generating Multi-Resolution Windows .ico...")
    ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    for ico_name in ["flameshot.ico", "slickshot.ico"]:
        ico_p = f"data/img/app/{ico_name}"
        master_im.save(ico_p, format="ICO", sizes=ico_sizes)
        print(f"  -> {ico_p} saved with sizes: {ico_sizes}")

    print("\nAll assets successfully generated!")

if __name__ == "__main__":
    main()
