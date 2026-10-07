#include "color_library.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <string>
#include <vector>

namespace dkcolorlib {
namespace {

std::wstring LibraryPath() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        buffer.data(),
        static_cast<DWORD>(buffer.size()));

    if (length == 0 || length >= buffer.size()) {
        return {};
    }

    std::wstring directory(buffer.data(), length);
    directory += L"\\DKColorPicker";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\colors.ini";
}

std::wstring Hex(COLORREF color) {
    wchar_t buffer[16]{};
    swprintf_s(
        buffer,
        L"#%02X%02X%02X",
        static_cast<unsigned>(GetRValue(color)),
        static_cast<unsigned>(GetGValue(color)),
        static_cast<unsigned>(GetBValue(color)));
    return buffer;
}

int HexDigit(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'A' && ch <= L'F') {
        return 10 + ch - L'A';
    }
    if (ch >= L'a' && ch <= L'f') {
        return 10 + ch - L'a';
    }
    return -1;
}

bool ParseHex(const std::wstring& text, COLORREF* color) {
    if (color == nullptr || text.size() != 7 || text[0] != L'#') {
        return false;
    }

    std::array<int, 6> digits{};
    for (std::size_t index = 0; index < digits.size(); ++index) {
        digits[index] = HexDigit(text[index + 1]);
        if (digits[index] < 0) {
            return false;
        }
    }

    *color = RGB(
        digits[0] * 16 + digits[1],
        digits[2] * 16 + digits[3],
        digits[4] * 16 + digits[5]);
    return true;
}

std::wstring Serialize(const std::vector<COLORREF>& colors) {
    std::wstring result;
    for (std::size_t index = 0; index < colors.size(); ++index) {
        if (index != 0) {
            result += L";";
        }
        result += Hex(colors[index]);
    }
    return result;
}

std::vector<COLORREF> Deserialize(
    const std::wstring& text,
    std::size_t maximum) {
    std::vector<COLORREF> colors;
    std::size_t offset = 0;

    while (offset < text.size() && colors.size() < maximum) {
        const std::size_t separator = text.find(L';', offset);
        const std::size_t end =
            separator == std::wstring::npos ? text.size() : separator;
        const std::wstring token = text.substr(offset, end - offset);

        COLORREF color = RGB(0, 0, 0);
        if (ParseHex(token, &color) &&
            std::find(colors.begin(), colors.end(), color) == colors.end()) {
            colors.push_back(color);
        }

        if (separator == std::wstring::npos) {
            break;
        }
        offset = separator + 1;
    }

    return colors;
}

std::vector<COLORREF> LoadList(
    const wchar_t* section,
    std::size_t maximum) {
    const std::wstring path = LibraryPath();
    if (path.empty()) {
        return {};
    }

    std::array<wchar_t, 4096> buffer{};
    GetPrivateProfileStringW(
        section,
        L"Colors",
        L"",
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        path.c_str());

    return Deserialize(buffer.data(), maximum);
}

void SaveList(
    const wchar_t* section,
    const std::vector<COLORREF>& colors) {
    const std::wstring path = LibraryPath();
    if (path.empty()) {
        return;
    }

    const std::wstring serialized = Serialize(colors);
    WritePrivateProfileStringW(
        section,
        L"Colors",
        serialized.c_str(),
        path.c_str());
}

} // namespace

std::vector<COLORREF> LoadRecentColors() {
    return LoadList(L"Recent", kMaxRecentColors);
}

std::vector<COLORREF> LoadFavoriteColors() {
    return LoadList(L"Favorites", kMaxFavoriteColors);
}

void AddRecentColor(COLORREF color) {
    std::vector<COLORREF> colors = LoadRecentColors();
    colors.erase(
        std::remove(colors.begin(), colors.end(), color),
        colors.end());
    colors.insert(colors.begin(), color);

    if (colors.size() > kMaxRecentColors) {
        colors.resize(kMaxRecentColors);
    }
    SaveList(L"Recent", colors);
}

bool ToggleFavoriteColor(COLORREF color) {
    std::vector<COLORREF> colors = LoadFavoriteColors();
    const auto existing = std::find(colors.begin(), colors.end(), color);

    if (existing != colors.end()) {
        colors.erase(existing);
        SaveList(L"Favorites", colors);
        return false;
    }

    colors.insert(colors.begin(), color);
    if (colors.size() > kMaxFavoriteColors) {
        colors.resize(kMaxFavoriteColors);
    }
    SaveList(L"Favorites", colors);
    return true;
}

bool IsFavoriteColor(COLORREF color) {
    const std::vector<COLORREF> colors = LoadFavoriteColors();
    return std::find(colors.begin(), colors.end(), color) != colors.end();
}

void ClearRecentColors() {
    SaveList(L"Recent", {});
}

} // namespace dkcolorlib
