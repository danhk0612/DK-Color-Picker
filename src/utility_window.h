#pragma once

#include <windows.h>

#include <string>

#include "color_formats.h"
#include "theme.h"

namespace dkcolorui {

using ColorChangedCallback = void (*)(COLORREF color);
using CopyColorCallback = void (*)(COLORREF color);
using CopyFormatChangedCallback = void (*)(dkcolor::CopyFormat format);

bool RegisterUtilityWindowClass(HINSTANCE instance);

HWND CreateUtilityWindow(
    HINSTANCE instance,
    HWND owner,
    COLORREF color,
    dkcolor::CopyFormat copyFormat,
    const std::wstring& customTemplate,
    dktheme::ThemeMode theme,
    ColorChangedCallback onColorChanged,
    CopyColorCallback onCopyColor,
    CopyFormatChangedCallback onCopyFormatChanged);

void ShowUtilityWindow(HWND hwnd, COLORREF color);
void SetUtilityWindowColor(HWND hwnd, COLORREF color);
void SetUtilityCopyFormat(HWND hwnd, dkcolor::CopyFormat copyFormat);
void SetUtilityCustomTemplate(HWND hwnd, const std::wstring& customTemplate);
void RefreshUtilityWindow(HWND hwnd, dktheme::ThemeMode theme);

} // namespace dkcolorui
