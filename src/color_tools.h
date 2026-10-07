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
    std::array<COLORREF, 2> analogous{
        RGB(0, 0, 0),
        RGB(0, 0, 0)};
    std::array<COLORREF, 2> triadic{
        RGB(0, 0, 0),
        RGB(0, 0, 0)};
    std::array<COLORREF, 2> splitComplementary{
        RGB(0, 0, 0),
        RGB(0, 0, 0)};
    std::array<COLORREF, 3> square{
        RGB(0, 0, 0),
        RGB(0, 0, 0),
        RGB(0, 0, 0)};
};

bool ParseColorText(const std::wstring& text, COLORREF* color);
CssNamedColor NearestCssNamedColor(COLORREF color);
std::array<COLORREF, 5> ToneSteps(COLORREF color);
HarmonySet HarmonyColors(COLORREF color);
double RelativeLuminance(COLORREF color);
double ContrastRatio(COLORREF first, COLORREF second);

} // namespace dkcolor
