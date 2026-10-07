#pragma once

#include <windows.h>

#include <array>
#include <string>

namespace dkcolor {

struct CssNamedColor {
    const wchar_t* name;
    COLORREF color;
};

struct HarmonySet {
    COLORREF complementary = RGB(0, 0, 0);
    COLORREF analogousLeft = RGB(0, 0, 0);
    COLORREF analogousRight = RGB(0, 0, 0);
    COLORREF triadicLeft = RGB(0, 0, 0);
    COLORREF triadicRight = RGB(0, 0, 0);
};

bool ParseColorText(const std::wstring& text, COLORREF* color);
CssNamedColor NearestCssNamedColor(COLORREF color);
std::array<COLORREF, 5> ToneSteps(COLORREF color);
HarmonySet HarmonyColors(COLORREF color);
double RelativeLuminance(COLORREF color);
double ContrastRatio(COLORREF first, COLORREF second);

} // namespace dkcolor
