#include "theme.h"

#include <dwmapi.h>
#include <uxtheme.h>

namespace dktheme {
namespace {

bool SystemUsesDarkApps() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0,
            KEY_QUERY_VALUE,
            &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 1;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LONG result = RegQueryValueExW(
        key,
        L"AppsUseLightTheme",
        nullptr,
        &type,
        reinterpret_cast<BYTE*>(&value),
        &size);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_DWORD) {
        return false;
    }
    return value == 0;
}

} // namespace

bool IsDark(ThemeMode mode) {
    if (mode == ThemeMode::Dark) {
        return true;
    }
    if (mode == ThemeMode::Light) {
        return false;
    }
    return SystemUsesDarkApps();
}

void ApplyWindow(HWND hwnd, ThemeMode mode) {
    if (hwnd == nullptr) {
        return;
    }

    const BOOL dark = IsDark(mode) ? TRUE : FALSE;
    constexpr DWORD kImmersiveDarkMode = 20;
    DwmSetWindowAttribute(
        hwnd,
        kImmersiveDarkMode,
        &dark,
        sizeof(dark));

    SetWindowTheme(
        hwnd,
        dark ? L"DarkMode_Explorer" : L"Explorer",
        nullptr);

    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

void ApplyControl(HWND hwnd, ThemeMode mode) {
    if (hwnd == nullptr) {
        return;
    }

    SetWindowTheme(
        hwnd,
        IsDark(mode) ? L"DarkMode_Explorer" : L"Explorer",
        nullptr);
    InvalidateRect(hwnd, nullptr, TRUE);
}

COLORREF BackgroundColor(ThemeMode mode) {
    return IsDark(mode) ? RGB(32, 32, 32) : GetSysColor(COLOR_WINDOW);
}

COLORREF TextColor(ThemeMode mode) {
    return IsDark(mode) ? RGB(240, 240, 240) : GetSysColor(COLOR_WINDOWTEXT);
}

COLORREF MutedTextColor(ThemeMode mode) {
    return IsDark(mode) ? RGB(175, 175, 175) : GetSysColor(COLOR_GRAYTEXT);
}

COLORREF ControlBackgroundColor(ThemeMode mode) {
    return IsDark(mode) ? RGB(45, 45, 45) : GetSysColor(COLOR_WINDOW);
}

} // namespace dktheme
