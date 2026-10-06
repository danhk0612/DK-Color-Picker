#pragma once

#include <windows.h>

#include <string>

namespace dkcolor {

enum class CopyFormat : int {
    Hex = 0,
    Rgb,
    Hsl,
    Hsv,
    Hwb,
    Cmyk,
    Lab,
    Oklch,
    Custom,
    Count,
};

struct ColorValues {
    int r = 0;
    int g = 0;
    int b = 0;

    double hue = 0.0;

    double hslS = 0.0;
    double hslL = 0.0;

    double hsvS = 0.0;
    double hsvV = 0.0;

    double hwbW = 0.0;
    double hwbB = 0.0;

    double cmykC = 0.0;
    double cmykM = 0.0;
    double cmykY = 0.0;
    double cmykK = 0.0;

    double labL = 0.0;
    double labA = 0.0;
    double labB = 0.0;

    double oklchL = 0.0;
    double oklchC = 0.0;
    double oklchH = 0.0;
};

ColorValues ConvertColor(COLORREF color);

const wchar_t* CopyFormatLabel(CopyFormat format);
std::wstring DefaultCustomTemplate();

std::wstring FormatColor(
    COLORREF color,
    CopyFormat format,
    const std::wstring& customTemplate);

} // namespace dkcolor
