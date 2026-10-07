#pragma once

#include <windows.h>

namespace dktheme {

enum class ThemeMode : int {
    System = 0,
    Light = 1,
    Dark = 2,
};

bool IsDark(ThemeMode mode);
void ApplyWindow(HWND hwnd, ThemeMode mode);
void ApplyControl(HWND hwnd, ThemeMode mode);

COLORREF BackgroundColor(ThemeMode mode);
COLORREF TextColor(ThemeMode mode);
COLORREF MutedTextColor(ThemeMode mode);
COLORREF ControlBackgroundColor(ThemeMode mode);

} // namespace dktheme
