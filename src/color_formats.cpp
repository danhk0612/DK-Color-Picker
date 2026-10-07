#include "color_formats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <string>

namespace dkcolor {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;

constexpr std::array<CopyFormat, static_cast<std::size_t>(CopyFormat::Count)>
    kCopyFormatDisplayOrder{
        CopyFormat::Hex,
        CopyFormat::Rgb,
        CopyFormat::Rgba,
        CopyFormat::Hsl,
        CopyFormat::Hsv,
        CopyFormat::Hwb,
        CopyFormat::Cmyk,
        CopyFormat::Lab,
        CopyFormat::Oklch,
        CopyFormat::Custom,
    };

double Clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double SrgbToLinear(double value) {
    return value <= 0.04045
        ? value / 12.92
        : std::pow((value + 0.055) / 1.055, 2.4);
}

double HueFromRgb(double r, double g, double b) {
    const double maximum = std::max({r, g, b});
    const double minimum = std::min({r, g, b});
    const double delta = maximum - minimum;

    if (delta <= 0.0) {
        return 0.0;
    }

    double hue = 0.0;
    if (maximum == r) {
        hue = 60.0 * std::fmod((g - b) / delta, 6.0);
    } else if (maximum == g) {
        hue = 60.0 * (((b - r) / delta) + 2.0);
    } else {
        hue = 60.0 * (((r - g) / delta) + 4.0);
    }

    if (hue < 0.0) {
        hue += 360.0;
    }
    return hue;
}

double LabCurve(double value) {
    constexpr double delta = 6.0 / 29.0;
    constexpr double deltaCubed = delta * delta * delta;

    if (value > deltaCubed) {
        return std::cbrt(value);
    }

    return value / (3.0 * delta * delta) + 4.0 / 29.0;
}

std::wstring FormatDouble(double value, int decimals) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%.*f", decimals, value);
    return buffer;
}

std::wstring HexString(const ColorValues& values) {
    wchar_t buffer[16]{};
    swprintf_s(
        buffer,
        L"#%02X%02X%02X",
        values.r,
        values.g,
        values.b);
    return buffer;
}

std::wstring RgbString(const ColorValues& values) {
    wchar_t buffer[48]{};
    swprintf_s(
        buffer,
        L"rgb(%d, %d, %d)",
        values.r,
        values.g,
        values.b);
    return buffer;
}

std::wstring RgbaString(const ColorValues& values) {
    wchar_t buffer[56]{};
    swprintf_s(
        buffer,
        L"rgba(%d, %d, %d, 1)",
        values.r,
        values.g,
        values.b);
    return buffer;
}

std::wstring HslString(const ColorValues& values) {
    wchar_t buffer[96]{};
    swprintf_s(
        buffer,
        L"hsl(%.1f, %.1f%%, %.1f%%)",
        values.hue,
        values.hslS * 100.0,
        values.hslL * 100.0);
    return buffer;
}

std::wstring HsvString(const ColorValues& values) {
    wchar_t buffer[96]{};
    swprintf_s(
        buffer,
        L"hsv(%.1f, %.1f%%, %.1f%%)",
        values.hue,
        values.hsvS * 100.0,
        values.hsvV * 100.0);
    return buffer;
}

std::wstring HwbString(const ColorValues& values) {
    wchar_t buffer[96]{};
    swprintf_s(
        buffer,
        L"hwb(%.1f, %.1f%%, %.1f%%)",
        values.hue,
        values.hwbW * 100.0,
        values.hwbB * 100.0);
    return buffer;
}

std::wstring CmykString(const ColorValues& values) {
    wchar_t buffer[128]{};
    swprintf_s(
        buffer,
        L"cmyk(%.1f%%, %.1f%%, %.1f%%, %.1f%%)",
        values.cmykC * 100.0,
        values.cmykM * 100.0,
        values.cmykY * 100.0,
        values.cmykK * 100.0);
    return buffer;
}

std::wstring LabString(const ColorValues& values) {
    wchar_t buffer[96]{};
    swprintf_s(
        buffer,
        L"Lab(%.2f, %.2f, %.2f)",
        values.labL,
        values.labA,
        values.labB);
    return buffer;
}

std::wstring OklchString(const ColorValues& values) {
    wchar_t buffer[112]{};
    swprintf_s(
        buffer,
        L"oklch(%.2f%% %.4f %.1f)",
        values.oklchL * 100.0,
        values.oklchC,
        values.oklchH);
    return buffer;
}

void ReplaceAll(
    std::wstring& text,
    const std::wstring& token,
    const std::wstring& replacement) {
    if (token.empty()) {
        return;
    }

    std::size_t offset = 0;
    while ((offset = text.find(token, offset)) != std::wstring::npos) {
        text.replace(offset, token.size(), replacement);
        offset += replacement.size();
    }
}

std::wstring ExpandTemplate(
    const ColorValues& values,
    const std::wstring& customTemplate) {
    std::wstring result = customTemplate.empty()
        ? DefaultCustomTemplate()
        : customTemplate;

    ReplaceAll(result, L"{hex}", HexString(values));
    ReplaceAll(result, L"{rgb}", RgbString(values));
    ReplaceAll(result, L"{rgba}", RgbaString(values));
    ReplaceAll(result, L"{hsl}", HslString(values));
    ReplaceAll(result, L"{hsv}", HsvString(values));
    ReplaceAll(result, L"{hwb}", HwbString(values));
    ReplaceAll(result, L"{cmyk}", CmykString(values));
    ReplaceAll(result, L"{lab}", LabString(values));
    ReplaceAll(result, L"{oklch}", OklchString(values));

    ReplaceAll(result, L"{r}", std::to_wstring(values.r));
    ReplaceAll(result, L"{g}", std::to_wstring(values.g));
    ReplaceAll(result, L"{b}", std::to_wstring(values.b));

    ReplaceAll(result, L"{hsl_h}", FormatDouble(values.hue, 1));
    ReplaceAll(result, L"{hsl_s}", FormatDouble(values.hslS * 100.0, 1));
    ReplaceAll(result, L"{hsl_l}", FormatDouble(values.hslL * 100.0, 1));

    ReplaceAll(result, L"{hsv_h}", FormatDouble(values.hue, 1));
    ReplaceAll(result, L"{hsv_s}", FormatDouble(values.hsvS * 100.0, 1));
    ReplaceAll(result, L"{hsv_v}", FormatDouble(values.hsvV * 100.0, 1));

    ReplaceAll(result, L"{hwb_h}", FormatDouble(values.hue, 1));
    ReplaceAll(result, L"{hwb_w}", FormatDouble(values.hwbW * 100.0, 1));
    ReplaceAll(result, L"{hwb_b}", FormatDouble(values.hwbB * 100.0, 1));

    ReplaceAll(result, L"{cmyk_c}", FormatDouble(values.cmykC * 100.0, 1));
    ReplaceAll(result, L"{cmyk_m}", FormatDouble(values.cmykM * 100.0, 1));
    ReplaceAll(result, L"{cmyk_y}", FormatDouble(values.cmykY * 100.0, 1));
    ReplaceAll(result, L"{cmyk_k}", FormatDouble(values.cmykK * 100.0, 1));

    ReplaceAll(result, L"{lab_l}", FormatDouble(values.labL, 2));
    ReplaceAll(result, L"{lab_a}", FormatDouble(values.labA, 2));
    ReplaceAll(result, L"{lab_b}", FormatDouble(values.labB, 2));

    ReplaceAll(result, L"{oklch_l}", FormatDouble(values.oklchL * 100.0, 2));
    ReplaceAll(result, L"{oklch_c}", FormatDouble(values.oklchC, 4));
    ReplaceAll(result, L"{oklch_h}", FormatDouble(values.oklchH, 1));

    return result;
}

} // namespace

ColorValues ConvertColor(COLORREF color) {
    ColorValues values;
    values.r = static_cast<int>(GetRValue(color));
    values.g = static_cast<int>(GetGValue(color));
    values.b = static_cast<int>(GetBValue(color));

    const double r = values.r / 255.0;
    const double g = values.g / 255.0;
    const double b = values.b / 255.0;

    const double maximum = std::max({r, g, b});
    const double minimum = std::min({r, g, b});
    const double delta = maximum - minimum;

    values.hue = HueFromRgb(r, g, b);

    values.hslL = (maximum + minimum) / 2.0;
    const double hslDenominator = 1.0 - std::abs(2.0 * values.hslL - 1.0);
    values.hslS = delta <= 0.0 || hslDenominator <= 0.0
        ? 0.0
        : delta / hslDenominator;

    values.hsvV = maximum;
    values.hsvS = maximum <= 0.0 ? 0.0 : delta / maximum;

    values.hwbW = minimum;
    values.hwbB = 1.0 - maximum;

    values.cmykK = 1.0 - maximum;
    if (values.cmykK >= 1.0 - 1e-12) {
        values.cmykC = 0.0;
        values.cmykM = 0.0;
        values.cmykY = 0.0;
    } else {
        const double denominator = 1.0 - values.cmykK;
        values.cmykC = Clamp01((1.0 - r - values.cmykK) / denominator);
        values.cmykM = Clamp01((1.0 - g - values.cmykK) / denominator);
        values.cmykY = Clamp01((1.0 - b - values.cmykK) / denominator);
    }

    const double linearR = SrgbToLinear(r);
    const double linearG = SrgbToLinear(g);
    const double linearB = SrgbToLinear(b);

    const double x =
        0.4124564 * linearR +
        0.3575761 * linearG +
        0.1804375 * linearB;
    const double y =
        0.2126729 * linearR +
        0.7151522 * linearG +
        0.0721750 * linearB;
    const double z =
        0.0193339 * linearR +
        0.1191920 * linearG +
        0.9503041 * linearB;

    const double fx = LabCurve(x / 0.95047);
    const double fy = LabCurve(y);
    const double fz = LabCurve(z / 1.08883);

    values.labL = 116.0 * fy - 16.0;
    values.labA = 500.0 * (fx - fy);
    values.labB = 200.0 * (fy - fz);

    if (std::abs(values.labA) < 0.0005) {
        values.labA = 0.0;
    }
    if (std::abs(values.labB) < 0.0005) {
        values.labB = 0.0;
    }

    const double l =
        0.4122214708 * linearR +
        0.5363325363 * linearG +
        0.0514459929 * linearB;
    const double m =
        0.2119034982 * linearR +
        0.6806995451 * linearG +
        0.1073969566 * linearB;
    const double s =
        0.0883024619 * linearR +
        0.2817188376 * linearG +
        0.6299787005 * linearB;

    const double lRoot = std::cbrt(l);
    const double mRoot = std::cbrt(m);
    const double sRoot = std::cbrt(s);

    const double okL =
        0.2104542553 * lRoot +
        0.7936177850 * mRoot -
        0.0040720468 * sRoot;
    const double okA =
        1.9779984951 * lRoot -
        2.4285922050 * mRoot +
        0.4505937099 * sRoot;
    const double okB =
        0.0259040371 * lRoot +
        0.7827717662 * mRoot -
        0.8086757660 * sRoot;

    values.oklchL = Clamp01(okL);
    values.oklchC = std::sqrt(okA * okA + okB * okB);

    if (values.oklchC < 1e-7) {
        values.oklchC = 0.0;
        values.oklchH = 0.0;
    } else {
        values.oklchH = std::atan2(okB, okA) * 180.0 / kPi;
        if (values.oklchH < 0.0) {
            values.oklchH += 360.0;
        }
    }

    return values;
}

const wchar_t* CopyFormatLabel(CopyFormat format) {
    switch (format) {
    case CopyFormat::Hex:
        return L"HEX";
    case CopyFormat::Rgb:
        return L"RGB";
    case CopyFormat::Rgba:
        return L"RGBA";
    case CopyFormat::Hsl:
        return L"HSL";
    case CopyFormat::Hsv:
        return L"HSV";
    case CopyFormat::Hwb:
        return L"HWB";
    case CopyFormat::Cmyk:
        return L"CMYK";
    case CopyFormat::Lab:
        return L"CIELAB";
    case CopyFormat::Oklch:
        return L"OKLCH";
    case CopyFormat::Custom:
        return L"사용자 템플릿";
    case CopyFormat::Count:
        break;
    }

    return L"HEX";
}

CopyFormat CopyFormatAtDisplayIndex(std::size_t index) {
    if (index >= kCopyFormatDisplayOrder.size()) {
        return CopyFormat::Hex;
    }
    return kCopyFormatDisplayOrder[index];
}

std::size_t CopyFormatDisplayIndex(CopyFormat format) {
    const auto found = std::find(
        kCopyFormatDisplayOrder.begin(),
        kCopyFormatDisplayOrder.end(),
        format);

    if (found == kCopyFormatDisplayOrder.end()) {
        return 0;
    }

    return static_cast<std::size_t>(
        std::distance(kCopyFormatDisplayOrder.begin(), found));
}

std::wstring DefaultCustomTemplate() {
    return L"{hex} / {rgb} / {rgba}";
}

std::wstring FormatColor(
    COLORREF color,
    CopyFormat format,
    const std::wstring& customTemplate) {
    const ColorValues values = ConvertColor(color);

    switch (format) {
    case CopyFormat::Hex:
        return HexString(values);
    case CopyFormat::Rgb:
        return RgbString(values);
    case CopyFormat::Rgba:
        return RgbaString(values);
    case CopyFormat::Hsl:
        return HslString(values);
    case CopyFormat::Hsv:
        return HsvString(values);
    case CopyFormat::Hwb:
        return HwbString(values);
    case CopyFormat::Cmyk:
        return CmykString(values);
    case CopyFormat::Lab:
        return LabString(values);
    case CopyFormat::Oklch:
        return OklchString(values);
    case CopyFormat::Custom:
        return ExpandTemplate(values, customTemplate);
    case CopyFormat::Count:
        break;
    }

    return HexString(values);
}

} // namespace dkcolor
