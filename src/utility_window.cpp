#include "utility_window.h"

#include <windowsx.h>

#include "color_formats.h"
#include "color_library.h"
#include "color_tools.h"
#include "localization.h"
#include "resource.h"
#include "theme.h"

#include <algorithm>
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
constexpr int kClientWidth = 720;
constexpr int kMargin = 14;
constexpr int kLibraryColumns = 4;
constexpr int kLibraryRowHeight = 32;
constexpr std::size_t kHarmonyColorCount = 10;

struct UtilityState {
    std::array<HWND, static_cast<std::size_t>(dkcolor::CopyFormat::Count)>
        formatRadios{};
    HWND clearRecent = nullptr;

    COLORREF color = RGB(59, 130, 246);
    dkcolor::CopyFormat copyFormat = dkcolor::CopyFormat::Hex;
    std::wstring customTemplate = dkcolor::DefaultCustomTemplate();
    dktheme::ThemeMode theme = dktheme::ThemeMode::System;

    CopyColorCallback onCopyColor = nullptr;
    CopyTextCallback onCopyText = nullptr;
    CopyFormatChangedCallback onCopyFormatChanged = nullptr;
    RecentColorsChangedCallback onRecentColorsChanged = nullptr;

    HBRUSH backgroundBrush = nullptr;
    HBRUSH controlBrush = nullptr;

    RECT currentRect{};
    RECT cssNameRect{};

    std::array<RECT, kHarmonyColorCount> harmonyRects{};
    std::array<COLORREF, kHarmonyColorCount> harmonies{};

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

void RefreshLibraryState(UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    state->recentColors = dkcolorlib::LoadRecentColors();
    state->favoriteColors = dkcolorlib::LoadFavoriteColors();
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

void RefreshFormatRadios(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < state->formatRadios.size(); ++index) {
        HWND radio = state->formatRadios[index];
        if (radio == nullptr) {
            continue;
        }

        const auto format = dkcolor::CopyFormatAtDisplayIndex(index);
        const std::wstring label = CopyFormatDisplayName(format);
        SetWindowTextW(radio, label.c_str());
    }

    CheckRadioButton(
        hwnd,
        kFormatRadioBaseId,
        kFormatRadioBaseId +
            static_cast<int>(dkcolor::CopyFormat::Count) - 1,
        kFormatRadioBaseId +
            static_cast<int>(
                dkcolor::CopyFormatDisplayIndex(state->copyFormat)));
}

void ApplyLocalizedLabels(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    const std::wstring title = dkl10n::Text(L"tools.title");
    const std::wstring clear = dkl10n::Text(L"tools.clear_recent");

    SetWindowTextW(hwnd, title.c_str());
    SetWindowTextW(state->clearRecent, clear.c_str());
    RefreshFormatRadios(hwnd, state);
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

int LibraryRows(std::size_t count) {
    if (count == 0) {
        return 0;
    }
    return static_cast<int>(
        (count + kLibraryColumns - 1) / kLibraryColumns);
}

struct CompactLayout {
    int recentTitleY = 0;
    int recentTop = 0;
    int favoritesTitleY = 0;
    int favoritesTop = 0;
    int clientHeight = 0;
};

CompactLayout CalculateCompactLayout(
    HWND hwnd,
    const UtilityState* state) {
    CompactLayout layout{};

    layout.recentTitleY = Scale(hwnd, 344);
    layout.recentTop = layout.recentTitleY + Scale(hwnd, 30);

    const int recentRows =
        state == nullptr ? 0 : LibraryRows(state->recentColors.size());
    const int recentBottom =
        layout.recentTop +
        recentRows * Scale(hwnd, kLibraryRowHeight);

    if (state != nullptr && !state->favoriteColors.empty()) {
        layout.favoritesTitleY = recentBottom + Scale(hwnd, 10);
        layout.favoritesTop =
            layout.favoritesTitleY + Scale(hwnd, 22);

        const int favoriteRows =
            LibraryRows(state->favoriteColors.size());
        layout.clientHeight =
            layout.favoritesTop +
            favoriteRows * Scale(hwnd, kLibraryRowHeight) +
            Scale(hwnd, 12);
    } else {
        layout.clientHeight =
            recentBottom + Scale(hwnd, 12);
    }

    const int minimumHeight = Scale(hwnd, 382);
    if (layout.clientHeight < minimumHeight) {
        layout.clientHeight = minimumHeight;
    }

    return layout;
}

void ResizeUtilityToContent(HWND hwnd, UtilityState* state) {
    if (hwnd == nullptr || state == nullptr) {
        return;
    }

    const CompactLayout layout =
        CalculateCompactLayout(hwnd, state);
    const UINT dpi = GetDpiForWindow(hwnd);

    RECT rect{
        0,
        0,
        Scale(hwnd, kClientWidth),
        layout.clientHeight};

    const DWORD style =
        static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const DWORD exStyle =
        static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));

    AdjustWindowRectExForDpi(
        &rect,
        style,
        FALSE,
        exStyle,
        dpi);

    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void LayoutControls(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    const int margin = Scale(hwnd, kMargin);
    const int formatY = Scale(hwnd, 10);

    const std::array<int, static_cast<std::size_t>(dkcolor::CopyFormat::Count)>
        formatWidths{54, 54, 64, 54, 54, 58, 62, 68, 68, 108};

    int formatX = margin;
    for (std::size_t index = 0; index < state->formatRadios.size(); ++index) {
        const int width = Scale(hwnd, formatWidths[index]);
        MoveWindow(
            state->formatRadios[index],
            formatX,
            formatY,
            width,
            Scale(hwnd, 22),
            TRUE);
        formatX += width + Scale(hwnd, 3);
    }

    const CompactLayout layout =
        CalculateCompactLayout(hwnd, state);

    const int clearWidth = Scale(hwnd, 116);
    MoveWindow(
        state->clearRecent,
        Scale(hwnd, kClientWidth) - margin - clearWidth,
        layout.recentTitleY - Scale(hwnd, 2),
        clearWidth,
        Scale(hwnd, 24),
        TRUE);

    EnableWindow(
        state->clearRecent,
        state->recentColors.empty() ? FALSE : TRUE);
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
    ResizeUtilityToContent(hwnd, state);
    InvalidateRect(hwnd, nullptr, TRUE);
}

bool PointInside(const RECT& rect, POINT point) {
    return
        point.x >= rect.left &&
        point.x < rect.right &&
        point.y >= rect.top &&
        point.y < rect.bottom;
}

RECT StarRect(HWND hwnd, const RECT& rect) {
    const int minSize = Scale(hwnd, 16);
    const int maxSize = Scale(hwnd, 22);
    int size =
        static_cast<int>(rect.bottom - rect.top) - Scale(hwnd, 4);

    if (size < minSize) {
        size = minSize;
    }
    if (size > maxSize) {
        size = maxSize;
    }

    return {
        rect.right - size - Scale(hwnd, 2),
        rect.top + Scale(hwnd, 2),
        rect.right - Scale(hwnd, 2),
        rect.top + Scale(hwnd, 2) + size};
}

void FlashRect(HWND hwnd, UtilityState* state, const RECT& rect) {
    if (state == nullptr) {
        return;
    }

    state->feedbackRect = rect;
    state->feedbackActive = true;

    KillTimer(hwnd, kSwatchFeedbackTimerId);
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    SetTimer(hwnd, kSwatchFeedbackTimerId, 160, nullptr);
}

void DrawFeedback(HWND hwnd, UtilityState* state, HDC hdc) {
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
    const double blackRatio =
        dkcolor::ContrastRatio(background, RGB(0, 0, 0));
    const double whiteRatio =
        dkcolor::ContrastRatio(background, RGB(255, 255, 255));

    return blackRatio >= whiteRatio
        ? RGB(0, 0, 0)
        : RGB(255, 255, 255);
}

void DrawSwatch(
    HWND hwnd,
    HDC hdc,
    const RECT& rect,
    COLORREF color,
    const std::wstring& label,
    HFONT font,
    bool favorite,
    bool compact) {
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
    const COLORREF oldTextColor =
        SetTextColor(hdc, BestTextColor(color));

    RECT textRect = rect;
    textRect.left += Scale(hwnd, 4);
    textRect.right -= Scale(hwnd, 25);

    DrawTextW(
        hdc,
        label.c_str(),
        -1,
        &textRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE |
            (compact ? DT_END_ELLIPSIS : 0));

    RECT star = StarRect(hwnd, rect);
    DrawTextW(
        hdc,
        favorite ? L"★" : L"☆",
        -1,
        &star,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

bool HandleColorClick(
    HWND hwnd,
    UtilityState* state,
    POINT point,
    const RECT& rect,
    COLORREF color) {
    if (state == nullptr || !PointInside(rect, point)) {
        return false;
    }

    FlashRect(hwnd, state, rect);

    if (PointInside(StarRect(hwnd, rect), point)) {
        const dkcolorlib::FavoriteToggleResult result =
            dkcolorlib::ToggleFavoriteColor(color);

        if (result == dkcolorlib::FavoriteToggleResult::LimitReached) {
            const std::wstring message =
                dkl10n::Text(L"dialog.favorite_limit");
            MessageBoxW(
                hwnd,
                message.c_str(),
                L"DK Color Picker",
                MB_OK | MB_ICONWARNING);
            return true;
        }

        RefreshLibraryState(state);
        ResizeUtilityToContent(hwnd, state);
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
    const int margin = Scale(hwnd, kMargin);
    const int gap = Scale(hwnd, 5);
    const int usableWidth =
        Scale(hwnd, kClientWidth - kMargin * 2);
    constexpr int columns = kLibraryColumns;
    const int swatchWidth =
        (usableWidth - gap * (columns - 1)) / columns;
    const int swatchHeight = Scale(hwnd, 28);
    const int rowGap = Scale(hwnd, 4);

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

const wchar_t* BoolText(bool value) {
    return value ? L"True" : L"False";
}

void DrawHarmonyRow(
    HWND hwnd,
    UtilityState* state,
    HDC hdc,
    HFONT font,
    const std::wstring& label,
    const COLORREF* colors,
    std::size_t count,
    std::size_t stateOffset,
    int top) {
    if (state == nullptr || colors == nullptr || count == 0) {
        return;
    }

    const int margin = Scale(hwnd, kMargin);
    const int labelWidth = Scale(hwnd, 96);
    const int gap = Scale(hwnd, 5);
    const int swatchLeft = margin + labelWidth;
    const int swatchRight =
        Scale(hwnd, kClientWidth - kMargin);
    const int usableWidth = swatchRight - swatchLeft;
    const int totalGap =
        gap * static_cast<int>(count - 1);
    const int colorWidth = usableWidth - totalGap;
    const int swatchHeight = Scale(hwnd, 28);

    RECT labelRect{
        margin,
        top,
        swatchLeft - Scale(hwnd, 6),
        top + swatchHeight};

    SetTextColor(hdc, dktheme::TextColor(state->theme));
    DrawTextW(
        hdc,
        label.c_str(),
        -1,
        &labelRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t target = stateOffset + index;
        if (target >= state->harmonies.size()) {
            break;
        }

        const int indexInt = static_cast<int>(index);
        const int countInt = static_cast<int>(count);
        const int left =
            swatchLeft +
            (colorWidth * indexInt) / countInt +
            gap * indexInt;
        const int right =
            swatchLeft +
            (colorWidth * (indexInt + 1)) / countInt +
            gap * indexInt;

        RECT rect{
            left,
            top,
            right,
            top + swatchHeight};

        state->harmonies[target] = colors[index];
        state->harmonyRects[target] = rect;

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

    const int margin = Scale(hwnd, kMargin);
    const int rowHeight = Scale(hwnd, 18);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(hdc, font);
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldTextColor =
        SetTextColor(hdc, dktheme::TextColor(state->theme));

    RECT pickLabelRect{
        margin,
        Scale(hwnd, 40),
        Scale(hwnd, 150),
        Scale(hwnd, 58)};
    const std::wstring pickLabel = dkl10n::Text(L"tools.pick");
    DrawTextW(
        hdc,
        pickLabel.c_str(),
        -1,
        &pickLabelRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    state->currentRect = {
        margin,
        Scale(hwnd, 58),
        margin + Scale(hwnd, 130),
        Scale(hwnd, 102)};

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

    state->cssNameRect = {
        margin + Scale(hwnd, 144),
        Scale(hwnd, 62),
        Scale(hwnd, kClientWidth - kMargin),
        Scale(hwnd, 90)};

    const std::wstring cssText =
        dkl10n::Text(L"tools.nearest_css") +
        L": " +
        std::wstring(nearest.name);

    SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
    DrawTextW(
        hdc,
        cssText.c_str(),
        -1,
        &state->cssNameRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    for (RECT& rect : state->harmonyRects) {
        rect = {};
    }

    const dkcolor::HarmonySet harmony =
        dkcolor::HarmonyColors(state->color);

    const COLORREF complementary[1]{
        harmony.complementary};

    DrawHarmonyRow(
        hwnd,
        state,
        hdc,
        font,
        dkl10n::Text(L"tools.complementary"),
        complementary,
        1,
        0,
        Scale(hwnd, 112));

    DrawHarmonyRow(
        hwnd,
        state,
        hdc,
        font,
        dkl10n::Text(L"tools.analogous"),
        harmony.analogous.data(),
        harmony.analogous.size(),
        1,
        Scale(hwnd, 144));

    DrawHarmonyRow(
        hwnd,
        state,
        hdc,
        font,
        dkl10n::Text(L"tools.triadic"),
        harmony.triadic.data(),
        harmony.triadic.size(),
        3,
        Scale(hwnd, 176));

    DrawHarmonyRow(
        hwnd,
        state,
        hdc,
        font,
        dkl10n::Text(L"tools.split_complementary"),
        harmony.splitComplementary.data(),
        harmony.splitComplementary.size(),
        5,
        Scale(hwnd, 208));

    DrawHarmonyRow(
        hwnd,
        state,
        hdc,
        font,
        dkl10n::Text(L"tools.square"),
        harmony.square.data(),
        harmony.square.size(),
        7,
        Scale(hwnd, 240));

    const double whiteRatio =
        dkcolor::ContrastRatio(state->color, RGB(255, 255, 255));
    const double blackRatio =
        dkcolor::ContrastRatio(state->color, RGB(0, 0, 0));

    RECT wcagTitleRect{
        margin,
        Scale(hwnd, 278),
        Scale(hwnd, kClientWidth - kMargin),
        Scale(hwnd, 296)};

    SetTextColor(hdc, dktheme::TextColor(state->theme));
    const std::wstring wcagTitle =
        dkl10n::Text(L"tools.wcag");
    DrawTextW(
        hdc,
        wcagTitle.c_str(),
        -1,
        &wcagTitleRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const auto drawWcagLine = [&](double ratio, bool white, int top) {
        const bool normalAa = ratio >= 4.5;
        const bool normalAaa = ratio >= 7.0;
        const bool largeAa = ratio >= 3.0;
        const bool largeAaa = ratio >= 4.5;

        wchar_t buffer[320]{};
        if (dkl10n::GetLanguage() == dkl10n::Language::English) {
            swprintf_s(
                buffer,
                L"%s text %.2f:1 | Normal AA %s / AAA %s | Large AA %s / AAA %s",
                white ? L"White" : L"Black",
                ratio,
                BoolText(normalAa),
                BoolText(normalAaa),
                BoolText(largeAa),
                BoolText(largeAaa));
        } else {
            swprintf_s(
                buffer,
                L"%s 글자 %.2f:1 | 일반 AA %s / AAA %s | 큰 글자 AA %s / AAA %s",
                white ? L"흰색" : L"검정",
                ratio,
                BoolText(normalAa),
                BoolText(normalAaa),
                BoolText(largeAa),
                BoolText(largeAaa));
        }

        RECT rect{
            margin,
            Scale(hwnd, top),
            Scale(hwnd, kClientWidth - kMargin),
            Scale(hwnd, top + 18)};

        SetTextColor(hdc, dktheme::TextColor(state->theme));
        DrawTextW(
            hdc,
            buffer,
            -1,
            &rect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    };

    drawWcagLine(whiteRatio, true, 296);
    drawWcagLine(blackRatio, false, 316);

    const CompactLayout layout =
        CalculateCompactLayout(hwnd, state);

    RECT recentTitleRect{
        margin,
        layout.recentTitleY,
        Scale(hwnd, kClientWidth - 140),
        layout.recentTitleY + rowHeight};

    const std::wstring recentTitle =
        dkl10n::Text(L"tools.recent");

    SetTextColor(hdc, dktheme::TextColor(state->theme));
    DrawTextW(
        hdc,
        recentTitle.c_str(),
        -1,
        &recentTitleRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawLibrarySwatches(
        hwnd,
        state,
        hdc,
        font,
        state->recentColors,
        state->recentRects.data(),
        state->recentRects.size(),
        layout.recentTop);

    for (RECT& rect : state->favoriteRects) {
        rect = {};
    }

    if (!state->favoriteColors.empty()) {
        RECT favoritesTitleRect{
            margin,
            layout.favoritesTitleY,
            Scale(hwnd, kClientWidth - kMargin),
            layout.favoritesTitleY + rowHeight};

        const std::wstring favoritesTitle =
            dkl10n::Text(L"tools.favorites");

        SetTextColor(hdc, dktheme::TextColor(state->theme));
        DrawTextW(
            hdc,
            favoritesTitle.c_str(),
            -1,
            &favoritesTitleRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        DrawLibrarySwatches(
            hwnd,
            state,
            hdc,
            font,
            state->favoriteColors,
            state->favoriteRects.data(),
            state->favoriteRects.size(),
            layout.favoritesTop);
    }

    DrawFeedback(hwnd, state, hdc);

    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

bool HandleLibraryClick(
    HWND hwnd,
    UtilityState* state,
    POINT point) {
    if (state == nullptr) {
        return false;
    }

    for (std::size_t index = 0;
         index < state->recentColors.size() &&
         index < state->recentRects.size();
         ++index) {
        if (HandleColorClick(
                hwnd,
                state,
                point,
                state->recentRects[index],
                state->recentColors[index])) {
            return true;
        }
    }

    for (std::size_t index = 0;
         index < state->favoriteColors.size() &&
         index < state->favoriteRects.size();
         ++index) {
        if (HandleColorClick(
                hwnd,
                state,
                point,
                state->favoriteRects[index],
                state->favoriteColors[index])) {
            return true;
        }
    }

    return false;
}

LRESULT CALLBACK UtilityProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<const CREATESTRUCTW*>(lParam);
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

        HFONT font =
            static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        for (std::size_t index = 0;
             index < state->formatRadios.size();
             ++index) {
            const DWORD groupStyle =
                index == 0 ? WS_GROUP : 0;

            state->formatRadios[index] = CreateWindowExW(
                0,
                L"BUTTON",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                    BS_AUTORADIOBUTTON | groupStyle,
                0,
                0,
                0,
                0,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        kFormatRadioBaseId +
                        static_cast<int>(index))),
                GetModuleHandleW(nullptr),
                nullptr);

            SetControlFont(state->formatRadios[index], font);
        }

        state->clearRecent = CreateWindowExW(
            0,
            L"BUTTON",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0,
            0,
            0,
            0,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(kClearRecentId)),
            GetModuleHandleW(nullptr),
            nullptr);

        SetControlFont(state->clearRecent, font);

        RefreshLibraryState(state);
        LayoutControls(hwnd, state);
        ApplyLocalizedLabels(hwnd, state);
        ApplyTheme(hwnd, state);
        ResizeUtilityToContent(hwnd, state);
        return 0;
    }

    case WM_SIZE:
        LayoutControls(hwnd, state);
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested =
            reinterpret_cast<const RECT*>(lParam);

        SetWindowPos(
            hwnd,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);

        LayoutControls(hwnd, state);
        ResizeUtilityToContent(hwnd, state);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        if (state == nullptr) {
            break;
        }

        const int controlId = LOWORD(wParam);
        const int formatCount =
            static_cast<int>(dkcolor::CopyFormat::Count);

        if (controlId >= kFormatRadioBaseId &&
            controlId < kFormatRadioBaseId + formatCount &&
            HIWORD(wParam) == BN_CLICKED) {
            state->copyFormat =
                dkcolor::CopyFormatAtDisplayIndex(
                    static_cast<std::size_t>(
                        controlId - kFormatRadioBaseId));

            RefreshFormatRadios(hwnd, state);

            if (state->onCopyFormatChanged != nullptr) {
                state->onCopyFormatChanged(state->copyFormat);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        if (controlId == kClearRecentId &&
            HIWORD(wParam) == BN_CLICKED) {
            dkcolorlib::ClearRecentColors();
            RefreshLibraryState(state);

            if (state->onRecentColorsChanged != nullptr) {
                state->onRecentColorsChanged();
            }

            ResizeUtilityToContent(hwnd, state);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        break;
    }

    case WM_LBUTTONDOWN:
        if (state != nullptr) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)};

            if (HandleColorClick(
                    hwnd,
                    state,
                    point,
                    state->currentRect,
                    state->color)) {
                return 0;
            }

            if (PointInside(state->cssNameRect, point)) {
                FlashRect(hwnd, state, state->cssNameRect);

                if (state->onCopyText != nullptr) {
                    const dkcolor::CssNamedColor nearest =
                        dkcolor::NearestCssNamedColor(state->color);
                    state->onCopyText(nearest.name);
                }
                return 0;
            }

            for (std::size_t index = 0;
                 index < state->harmonyRects.size();
                 ++index) {
                if (HandleColorClick(
                        hwnd,
                        state,
                        point,
                        state->harmonyRects[index],
                        state->harmonies[index])) {
                    return 0;
                }
            }

            if (HandleLibraryClick(hwnd, state, point)) {
                return 0;
            }
        }
        return 0;

    case WM_TIMER:
        if (wParam == kSwatchFeedbackTimerId &&
            state != nullptr) {
            KillTimer(hwnd, kSwatchFeedbackTimerId);
            state->feedbackActive = false;

            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        if (state != nullptr &&
            state->backgroundBrush != nullptr) {
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
            return reinterpret_cast<LRESULT>(
                state->backgroundBrush);
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
            KillTimer(hwnd, kSwatchFeedbackTimerId);
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
    windowClass.hIcon =
        LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
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
    CopyColorCallback onCopyColor,
    CopyTextCallback onCopyText,
    CopyFormatChangedCallback onCopyFormatChanged,
    RecentColorsChangedCallback onRecentColorsChanged) {
    auto* state = new UtilityState();
    state->color = color;
    state->copyFormat = copyFormat;
    state->customTemplate = customTemplate;
    state->theme = theme;
    state->onCopyColor = onCopyColor;
    state->onCopyText = onCopyText;
    state->onCopyFormatChanged = onCopyFormatChanged;
    state->onRecentColorsChanged = onRecentColorsChanged;

    HWND window = CreateWindowExW(
        WS_EX_APPWINDOW,
        kUtilityClass,
        dkl10n::Text(L"tools.title").c_str(),
        WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        740,
        500,
        owner,
        nullptr,
        instance,
        state);

    if (window == nullptr) {
        delete state;
        return nullptr;
    }

    ResizeUtilityToContent(window, state);

    return window;
}

void ShowUtilityWindow(HWND hwnd, COLORREF color) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    SetColorInternal(hwnd, state, color);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
}

void SetUtilityWindowColor(HWND hwnd, COLORREF color) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    SetColorInternal(hwnd, state, color);
}

void SetUtilityCopyFormat(
    HWND hwnd,
    dkcolor::CopyFormat copyFormat) {
    if (hwnd == nullptr) {
        return;
    }

    auto* state = reinterpret_cast<UtilityState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (state == nullptr) {
        return;
    }

    state->copyFormat = copyFormat;
    RefreshFormatRadios(hwnd, state);
    InvalidateRect(hwnd, nullptr, TRUE);
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

void RefreshUtilityWindow(
    HWND hwnd,
    dktheme::ThemeMode theme) {
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
    ResizeUtilityToContent(hwnd, state);
    InvalidateRect(hwnd, nullptr, TRUE);
}

} // namespace dkcolorui
