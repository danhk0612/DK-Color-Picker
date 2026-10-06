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

- HEX
- RGB
- HSL / HSV / HWB
- CMYK
- CIELAB
- OKLCH
- User-defined copy template
- Copy-format selection

## T04 - Main utility UI

- Current color card
- Direct color entry
- Nearest CSS named color
- Tone steps
- Harmony suggestions
- WCAG contrast checks

## T05 - Library and export

- Recent history
- Favorites
- CSS variables
- JSON
- Tailwind color object
- GIMP GPL palette

## T06 - Product polish

- Dark / light / system theme
- Korean / English first-party localization
- External localization files
- Custom application icon
- Release packaging
- Startup and memory measurements
