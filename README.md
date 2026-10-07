# DK Color Picker

[English](README.md) | [한국어](README.ko.md)

A lightweight, portable color picker for Windows 10/11.

Press a hotkey, pick a color anywhere on your screen, and the selected color is copied to the clipboard immediately. DK Color Picker stays in the system tray and includes a compact Color Tools window for tones, harmony colors, recent colors, favorites, CSS color names, and WCAG contrast.

## Download

Download the latest release:

**https://github.com/danhk0612/DK-Color-Picker/releases/latest**

1. Download `DKColorPicker-win-x64.zip`.
2. Extract the ZIP anywhere you like.
3. Run `DKColorPicker.exe`.

No installer or separate runtime is required.

## Quick start

1. Run `DKColorPicker.exe`.
2. Press **Ctrl+Alt+C**.
3. Move the cursor to the color you want.
4. Left-click or press **Enter/Space**.
5. The color is copied to the clipboard in the selected format.

The app keeps running in the system tray.

- Double-click the tray icon to open **Color Tools**.
- Right-click the tray icon to change picker settings or exit.
- Closing the Color Tools window hides it; it does not exit the app.

## Picking colors

While the picker is open:

| Action | Control |
| --- | --- |
| Pick color | Left-click / Enter / Space |
| Cancel | Esc / Right-click |
| Move by 1 pixel | Arrow keys |
| Zoom | Mouse wheel / + / - |
| Average area | 1 / 3 / 5 / 7 / 9 |

Available zoom levels:

`8x · 12x · 16x · 24x · 32x · 48x`

Available average areas:

`1x1 · 3x3 · 5x5 · 7x7 · 9x9`

The screen is frozen while picking, so moving the cursor does not change the captured image.

## Copy formats

Supported formats:

`HEX · RGB · RGBA · HSL · HSV · HWB · CMYK · CIELAB · OKLCH · Custom template`

RGBA uses an alpha value of `1` because screen pixels do not contain transparency information.

You can change the format from the tray menu or directly from the radio buttons at the top of Color Tools.

When you change the format in Color Tools:

- the current picked color is copied immediately in the new format;
- all visible color codes in Color Tools switch to that format;
- the same format is used for future picks and swatch clicks.

## Color Tools

Open Color Tools by double-clicking the tray icon or choosing **Open Color Tools** from the tray menu.

### Current color

The large current-color swatch shows the most recently picked screen color.

Click the swatch to copy it immediately.

Click the **☆ / ★** area on the swatch to add or remove it from Favorites.

### Similar CSS code

DK Color Picker compares the current color with all 148 CSS named colors and shows the closest name.

Click **Similar CSS: color-name** to copy the CSS color name itself.

### Tones

Five tone swatches are generated from the current color:

`lighter 50% · lighter 25% · original · darker 25% · darker 50%`

Click a tone to copy it. Tone clicks do not replace the current picked color and are not added to Recent Colors.

### Harmony

Harmony suggestions include:

`complementary · analogous -30° · analogous +30° · triadic -120° · triadic +120°`

Click a harmony color to copy it. Harmony clicks do not replace the current picked color and are not added to Recent Colors.

### Recent colors

Only colors actually picked from the screen are added to Recent Colors.

- Up to 20 unique colors are stored.
- Picking the same color again moves it to the front.
- Clicking a recent color copies it without creating another history entry.
- Use **Clear recent colors** to empty the list.

### Favorites

Use the **☆ / ★** control inside any color swatch to toggle Favorites.

Favorites can be added from the current color, tones, harmony colors, recent colors, or existing favorites.

The Favorites section is hidden when empty and appears automatically when the first favorite is added.

## Compact window behavior

Color Tools automatically adjusts its height to the amount of content.

- Empty Recent/Favorites rows are not reserved.
- Favorites are hidden when there are none.
- Adding the first favorite expands the window automatically.
- Removing the last favorite shrinks it again.
- Clearing recent colors removes unused rows immediately.

The width is kept large enough to display color codes without making the window unnecessarily wide.

## WCAG contrast

Color Tools shows contrast ratios between the current color and white/black.

This is useful when checking text/background combinations for accessibility.

## Tray settings

The tray menu provides:

- **Pick color**
- **Open Color Tools**
- Zoom level
- Average area
- Copy format
- Global hotkey
- **Always open Color Tools after picking**
- **Start with Windows**
- Theme: System / Light / Dark
- Language: Korean / English
- Exit

Default global hotkey: **Ctrl+Alt+C**

Alternative presets are available if that shortcut conflicts with another application.

## Custom copy template

Open:

**Tray → Copy format → Edit custom template**

Default template:

```text
{hex} / {rgb} / {rgba}
```

Whole-format placeholders:

```text
{hex} {rgb} {rgba} {hsl} {hsv} {hwb} {cmyk} {lab} {oklch}
```

Component placeholders:

```text
{r} {g} {b}
{hsl_h} {hsl_s} {hsl_l}
{hsv_h} {hsv_s} {hsv_v}
{hwb_h} {hwb_w} {hwb_b}
{cmyk_c} {cmyk_m} {cmyk_y} {cmyk_k}
{lab_l} {lab_a} {lab_b}
{oklch_l} {oklch_c} {oklch_h}
```

## Settings and data

Settings are stored in:

```text
%LOCALAPPDATA%\DKColorPicker\settings.ini
```

Recent colors and Favorites are stored in:

```text
%LOCALAPPDATA%\DKColorPicker\colors.ini
```

Windows startup uses the current user's `HKCU\...\Run` entry, so administrator rights are not required.

Optional external localization overrides can be placed next to the EXE:

```text
locales\ko.ini
locales\en.ini
```

The app works normally without these files because Korean and English are built in.

## Troubleshooting

### Ctrl+Alt+C does not work

Another application may already be using the shortcut.

Open the tray menu and choose another global hotkey preset.

### I closed the window but the app is still running

This is expected. Closing Color Tools hides the window while DK Color Picker remains in the tray.

Use **Tray → Exit** to quit completely.

### Windows startup stopped working after I moved the EXE

The startup entry stores the current EXE path.

Turn **Start with Windows** off and on again after moving the program.

## Build from source

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 Build Tools or newer
- CMake 3.23 or newer

Build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output:

```text
build\Release\DKColorPicker.exe
```

Create a release ZIP locally:

```powershell
./tools/package.ps1 -ExePath build/Release/DKColorPicker.exe
```

Measure startup and memory usage:

```powershell
./tools/measure.ps1 -ExePath build/Release/DKColorPicker.exe -Samples 5
```

Development status and follow-up work are tracked in [ROADMAP.md](ROADMAP.md).

## Project notes

DK Color Picker is an independent MIT-licensed implementation. It does not use the source code, assets, icons, or UI files of ColorPick or other color-picker applications.

## License

[MIT License](LICENSE)
