#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>

namespace {

constexpr wchar_t kAppName[] = L"DK Color Picker";
constexpr wchar_t kMessageClass[] = L"DKColorPicker.MessageWindow";
constexpr wchar_t kOverlayClass[] = L"DKColorPicker.PickerOverlay";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"DKColorPicker";
constexpr wchar_t kMutexName[] = L"Local\\DKColorPicker.SingleInstance";

constexpr UINT kTrayCallback = WM_APP + 1;
constexpr int kHotkeyId = 1;
constexpr UINT kTrayId = 1;

constexpr UINT kMenuPick = 1001;
constexpr UINT kMenuAutoStart = 1002;
constexpr UINT kMenuExit = 1003;
constexpr UINT kMenuZoomBase = 1100;
constexpr UINT kMenuAverageBase = 1200;
constexpr UINT kMenuHotkeyBase = 1300;

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
NOTIFYICONDATAW g_tray{};
HANDLE g_mutex = nullptr;

DesktopCapture g_capture;
Settings g_settings;
POINT g_cursorPoint{0, 0};
bool g_hasCursorPoint = false;
bool g_hotkeyRegistered = false;

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

    WritePrivateProfileStringW(L"Picker", L"Zoom", zoom.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Picker", L"AverageSize", average.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Hotkey", L"Preset", hotkey.c_str(), path.c_str());
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

    if (Contains(kZoomLevels, zoom)) {
        g_settings.zoom = zoom;
    }
    if (Contains(kAverageSizes, average)) {
        g_settings.averageSize = average;
    }
    if (hotkey >= 0 && hotkey < static_cast<int>(kHotkeyPresets.size())) {
        g_settings.hotkeyPreset = hotkey;
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
    g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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

void FinishPicking(HWND hwnd, int x, int y) {
    const COLORREF color = SampleColorAt(x, y);
    const std::wstring text = HexString(color);
    CopyTextToClipboard(hwnd, text);
    CancelPicking();
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
    swprintf_s(
        modeBuffer,
        L"확대 %dx · 평균 %dx%d",
        g_settings.zoom,
        g_settings.averageSize,
        g_settings.averageSize);

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
    DrawTextW(hdc, modeBuffer, -1, &modeRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, RGB(190, 190, 190));
    DrawTextW(
        hdc,
        L"휠/± 확대 · 1/3/5/7/9 평균 · 방향키 이동 · Enter 선택",
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
        MessageBoxW(
            g_messageWindow,
            L"화면을 캡처하지 못했습니다.",
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

    if (menu == nullptr || zoomMenu == nullptr || averageMenu == nullptr || hotkeyMenu == nullptr) {
        if (menu != nullptr) {
            DestroyMenu(menu);
        }
        if (zoomMenu != nullptr) {
            DestroyMenu(zoomMenu);
        }
        if (averageMenu != nullptr) {
            DestroyMenu(averageMenu);
        }
        if (hotkeyMenu != nullptr) {
            DestroyMenu(hotkeyMenu);
        }
        return;
    }

    std::wstring pickLabel = L"색 추출\t";
    pickLabel += CurrentHotkey().label;
    AppendMenuW(menu, MF_STRING, kMenuPick, pickLabel.c_str());
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
            std::to_wstring(kAverageSizes[i]) + L"x" + std::to_wstring(kAverageSizes[i]);
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

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(zoomMenu),
        L"확대 배율");
    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(averageMenu),
        L"평균 추출");
    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(hotkeyMenu),
        L"전역 단축키");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    UINT autoStartFlags = MF_STRING;
    if (IsAutoStartEnabled()) {
        autoStartFlags |= MF_CHECKED;
    }
    AppendMenuW(menu, autoStartFlags, kMenuAutoStart, L"Windows 시작 시 자동 실행");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"종료");

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
        if (!ChangeHotkeyPreset(hwnd, static_cast<int>(selected - kMenuHotkeyBase))) {
            MessageBoxW(
                hwnd,
                L"선택한 전역 단축키를 등록하지 못했습니다.\n다른 프로그램에서 이미 사용 중일 수 있습니다.",
                kAppName,
                MB_OK | MB_ICONWARNING);
        }
        return;
    }

    switch (selected) {
    case kMenuPick:
        StartPicking();
        break;

    case kMenuAutoStart: {
        const bool enable = !IsAutoStartEnabled();
        if (!SetAutoStart(enable)) {
            MessageBoxW(
                hwnd,
                L"자동 시작 설정을 변경하지 못했습니다.",
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
                    MessageBoxW(
                        hwnd,
                        L"저장된 전역 단축키를 등록할 수 없어 Ctrl+Alt+C로 되돌렸습니다.",
                        kAppName,
                        MB_OK | MB_ICONWARNING);
                    return 0;
                }
            }

            MessageBoxW(
                hwnd,
                L"전역 단축키를 등록하지 못했습니다.\n트레이 메뉴에서는 색 추출을 계속 사용할 수 있습니다.",
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
            StartPicking();
        }
        return 0;

    case WM_DESTROY:
        CancelPicking();
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

    if (RegisterClassExW(&messageClass) == 0) {
        return false;
    }

    WNDCLASSEXW overlayClass{};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.hInstance = g_instance;
    overlayClass.lpfnWndProc = OverlayProc;
    overlayClass.lpszClassName = kOverlayClass;
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);

    return RegisterClassExW(&overlayClass) != 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_instance = instance;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    LoadSettings();

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
