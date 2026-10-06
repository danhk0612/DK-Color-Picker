#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
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
constexpr UINT kStartPickMessage = WM_APP + 2;
constexpr int kHotkeyId = 1;
constexpr UINT kTrayId = 1;

constexpr UINT kMenuPick = 1001;
constexpr UINT kMenuAutoStart = 1002;
constexpr UINT kMenuExit = 1003;

HINSTANCE g_instance = nullptr;
HWND g_messageWindow = nullptr;
HWND g_overlayWindow = nullptr;
NOTIFYICONDATAW g_tray{};
HANDLE g_mutex = nullptr;

struct DesktopCapture {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

DesktopCapture g_capture;
POINT g_cursorPoint{0, 0};
bool g_hasCursorPoint = false;

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

COLORREF SampleColorAt(int x, int y) {
    if (g_capture.dc == nullptr ||
        x < 0 || y < 0 ||
        x >= g_capture.width || y >= g_capture.height) {
        return RGB(0, 0, 0);
    }

    const COLORREF color = GetPixel(g_capture.dc, x, y);
    return color == CLR_INVALID ? RGB(0, 0, 0) : color;
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
    return Shell_NotifyIconW(NIM_ADD, &g_tray) != FALSE;
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

    constexpr int samplePixels = 15;
    constexpr int scale = 12;
    constexpr int magnifierSize = samplePixels * scale;
    constexpr int infoHeight = 54;
    constexpr int margin = 24;

    int left = cursorX + margin;
    int top = cursorY + margin;

    if (left + magnifierSize > g_capture.width) {
        left = cursorX - margin - magnifierSize;
    }
    if (top + magnifierSize + infoHeight > g_capture.height) {
        top = cursorY - margin - magnifierSize - infoHeight;
    }

    left = std::clamp(left, 0, std::max(0, g_capture.width - magnifierSize));
    top = std::clamp(top, 0, std::max(0, g_capture.height - magnifierSize - infoHeight));

    const int half = samplePixels / 2;
    const int sourceX = std::clamp(cursorX - half, 0, std::max(0, g_capture.width - samplePixels));
    const int sourceY = std::clamp(cursorY - half, 0, std::max(0, g_capture.height - samplePixels));

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(
        hdc,
        left,
        top,
        magnifierSize,
        magnifierSize,
        g_capture.dc,
        sourceX,
        sourceY,
        samplePixels,
        samplePixels,
        SRCCOPY);

    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(115, 115, 115));
    HGDIOBJ oldPen = SelectObject(hdc, gridPen);
    for (int i = 0; i <= samplePixels; ++i) {
        const int p = i * scale;
        MoveToEx(hdc, left + p, top, nullptr);
        LineTo(hdc, left + p, top + magnifierSize);
        MoveToEx(hdc, left, top + p, nullptr);
        LineTo(hdc, left + magnifierSize, top + p);
    }

    HPEN centerPen = CreatePen(PS_SOLID, 2, RGB(255, 48, 48));
    SelectObject(hdc, centerPen);
    const int centerLeft = left + half * scale;
    const int centerTop = top + half * scale;
    Rectangle(
        hdc,
        centerLeft,
        centerTop,
        centerLeft + scale + 1,
        centerTop + scale + 1);

    SelectObject(hdc, oldPen);
    DeleteObject(centerPen);
    DeleteObject(gridPen);

    HBRUSH infoBrush = CreateSolidBrush(RGB(25, 25, 25));
    RECT infoRect{left, top + magnifierSize, left + magnifierSize, top + magnifierSize + infoHeight};
    FillRect(hdc, &infoRect, infoBrush);
    DeleteObject(infoBrush);

    const COLORREF color = SampleColorAt(cursorX, cursorY);
    HBRUSH swatch = CreateSolidBrush(color);
    RECT swatchRect{left + 8, top + magnifierSize + 8, left + 42, top + magnifierSize + 46};
    FillRect(hdc, &swatchRect, swatch);
    DeleteObject(swatch);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(245, 245, 245));

    const std::wstring hex = HexString(color);
    const std::wstring rgb = RgbString(color);

    RECT hexRect{left + 50, top + magnifierSize + 6, left + magnifierSize - 6, top + magnifierSize + 27};
    RECT rgbRect{left + 50, top + magnifierSize + 27, left + magnifierSize - 6, top + magnifierSize + 49};
    DrawTextW(hdc, hex.c_str(), -1, &hexRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, rgb.c_str(), -1, &rgbRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(240, 240, 240));
    oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, left, top, left + magnifierSize, top + magnifierSize + infoHeight);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_MOUSEMOVE:
        g_cursorPoint = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        g_hasCursorPoint = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
        FinishPicking(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_RBUTTONDOWN:
        CancelPicking();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            CancelPicking();
        }
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_capture.dc != nullptr) {
            BitBlt(
                hdc,
                0,
                0,
                g_capture.width,
                g_capture.height,
                g_capture.dc,
                0,
                0,
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
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    AppendMenuW(menu, MF_STRING, kMenuPick, L"색 추출\tCtrl+Alt+C");
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
        if (!RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'C')) {
            MessageBoxW(
                hwnd,
                L"Ctrl+Alt+C 전역 단축키를 등록하지 못했습니다.\n트레이 메뉴에서는 색 추출을 계속 사용할 수 있습니다.",
                kAppName,
                MB_OK | MB_ICONWARNING);
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            StartPicking();
        }
        return 0;

    case kStartPickMessage:
        StartPicking();
        return 0;

    case kTrayCallback:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowTrayMenu(hwnd);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            StartPicking();
        }
        return 0;

    case WM_DESTROY:
        CancelPicking();
        UnregisterHotKey(hwnd, kHotkeyId);
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
