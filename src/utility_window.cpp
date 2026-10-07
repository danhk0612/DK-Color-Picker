#include "utility_window.h"

#include <windowsx.h>
#include "color_formats.h"
#include "color_library.h"
#include "color_tools.h"
#include "localization.h"
#include "theme.h"
#include "resource.h"

#include <array>
#include <cwchar>
#include <string>
#include <vector>

namespace dkcolorui {
namespace {

constexpr wchar_t kUtilityClass[] = L"DKColorPicker.UtilityWindow";
constexpr int kClearRecentId = 3004;
constexpr int kFormatRadioBaseId = 3020;
constexpr UINT_PTR kSwatchFeedbackTimerId = 1;

struct UtilityState {
    std::array<HWND, static_cast<std::size_t>(dkcolor::CopyFormat::Count)> formatRadios{};
    HWND clearRecent = nullptr;

    COLORREF color = RGB(59, 130, 246);
    dkcolor::CopyFormat copyFormat = dkcolor::CopyFormat::Hex;
    std::wstring customTemplate = dkcolor::DefaultCustomTemplate();
    dktheme::ThemeMode theme = dktheme::ThemeMode::System;
    CopyColorCallback onCopyColor = nullptr;
    CopyTextCallback onCopyText = nullptr;
    CopyFormatChangedCallback onCopyFormatChanged = nullptr;

    HBRUSH backgroundBrush = nullptr;
    HBRUSH controlBrush = nullptr;

    RECT currentRect{};
    RECT cssNameRect{};
    std::array<RECT, 5> toneRects{};
    std::array<RECT, 5> harmonyRects{};
    std::array<COLORREF, 5> tones{};
    std::array<COLORREF, 5> harmonies{};

    std::array<RECT, dkcolorlib::kMaxRecentColors> recentRects{};
    std::array<RECT, dkcolorlib::kMaxFavoriteColors> favoriteRects{};
    std::vector<COLORREF> recentColors;
    std::vector<COLORREF> favoriteColors;

    RECT feedbackRect{};
    bool feedbackActive = false;
};
    HWND clearRecent = nullptr;
    HWND exportCss = nullptr;
    HWND exportJson = nullptr;
    HWND exportTailwind = nullptr;
    HWND exportGimp = nullptr;

    COLORREF color = RGB(59, 130, 246);
    dkcolor::CopyFormat copyFormat = dkcolor::CopyFormat::Hex;
    std::wstring customTemplate = dkcolor::DefaultCustomTemplate();
    dktheme::ThemeMode theme = dktheme::ThemeMode::System;
    ColorChangedCallback onColorChanged = nullptr;
    CopyColorCallback onCopyColor = nullptr;
    CopyFormatChangedCallback onCopyFormatChanged = nullptr;

    HBRUSH backgroundBrush = nullptr;
    HBRUSH controlBrush = nullptr;

    std::array<RECT, 5> toneRects{};
    std::array<RECT, 5> harmonyRects{};
    std::array<COLORREF, 5> tones{};
    std::array<COLORREF, 5> harmonies{};

    std::array<RECT, dkcolorlib::kMaxRecentColors> recentRects{};
    std::array<RECT, dkcolorlib::kMaxFavoriteColors> favoriteRects{};
    std::vector<COLORREF> recentColors;
    std::vector<COLORREF> favoriteColors;

    RECT feedbackRect{};
    bool feedbackActive = false;
};

int Scale(HWND hwnd, int value) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(hwnd)), 96);
}

std::wstring Hex(COLORREF color) {
    return dkcolor::FormatColor(color, dkcolor::CopyFormat::Hex, L"");
}

std::wstring DisplayCode(const UtilityState* state, COLORREF color) {
    if (state == nullptr) {
        return Hex(color);
    }

    return dkcolor::FormatColor(
        color,
        state->copyFormat,
        state->customTemplate);
}

std::wstring CopyFormatDisplayName(dkcolor::CopyFormat format) {
    if (format == dkcolor::CopyFormat::Custom) {
        return dkl10n::Text(L"format.custom");
    }
    return dkcolor::CopyFormatLabel(format);
}

std::wstring ContrastGrade(double ratio) {
    const bool english = dkl10n::GetLanguage() == dkl10n::Language::English;
    if (ratio >= 7.0) {
        return english
            ? L"AA/AAA normal & large pass"
            : L"AA/AAA 일반·큰 글자 통과";
    }
    if (ratio >= 4.5) {
        return english
            ? L"AA normal, AA/AAA large pass"
            : L"AA 일반, AA/AAA 큰 글자 통과";
    }
    if (ratio >= 3.0) {
        return english
            ? L"AA large pass"
            : L"AA 큰 글자 통과";
    }
    return english ? L"Below WCAG AA" : L"WCAG AA 대비 기준 미달";
}

void DeleteThemeBrushes(UtilityState* state) {
    if (state == nullptr) {
        return;
    }
    if (state->backgroundBrush != nullptr) {
        DeleteObject(state->backgroundBrush);
        state->backgroundBrush = nullptr;
    }
    if (state->controlBrush != nullptr) {
        DeleteObject(state->controlBrush);
        state->controlBrush = nullptr;
    }
}

void RecreateThemeBrushes(UtilityState* state) {
    if (state == nullptr) {
        return;
    }
    DeleteThemeBrushes(state);
    state->backgroundBrush = CreateSolidBrush(
        dktheme::BackgroundColor(state->theme));
    state->controlBrush = CreateSolidBrush(
        dktheme::ControlBackgroundColor(state->theme));
}

void SetControlFont(HWND control, HFONT font) {
    if (control != nullptr) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(font),
            TRUE);
    }
}

void RefreshFormatRadios(UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < state->formatRadios.size(); ++index) {
        HWND radio = state->formatRadios[index];
        if (radio == nullptr) {
            continue;
        }

        const auto format = static_cast<dkcolor::CopyFormat>(index);
        const std::wstring label = CopyFormatDisplayName(format);
        SetWindowTextW(radio, label.c_str());
    }

    CheckRadioButton(
        GetParent(state->formatLabel),
        kFormatRadioBaseId,
        kFormatRadioBaseId +
            static_cast<int>(dkcolor::CopyFormat::Count) - 1,
        kFormatRadioBaseId + static_cast<int>(state->copyFormat));
}

void ApplyLocalizedLabels(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    const std::wstring title = dkl10n::Text(L"tools.title");
    const std::wstring clear = dkl10n::Text(L"tools.clear_recent");

    SetWindowTextW(hwnd, title.c_str());
    SetWindowTextW(state->clearRecent, clear.c_str());
    RefreshFormatRadios(state);
}

void ApplyTheme(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    RecreateThemeBrushes(state);
    dktheme::ApplyWindow(hwnd, state->theme);
    dktheme::ApplyControl(state->clearRecent, state->theme);

    for (HWND radio : state->formatRadios) {
        dktheme::ApplyControl(radio, state->theme);
    }
}

void LayoutControls(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    const int margin = Scale(hwnd, 20);
    const int formatY = Scale(hwnd, 14);

    const std::array<int, static_cast<std::size_t>(dkcolor::CopyFormat::Count)> formatWidths{
        54, 54, 54, 54, 58, 62, 68, 68, 108};

    int formatX = margin;
    for (std::size_t index = 0; index < state->formatRadios.size(); ++index) {
        const int width = Scale(hwnd, formatWidths[index]);
        MoveWindow(
            state->formatRadios[index],
            formatX,
            formatY,
            width,
            Scale(hwnd, 24),
            TRUE);
        formatX += width + Scale(hwnd, 4);
    }

    MoveWindow(
        state->clearRecent,
        margin,
        Scale(hwnd, 620),
        Scale(hwnd, 120),
        Scale(hwnd, 28),
        TRUE);
}

void RefreshLibraryState(UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    state->recentColors = dkcolorlib::LoadRecentColors();
    state->favoriteColors = dkcolorlib::LoadFavoriteColors();

    if (state->favorite != nullptr) {
        const std::wstring text = dkl10n::Text(
            dkcolorlib::IsFavoriteColor(state->color)
                ? L"tools.favorite_remove"
                : L"tools.favorite_add");
        SetWindowTextW(state->favorite, text.c_str());
    }
}

void SetColorInternal(
    HWND hwnd,
    UtilityState* state,
    COLORREF color) {
    if (state == nullptr) {
        return;
    }

    state->color = color;
    RefreshLibraryState(state);
    InvalidateRect(hwnd, nullptr, TRUE);
}

bool PointInside(const RECT& rect, POINT point) {
    return
        point.x >= rect.left &&
        point.x < rect.right &&
        point.y >= rect.top &&
        point.y < rect.bottom;
}

bool IsFavoriteInState(const UtilityState* state, COLORREF color) {
    if (state == nullptr) {
        return false;
    }

    return std::find(
        state->favoriteColors.begin(),
        state->favoriteColors.end(),
        color) != state->favoriteColors.end();
}

RECT StarRect(HWND hwnd, const RECT& rect) {
    const int size = std::min(
        Scale(hwnd, 22),
        std::max(Scale(hwnd, 16), rect.bottom - rect.top - Scale(hwnd, 4)));

    return {
        rect.right - size - Scale(hwnd, 2),
        rect.top + Scale(hwnd, 2),
        rect.right - Scale(hwnd, 2),
        rect.top + Scale(hwnd, 2) + size};
}

void FlashSwatch(HWND hwnd, UtilityState* state, const RECT& rect) {
    if (state == nullptr) {
        return;
    }

    state->feedbackRect = rect;
    state->feedbackActive = true;

    RECT dirty = rect;
    InflateRect(&dirty, Scale(hwnd, 4), Scale(hwnd, 4));
    InvalidateRect(hwnd, &dirty, FALSE);

    KillTimer(hwnd, kSwatchFeedbackTimerId);
    SetTimer(hwnd, kSwatchFeedbackTimerId, 160, nullptr);
}

void DrawSwatchFeedback(HWND hwnd, UtilityState* state, HDC hdc) {
    if (state == nullptr || !state->feedbackActive) {
        return;
    }

    HPEN pen = CreatePen(
        PS_SOLID,
        Scale(hwnd, 3),
        GetSysColor(COLOR_HIGHLIGHT));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    RECT rect = state->feedbackRect;
    InflateRect(&rect, Scale(hwnd, 2), Scale(hwnd, 2));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

COLORREF BestTextColor(COLORREF background) {
    const double blackRatio = dkcolor::ContrastRatio(background, RGB(0, 0, 0));
    const double whiteRatio = dkcolor::ContrastRatio(background, RGB(255, 255, 255));
    return blackRatio >= whiteRatio ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

void DrawSwatch(
    HWND hwnd,
    HDC hdc,
    const RECT& rect,
    COLORREF color,
    const std::wstring& label,
    HFONT font,
    bool favorite,
    bool compact = false) {
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
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF textColor = BestTextColor(color);
    const COLORREF oldTextColor = SetTextColor(hdc, textColor);

    RECT textRect = rect;
    textRect.left += Scale(hwnd, 4);
    textRect.right -= Scale(hwnd, 24);
    DrawTextW(
        hdc,
        label.c_str(),
        -1,
        &textRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE |
            (compact ? DT_END_ELLIPSIS : 0));

    RECT starRect = StarRect(hwnd, rect);
    DrawTextW(
        hdc,
        favorite ? L"★" : L"☆",
        -1,
        &starRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

bool HandleColorSwatchClick(
    HWND hwnd,
    UtilityState* state,
    POINT point,
    const RECT& rect,
    COLORREF color) {
    if (state == nullptr || !PointInside(rect, point)) {
        return false;
    }

    FlashSwatch(hwnd, state, rect);

    if (PointInside(StarRect(hwnd, rect), point)) {
        dkcolorlib::ToggleFavoriteColor(color);
        RefreshLibraryState(state);
        InvalidateRect(hwnd, nullptr, TRUE);
        return true;
    }

    if (state->onCopyColor != nullptr) {
        state->onCopyColor(color);
    }
    return true;
}

void DrawLibrarySwatches(
    HWND hwnd,
    UtilityState* state,
    HDC hdc,
    HFONT font,
    const std::vector<COLORREF>& colors,
    RECT* rects,
    std::size_t rectCount,
    int top) {
    const int margin = Scale(hwnd, 20);
    const int gap = Scale(hwnd, 6);
    const int usableWidth = Scale(hwnd, 720);
    constexpr int columns = 10;
    const int swatchWidth =
        (usableWidth - gap * (columns - 1)) / columns;
    const int swatchHeight = Scale(hwnd, 38);
    const int rowGap = Scale(hwnd, 6);

    for (std::size_t index = 0; index < rectCount; ++index) {
        rects[index] = {};
    }

    for (std::size_t index = 0;
         index < colors.size() && index < rectCount;
         ++index) {
        const int column = static_cast<int>(index % columns);
        const int row = static_cast<int>(index / columns);
        RECT rect{
            margin + column * (swatchWidth + gap),
            top + row * (swatchHeight + rowGap),
            margin + column * (swatchWidth + gap) + swatchWidth,
            top + row * (swatchHeight + rowGap) + swatchHeight};
        rects[index] = rect;
        DrawSwatch(
            hwnd,
            hdc,
            rect,
            colors[index],
            DisplayCode(state, colors[index]),
            font,
            IsFavoriteInState(state, colors[index]),
            true);
    }
}

void PaintUtility(HWND hwnd, UtilityState* state, HDC hdc) {
    if (state == nullptr) {
        return;
    }

    RefreshLibraryState(state);

    const int margin = Scale(hwnd, 20);
    const int top = Scale(hwnd, 54);
    const int cardHeight = Scale(hwnd, 104);
    const int swatchWidth = Scale(hwnd, 152);
    const int textLeft = margin + swatchWidth + Scale(hwnd, 20);
    const int rowHeight = Scale(hwnd, 28);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(hdc, font);
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldTextColor =
        SetTextColor(hdc, dktheme::TextColor(state->theme));

    state->currentRect = {
        margin,
        top,
        margin + swatchWidth,
        top + cardHeight};

    DrawSwatch(
        hwnd,
        hdc,
        state->currentRect,
        state->color,
        DisplayCode(state, state->color),
        font,
        IsFavoriteInState(state, state->color),
        true);

    const dkcolor::CssNamedColor nearest =
        dkcolor::NearestCssNamedColor(state->color);

    const std::wstring currentCodeLabel =
        dkl10n::Text(L"tools.current_code");
    const std::wstring nearestLabel =
        dkl10n::Text(L"tools.nearest_css");

    RECT currentCodeRect{
        textLeft,
        top,
        Scale(hwnd, 740),
        top + rowHeight};
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    const std::wstring currentCodeText =
        currentCodeLabel + L": " + DisplayCode(state, state->color);
    DrawTextW(
        hdc,
        currentCodeText.c_str(),
        -1,
        &currentCodeRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    state->cssNameRect = {
        textLeft,
        top + rowHeight,
        Scale(hwnd, 740),
        top + rowHeight * 2};
    const std::wstring cssText =
        nearestLabel + L": " +
        std::wstring(nearest.name) +
        L"  " +
        DisplayCode(state, nearest.color);
    DrawTextW(
        hdc,
        cssText.c_str(),
        -1,
        &state->cssNameRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int tonesTitleY = top + cardHeight + Scale(hwnd, 15);
    RECT tonesTitle{
        margin,
        tonesTitleY,
        Scale(hwnd, 740),
        tonesTitleY + rowHeight};
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    const std::wstring tonesTitleText = dkl10n::Text(L"tools.tones");
    DrawTextW(
        hdc,
        tonesTitleText.c_str(),
        -1,
        &tonesTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    state->tones = dkcolor::ToneSteps(state->color);

    const int swatchTop = tonesTitleY + Scale(hwnd, 22);
    const int swatchGap = Scale(hwnd, 8);
    const int usableWidth = Scale(hwnd, 720);
    const int smallSwatchWidth = (usableWidth - swatchGap * 4) / 5;
    const int smallSwatchHeight = Scale(hwnd, 46);

    for (std::size_t index = 0; index < state->tones.size(); ++index) {
        RECT rect{
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap),
            swatchTop,
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap) + smallSwatchWidth,
            swatchTop + smallSwatchHeight};
        state->toneRects[index] = rect;
        DrawSwatch(
            hwnd,
            hdc,
            rect,
            state->tones[index],
            DisplayCode(state, state->tones[index]),
            font,
            IsFavoriteInState(state, state->tones[index]),
            true);
    }

    const int harmonyTitleY = swatchTop + smallSwatchHeight + Scale(hwnd, 14);
    RECT harmonyTitle{
        margin,
        harmonyTitleY,
        Scale(hwnd, 740),
        harmonyTitleY + rowHeight};
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    const std::wstring harmonyTitleText = dkl10n::Text(L"tools.harmony");
    DrawTextW(
        hdc,
        harmonyTitleText.c_str(),
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

    const int harmonyTop = harmonyTitleY + Scale(hwnd, 22);
    for (std::size_t index = 0; index < state->harmonies.size(); ++index) {
        RECT rect{
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap),
            harmonyTop,
            margin + static_cast<int>(index) * (smallSwatchWidth + swatchGap) + smallSwatchWidth,
            harmonyTop + smallSwatchHeight};
        state->harmonyRects[index] = rect;
        DrawSwatch(
            hwnd,
            hdc,
            rect,
            state->harmonies[index],
            DisplayCode(state, state->harmonies[index]),
            font,
            IsFavoriteInState(state, state->harmonies[index]),
            true);
    }

    const int contrastY = harmonyTop + smallSwatchHeight + Scale(hwnd, 14);
    const double whiteRatio =
        dkcolor::ContrastRatio(state->color, RGB(255, 255, 255));
    const double blackRatio =
        dkcolor::ContrastRatio(state->color, RGB(0, 0, 0));

    const bool english = dkl10n::GetLanguage() == dkl10n::Language::English;
    wchar_t contrastText[420]{};
    swprintf_s(
        contrastText,
        english
            ? L"WCAG contrast  White %.2f:1 (%s)   |   Black %.2f:1 (%s)"
            : L"WCAG 대비  흰색 %.2f:1 (%s)   |   검정 %.2f:1 (%s)",
        whiteRatio,
        ContrastGrade(whiteRatio).c_str(),
        blackRatio,
        ContrastGrade(blackRatio).c_str());

    RECT contrastRect{
        margin,
        contrastY,
        Scale(hwnd, 740),
        contrastY + Scale(hwnd, 36)};
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    DrawTextW(
        hdc,
        contrastText,
        -1,
        &contrastRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int recentTitleY = contrastY + Scale(hwnd, 38);
    RECT recentTitle{
        margin,
        recentTitleY,
        Scale(hwnd, 740),
        recentTitleY + rowHeight};
    const std::wstring recentTitleText = dkl10n::Text(L"tools.recent");
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    DrawTextW(
        hdc,
        recentTitleText.c_str(),
        -1,
        &recentTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const int recentTop = recentTitleY + Scale(hwnd, 22);
    DrawLibrarySwatches(
        hwnd,
        state,
        hdc,
        font,
        state->recentColors,
        state->recentRects.data(),
        state->recentRects.size(),
        recentTop);

    const int favoritesTitleY = recentTop + Scale(hwnd, 84) + Scale(hwnd, 12);
    RECT favoritesTitle{
        margin,
        favoritesTitleY,
        Scale(hwnd, 740),
        favoritesTitleY + rowHeight};
    const std::wstring favoritesTitleText = dkl10n::Text(L"tools.favorites");
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    DrawTextW(
        hdc,
        favoritesTitleText.c_str(),
        -1,
        &favoritesTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const int favoritesTop = favoritesTitleY + Scale(hwnd, 22);
    DrawLibrarySwatches(
        hwnd,
        state,
        hdc,
        font,
        state->favoriteColors,
        state->favoriteRects.data(),
        state->favoriteRects.size(),
        favoritesTop);

    RECT hintRect{
        margin + Scale(hwnd, 138),
        Scale(hwnd, 620),
        Scale(hwnd, 740),
        Scale(hwnd, 648)};
    SetTextColor(hdc, dktheme::MutedTextColor(state->theme));
    const std::wstring hintText = dkl10n::Text(L"tools.star_hint");
    DrawTextW(
        hdc,
        hintText.c_str(),
        -1,
        &hintRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawSwatchFeedback(hwnd, state, hdc);

    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

void HandleLibraryClick(HWND hwnd, UtilityState* state, POINT point) {
    if (state == nullptr) {
        return;
    }

    for (std::size_t index = 0;
         index < state->recentColors.size() &&
         index < state->recentRects.size();
         ++index) {
        if (HandleColorSwatchClick(
                hwnd,
                state,
                point,
                state->recentRects[index],
                state->recentColors[index])) {
            return;
        }
    }

    for (std::size_t index = 0;
         index < state->favoriteColors.size() &&
         index < state->favoriteRects.size();
         ++index) {
        if (HandleColorSwatchClick(
                hwnd,
                state,
                point,
                state->favoriteRects[index],
                state->favoriteColors[index])) {
            return;
        }
    }
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

        state->inputLabel = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kInputLabelId)),
            GetModuleHandleW(nullptr),
            nullptr);

        state->input = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kInputId)),
            GetModuleHandleW(nullptr),
            nullptr);

        state->apply = CreateWindowExW(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kApplyId)),
            GetModuleHandleW(nullptr), nullptr);

        state->favorite = CreateWindowExW(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFavoriteId)),
            GetModuleHandleW(nullptr), nullptr);

        state->formatLabel = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFormatLabelId)),
            GetModuleHandleW(nullptr), nullptr);

        for (std::size_t index = 0; index < state->formatRadios.size(); ++index) {
            const DWORD groupStyle = index == 0 ? WS_GROUP : 0;
            state->formatRadios[index] = CreateWindowExW(
                0,
                L"BUTTON",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                    BS_AUTORADIOBUTTON | groupStyle,
                0, 0, 0, 0,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        kFormatRadioBaseId + static_cast<int>(index))),
                GetModuleHandleW(nullptr),
                nullptr);
        }

        state->clearRecent = CreateWindowExW(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kClearRecentId)),
            GetModuleHandleW(nullptr), nullptr);

        state->exportCss = CreateWindowExW(
            0, L"BUTTON", L"CSS",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kExportCssId)),
            GetModuleHandleW(nullptr), nullptr);

        state->exportJson = CreateWindowExW(
            0, L"BUTTON", L"JSON",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kExportJsonId)),
            GetModuleHandleW(nullptr), nullptr);

        state->exportTailwind = CreateWindowExW(
            0, L"BUTTON", L"Tailwind",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kExportTailwindId)),
            GetModuleHandleW(nullptr), nullptr);

        state->exportGimp = CreateWindowExW(
            0, L"BUTTON", L"GIMP GPL",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kExportGimpId)),
            GetModuleHandleW(nullptr), nullptr);

        const std::array<HWND, 10> controls{
            state->inputLabel,
            state->input,
            state->apply,
            state->favorite,
            state->formatLabel,
            state->clearRecent,
            state->exportCss,
            state->exportJson,
            state->exportTailwind,
            state->exportGimp};

        for (HWND control : controls) {
            SetControlFont(control, font);
        }
        for (HWND radio : state->formatRadios) {
            SetControlFont(radio, font);
        }

        SendMessageW(state->input, EM_SETLIMITTEXT, 128, 0);

        LayoutControls(hwnd, state);
        UpdateInput(state);
        RefreshLibraryState(state);
        ApplyLocalizedLabels(hwnd, state);
        ApplyTheme(hwnd, state);
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

    case WM_COMMAND: {
        if (state == nullptr) {
            break;
        }

        const int controlId = LOWORD(wParam);
        const int formatCount = static_cast<int>(dkcolor::CopyFormat::Count);
        if (controlId >= kFormatRadioBaseId &&
            controlId < kFormatRadioBaseId + formatCount &&
            HIWORD(wParam) == BN_CLICKED) {
            state->copyFormat = static_cast<dkcolor::CopyFormat>(
                controlId - kFormatRadioBaseId);
            RefreshFormatRadios(state);

            if (state->onCopyFormatChanged != nullptr) {
                state->onCopyFormatChanged(state->copyFormat);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        switch (LOWORD(wParam)) {
        case kApplyId: {
            const int length = GetWindowTextLengthW(state->input);
            std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
            GetWindowTextW(state->input, value.data(), length + 1);
            value.resize(static_cast<std::size_t>(length));

            COLORREF parsed = RGB(0, 0, 0);
            if (!dkcolor::ParseColorText(value, &parsed)) {
                const std::wstring dialogText = dkl10n::Text(L"dialog.invalid_color");
                MessageBoxW(
                    hwnd,
                    dialogText.c_str(),
                    L"DK Color Picker",
                    MB_OK | MB_ICONWARNING);
                return 0;
            }

            SetColorInternal(hwnd, state, parsed, true, false);
            return 0;
        }

        case kFavoriteId:
            dkcolorlib::ToggleFavoriteColor(state->color);
            RefreshLibraryState(state);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case kClearRecentId:
            dkcolorlib::ClearRecentColors();
            RefreshLibraryState(state);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case kExportCssId:
            ExportFavorites(
                hwnd, state, dkcolorlib::PaletteExportFormat::CssVariables);
            return 0;
        case kExportJsonId:
            ExportFavorites(
                hwnd, state, dkcolorlib::PaletteExportFormat::Json);
            return 0;
        case kExportTailwindId:
            ExportFavorites(
                hwnd, state, dkcolorlib::PaletteExportFormat::Tailwind);
            return 0;
        case kExportGimpId:
            ExportFavorites(
                hwnd, state, dkcolorlib::PaletteExportFormat::GimpGpl);
            return 0;
        default:
            break;
        }
        break;
    }

    case WM_LBUTTONDOWN:
        if (state != nullptr) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            for (std::size_t index = 0; index < state->toneRects.size(); ++index) {
                if (PointInside(state->toneRects[index], point)) {
                    FlashSwatch(hwnd, state, state->toneRects[index]);
                    if (state->onCopyColor != nullptr) {
                        state->onCopyColor(state->tones[index]);
                    }
                    return 0;
                }
            }

            for (std::size_t index = 0; index < state->harmonyRects.size(); ++index) {
                if (PointInside(state->harmonyRects[index], point)) {
                    FlashSwatch(hwnd, state, state->harmonyRects[index]);
                    if (state->onCopyColor != nullptr) {
                        state->onCopyColor(state->harmonies[index]);
                    }
                    return 0;
                }
            }

            HandleLibraryClick(hwnd, state, point);
        }
        return 0;

    case WM_TIMER:
        if (wParam == kSwatchFeedbackTimerId && state != nullptr) {
            KillTimer(hwnd, kSwatchFeedbackTimerId);
            state->feedbackActive = false;

            RECT dirty = state->feedbackRect;
            InflateRect(&dirty, Scale(hwnd, 4), Scale(hwnd, 4));
            InvalidateRect(hwnd, &dirty, FALSE);
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        if (state != nullptr && state->backgroundBrush != nullptr) {
            RECT client{};
            GetClientRect(hwnd, &client);
            FillRect(
                reinterpret_cast<HDC>(wParam),
                &client,
                state->backgroundBrush);
            return 1;
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (state != nullptr) {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, dktheme::TextColor(state->theme));
            SetBkColor(hdc, dktheme::BackgroundColor(state->theme));
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }
        break;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (state != nullptr) {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, dktheme::TextColor(state->theme));
            SetBkColor(hdc, dktheme::ControlBackgroundColor(state->theme));
            return reinterpret_cast<LRESULT>(state->controlBrush);
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
        if (state != nullptr) {
            DeleteThemeBrushes(state);
        }
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
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hbrBackground = nullptr;

    return RegisterClassExW(&windowClass) != 0;
}

HWND CreateUtilityWindow(
    HINSTANCE instance,
    HWND owner,
    COLORREF color,
    dkcolor::CopyFormat copyFormat,
    const std::wstring& customTemplate,
    dktheme::ThemeMode theme,
    ColorChangedCallback onColorChanged,
    CopyColorCallback onCopyColor,
    CopyFormatChangedCallback onCopyFormatChanged) {
    auto* state = new UtilityState();
    state->color = color;
    state->copyFormat = copyFormat;
    state->customTemplate = customTemplate;
    state->theme = theme;
    state->onColorChanged = onColorChanged;
    state->onCopyColor = onCopyColor;
    state->onCopyFormatChanged = onCopyFormatChanged;

    HWND window = CreateWindowExW(
        WS_EX_APPWINDOW,
        kUtilityClass,
        dkl10n::Text(L"tools.title").c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        780,
        805,
        owner,
        nullptr,
        instance,
        state);

    if (window == nullptr) {
        delete state;
        return nullptr;
    }

    const int dpi = static_cast<int>(GetDpiForWindow(window));
    RECT client{0, 0, MulDiv(760, dpi, 96), MulDiv(770, dpi, 96)};
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

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    SetColorInternal(hwnd, state, color, false, false);

    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
}

void SetUtilityWindowColor(HWND hwnd, COLORREF color) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    SetColorInternal(hwnd, state, color, false, false);
}

void SetUtilityCopyFormat(HWND hwnd, dkcolor::CopyFormat copyFormat) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) {
        return;
    }

    state->copyFormat = copyFormat;
    RefreshFormatRadios(state);
}

void SetUtilityCustomTemplate(
    HWND hwnd,
    const std::wstring& customTemplate) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) {
        return;
    }

    state->customTemplate = customTemplate;
    InvalidateRect(hwnd, nullptr, TRUE);
}

void RefreshUtilityWindow(HWND hwnd, dktheme::ThemeMode theme) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) {
        return;
    }

    state->theme = theme;
    ApplyLocalizedLabels(hwnd, state);
    RefreshLibraryState(state);
    ApplyTheme(hwnd, state);
    InvalidateRect(hwnd, nullptr, TRUE);
}

} // namespace dkcolorui
