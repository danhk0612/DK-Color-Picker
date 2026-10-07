#pragma once

#include <windows.h>

#include <cstddef>
#include <string>

namespace dkcolor {

enum class CopyFormat : int {
    Hex = 0,
    Rgb = 1,
    Hsl = 2,
    Hsv = 3,
    Hwb = 4,
    Cmyk = 5,
    Lab = 6,
    Oklch = 7,
    Custom = 8,
    Rgba = 9,
    Count = 10,
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
CopyFormat CopyFormatAtDisplayIndex(std::size_t index);
std::size_t CopyFormatDisplayIndex(CopyFormat format);
std::wstring DefaultCustomTemplate();

std::wstring FormatColor(
    COLORREF color,
    CopyFormat format,
    const std::wstring& customTemplate);

} // namespace dkcolor
