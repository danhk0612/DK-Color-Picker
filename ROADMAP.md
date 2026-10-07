# Roadmap

## T01 - Native picker core

Status: implemented on task/t01-core-picker

- Native Win32 tray resident process
- Ctrl+Alt+C global hotkey
- Frozen desktop capture
- Pixel magnifier
- HEX clipboard copy
- Multi-monitor virtual desktop handling
- Per-Monitor DPI Awareness V2
- Auto-start toggle
- Tray exit
- Windows x64 CI build

## T02 - Picker quality

Status: implemented on task/t02-picker-quality

- Configurable magnification: 8x / 12x / 16x / 24x / 32x / 48x
- 1x1, 3x3, 5x5, 7x7, 9x9 averaging
- Average-area visualization in the magnifier
- Keyboard pixel movement
- Enter / Space selection and Escape cancellation
- Mouse wheel and +/- zoom adjustment
- Number-key averaging adjustment
- Robust magnifier placement and sampling at virtual-screen edges
- Configurable global hotkey presets
- Persisted lightweight INI settings in LocalAppData
- Partial invalidation while moving the magnifier to avoid full-desktop repaint on every cursor move

## T03 - Color formats

Status: implemented on task/t03-color-formats

- HEX
- RGB
- HSL / HSV / HWB
- CMYK
- CIELAB using D65 sRGB conversion
- OKLCH using linear sRGB -> Oklab conversion
- User-defined copy template with whole-format and component placeholders
- Native template editor window
- Copy-format selection from the tray menu
- Persisted copy format and custom template

## T04 - Main utility UI

Status: implemented on task/t04-main-ui

- Native Win32 color utility window opened from tray
- Current color card with HEX / RGB / HSL / OKLCH
- Nearest CSS named color across all 148 names using CIELAB distance
- Five tone steps using white/black mixing
- Complementary, analogous and triadic harmony suggestions
- Clickable tone and harmony swatches
- WCAG contrast ratios against white and black
- AA / AAA normal and large-text threshold labels
- Picker selections update the utility window when it is open
- Closing the utility window hides it while the tray process remains resident
- Tray double-click opens the utility window

## T05 - Color library

Status: implemented on task/t05-library-export

- Persistent recent-color library, maximum 20 unique colors
- Duplicate recent colors move to the front
- Persistent favorites, maximum 20 unique colors
- Inline favorite toggles on color swatches
- Copyable current / tone / harmony / recent / favorite swatches
- Clear-recent action
- Lightweight LocalAppData INI persistence without a database

## T06 - Product polish

Status: implemented on task/t06-product-polish

- System / light / dark theme modes
- Korean / English first-party localization
- Optional external localization INI overrides
- Custom application / window / tray icon
- Tagged GitHub release packaging workflow
- Reproducible PowerShell release-package script
- Local startup / Working Set / private-memory measurement script
- Tray option: always open Color Tools after picking
- Copy-format selector directly in Color Tools
- Bidirectional copy-format synchronization between tray and Color Tools

## Follow-up polish

- Visual QA of light/dark controls on real Windows 10/11 desktops
- Tune layout if translated strings are expanded substantially by external locales
- Record representative startup/memory figures on release hardware
- Add automated non-GUI tests for color conversion and color-library modules
