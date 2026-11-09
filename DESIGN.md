# Design System: SlickShot Black Edition

<!-- impeccable:design-schema 1 -->

## Design Thesis

SlickShot's visual language is **Surgical Dark-Mode Precision**—built to complement the Black Edition logo. The interface rejects dated skeuomorphic chrome, clunky stationery metaphors, and muddy legacy purple palettes in favor of pitch obsidian backgrounds (`#000000` / `#202020` / `#2b2b2b`), razor-sharp glass outlines, pure white tool icons, and vibrant electric azure focus accents (`#0284c7` / `#38bdf8`).

## Color System

### Base Surfaces
- **Canvas / App Root**: `#000000` (Logo & icon canvas) / `#202020` (Theme base surface)
- **Overlay Panels & Toolbars**: `#202020` / `rgba(32, 32, 32, 0.92)` (Fluent dark surface)
- **Dialogs & Settings Cards**: `#2b2b2b` (Sub-surface card)
- **Elevated Controls & Hover States**: `rgba(255, 255, 255, 0.10)`
- **Selection Dimming Mask**: `rgba(0, 0, 0, 0.75)` (190/255 opacity scrim)

### Borders & Glass Rims
- **Subtle Glass Rim**: `rgba(255, 255, 255, 0.12)` (1px boundary highlight)
- **Card Separators**: `rgba(255, 255, 255, 0.08)`
- **Focus & Selection Accent**: `#0284c7` (Electric Azure / Sky 600)
- **Selection Marquee Outline**: Dual-stroke (inner `#0284c7`, outer `rgba(0, 0, 0, 0.7)`) for universal legibility against 100% white and 100% black captured screen content.

### Text & Icons
- **Primary Text & Active Glyphs**: `#f8fafc` / `#ffffff`
- **Secondary / Muted Text**: `#94a3b8` (Slate 400)
- **Subtle / Shortcuts**: `#64748b` (Slate 500)
- **Disabled**: `#808080` (Gray)

### Brand & Markup Accent Palette
- **Primary Accent (`uiColor`)**: `#0284c7` (Electric Azure, RGB `{2, 132, 199}`)
- **Contrast Base (`contrastUiColor`)**: `#121214` (Deep Obsidian Black, RGB `{18, 18, 20}`)
- **Default Drawing Colors**:
  - Primary Draw: `Qt::red`
  - Palette: Red, Green, Blue, Cyan, Magenta, Yellow, White, Black (via ColorPickerWidget)

## Typography

- **UI Sans**: `Segoe UI Variable`, `Segoe UI`, `Inter`, system-ui, -apple-system, sans-serif
- **Data & Readouts (Coordinates, Dimensions, Hex Colors)**: `Cascadia Code`, `Consolas`, `JetBrains Mono`, monospace
- **Scale**:
  - Micro / Badges: `10px` / `11px`, weight `600`, tracking `+0.04em`
  - Standard UI / Controls: `12px` / `13px`, weight `500`
  - Section Headers: `14px` / `16px`, weight `600`, tracking `-0.01em`
  - Window Titles: `18px` / `20px`, weight `700`, tracking `-0.02em`

## Component Styling & Geometry

### 1. Floating In-Place Capture Toolbar
- Buttons: Floating tool buttons with transparent backgrounds over the screenshot/toolbar, 6px rounded corners, and drop shadows.
- States:
  - Default: transparent background, `#ffffff` glyph.
  - Hover: `rgba(255, 255, 255, 0.10)` background, `1px solid rgba(255, 255, 255, 0.08)` border.
  - Pressed: `rgba(255, 255, 255, 0.18)` background.

### 2. Selection Marquee & Dimension Tooltip
- Marquee border: Dual-stroke precision 1px line with high-contrast dual-tone visibility (dark outer boundary + vibrant accent inner stroke, drawn with AA-off for integer pixel crispness).
- Resize handles: Circular handles with high-contrast dark border and accent fill.
- Dimension Tooltip: Floating badge filled with `uiColor` (alpha 200) displaying `W × H px` in Cascadia Code monospace.

### 3. Pixel Magnifier Loupe
- High-precision circular optical loupe with 4px border `#0284c7`.
- Crosshair reticle focusing on the exact hovered pixel.
- Real-time RGB / HEX readout badge at the bottom of the loupe.

### 4. Floating Pinned Screenshot Window
- Frameless floating capture window.
- Drop shadow: Ambient physical drop shadow (`blur: 14px`, `offset: (0, 0)` fitting within window margin).
- Smooth interactive zoom and pan controls.

### 5. Settings / Configuration Dialog
- Deep dark theme with clean tab bar navigation.
- Active tab indicator: Clean `#38bdf8` underline or pill highlight.
- Form controls: Inputs using `QPalette::Base` (`#2b2b2b`) with 1px border (`rgba(255, 255, 255, 0.12)`), focusing to `#0284c7`.
