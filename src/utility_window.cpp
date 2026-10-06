#include "utility_window.h"

#include <windowsx.h>

#include "color_formats.h"
#include "color_tools.h"

#include <array>
#include <cwchar>
#include <string>

namespace dkcolorui {
namespace {

constexpr wchar_t kUtilityClass[] = L"DKColorPicker.UtilityWindow";
constexpr int kInputId = 3001;
constexpr int kApplyId = 3002;

struct UtilityState {
    HWND input = nullptr;
    HWND apply = nullptr;
    COLORREF color = RGB(59, 130, 246);
    ColorChangedCallback onColorChanged = nullptr;
    std::array<RECT, 5> toneRects{};
    std::array<RECT, 5> harmonyRects{};
    std::array<COLORREF, 5> tones{};
    std::array<COLORREF, 5> harmonies{};
};

int Scale(HWND hwnd, int value) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(hwnd)), 96);
}

std::wstring Hex(COLORREF color) {
    return dkcolor::FormatColor(color, dkcolor::CopyFormat::Hex, L"");
}

std::wstring ContrastGrade(double ratio) {
    if (ratio >= 7.0) {
        return L"AA/AAA 일반·큰 글자 통과";
    }
    if (ratio >= 4.5) {
        return L"AA 일반, AA/AAA 큰 글자 통과";
    }
    if (ratio >= 3.0) {
        return L"AA 큰 글자 통과";
    }
    return L"WCAG AA 대비 기준 미달";
}

void LayoutControls(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    const int margin = Scale(hwnd, 20);
    const int top = Scale(hwnd, 18);
    const int inputWidth = Scale(hwnd, 390);
    const int inputHeight = Scale(hwnd, 28);
    const int buttonWidth = Scale(hwnd, 86);
    const int gap = Scale(hwnd, 8);

    MoveWindow(
        state->input,
        margin,
        top,
        inputWidth,
        inputHeight,
        TRUE);

    MoveWindow(
        state->apply,
        margin + inputWidth + gap,
        top,
        buttonWidth,
        inputHeight,
        TRUE);
}

void UpdateInput(UtilityState* state) {
    if (state != nullptr && state->input != nullptr) {
        const std::wstring text = Hex(state->color);
        SetWindowTextW(state->input, text.c_str());
    }
}

void SetColorInternal(HWND hwnd, UtilityState* state, COLORREF color, bool notify) {
    if (state == nullptr) {
        return;
    }

    state->color = color;
    UpdateInput(state);
    InvalidateRect(hwnd, nullptr, TRUE);

    if (notify && state->onColorChanged != nullptr) {
        state->onColorChanged(color);
    }
}

bool PointInside(const RECT& rect, POINT point) {
    return
        point.x >= rect.left &&
        point.x < rect.right &&
        point.y >= rect.top &&
        point.y < rect.bottom;
}

COLORREF BestTextColor(COLORREF background) {
    const double blackRatio = dkcolor::ContrastRatio(background, RGB(0, 0, 0));
    const double whiteRatio = dkcolor::ContrastRatio(background, RGB(255, 255, 255));
    return blackRatio >= whiteRatio ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

void DrawSwatch(
    HDC hdc,
    const RECT& rect,
    COLORREF color,
    const std::wstring& label,
    HFONT font) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
    HGDIOBJ oldPen = SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, BestTextColor(color));

    RECT textRect = rect;
    DrawTextW(
        hdc,
        label.c_str(),
        -1,
        &textRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

void PaintUtility(HWND hwnd, UtilityState* state, HDC hdc) {
    if (state == nullptr) {
        return;
    }

    const int margin = Scale(hwnd, 20);
    const int top = Scale(hwnd, 62);
    const int cardHeight = Scale(hwnd, 126);
    const int swatchWidth = Scale(hwnd, 160);
    const int gap = Scale(hwnd, 12);
    const int textLeft = margin + swatchWidth + Scale(hwnd, 22);
    const int rowHeight = Scale(hwnd, 22);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    RECT currentSwatch{
        margin,
        top,
        margin + swatchWidth,
        top + cardHeight};
    DrawSwatch(hdc, currentSwatch, state->color, Hex(state->color), font);

    const dkcolor::CssNamedColor nearest =
        dkcolor::NearestCssNamedColor(state->color);

    std::array<std::wstring, 5> lines{
        L"HEX  " + Hex(state->color),
        L"RGB  " + dkcolor::FormatColor(
            state->color, dkcolor::CopyFormat::Rgb, L""),
        L"HSL  " + dkcolor::FormatColor(
            state->color, dkcolor::CopyFormat::Hsl, L""),
        L"OKLCH  " + dkcolor::FormatColor(
            state->color, dkcolor::CopyFormat::Oklch, L""),
        L"가장 가까운 CSS 색상: " +
            std::wstring(nearest.name) +
            L"  " +
            Hex(nearest.color),
    };

    for (std::size_t index = 0; index < lines.size(); ++index) {
        RECT lineRect{
            textLeft,
            top + static_cast<int>(index) * rowHeight,
            Scale(hwnd, 740),
            top + static_cast<int>(index + 1) * rowHeight};
        DrawTextW(
            hdc,
            lines[index].c_str(),
            -1,
            &lineRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    const int tonesTitleY = top + cardHeight + Scale(hwnd, 24);
    RECT tonesTitle{
        margin,
        tonesTitleY,
        Scale(hwnd, 740),
        tonesTitleY + rowHeight};
    DrawTextW(
        hdc,
        L"톤 단계 — 클릭하면 현재 색상으로 적용",
        -1,
        &tonesTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    state->tones = dkcolor::ToneSteps(state->color);

    const int swatchTop = tonesTitleY + Scale(hwnd, 28);
    const int swatchGap = Scale(hwnd, 8);
    const int usableWidth = Scale(hwnd, 720);
    const int smallSwatchWidth = (usableWidth - swatchGap * 4) / 5;
    const int smallSwatchHeight = Scale(hwnd, 66);

    for (std::size_t index = 0; index < state->tones.size(); ++index) {
        RECT rect{
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap),
            swatchTop,
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap) + smallSwatchWidth,
            swatchTop + smallSwatchHeight};
        state->toneRects[index] = rect;
        DrawSwatch(hdc, rect, state->tones[index], Hex(state->tones[index]), font);
    }

    const int harmonyTitleY = swatchTop + smallSwatchHeight + Scale(hwnd, 24);
    RECT harmonyTitle{
        margin,
        harmonyTitleY,
        Scale(hwnd, 740),
        harmonyTitleY + rowHeight};
    DrawTextW(
        hdc,
        L"조화 배색 — 보색 / 유사색 -30° / 유사색 +30° / 삼각 -120° / 삼각 +120°",
        -1,
        &harmonyTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const dkcolor::HarmonySet harmony = dkcolor::HarmonyColors(state->color);
    state->harmonies = {
        harmony.complementary,
        harmony.analogousLeft,
        harmony.analogousRight,
        harmony.triadicLeft,
        harmony.triadicRight,
    };

    const int harmonyTop = harmonyTitleY + Scale(hwnd, 28);
    for (std::size_t index = 0; index < state->harmonies.size(); ++index) {
        RECT rect{
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap),
            harmonyTop,
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap) + smallSwatchWidth,
            harmonyTop + smallSwatchHeight};
        state->harmonyRects[index] = rect;
        DrawSwatch(
            hdc,
            rect,
            state->harmonies[index],
            Hex(state->harmonies[index]),
            font);
    }

    const int contrastTitleY = harmonyTop + smallSwatchHeight + Scale(hwnd, 24);
    RECT contrastTitle{
        margin,
        contrastTitleY,
        Scale(hwnd, 740),
        contrastTitleY + rowHeight};
    DrawTextW(
        hdc,
        L"WCAG 대비",
        -1,
        &contrastTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const double whiteRatio =
        dkcolor::ContrastRatio(state->color, RGB(255, 255, 255));
    const double blackRatio =
        dkcolor::ContrastRatio(state->color, RGB(0, 0, 0));

    wchar_t whiteText[160]{};
    wchar_t blackText[160]{};
    swprintf_s(
        whiteText,
        L"흰색 배경/글자 대비: %.2f:1 — %s",
        whiteRatio,
        ContrastGrade(whiteRatio).c_str());
    swprintf_s(
        blackText,
        L"검정 배경/글자 대비: %.2f:1 — %s",
        blackRatio,
        ContrastGrade(blackRatio).c_str());

    RECT whiteRect{
        margin,
        contrastTitleY + Scale(hwnd, 26),
        Scale(hwnd, 740),
        contrastTitleY + Scale(hwnd, 48)};
    RECT blackRect{
        margin,
        contrastTitleY + Scale(hwnd, 50),
        Scale(hwnd, 740),
        contrastTitleY + Scale(hwnd, 72)};

    DrawTextW(
        hdc,
        whiteText,
        -1,
        &whiteRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(
        hdc,
        blackText,
        -1,
        &blackRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT helpRect{
        margin,
        Scale(hwnd, 548),
        Scale(hwnd, 740),
        Scale(hwnd, 580)};
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawTextW(
        hdc,
        L"직접 입력: #RGB, #RRGGBB, rgb(r,g,b), CSS 색상 이름. "
        L"창을 닫아도 프로그램은 트레이에 계속 상주합니다.",
        -1,
        &helpRect,
        DT_LEFT | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
}

LRESULT CALLBACK UtilityProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = static_cast<UtilityState*>(create->lpCreateParams);
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

        state->input = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0,
            0,
            0,
            0,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kInputId)),
            GetModuleHandleW(nullptr),
            nullptr);

        state->apply = CreateWindowExW(
            0,
            L"BUTTON",
            L"적용",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0,
            0,
            0,
            0,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kApplyId)),
            GetModuleHandleW(nullptr),
            nullptr);

        if (state->input != nullptr) {
            SendMessageW(
                state->input,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(font),
                TRUE);
            SendMessageW(state->input, EM_SETLIMITTEXT, 128, 0);
        }
        if (state->apply != nullptr) {
            SendMessageW(
                state->apply,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(font),
                TRUE);
        }

        LayoutControls(hwnd, state);
        UpdateInput(state);
        return 0;
    }

    case WM_SIZE:
        LayoutControls(hwnd, state);
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(
            hwnd,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutControls(hwnd, state);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == kApplyId && state != nullptr && state->input != nullptr) {
            const int length = GetWindowTextLengthW(state->input);
            std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
            GetWindowTextW(state->input, value.data(), length + 1);
            value.resize(static_cast<std::size_t>(length));

            COLORREF parsed = RGB(0, 0, 0);
            if (!dkcolor::ParseColorText(value, &parsed)) {
                MessageBoxW(
                    hwnd,
                    L"색상 형식을 확인해주세요.\n"
                    L"#RGB, #RRGGBB, rgb(r,g,b), CSS 색상 이름을 사용할 수 있습니다.",
                    L"DK Color Picker",
                    MB_OK | MB_ICONWARNING);
                return 0;
            }

            SetColorInternal(hwnd, state, parsed, true);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (state != nullptr) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            for (std::size_t index = 0; index < state->toneRects.size(); ++index) {
                if (PointInside(state->toneRects[index], point)) {
                    SetColorInternal(hwnd, state, state->tones[index], true);
                    return 0;
                }
            }

            for (std::size_t index = 0; index < state->harmonyRects.size(); ++index) {
                if (PointInside(state->harmonyRects[index], point)) {
                    SetColorInternal(hwnd, state, state->harmonies[index], true);
                    return 0;
                }
            }
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(hwnd, &paint);
        PaintUtility(hwnd, state, hdc);
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        delete state;
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

bool RegisterUtilityWindowClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = UtilityProc;
    windowClass.lpszClassName = kUtilityClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));

    return RegisterClassExW(&windowClass) != 0;
}

HWND CreateUtilityWindow(
    HINSTANCE instance,
    HWND owner,
    COLORREF color,
    ColorChangedCallback onColorChanged) {
    auto* state = new UtilityState();
    state->color = color;
    state->onColorChanged = onColorChanged;

    HWND window = CreateWindowExW(
        WS_EX_APPWINDOW,
        kUtilityClass,
        L"DK Color Picker - 색상 도구",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        780,
        620,
        owner,
        nullptr,
        instance,
        state);

    if (window == nullptr) {
        delete state;
        return nullptr;
    }

    const int dpi = static_cast<int>(GetDpiForWindow(window));
    RECT client{0, 0, MulDiv(760, dpi, 96), MulDiv(590, dpi, 96)};
    AdjustWindowRectExForDpi(
        &client,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        FALSE,
        WS_EX_APPWINDOW,
        static_cast<UINT>(dpi));

    SetWindowPos(
        window,
        nullptr,
        0,
        0,
        client.right - client.left,
        client.bottom - client.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    return window;
}

void ShowUtilityWindow(HWND hwnd, COLORREF color) {
    if (hwnd == nullptr) {
        return;
    }

    SetUtilityWindowColor(hwnd, color);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
}

void SetUtilityWindowColor(HWND hwnd, COLORREF color) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    SetColorInternal(hwnd, state, color, false);
}

} // namespace dkcolorui
