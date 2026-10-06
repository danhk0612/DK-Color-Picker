#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include "color_formats.h"
#include "color_library.h"
#include "localization.h"
#include "theme.h"
#include "utility_window.h"
#include "resource.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>

namespace {

constexpr wchar_t kAppName[] = L"DK Color Picker";
constexpr wchar_t kMessageClass[] = L"DKColorPicker.MessageWindow";
constexpr wchar_t kOverlayClass[] = L"DKColorPicker.PickerOverlay";
constexpr wchar_t kTemplateClass[] = L"DKColorPicker.TemplateEditor";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"DKColorPicker";
constexpr wchar_t kMutexName[] = L"Local\\DKColorPicker.SingleInstance";

constexpr UINT kTrayCallback = WM_APP + 1;
constexpr int kHotkeyId = 1;
constexpr UINT kTrayId = 1;

constexpr UINT kMenuPick = 1001;
constexpr UINT kMenuAutoStart = 1002;
constexpr UINT kMenuExit = 1003;
constexpr UINT kMenuOpenTools = 1004;
constexpr UINT kMenuAlwaysOpenTools = 1005;
constexpr UINT kMenuZoomBase = 1100;
constexpr UINT kMenuAverageBase = 1200;
constexpr UINT kMenuHotkeyBase = 1300;
constexpr UINT kMenuFormatBase = 1400;
constexpr UINT kMenuEditTemplate = 1500;
constexpr UINT kMenuThemeBase = 1600;
constexpr UINT kMenuLanguageBase = 1700;

constexpr int kTemplateEditId = 2001;

constexpr std::array<int, 6> kZoomLevels{8, 12, 16, 24, 32, 48};
constexpr std::array<int, 5> kAverageSizes{1, 3, 5, 7, 9};

struct HotkeyPreset {
    UINT modifiers;
    UINT vk;
    const wchar_t* label;
};

constexpr std::array<HotkeyPreset, 5> kHotkeyPresets{{
    {MOD_CONTROL | MOD_ALT, 'C', L"Ctrl+Alt+C"},
    {MOD_CONTROL | MOD_SHIFT, 'C', L"Ctrl+Shift+C"},
    {MOD_ALT | MOD_SHIFT, 'C', L"Alt+Shift+C"},
    {MOD_CONTROL | MOD_ALT, 'P', L"Ctrl+Alt+P"},
    {0, VK_F8, L"F8"},
}};

struct Settings {
    int zoom = 12;
    int averageSize = 1;
    int hotkeyPreset = 0;
    int copyFormat = static_cast<int>(dkcolor::CopyFormat::Hex);
    std::wstring customTemplate = dkcolor::DefaultCustomTemplate();
    bool alwaysOpenTools = false;
    int themeMode = static_cast<int>(dktheme::ThemeMode::System);
    int language = static_cast<int>(dkl10n::Language::Korean);
};

struct TemplateEditorState {
    HWND edit = nullptr;
    bool accepted = false;
    std::wstring value;
};

struct DesktopCapture {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct MagnifierLayout {
    int left = 0;
    int top = 0;
    int samplePixels = 0;
    int zoom = 0;
    int imageSize = 0;
    int infoHeight = 78;
    int sourceX = 0;
    int sourceY = 0;

    RECT FullRect() const {
        return {left, top, left + imageSize, top + imageSize + infoHeight};
    }

    RECT ImageRect() const {
        return {left, top, left + imageSize, top + imageSize};
    }
};

HINSTANCE g_instance = nullptr;
HWND g_messageWindow = nullptr;
HWND g_overlayWindow = nullptr;
HWND g_utilityWindow = nullptr;
NOTIFYICONDATAW g_tray{};
HANDLE g_mutex = nullptr;

DesktopCapture g_capture;
Settings g_settings;
POINT g_cursorPoint{0, 0};
bool g_hasCursorPoint = false;
bool g_hotkeyRegistered = false;
COLORREF g_currentColor = RGB(59, 130, 246);

template <typename T, std::size_t N>
bool Contains(const std::array<T, N>& values, const T& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::wstring SettingsPath() {
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
    return directory + L"\\settings.ini";
}

void SaveSettings() {
    const std::wstring path = SettingsPath();
    if (path.empty()) {
        return;
    }

    const std::wstring zoom = std::to_wstring(g_settings.zoom);
    const std::wstring average = std::to_wstring(g_settings.averageSize);
    const std::wstring hotkey = std::to_wstring(g_settings.hotkeyPreset);
    const std::wstring copyFormat = std::to_wstring(g_settings.copyFormat);
    const std::wstring alwaysOpenTools = g_settings.alwaysOpenTools ? L"1" : L"0";
    const std::wstring themeMode = std::to_wstring(g_settings.themeMode);
    const std::wstring language = std::to_wstring(g_settings.language);

    WritePrivateProfileStringW(L"Picker", L"Zoom", zoom.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Picker", L"AverageSize", average.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Hotkey", L"Preset", hotkey.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Copy", L"Format", copyFormat.c_str(), path.c_str());
    WritePrivateProfileStringW(
        L"Copy",
        L"Template",
        g_settings.customTemplate.c_str(),
        path.c_str());
    WritePrivateProfileStringW(
        L"Behavior",
        L"AlwaysOpenTools",
        alwaysOpenTools.c_str(),
        path.c_str());
    WritePrivateProfileStringW(
        L"Appearance",
        L"Theme",
        themeMode.c_str(),
        path.c_str());
    WritePrivateProfileStringW(
        L"Appearance",
        L"Language",
        language.c_str(),
        path.c_str());
}

void LoadSettings() {
    const std::wstring path = SettingsPath();
    if (path.empty()) {
        return;
    }

    const int zoom = static_cast<int>(
        GetPrivateProfileIntW(L"Picker", L"Zoom", g_settings.zoom, path.c_str()));
    const int average = static_cast<int>(
        GetPrivateProfileIntW(L"Picker", L"AverageSize", g_settings.averageSize, path.c_str()));
    const int hotkey = static_cast<int>(
        GetPrivateProfileIntW(L"Hotkey", L"Preset", g_settings.hotkeyPreset, path.c_str()));
    const int copyFormat = static_cast<int>(
        GetPrivateProfileIntW(L"Copy", L"Format", g_settings.copyFormat, path.c_str()));
    const int alwaysOpenTools = static_cast<int>(
        GetPrivateProfileIntW(
            L"Behavior",
            L"AlwaysOpenTools",
            g_settings.alwaysOpenTools ? 1 : 0,
            path.c_str()));
    const int themeMode = static_cast<int>(
        GetPrivateProfileIntW(
            L"Appearance",
            L"Theme",
            g_settings.themeMode,
            path.c_str()));
    const int language = static_cast<int>(
        GetPrivateProfileIntW(
            L"Appearance",
            L"Language",
            g_settings.language,
            path.c_str()));

    std::array<wchar_t, 2049> customTemplate{};
    const std::wstring defaultTemplate = dkcolor::DefaultCustomTemplate();
    GetPrivateProfileStringW(
        L"Copy",
        L"Template",
        defaultTemplate.c_str(),
        customTemplate.data(),
        static_cast<DWORD>(customTemplate.size()),
        path.c_str());

    if (Contains(kZoomLevels, zoom)) {
        g_settings.zoom = zoom;
    }
    if (Contains(kAverageSizes, average)) {
        g_settings.averageSize = average;
    }
    if (hotkey >= 0 && hotkey < static_cast<int>(kHotkeyPresets.size())) {
        g_settings.hotkeyPreset = hotkey;
    }
    if (copyFormat >= 0 &&
        copyFormat < static_cast<int>(dkcolor::CopyFormat::Count)) {
        g_settings.copyFormat = copyFormat;
    }
    g_settings.customTemplate = customTemplate.data();
    g_settings.alwaysOpenTools = alwaysOpenTools != 0;
    if (themeMode >= static_cast<int>(dktheme::ThemeMode::System) &&
        themeMode <= static_cast<int>(dktheme::ThemeMode::Dark)) {
        g_settings.themeMode = themeMode;
    }
    if (language >= static_cast<int>(dkl10n::Language::Korean) &&
        language <= static_cast<int>(dkl10n::Language::English)) {
        g_settings.language = language;
    }
}

void ReleaseDesktopCapture() {
    if (g_capture.dc != nullptr && g_capture.oldBitmap != nullptr) {
        SelectObject(g_capture.dc, g_capture.oldBitmap);
    }
    if (g_capture.bitmap != nullptr) {
        DeleteObject(g_capture.bitmap);
    }
    if (g_capture.dc != nullptr) {
        DeleteDC(g_capture.dc);
    }
    g_capture = {};
}

bool CaptureDesktop() {
    ReleaseDesktopCapture();

    g_capture.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_capture.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_capture.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_capture.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (g_capture.width <= 0 || g_capture.height <= 0) {
        return false;
    }

    HDC screen = GetDC(nullptr);
    if (screen == nullptr) {
        return false;
    }

    g_capture.dc = CreateCompatibleDC(screen);
    g_capture.bitmap = CreateCompatibleBitmap(screen, g_capture.width, g_capture.height);

    bool ok = g_capture.dc != nullptr && g_capture.bitmap != nullptr;
    if (ok) {
        g_capture.oldBitmap = SelectObject(g_capture.dc, g_capture.bitmap);
        ok = BitBlt(
            g_capture.dc,
            0,
            0,
            g_capture.width,
            g_capture.height,
            screen,
            g_capture.x,
            g_capture.y,
            SRCCOPY | CAPTUREBLT) != FALSE;
    }

    ReleaseDC(nullptr, screen);

    if (!ok) {
        ReleaseDesktopCapture();
    }
    return ok;
}

std::wstring HexString(COLORREF color) {
    wchar_t buffer[16]{};
    swprintf_s(
        buffer,
        L"#%02X%02X%02X",
        GetRValue(color),
        GetGValue(color),
        GetBValue(color));
    return buffer;
}

std::wstring RgbString(COLORREF color) {
    wchar_t buffer[48]{};
    swprintf_s(
        buffer,
        L"RGB(%u, %u, %u)",
        static_cast<unsigned>(GetRValue(color)),
        static_cast<unsigned>(GetGValue(color)),
        static_cast<unsigned>(GetBValue(color)));
    return buffer;
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) {
        return false;
    }

    EmptyClipboard();

    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    void* target = GlobalLock(memory);
    if (target == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    memcpy(target, text.c_str(), bytes);
    GlobalUnlock(memory);

    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

COLORREF PixelColorAt(int x, int y) {
    if (g_capture.dc == nullptr ||
        x < 0 || y < 0 ||
        x >= g_capture.width || y >= g_capture.height) {
        return CLR_INVALID;
    }

    return GetPixel(g_capture.dc, x, y);
}

COLORREF SampleColorAt(int x, int y) {
    const int half = g_settings.averageSize / 2;

    std::uint64_t red = 0;
    std::uint64_t green = 0;
    std::uint64_t blue = 0;
    std::uint64_t count = 0;

    for (int offsetY = -half; offsetY <= half; ++offsetY) {
        for (int offsetX = -half; offsetX <= half; ++offsetX) {
            const COLORREF color = PixelColorAt(x + offsetX, y + offsetY);
            if (color == CLR_INVALID) {
                continue;
            }

            red += GetRValue(color);
            green += GetGValue(color);
            blue += GetBValue(color);
            ++count;
        }
    }

    if (count == 0) {
        return RGB(0, 0, 0);
    }

    return RGB(
        static_cast<BYTE>((red + count / 2) / count),
        static_cast<BYTE>((green + count / 2) / count),
        static_cast<BYTE>((blue + count / 2) / count));
}

bool IsAutoStartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    std::array<wchar_t, 4096> value{};
    DWORD type = 0;
    DWORD bytes = static_cast<DWORD>(value.size() * sizeof(wchar_t));
    const LONG result = RegQueryValueExW(
        key,
        kRunValue,
        nullptr,
        &type,
        reinterpret_cast<BYTE*>(value.data()),
        &bytes);
    RegCloseKey(key);

    return result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
}

bool SetAutoStart(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kRunKey,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        std::array<wchar_t, 32768> path{};
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size()) {
            RegCloseKey(key);
            return false;
        }

        const std::wstring command = L"\"" + std::wstring(path.data(), length) + L"\"";
        result = RegSetValueExW(
            key,
            kRunValue,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunValue);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

const HotkeyPreset& CurrentHotkey() {
    return kHotkeyPresets[static_cast<std::size_t>(g_settings.hotkeyPreset)];
}

bool RegisterCurrentHotkey(HWND hwnd) {
    const HotkeyPreset& preset = CurrentHotkey();
    g_hotkeyRegistered = RegisterHotKey(
        hwnd,
        kHotkeyId,
        preset.modifiers | MOD_NOREPEAT,
        preset.vk) != FALSE;
    return g_hotkeyRegistered;
}

void UnregisterCurrentHotkey(HWND hwnd) {
    if (g_hotkeyRegistered) {
        UnregisterHotKey(hwnd, kHotkeyId);
        g_hotkeyRegistered = false;
    }
}

bool ChangeHotkeyPreset(HWND hwnd, int presetIndex) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(kHotkeyPresets.size())) {
        return false;
    }
    if (presetIndex == g_settings.hotkeyPreset) {
        return true;
    }

    const int previous = g_settings.hotkeyPreset;
    UnregisterCurrentHotkey(hwnd);

    g_settings.hotkeyPreset = presetIndex;
    if (RegisterCurrentHotkey(hwnd)) {
        SaveSettings();
        return true;
    }

    g_settings.hotkeyPreset = previous;
    RegisterCurrentHotkey(hwnd);
    return false;
}

void RemoveTrayIcon() {
    if (g_tray.cbSize != 0) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
        g_tray = {};
    }
}

bool AddTrayIcon(HWND hwnd) {
    g_tray = {};
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = hwnd;
    g_tray.uID = kTrayId;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = kTrayCallback;
    g_tray.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (g_tray.hIcon == nullptr) {
        g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(g_tray.szTip, kAppName);

    if (Shell_NotifyIconW(NIM_ADD, &g_tray) == FALSE) {
        return false;
    }

    g_tray.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
    return true;
}

int OddAtMost(int value, int maximum) {
    value = std::max(1, std::min(value, maximum));
    if ((value % 2) == 0) {
        --value;
    }
    return std::max(1, value);
}

MagnifierLayout CalculateMagnifierLayout(int cursorX, int cursorY) {
    MagnifierLayout layout;
    layout.zoom = g_settings.zoom;

    constexpr int desiredPixelsWide = 240;
    constexpr int margin = 24;

    int desiredSamples = std::max(
        g_settings.averageSize + 4,
        desiredPixelsWide / std::max(1, layout.zoom));
    if ((desiredSamples % 2) == 0) {
        ++desiredSamples;
    }

    const int maxSamplesX = std::max(1, g_capture.width / std::max(1, layout.zoom));
    const int maxSamplesY = std::max(
        1,
        (g_capture.height - layout.infoHeight) / std::max(1, layout.zoom));
    const int maxSamples = std::max(
        1,
        std::min({maxSamplesX, maxSamplesY, g_capture.width, g_capture.height}));

    layout.samplePixels = OddAtMost(desiredSamples, maxSamples);
    layout.imageSize = layout.samplePixels * layout.zoom;

    const int halfSamples = layout.samplePixels / 2;
    layout.sourceX = std::clamp(
        cursorX - halfSamples,
        0,
        std::max(0, g_capture.width - layout.samplePixels));
    layout.sourceY = std::clamp(
        cursorY - halfSamples,
        0,
        std::max(0, g_capture.height - layout.samplePixels));

    const int fullWidth = layout.imageSize;
    const int fullHeight = layout.imageSize + layout.infoHeight;

    layout.left = cursorX + margin;
    layout.top = cursorY + margin;

    if (layout.left + fullWidth > g_capture.width) {
        layout.left = cursorX - margin - fullWidth;
    }
    if (layout.top + fullHeight > g_capture.height) {
        layout.top = cursorY - margin - fullHeight;
    }

    layout.left = std::clamp(layout.left, 0, std::max(0, g_capture.width - fullWidth));
    layout.top = std::clamp(layout.top, 0, std::max(0, g_capture.height - fullHeight));

    return layout;
}

void InvalidateMagnifier(HWND hwnd, POINT point) {
    if (g_capture.dc == nullptr) {
        return;
    }

    RECT rect = CalculateMagnifierLayout(point.x, point.y).FullRect();
    InflateRect(&rect, 3, 3);
    InvalidateRect(hwnd, &rect, FALSE);
}

void SetCursorPoint(HWND hwnd, POINT point, bool moveSystemCursor) {
    point.x = std::clamp<LONG>(
        point.x,
        0,
        static_cast<LONG>(std::max(0, g_capture.width - 1)));
    point.y = std::clamp<LONG>(
        point.y,
        0,
        static_cast<LONG>(std::max(0, g_capture.height - 1)));

    if (g_hasCursorPoint) {
        InvalidateMagnifier(hwnd, g_cursorPoint);
    }

    g_cursorPoint = point;
    g_hasCursorPoint = true;

    if (moveSystemCursor) {
        SetCursorPos(g_capture.x + point.x, g_capture.y + point.y);
    }

    InvalidateMagnifier(hwnd, g_cursorPoint);
}

void UpdateCursorPointFromSystem(HWND hwnd) {
    POINT screen{};
    if (!GetCursorPos(&screen)) {
        return;
    }

    SetCursorPoint(
        hwnd,
        {screen.x - g_capture.x, screen.y - g_capture.y},
        false);
}

void StepZoom(HWND hwnd, int direction) {
    auto current = std::find(kZoomLevels.begin(), kZoomLevels.end(), g_settings.zoom);
    std::size_t index = current == kZoomLevels.end()
        ? 1
        : static_cast<std::size_t>(std::distance(kZoomLevels.begin(), current));

    if (direction > 0 && index + 1 < kZoomLevels.size()) {
        ++index;
    } else if (direction < 0 && index > 0) {
        --index;
    } else {
        return;
    }

    if (g_hasCursorPoint) {
        InvalidateMagnifier(hwnd, g_cursorPoint);
    }
    g_settings.zoom = kZoomLevels[index];
    SaveSettings();
    if (g_hasCursorPoint) {
        InvalidateMagnifier(hwnd, g_cursorPoint);
    }
}

void SetAverageSize(HWND hwnd, int averageSize) {
    if (!Contains(kAverageSizes, averageSize) || g_settings.averageSize == averageSize) {
        return;
    }

    if (g_hasCursorPoint) {
        InvalidateMagnifier(hwnd, g_cursorPoint);
    }
    g_settings.averageSize = averageSize;
    SaveSettings();
    if (g_hasCursorPoint) {
        InvalidateMagnifier(hwnd, g_cursorPoint);
    }
}


void OnUtilityColorChanged(COLORREF color) {
    g_currentColor = color;
}

void OnUtilityCopyFormatChanged(dkcolor::CopyFormat format) {
    g_settings.copyFormat = static_cast<int>(format);
    SaveSettings();
}

dktheme::ThemeMode CurrentThemeMode() {
    const int value = g_settings.themeMode;
    if (value < static_cast<int>(dktheme::ThemeMode::System) ||
        value > static_cast<int>(dktheme::ThemeMode::Dark)) {
        return dktheme::ThemeMode::System;
    }
    return static_cast<dktheme::ThemeMode>(value);
}

dkl10n::Language CurrentLanguage() {
    return g_settings.language == static_cast<int>(dkl10n::Language::English)
        ? dkl10n::Language::English
        : dkl10n::Language::Korean;
}

std::wstring CopyFormatDisplayName(dkcolor::CopyFormat format) {
    if (format == dkcolor::CopyFormat::Custom) {
        return dkl10n::Text(L"format.custom");
    }
    return dkcolor::CopyFormatLabel(format);
}

void OpenUtilityWindow() {
    if (g_utilityWindow == nullptr || !IsWindow(g_utilityWindow)) {
        g_utilityWindow = dkcolorui::CreateUtilityWindow(
            g_instance,
            g_messageWindow,
            g_currentColor,
            CurrentCopyFormat(),
            CurrentThemeMode(),
            OnUtilityColorChanged,
            OnUtilityCopyFormatChanged);
    }

    if (g_utilityWindow != nullptr) {
        dkcolorui::ShowUtilityWindow(g_utilityWindow, g_currentColor);
    }
}

void CancelPicking() {
    if (g_overlayWindow != nullptr) {
        HWND overlay = g_overlayWindow;
        g_overlayWindow = nullptr;
        ReleaseCapture();
        DestroyWindow(overlay);
    }
    ReleaseDesktopCapture();
    g_hasCursorPoint = false;
}

dkcolor::CopyFormat CurrentCopyFormat() {
    const int value = g_settings.copyFormat;
    if (value < 0 || value >= static_cast<int>(dkcolor::CopyFormat::Count)) {
        return dkcolor::CopyFormat::Hex;
    }
    return static_cast<dkcolor::CopyFormat>(value);
}

void FinishPicking(HWND hwnd, int x, int y) {
    const COLORREF color = SampleColorAt(x, y);
    g_currentColor = color;
    dkcolorlib::AddRecentColor(color);

    if (g_utilityWindow != nullptr && IsWindow(g_utilityWindow)) {
        dkcolorui::SetUtilityWindowColor(g_utilityWindow, color);
    }

    const std::wstring text = dkcolor::FormatColor(
        color,
        CurrentCopyFormat(),
        g_settings.customTemplate);
    CopyTextToClipboard(hwnd, text);

    const bool openTools = g_settings.alwaysOpenTools;
    CancelPicking();

    if (openTools) {
        OpenUtilityWindow();
    }
}

void DrawMagnifier(HDC hdc, int cursorX, int cursorY) {
    if (g_capture.dc == nullptr) {
        return;
    }

    const MagnifierLayout layout = CalculateMagnifierLayout(cursorX, cursorY);

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(
        hdc,
        layout.left,
        layout.top,
        layout.imageSize,
        layout.imageSize,
        g_capture.dc,
        layout.sourceX,
        layout.sourceY,
        layout.samplePixels,
        layout.samplePixels,
        SRCCOPY);

    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
    HGDIOBJ oldPen = SelectObject(hdc, gridPen);
    for (int i = 0; i <= layout.samplePixels; ++i) {
        const int p = i * layout.zoom;
        MoveToEx(hdc, layout.left + p, layout.top, nullptr);
        LineTo(hdc, layout.left + p, layout.top + layout.imageSize);
        MoveToEx(hdc, layout.left, layout.top + p, nullptr);
        LineTo(hdc, layout.left + layout.imageSize, layout.top + p);
    }

    const int cursorCellX = cursorX - layout.sourceX;
    const int cursorCellY = cursorY - layout.sourceY;

    const int averageHalf = g_settings.averageSize / 2;
    RECT averageRect{
        layout.left + (cursorCellX - averageHalf) * layout.zoom,
        layout.top + (cursorCellY - averageHalf) * layout.zoom,
        layout.left + (cursorCellX + averageHalf + 1) * layout.zoom,
        layout.top + (cursorCellY + averageHalf + 1) * layout.zoom,
    };
    RECT clippedAverage{};
    const RECT imageRect = layout.ImageRect();

    if (IntersectRect(&clippedAverage, &averageRect, &imageRect)) {
        HPEN averagePen = CreatePen(PS_SOLID, 3, RGB(255, 213, 64));
        SelectObject(hdc, averagePen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(
            hdc,
            clippedAverage.left,
            clippedAverage.top,
            clippedAverage.right,
            clippedAverage.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(averagePen);
    }

    HPEN centerPen = CreatePen(PS_SOLID, 2, RGB(255, 48, 48));
    SelectObject(hdc, centerPen);
    HGDIOBJ centerBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    const int centerLeft = layout.left + cursorCellX * layout.zoom;
    const int centerTop = layout.top + cursorCellY * layout.zoom;
    Rectangle(
        hdc,
        centerLeft,
        centerTop,
        centerLeft + layout.zoom + 1,
        centerTop + layout.zoom + 1);
    SelectObject(hdc, centerBrush);

    SelectObject(hdc, oldPen);
    DeleteObject(centerPen);
    DeleteObject(gridPen);

    HBRUSH infoBrush = CreateSolidBrush(RGB(25, 25, 25));
    RECT infoRect{
        layout.left,
        layout.top + layout.imageSize,
        layout.left + layout.imageSize,
        layout.top + layout.imageSize + layout.infoHeight};
    FillRect(hdc, &infoRect, infoBrush);
    DeleteObject(infoBrush);

    const COLORREF color = SampleColorAt(cursorX, cursorY);
    HBRUSH swatch = CreateSolidBrush(color);
    RECT swatchRect{
        layout.left + 8,
        layout.top + layout.imageSize + 8,
        layout.left + 42,
        layout.top + layout.imageSize + 48};
    FillRect(hdc, &swatchRect, swatch);
    DeleteObject(swatch);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(245, 245, 245));

    const std::wstring hex = HexString(color);
    const std::wstring rgb = RgbString(color);

    wchar_t modeBuffer[80]{};
    const std::wstring modeFormat = dkl10n::Text(L"picker.mode");
    const std::wstring formatLabel = CopyFormatDisplayName(CurrentCopyFormat());
    swprintf_s(
        modeBuffer,
        modeFormat.c_str(),
        g_settings.zoom,
        g_settings.averageSize,
        g_settings.averageSize,
        formatLabel.c_str());

    RECT hexRect{
        layout.left + 50,
        layout.top + layout.imageSize + 4,
        layout.left + layout.imageSize - 6,
        layout.top + layout.imageSize + 25};
    RECT rgbRect{
        layout.left + 50,
        layout.top + layout.imageSize + 24,
        layout.left + layout.imageSize - 6,
        layout.top + layout.imageSize + 45};
    RECT modeRect{
        layout.left + 50,
        layout.top + layout.imageSize + 44,
        layout.left + layout.imageSize - 6,
        layout.top + layout.imageSize + 65};
    RECT hintRect{
        layout.left + 8,
        layout.top + layout.imageSize + 59,
        layout.left + layout.imageSize - 6,
        layout.top + layout.imageSize + 76};

    DrawTextW(hdc, hex.c_str(), -1, &hexRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, rgb.c_str(), -1, &rgbRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(
        hdc,
        modeBuffer,
        -1,
        &modeRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetTextColor(hdc, RGB(190, 190, 190));
    const std::wstring hintText = dkl10n::Text(L"picker.hint");
    DrawTextW(
        hdc,
        hintText.c_str(),
        -1,
        &hintRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(240, 240, 240));
    oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(
        hdc,
        layout.left,
        layout.top,
        layout.left + layout.imageSize,
        layout.top + layout.imageSize + layout.infoHeight);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_MOUSEMOVE:
        SetCursorPoint(
            hwnd,
            {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
            false);
        return 0;

    case WM_MOUSEWHEEL:
        StepZoom(hwnd, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1);
        return 0;

    case WM_LBUTTONDOWN:
        FinishPicking(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_RBUTTONDOWN:
        CancelPicking();
        return 0;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            CancelPicking();
            return 0;
        case VK_RETURN:
        case VK_SPACE:
            if (g_hasCursorPoint) {
                FinishPicking(hwnd, g_cursorPoint.x, g_cursorPoint.y);
            }
            return 0;
        case VK_LEFT:
            SetCursorPoint(hwnd, {g_cursorPoint.x - 1, g_cursorPoint.y}, true);
            return 0;
        case VK_RIGHT:
            SetCursorPoint(hwnd, {g_cursorPoint.x + 1, g_cursorPoint.y}, true);
            return 0;
        case VK_UP:
            SetCursorPoint(hwnd, {g_cursorPoint.x, g_cursorPoint.y - 1}, true);
            return 0;
        case VK_DOWN:
            SetCursorPoint(hwnd, {g_cursorPoint.x, g_cursorPoint.y + 1}, true);
            return 0;
        case VK_ADD:
        case VK_OEM_PLUS:
            StepZoom(hwnd, 1);
            return 0;
        case VK_SUBTRACT:
        case VK_OEM_MINUS:
            StepZoom(hwnd, -1);
            return 0;
        case '1':
            SetAverageSize(hwnd, 1);
            return 0;
        case '3':
            SetAverageSize(hwnd, 3);
            return 0;
        case '5':
            SetAverageSize(hwnd, 5);
            return 0;
        case '7':
            SetAverageSize(hwnd, 7);
            return 0;
        case '9':
            SetAverageSize(hwnd, 9);
            return 0;
        default:
            return 0;
        }

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_capture.dc != nullptr) {
            const int paintWidth = ps.rcPaint.right - ps.rcPaint.left;
            const int paintHeight = ps.rcPaint.bottom - ps.rcPaint.top;
            BitBlt(
                hdc,
                ps.rcPaint.left,
                ps.rcPaint.top,
                paintWidth,
                paintHeight,
                g_capture.dc,
                ps.rcPaint.left,
                ps.rcPaint.top,
                SRCCOPY);

            if (g_hasCursorPoint) {
                DrawMagnifier(hdc, g_cursorPoint.x, g_cursorPoint.y);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (g_overlayWindow == hwnd) {
            g_overlayWindow = nullptr;
        }
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void StartPicking() {
    if (g_overlayWindow != nullptr) {
        SetForegroundWindow(g_overlayWindow);
        return;
    }

    if (!CaptureDesktop()) {
        const std::wstring message = dkl10n::Text(L"dialog.capture_failed");
        MessageBoxW(
            g_messageWindow,
            message.c_str(),
            kAppName,
            MB_OK | MB_ICONERROR);
        return;
    }

    g_overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kOverlayClass,
        kAppName,
        WS_POPUP,
        g_capture.x,
        g_capture.y,
        g_capture.width,
        g_capture.height,
        nullptr,
        nullptr,
        g_instance,
        nullptr);

    if (g_overlayWindow == nullptr) {
        ReleaseDesktopCapture();
        return;
    }

    ShowWindow(g_overlayWindow, SW_SHOW);
    UpdateWindow(g_overlayWindow);
    SetForegroundWindow(g_overlayWindow);
    SetFocus(g_overlayWindow);
    SetCapture(g_overlayWindow);
    UpdateCursorPointFromSystem(g_overlayWindow);
}


LRESULT CALLBACK TemplateEditorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<TemplateEditorState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = static_cast<TemplateEditorState*>(create->lpCreateParams);
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_CREATE: {
        if (state == nullptr) {
            return -1;
        }

        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        HWND title = CreateWindowExW(
            0,
            L"STATIC",
            dkl10n::Text(L"template.heading").c_str(),
            WS_CHILD | WS_VISIBLE,
            16,
            14,
            560,
            20,
            hwnd,
            nullptr,
            g_instance,
            nullptr);

        state->edit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            state->value.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            16,
            40,
            560,
            26,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTemplateEditId)),
            g_instance,
            nullptr);

        HWND help = CreateWindowExW(
            0,
            L"STATIC",
            L"{hex} {rgb} {hsl} {hsv} {hwb} {cmyk} {lab} {oklch}\r\n"
            (dkl10n::Text(L"template.components") +
             L": {r} {g} {b}, {hsl_h} {hsl_s} {hsl_l}, "
             L"{hsv_h} {hsv_s} {hsv_v}, {hwb_h} {hwb_w} {hwb_b},\r\n"
             L"{cmyk_c} {cmyk_m} {cmyk_y} {cmyk_k}, "
             L"{lab_l} {lab_a} {lab_b}, {oklch_l} {oklch_c} {oklch_h}").c_str(),
            WS_CHILD | WS_VISIBLE,
            16,
            76,
            560,
            66,
            hwnd,
            nullptr,
            g_instance,
            nullptr);

        HWND okButton = CreateWindowExW(
            0,
            L"BUTTON",
            dkl10n::Text(L"template.save").c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            390,
            154,
            88,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
            g_instance,
            nullptr);

        HWND cancelButton = CreateWindowExW(
            0,
            L"BUTTON",
            dkl10n::Text(L"template.cancel").c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            488,
            154,
            88,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
            g_instance,
            nullptr);

        const std::array<HWND, 5> controls{
            title, state->edit, help, okButton, cancelButton};
        for (HWND control : controls) {
            if (control != nullptr) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(font),
                    TRUE);
            }
        }

        if (state->edit != nullptr) {
            SendMessageW(state->edit, EM_SETLIMITTEXT, 2048, 0);
            SetFocus(state->edit);
            SendMessageW(state->edit, EM_SETSEL, 0, -1);
        }
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && state != nullptr && state->edit != nullptr) {
            const int length = GetWindowTextLengthW(state->edit);
            std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
            GetWindowTextW(state->edit, value.data(), length + 1);
            value.resize(static_cast<std::size_t>(length));
            state->value = std::move(value);
            state->accepted = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ShowTemplateEditor(HWND owner) {
    TemplateEditorState state;
    state.value = g_settings.customTemplate;

    constexpr int clientWidth = 592;
    constexpr int clientHeight = 198;
    RECT rect{0, 0, clientWidth, clientHeight};
    AdjustWindowRectEx(
        &rect,
        WS_CAPTION | WS_SYSMENU,
        FALSE,
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT);

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    POINT cursor{};
    GetCursorPos(&cursor);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(monitor, &monitorInfo);

    const RECT work = monitorInfo.rcWork;
    const int x = work.left + ((work.right - work.left) - width) / 2;
    const int y = work.top + ((work.bottom - work.top) - height) / 2;

    HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        kTemplateClass,
        dkl10n::Text(L"template.title").c_str(),
        WS_CAPTION | WS_SYSMENU,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        g_instance,
        &state);

    if (window == nullptr) {
        return false;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);

    bool sawQuit = false;
    int quitCode = 0;
    MSG message{};
    while (IsWindow(window)) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) {
                sawQuit = true;
                quitCode = static_cast<int>(message.wParam);
            }
            break;
        }

        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);

    if (sawQuit) {
        PostQuitMessage(quitCode);
    }

    if (!state.accepted) {
        return false;
    }

    g_settings.customTemplate = state.value;
    SaveSettings();
    return true;
}

void AppendCheckedMenuItem(
    HMENU menu,
    UINT command,
    const wchar_t* label,
    bool checked) {
    AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), command, label);
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    HMENU zoomMenu = CreatePopupMenu();
    HMENU averageMenu = CreatePopupMenu();
    HMENU hotkeyMenu = CreatePopupMenu();
    HMENU formatMenu = CreatePopupMenu();
    HMENU themeMenu = CreatePopupMenu();
    HMENU languageMenu = CreatePopupMenu();

    if (menu == nullptr ||
        zoomMenu == nullptr ||
        averageMenu == nullptr ||
        hotkeyMenu == nullptr ||
        formatMenu == nullptr ||
        themeMenu == nullptr ||
        languageMenu == nullptr) {
        if (menu != nullptr) {
            DestroyMenu(menu);
        } else {
            if (zoomMenu != nullptr) DestroyMenu(zoomMenu);
            if (averageMenu != nullptr) DestroyMenu(averageMenu);
            if (hotkeyMenu != nullptr) DestroyMenu(hotkeyMenu);
            if (formatMenu != nullptr) DestroyMenu(formatMenu);
            if (themeMenu != nullptr) DestroyMenu(themeMenu);
            if (languageMenu != nullptr) DestroyMenu(languageMenu);
        }
        return;
    }

    std::wstring pickLabel = dkl10n::Text(L"tray.pick") + L"\t";
    pickLabel += CurrentHotkey().label;
    AppendMenuW(menu, MF_STRING, kMenuPick, pickLabel.c_str());

    const std::wstring toolsLabel = dkl10n::Text(L"tray.tools");
    AppendMenuW(menu, MF_STRING, kMenuOpenTools, toolsLabel.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    for (std::size_t i = 0; i < kZoomLevels.size(); ++i) {
        const std::wstring label = std::to_wstring(kZoomLevels[i]) + L"x";
        AppendCheckedMenuItem(
            zoomMenu,
            kMenuZoomBase + static_cast<UINT>(i),
            label.c_str(),
            g_settings.zoom == kZoomLevels[i]);
    }

    for (std::size_t i = 0; i < kAverageSizes.size(); ++i) {
        const std::wstring label =
            std::to_wstring(kAverageSizes[i]) + L"x" +
            std::to_wstring(kAverageSizes[i]);
        AppendCheckedMenuItem(
            averageMenu,
            kMenuAverageBase + static_cast<UINT>(i),
            label.c_str(),
            g_settings.averageSize == kAverageSizes[i]);
    }

    for (std::size_t i = 0; i < kHotkeyPresets.size(); ++i) {
        AppendCheckedMenuItem(
            hotkeyMenu,
            kMenuHotkeyBase + static_cast<UINT>(i),
            kHotkeyPresets[i].label,
            g_settings.hotkeyPreset == static_cast<int>(i));
    }

    const UINT formatCount = static_cast<UINT>(dkcolor::CopyFormat::Count);
    for (UINT i = 0; i < formatCount; ++i) {
        const auto format = static_cast<dkcolor::CopyFormat>(i);
        const std::wstring label = CopyFormatDisplayName(format);
        AppendCheckedMenuItem(
            formatMenu,
            kMenuFormatBase + i,
            label.c_str(),
            g_settings.copyFormat == static_cast<int>(i));
    }
    AppendMenuW(formatMenu, MF_SEPARATOR, 0, nullptr);
    const std::wstring templateLabel = dkl10n::Text(L"tray.template_edit");
    AppendMenuW(
        formatMenu,
        MF_STRING,
        kMenuEditTemplate,
        templateLabel.c_str());

    const std::array<std::wstring, 3> themeLabels{
        dkl10n::Text(L"theme.system"),
        dkl10n::Text(L"theme.light"),
        dkl10n::Text(L"theme.dark")};
    for (UINT i = 0; i < themeLabels.size(); ++i) {
        AppendCheckedMenuItem(
            themeMenu,
            kMenuThemeBase + i,
            themeLabels[i].c_str(),
            g_settings.themeMode == static_cast<int>(i));
    }

    const std::array<std::wstring, 2> languageLabels{
        dkl10n::Text(L"language.ko"),
        dkl10n::Text(L"language.en")};
    for (UINT i = 0; i < languageLabels.size(); ++i) {
        AppendCheckedMenuItem(
            languageMenu,
            kMenuLanguageBase + i,
            languageLabels[i].c_str(),
            g_settings.language == static_cast<int>(i));
    }

    const std::wstring zoomLabel = dkl10n::Text(L"tray.zoom");
    const std::wstring averageLabel = dkl10n::Text(L"tray.average");
    const std::wstring copyLabel = dkl10n::Text(L"tray.copy_format");
    const std::wstring hotkeyLabel = dkl10n::Text(L"tray.hotkey");
    const std::wstring themeLabel = dkl10n::Text(L"tray.theme");
    const std::wstring languageLabel = dkl10n::Text(L"tray.language");

    AppendMenuW(
        menu, MF_POPUP, reinterpret_cast<UINT_PTR>(zoomMenu), zoomLabel.c_str());
    AppendMenuW(
        menu, MF_POPUP, reinterpret_cast<UINT_PTR>(averageMenu), averageLabel.c_str());
    AppendMenuW(
        menu, MF_POPUP, reinterpret_cast<UINT_PTR>(formatMenu), copyLabel.c_str());
    AppendMenuW(
        menu, MF_POPUP, reinterpret_cast<UINT_PTR>(hotkeyMenu), hotkeyLabel.c_str());

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    const std::wstring alwaysOpenLabel = dkl10n::Text(L"tray.always_open");
    AppendMenuW(
        menu,
        MF_STRING | (g_settings.alwaysOpenTools ? MF_CHECKED : 0),
        kMenuAlwaysOpenTools,
        alwaysOpenLabel.c_str());

    UINT autoStartFlags = MF_STRING;
    if (IsAutoStartEnabled()) {
        autoStartFlags |= MF_CHECKED;
    }
    const std::wstring autoStartLabel = dkl10n::Text(L"tray.auto_start");
    AppendMenuW(
        menu,
        autoStartFlags,
        kMenuAutoStart,
        autoStartLabel.c_str());

    AppendMenuW(
        menu, MF_POPUP, reinterpret_cast<UINT_PTR>(themeMenu), themeLabel.c_str());
    AppendMenuW(
        menu, MF_POPUP, reinterpret_cast<UINT_PTR>(languageMenu), languageLabel.c_str());

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const std::wstring exitLabel = dkl10n::Text(L"tray.exit");
    AppendMenuW(menu, MF_STRING, kMenuExit, exitLabel.c_str());

    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(hwnd);

    const UINT selected = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        hwnd,
        nullptr);

    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    if (selected >= kMenuZoomBase &&
        selected < kMenuZoomBase + kZoomLevels.size()) {
        g_settings.zoom = kZoomLevels[selected - kMenuZoomBase];
        SaveSettings();
        return;
    }

    if (selected >= kMenuAverageBase &&
        selected < kMenuAverageBase + kAverageSizes.size()) {
        g_settings.averageSize = kAverageSizes[selected - kMenuAverageBase];
        SaveSettings();
        return;
    }

    if (selected >= kMenuHotkeyBase &&
        selected < kMenuHotkeyBase + kHotkeyPresets.size()) {
        if (!ChangeHotkeyPreset(
                hwnd,
                static_cast<int>(selected - kMenuHotkeyBase))) {
            const std::wstring message = dkl10n::Text(L"dialog.hotkey_failed");
            MessageBoxW(
                hwnd,
                message.c_str(),
                kAppName,
                MB_OK | MB_ICONWARNING);
        }
        return;
    }

    if (selected >= kMenuFormatBase &&
        selected < kMenuFormatBase + formatCount) {
        g_settings.copyFormat =
            static_cast<int>(selected - kMenuFormatBase);
        SaveSettings();

        if (g_utilityWindow != nullptr && IsWindow(g_utilityWindow)) {
            dkcolorui::SetUtilityCopyFormat(
                g_utilityWindow,
                CurrentCopyFormat());
        }
        return;
    }

    if (selected >= kMenuThemeBase &&
        selected < kMenuThemeBase + 3) {
        g_settings.themeMode =
            static_cast<int>(selected - kMenuThemeBase);
        SaveSettings();

        if (g_utilityWindow != nullptr && IsWindow(g_utilityWindow)) {
            dkcolorui::RefreshUtilityWindow(
                g_utilityWindow,
                CurrentThemeMode());
        }
        return;
    }

    if (selected >= kMenuLanguageBase &&
        selected < kMenuLanguageBase + 2) {
        g_settings.language =
            static_cast<int>(selected - kMenuLanguageBase);
        dkl10n::SetLanguage(CurrentLanguage());
        SaveSettings();

        if (g_utilityWindow != nullptr && IsWindow(g_utilityWindow)) {
            dkcolorui::RefreshUtilityWindow(
                g_utilityWindow,
                CurrentThemeMode());
        }
        return;
    }

    switch (selected) {
    case kMenuPick:
        StartPicking();
        break;

    case kMenuOpenTools:
        OpenUtilityWindow();
        break;

    case kMenuAlwaysOpenTools:
        g_settings.alwaysOpenTools = !g_settings.alwaysOpenTools;
        SaveSettings();
        break;

    case kMenuEditTemplate:
        ShowTemplateEditor(hwnd);
        break;

    case kMenuAutoStart: {
        const bool enable = !IsAutoStartEnabled();
        if (!SetAutoStart(enable)) {
            const std::wstring message =
                dkl10n::Text(L"dialog.autostart_failed");
            MessageBoxW(
                hwnd,
                message.c_str(),
                kAppName,
                MB_OK | MB_ICONERROR);
        }
        break;
    }

    case kMenuExit:
        DestroyWindow(hwnd);
        break;

    default:
        break;
    }
}

LRESULT CALLBACK MessageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        AddTrayIcon(hwnd);

        if (!RegisterCurrentHotkey(hwnd)) {
            const int requestedPreset = g_settings.hotkeyPreset;
            if (requestedPreset != 0) {
                g_settings.hotkeyPreset = 0;
                if (RegisterCurrentHotkey(hwnd)) {
                    SaveSettings();
                    const std::wstring message =
                        dkl10n::Text(L"dialog.hotkey_fallback");
                    MessageBoxW(
                        hwnd,
                        message.c_str(),
                        kAppName,
                        MB_OK | MB_ICONWARNING);
                    return 0;
                }
            }

            const std::wstring message =
                dkl10n::Text(L"dialog.hotkey_unavailable");
            MessageBoxW(
                hwnd,
                message.c_str(),
                kAppName,
                MB_OK | MB_ICONWARNING);
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            StartPicking();
        }
        return 0;

    case kTrayCallback:
        if (LOWORD(lParam) == WM_RBUTTONUP ||
            LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowTrayMenu(hwnd);
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            OpenUtilityWindow();
        }
        return 0;

    case WM_DESTROY:
        CancelPicking();
        if (g_utilityWindow != nullptr && IsWindow(g_utilityWindow)) {
            DestroyWindow(g_utilityWindow);
            g_utilityWindow = nullptr;
        }
        UnregisterCurrentHotkey(hwnd);
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterWindowClasses() {
    WNDCLASSEXW messageClass{};
    messageClass.cbSize = sizeof(messageClass);
    messageClass.hInstance = g_instance;
    messageClass.lpfnWndProc = MessageProc;
    messageClass.lpszClassName = kMessageClass;
    messageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    messageClass.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    messageClass.hIconSm = messageClass.hIcon;

    if (RegisterClassExW(&messageClass) == 0) {
        return false;
    }

    WNDCLASSEXW overlayClass{};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.hInstance = g_instance;
    overlayClass.lpfnWndProc = OverlayProc;
    overlayClass.lpszClassName = kOverlayClass;
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);

    if (RegisterClassExW(&overlayClass) == 0) {
        return false;
    }

    WNDCLASSEXW templateClass{};
    templateClass.cbSize = sizeof(templateClass);
    templateClass.hInstance = g_instance;
    templateClass.lpfnWndProc = TemplateEditorProc;
    templateClass.lpszClassName = kTemplateClass;
    templateClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    templateClass.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    templateClass.hIconSm = templateClass.hIcon;
    templateClass.hbrBackground =
        reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));

    if (RegisterClassExW(&templateClass) == 0) {
        return false;
    }

    return dkcolorui::RegisterUtilityWindowClass(g_instance);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_instance = instance;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    LoadSettings();
    dkl10n::SetLanguage(CurrentLanguage());

    g_mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (g_mutex == nullptr) {
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return 0;
    }

    if (!RegisterWindowClasses()) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return 1;
    }

    g_messageWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kMessageClass,
        kAppName,
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (g_messageWindow == nullptr) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_mutex != nullptr) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
    }

    return static_cast<int>(message.wParam);
}
