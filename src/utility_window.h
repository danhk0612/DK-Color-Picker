#pragma once

#include <windows.h>

namespace dkcolorui {

using ColorChangedCallback = void (*)(COLORREF color);

bool RegisterUtilityWindowClass(HINSTANCE instance);

HWND CreateUtilityWindow(
    HINSTANCE instance,
    HWND owner,
    COLORREF color,
    ColorChangedCallback onColorChanged);

void ShowUtilityWindow(HWND hwnd, COLORREF color);
void SetUtilityWindowColor(HWND hwnd, COLORREF color);

} // namespace dkcolorui
