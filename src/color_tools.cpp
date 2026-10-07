#include "color_tools.h"

#include "color_formats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <limits>
#include <string>

namespace dkcolor {
namespace {

constexpr std::array<CssNamedColor, 148> kCssColors{{
    {L"aliceblue", RGB(240, 248, 255)},
    {L"antiquewhite", RGB(250, 235, 215)},
    {L"aqua", RGB(0, 255, 255)},
    {L"aquamarine", RGB(127, 255, 212)},
    {L"azure", RGB(240, 255, 255)},
    {L"beige", RGB(245, 245, 220)},
    {L"bisque", RGB(255, 228, 196)},
    {L"black", RGB(0, 0, 0)},
    {L"blanchedalmond", RGB(255, 235, 205)},
    {L"blue", RGB(0, 0, 255)},
    {L"blueviolet", RGB(138, 43, 226)},
    {L"brown", RGB(165, 42, 42)},
    {L"burlywood", RGB(222, 184, 135)},
    {L"cadetblue", RGB(95, 158, 160)},
    {L"chartreuse", RGB(127, 255, 0)},
    {L"chocolate", RGB(210, 105, 30)},
    {L"coral", RGB(255, 127, 80)},
    {L"cornflowerblue", RGB(100, 149, 237)},
    {L"cornsilk", RGB(255, 248, 220)},
    {L"crimson", RGB(220, 20, 60)},
    {L"cyan", RGB(0, 255, 255)},
    {L"darkblue", RGB(0, 0, 139)},
    {L"darkcyan", RGB(0, 139, 139)},
    {L"darkgoldenrod", RGB(184, 134, 11)},
    {L"darkgray", RGB(169, 169, 169)},
    {L"darkgreen", RGB(0, 100, 0)},
    {L"darkgrey", RGB(169, 169, 169)},
    {L"darkkhaki", RGB(189, 183, 107)},
    {L"darkmagenta", RGB(139, 0, 139)},
    {L"darkolivegreen", RGB(85, 107, 47)},
    {L"darkorange", RGB(255, 140, 0)},
    {L"darkorchid", RGB(153, 50, 204)},
    {L"darkred", RGB(139, 0, 0)},
    {L"darksalmon", RGB(233, 150, 122)},
    {L"darkseagreen", RGB(143, 188, 143)},
    {L"darkslateblue", RGB(72, 61, 139)},
    {L"darkslategray", RGB(47, 79, 79)},
    {L"darkslategrey", RGB(47, 79, 79)},
    {L"darkturquoise", RGB(0, 206, 209)},
    {L"darkviolet", RGB(148, 0, 211)},
    {L"deeppink", RGB(255, 20, 147)},
    {L"deepskyblue", RGB(0, 191, 255)},
    {L"dimgray", RGB(105, 105, 105)},
    {L"dimgrey", RGB(105, 105, 105)},
    {L"dodgerblue", RGB(30, 144, 255)},
    {L"firebrick", RGB(178, 34, 34)},
    {L"floralwhite", RGB(255, 250, 240)},
    {L"forestgreen", RGB(34, 139, 34)},
    {L"fuchsia", RGB(255, 0, 255)},
    {L"gainsboro", RGB(220, 220, 220)},
    {L"ghostwhite", RGB(248, 248, 255)},
    {L"gold", RGB(255, 215, 0)},
    {L"goldenrod", RGB(218, 165, 32)},
    {L"gray", RGB(128, 128, 128)},
    {L"green", RGB(0, 128, 0)},
    {L"greenyellow", RGB(173, 255, 47)},
    {L"grey", RGB(128, 128, 128)},
    {L"honeydew", RGB(240, 255, 240)},
    {L"hotpink", RGB(255, 105, 180)},
    {L"indianred", RGB(205, 92, 92)},
    {L"indigo", RGB(75, 0, 130)},
    {L"ivory", RGB(255, 255, 240)},
    {L"khaki", RGB(240, 230, 140)},
    {L"lavender", RGB(230, 230, 250)},
    {L"lavenderblush", RGB(255, 240, 245)},
    {L"lawngreen", RGB(124, 252, 0)},
    {L"lemonchiffon", RGB(255, 250, 205)},
    {L"lightblue", RGB(173, 216, 230)},
    {L"lightcoral", RGB(240, 128, 128)},
    {L"lightcyan", RGB(224, 255, 255)},
    {L"lightgoldenrodyellow", RGB(250, 250, 210)},
    {L"lightgray", RGB(211, 211, 211)},
    {L"lightgreen", RGB(144, 238, 144)},
    {L"lightgrey", RGB(211, 211, 211)},
    {L"lightpink", RGB(255, 182, 193)},
    {L"lightsalmon", RGB(255, 160, 122)},
    {L"lightseagreen", RGB(32, 178, 170)},
    {L"lightskyblue", RGB(135, 206, 250)},
    {L"lightslategray", RGB(119, 136, 153)},
    {L"lightslategrey", RGB(119, 136, 153)},
    {L"lightsteelblue", RGB(176, 196, 222)},
    {L"lightyellow", RGB(255, 255, 224)},
    {L"lime", RGB(0, 255, 0)},
    {L"limegreen", RGB(50, 205, 50)},
    {L"linen", RGB(250, 240, 230)},
    {L"magenta", RGB(255, 0, 255)},
    {L"maroon", RGB(128, 0, 0)},
    {L"mediumaquamarine", RGB(102, 205, 170)},
    {L"mediumblue", RGB(0, 0, 205)},
    {L"mediumorchid", RGB(186, 85, 211)},
    {L"mediumpurple", RGB(147, 112, 219)},
    {L"mediumseagreen", RGB(60, 179, 113)},
    {L"mediumslateblue", RGB(123, 104, 238)},
    {L"mediumspringgreen", RGB(0, 250, 154)},
    {L"mediumturquoise", RGB(72, 209, 204)},
    {L"mediumvioletred", RGB(199, 21, 133)},
    {L"midnightblue", RGB(25, 25, 112)},
    {L"mintcream", RGB(245, 255, 250)},
    {L"mistyrose", RGB(255, 228, 225)},
    {L"moccasin", RGB(255, 228, 181)},
    {L"navajowhite", RGB(255, 222, 173)},
    {L"navy", RGB(0, 0, 128)},
    {L"oldlace", RGB(253, 245, 230)},
    {L"olive", RGB(128, 128, 0)},
    {L"olivedrab", RGB(107, 142, 35)},
    {L"orange", RGB(255, 165, 0)},
    {L"orangered", RGB(255, 69, 0)},
    {L"orchid", RGB(218, 112, 214)},
    {L"palegoldenrod", RGB(238, 232, 170)},
    {L"palegreen", RGB(152, 251, 152)},
    {L"paleturquoise", RGB(175, 238, 238)},
    {L"palevioletred", RGB(219, 112, 147)},
    {L"papayawhip", RGB(255, 239, 213)},
    {L"peachpuff", RGB(255, 218, 185)},
    {L"peru", RGB(205, 133, 63)},
    {L"pink", RGB(255, 192, 203)},
    {L"plum", RGB(221, 160, 221)},
    {L"powderblue", RGB(176, 224, 230)},
    {L"purple", RGB(128, 0, 128)},
    {L"rebeccapurple", RGB(102, 51, 153)},
    {L"red", RGB(255, 0, 0)},
    {L"rosybrown", RGB(188, 143, 143)},
    {L"royalblue", RGB(65, 105, 225)},
    {L"saddlebrown", RGB(139, 69, 19)},
    {L"salmon", RGB(250, 128, 114)},
    {L"sandybrown", RGB(244, 164, 96)},
    {L"seagreen", RGB(46, 139, 87)},
    {L"seashell", RGB(255, 245, 238)},
    {L"sienna", RGB(160, 82, 45)},
    {L"silver", RGB(192, 192, 192)},
    {L"skyblue", RGB(135, 206, 235)},
    {L"slateblue", RGB(106, 90, 205)},
    {L"slategray", RGB(112, 128, 144)},
    {L"slategrey", RGB(112, 128, 144)},
    {L"snow", RGB(255, 250, 250)},
    {L"springgreen", RGB(0, 255, 127)},
    {L"steelblue", RGB(70, 130, 180)},
    {L"tan", RGB(210, 180, 140)},
    {L"teal", RGB(0, 128, 128)},
    {L"thistle", RGB(216, 191, 216)},
    {L"tomato", RGB(255, 99, 71)},
    {L"turquoise", RGB(64, 224, 208)},
    {L"violet", RGB(238, 130, 238)},
    {L"wheat", RGB(245, 222, 179)},
    {L"white", RGB(255, 255, 255)},
    {L"whitesmoke", RGB(245, 245, 245)},
    {L"yellow", RGB(255, 255, 0)},
    {L"yellowgreen", RGB(154, 205, 50)},
}};

std::wstring Trim(std::wstring value) {
    auto notSpace = [](wchar_t ch) {
        return std::iswspace(ch) == 0;
    };

    const auto first = std::find_if(value.begin(), value.end(), notSpace);
    const auto last = std::find_if(value.rbegin(), value.rend(), notSpace).base();

    if (first >= last) {
        return {};
    }
    return std::wstring(first, last);
}

std::wstring Lower(std::wstring value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

bool IsHexDigit(wchar_t ch) {
    return
        (ch >= L'0' && ch <= L'9') ||
        (ch >= L'a' && ch <= L'f') ||
        (ch >= L'A' && ch <= L'F');
}

int HexValue(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f') {
        return 10 + ch - L'a';
    }
    return 10 + ch - L'A';
}

bool ParseHex(std::wstring value, COLORREF* color) {
    if (!value.empty() && value.front() == L'#') {
        value.erase(value.begin());
    }

    if (value.size() != 3 && value.size() != 6) {
        return false;
    }

    if (!std::all_of(value.begin(), value.end(), IsHexDigit)) {
        return false;
    }

    int red = 0;
    int green = 0;
    int blue = 0;

    if (value.size() == 3) {
        red = HexValue(value[0]) * 17;
        green = HexValue(value[1]) * 17;
        blue = HexValue(value[2]) * 17;
    } else {
        red = HexValue(value[0]) * 16 + HexValue(value[1]);
        green = HexValue(value[2]) * 16 + HexValue(value[3]);
        blue = HexValue(value[4]) * 16 + HexValue(value[5]);
    }

    *color = RGB(red, green, blue);
    return true;
}

bool ParseRgb(const std::wstring& value, COLORREF* color) {
    std::wstring lower = Lower(value);
    if (lower.size() < 5 || lower.rfind(L"rgb(", 0) != 0 || lower.back() != L')') {
        return false;
    }

    lower = lower.substr(4, lower.size() - 5);

    std::array<int, 3> channels{};
    std::size_t offset = 0;

    for (std::size_t index = 0; index < channels.size(); ++index) {
        const std::size_t comma = index + 1 < channels.size()
            ? lower.find(L',', offset)
            : std::wstring::npos;
        const std::size_t end = comma == std::wstring::npos ? lower.size() : comma;
        const std::wstring token = Trim(lower.substr(offset, end - offset));

        if (token.empty() ||
            !std::all_of(token.begin(), token.end(), [](wchar_t ch) {
                return ch >= L'0' && ch <= L'9';
            })) {
            return false;
        }

        try {
            const int channel = std::stoi(token);
            if (channel < 0 || channel > 255) {
                return false;
            }
            channels[index] = channel;
        } catch (...) {
            return false;
        }

        if (comma == std::wstring::npos) {
            offset = lower.size();
        } else {
            offset = comma + 1;
        }
    }

    if (offset != lower.size()) {
        return false;
    }

    *color = RGB(channels[0], channels[1], channels[2]);
    return true;
}

double WrapHue(double hue) {
    hue = std::fmod(hue, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    return hue;
}

double HueToRgb(double p, double q, double t) {
    if (t < 0.0) {
        t += 1.0;
    }
    if (t > 1.0) {
        t -= 1.0;
    }
    if (t < 1.0 / 6.0) {
        return p + (q - p) * 6.0 * t;
    }
    if (t < 1.0 / 2.0) {
        return q;
    }
    if (t < 2.0 / 3.0) {
        return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    }
    return p;
}

COLORREF HslToColor(double hueDegrees, double saturation, double lightness) {
    const double hue = WrapHue(hueDegrees) / 360.0;
    saturation = std::clamp(saturation, 0.0, 1.0);
    lightness = std::clamp(lightness, 0.0, 1.0);

    double r = lightness;
    double g = lightness;
    double b = lightness;

    if (saturation > 0.0) {
        const double q = lightness < 0.5
            ? lightness * (1.0 + saturation)
            : lightness + saturation - lightness * saturation;
        const double p = 2.0 * lightness - q;

        r = HueToRgb(p, q, hue + 1.0 / 3.0);
        g = HueToRgb(p, q, hue);
        b = HueToRgb(p, q, hue - 1.0 / 3.0);
    }

    return RGB(
        static_cast<BYTE>(std::lround(r * 255.0)),
        static_cast<BYTE>(std::lround(g * 255.0)),
        static_cast<BYTE>(std::lround(b * 255.0)));
}

COLORREF Mix(COLORREF color, COLORREF target, double amount) {
    amount = std::clamp(amount, 0.0, 1.0);

    const auto mixChannel = [amount](BYTE first, BYTE second) {
        return static_cast<BYTE>(
            std::lround(first * (1.0 - amount) + second * amount));
    };

    return RGB(
        mixChannel(GetRValue(color), GetRValue(target)),
        mixChannel(GetGValue(color), GetGValue(target)),
        mixChannel(GetBValue(color), GetBValue(target)));
}

double LinearChannel(BYTE channel) {
    const double value = channel / 255.0;
    return value <= 0.04045
        ? value / 12.92
        : std::pow((value + 0.055) / 1.055, 2.4);
}

} // namespace

bool ParseColorText(const std::wstring& text, COLORREF* color) {
    if (color == nullptr) {
        return false;
    }

    const std::wstring value = Trim(text);
    if (value.empty()) {
        return false;
    }

    if (ParseHex(value, color) || ParseRgb(value, color)) {
        return true;
    }

    const std::wstring lower = Lower(value);
    const auto it = std::find_if(
        kCssColors.begin(),
        kCssColors.end(),
        [&lower](const CssNamedColor& named) {
            return lower == named.name;
        });

    if (it == kCssColors.end()) {
        return false;
    }

    *color = it->color;
    return true;
}

CssNamedColor NearestCssNamedColor(COLORREF color) {
    const ColorValues source = ConvertColor(color);

    const CssNamedColor* best = &kCssColors.front();
    double bestDistance = std::numeric_limits<double>::infinity();

    for (const CssNamedColor& candidate : kCssColors) {
        const ColorValues converted = ConvertColor(candidate.color);
        const double deltaL = source.labL - converted.labL;
        const double deltaA = source.labA - converted.labA;
        const double deltaB = source.labB - converted.labB;
        const double distance =
            deltaL * deltaL +
            deltaA * deltaA +
            deltaB * deltaB;

        if (distance < bestDistance) {
            bestDistance = distance;
            best = &candidate;
        }
    }

    return *best;
}

std::array<COLORREF, 5> ToneSteps(COLORREF color) {
    return {
        Mix(color, RGB(255, 255, 255), 0.50),
        Mix(color, RGB(255, 255, 255), 0.25),
        color,
        Mix(color, RGB(0, 0, 0), 0.25),
        Mix(color, RGB(0, 0, 0), 0.50),
    };
}

HarmonySet HarmonyColors(COLORREF color) {
    const ColorValues values = ConvertColor(color);

    const auto shifted = [&](double degrees) {
        return HslToColor(
            values.hue + degrees,
            values.hslS,
            values.hslL);
    };

    HarmonySet result;
    result.complementary = shifted(180.0);
    result.analogous = {
        shifted(-30.0),
        shifted(30.0)};
    result.triadic = {
        shifted(-120.0),
        shifted(120.0)};
    result.splitComplementary = {
        shifted(150.0),
        shifted(210.0)};
    result.square = {
        shifted(90.0),
        shifted(180.0),
        shifted(270.0)};
    return result;
}

double RelativeLuminance(COLORREF color) {
    return
        0.2126 * LinearChannel(GetRValue(color)) +
        0.7152 * LinearChannel(GetGValue(color)) +
        0.0722 * LinearChannel(GetBValue(color));
}

double ContrastRatio(COLORREF first, COLORREF second) {
    const double firstLuminance = RelativeLuminance(first);
    const double secondLuminance = RelativeLuminance(second);
    const double lighter = std::max(firstLuminance, secondLuminance);
    const double darker = std::min(firstLuminance, secondLuminance);
    return (lighter + 0.05) / (darker + 0.05);
}

} // namespace dkcolor
