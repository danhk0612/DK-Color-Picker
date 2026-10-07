#pragma once

#include <windows.h>

#include <string>

#include "color_formats.h"
#include "theme.h"

namespace dkcolorui {

using CopyColorCallback = void (*)(COLORREF color);
using CopyTextCallback = void (*)(const std::wstring& text);
using CopyFormatChangedCallback = void (*)(dkcolor::CopyFormat format);
using RecentColorsChangedCallback = void (*)();

bool RegisterUtilityWindowClass(HINSTANCE instance);

HWND CreateUtilityWindow(
    HINSTANCE instance,
    HWND owner,
    COLORREF color,
    dkcolor::CopyFormat copyFormat,
    const std::wstring& customTemplate,
    dktheme::ThemeMode theme,
    CopyColorCallback onCopyColor,
    CopyTextCallback onCopyText,
    CopyFormatChangedCallback onCopyFormatChanged,
    RecentColorsChangedCallback onRecentColorsChanged);

void ShowUtilityWindow(HWND hwnd, COLORREF color);
void SetUtilityWindowColor(HWND hwnd, COLORREF color);
void SetUtilityCopyFormat(HWND hwnd, dkcolor::CopyFormat copyFormat);
void SetUtilityCustomTemplate(HWND hwnd, const std::wstring& customTemplate);
void RefreshUtilityWindow(HWND hwnd, dktheme::ThemeMode theme);

} // namespace dkcolorui
