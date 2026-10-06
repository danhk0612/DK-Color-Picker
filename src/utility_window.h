#pragma once

#include <windows.h>

#include "color_formats.h"
#include "theme.h"

namespace dkcolorui {

using ColorChangedCallback = void (*)(COLORREF color);
using CopyFormatChangedCallback = void (*)(dkcolor::CopyFormat format);

bool RegisterUtilityWindowClass(HINSTANCE instance);

HWND CreateUtilityWindow(
    HINSTANCE instance,
    HWND owner,
    COLORREF color,
    dkcolor::CopyFormat copyFormat,
    dktheme::ThemeMode theme,
    ColorChangedCallback onColorChanged,
    CopyFormatChangedCallback onCopyFormatChanged);

void ShowUtilityWindow(HWND hwnd, COLORREF color);
void SetUtilityWindowColor(HWND hwnd, COLORREF color);
void SetUtilityCopyFormat(HWND hwnd, dkcolor::CopyFormat copyFormat);
void RefreshUtilityWindow(HWND hwnd, dktheme::ThemeMode theme);

} // namespace dkcolorui
