#include "utility_window.h"

#include <windowsx.h>
#include <commdlg.h>

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
constexpr int kInputId = 3001;
constexpr int kApplyId = 3002;
constexpr int kFavoriteId = 3003;
constexpr int kClearRecentId = 3004;
constexpr int kFormatLabelId = 3005;
constexpr int kFormatRadioBaseId = 3020;
constexpr int kExportCssId = 3010;
constexpr int kExportJsonId = 3011;
constexpr int kExportTailwindId = 3012;
constexpr int kExportGimpId = 3013;

struct UtilityState {
    HWND input = nullptr;
    HWND apply = nullptr;
    HWND favorite = nullptr;
    HWND formatLabel = nullptr;
    std::array<HWND, static_cast<std::size_t>(dkcolor::CopyFormat::Count)> formatRadios{};
    HWND clearRecent = nullptr;
    HWND exportCss = nullptr;
    HWND exportJson = nullptr;
    HWND exportTailwind = nullptr;
    HWND exportGimp = nullptr;

    COLORREF color = RGB(59, 130, 246);
    dkcolor::CopyFormat copyFormat = dkcolor::CopyFormat::Hex;
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
};

int Scale(HWND hwnd, int value) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(hwnd)), 96);
}

std::wstring Hex(COLORREF color) {
    return dkcolor::FormatColor(color, dkcolor::CopyFormat::Hex, L"");
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
    SetWindowTextW(hwnd, title.c_str());

    const std::wstring apply = dkl10n::Text(L"tools.apply");
    const std::wstring format = dkl10n::Text(L"tools.copy_format");
    const std::wstring clear = dkl10n::Text(L"tools.clear_recent");

    SetWindowTextW(state->apply, apply.c_str());
    SetWindowTextW(state->formatLabel, format.c_str());
    SetWindowTextW(state->clearRecent, clear.c_str());

    RefreshFormatRadios(state);
}

void ApplyTheme(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    RecreateThemeBrushes(state);
    dktheme::ApplyWindow(hwnd, state->theme);

    const std::array<HWND, 9> controls{
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
        dktheme::ApplyControl(control, state->theme);
    }

    for (HWND radio : state->formatRadios) {
        dktheme::ApplyControl(radio, state->theme);
    }
}

void LayoutControls(HWND hwnd, UtilityState* state) {
    if (state == nullptr) {
        return;
    }

    const int margin = Scale(hwnd, 20);
    const int top = Scale(hwnd, 16);
    const int inputWidth = Scale(hwnd, 300);
    const int inputHeight = Scale(hwnd, 28);
    const int applyWidth = Scale(hwnd, 72);
    const int favoriteWidth = Scale(hwnd, 132);
    const int gap = Scale(hwnd, 8);

    MoveWindow(state->input, margin, top, inputWidth, inputHeight, TRUE);
    MoveWindow(
        state->apply,
        margin + inputWidth + gap,
        top,
        applyWidth,
        inputHeight,
        TRUE);
    MoveWindow(
        state->favorite,
        margin + inputWidth + gap + applyWidth + gap,
        top,
        favoriteWidth,
        inputHeight,
        TRUE);

    const int formatY = Scale(hwnd, 52);
    MoveWindow(
        state->formatLabel,
        margin,
        formatY + Scale(hwnd, 3),
        Scale(hwnd, 76),
        Scale(hwnd, 24),
        TRUE);

    const std::array<int, static_cast<std::size_t>(dkcolor::CopyFormat::Count)> formatWidths{
        54, 54, 54, 54, 58, 62, 68, 68, 108};
    int formatX = margin + Scale(hwnd, 80);
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

    const int actionY = Scale(hwnd, 700);
    MoveWindow(
        state->clearRecent,
        margin,
        actionY,
        Scale(hwnd, 120),
        Scale(hwnd, 28),
        TRUE);

    MoveWindow(
        state->exportCss,
        margin + Scale(hwnd, 248),
        actionY,
        Scale(hwnd, 100),
        Scale(hwnd, 28),
        TRUE);
    MoveWindow(
        state->exportJson,
        margin + Scale(hwnd, 354),
        actionY,
        Scale(hwnd, 84),
        Scale(hwnd, 28),
        TRUE);
    MoveWindow(
        state->exportTailwind,
        margin + Scale(hwnd, 444),
        actionY,
        Scale(hwnd, 110),
        Scale(hwnd, 28),
        TRUE);
    MoveWindow(
        state->exportGimp,
        margin + Scale(hwnd, 560),
        actionY,
        Scale(hwnd, 118),
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

void UpdateInput(UtilityState* state) {
    if (state != nullptr && state->input != nullptr) {
        const std::wstring text = Hex(state->color);
        SetWindowTextW(state->input, text.c_str());
    }
}

void SetColorInternal(
    HWND hwnd,
    UtilityState* state,
    COLORREF color,
    bool notify,
    bool addRecent) {
    if (state == nullptr) {
        return;
    }

    state->color = color;
    if (addRecent) {
        dkcolorlib::AddRecentColor(color);
    }

    UpdateInput(state);
    RefreshLibraryState(state);
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
    HFONT font,
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
    const COLORREF oldTextColor = SetTextColor(hdc, BestTextColor(color));

    RECT textRect = rect;
    DrawTextW(
        hdc,
        label.c_str(),
        -1,
        &textRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE |
            (compact ? DT_END_ELLIPSIS : 0));

    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

void DrawLibrarySwatches(
    HWND hwnd,
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
        DrawSwatch(hdc, rect, colors[index], Hex(colors[index]), font, true);
    }
}

bool WriteUtf8File(
    const std::wstring& path,
    const std::wstring& text) {
    const int bytesRequired = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (bytesRequired < 0) {
        return false;
    }

    std::string utf8(static_cast<std::size_t>(bytesRequired), '\0');
    if (bytesRequired > 0) {
        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(text.size()),
            utf8.data(),
            bytesRequired,
            nullptr,
            nullptr);
    }

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        utf8.data(),
        static_cast<DWORD>(utf8.size()),
        &written,
        nullptr);
    CloseHandle(file);

    return ok != FALSE &&
        written == static_cast<DWORD>(utf8.size());
}

bool ChooseExportPath(
    HWND owner,
    dkcolorlib::PaletteExportFormat format,
    std::wstring* path) {
    if (path == nullptr) {
        return false;
    }

    const wchar_t* filter = nullptr;
    const wchar_t* extension = nullptr;
    const wchar_t* defaultName = nullptr;

    switch (format) {
    case dkcolorlib::PaletteExportFormat::CssVariables:
        filter = L"CSS (*.css)\0*.css\0All files (*.*)\0*.*\0";
        extension = L"css";
        defaultName = L"dk-color-palette.css";
        break;
    case dkcolorlib::PaletteExportFormat::Json:
        filter = L"JSON (*.json)\0*.json\0All files (*.*)\0*.*\0";
        extension = L"json";
        defaultName = L"dk-color-palette.json";
        break;
    case dkcolorlib::PaletteExportFormat::Tailwind:
        filter = L"JavaScript (*.js)\0*.js\0All files (*.*)\0*.*\0";
        extension = L"js";
        defaultName = L"dk-color-tailwind.js";
        break;
    case dkcolorlib::PaletteExportFormat::GimpGpl:
        filter = L"GIMP Palette (*.gpl)\0*.gpl\0All files (*.*)\0*.*\0";
        extension = L"gpl";
        defaultName = L"dk-color-palette.gpl";
        break;
    }

    std::array<wchar_t, MAX_PATH> buffer{};
    wcscpy_s(buffer.data(), buffer.size(), defaultName);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = extension;
    dialog.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&dialog) == FALSE) {
        return false;
    }

    *path = buffer.data();
    return true;
}

void ExportFavorites(
    HWND hwnd,
    UtilityState* state,
    dkcolorlib::PaletteExportFormat format) {
    if (state == nullptr) {
        return;
    }

    RefreshLibraryState(state);
    if (state->favoriteColors.empty()) {
        const std::wstring message = dkl10n::Text(L"dialog.no_favorites");
        MessageBoxW(
            hwnd,
            message.c_str(),
            L"DK Color Picker",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring path;
    if (!ChooseExportPath(hwnd, format, &path)) {
        return;
    }

    const std::wstring output =
        dkcolorlib::ExportPaletteText(state->favoriteColors, format);

    if (!WriteUtf8File(path, output)) {
        const std::wstring message = dkl10n::Text(L"dialog.export_failed");
        MessageBoxW(
            hwnd,
            message.c_str(),
            L"DK Color Picker",
            MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring message = dkl10n::Text(L"dialog.export_done");
    MessageBoxW(
        hwnd,
        message.c_str(),
        L"DK Color Picker",
        MB_OK | MB_ICONINFORMATION);
}

void PaintUtility(HWND hwnd, UtilityState* state, HDC hdc) {
    if (state == nullptr) {
        return;
    }

    RefreshLibraryState(state);

    const int margin = Scale(hwnd, 20);
    const int top = Scale(hwnd, 92);
    const int cardHeight = Scale(hwnd, 104);
    const int swatchWidth = Scale(hwnd, 152);
    const int textLeft = margin + swatchWidth + Scale(hwnd, 20);
    const int rowHeight = Scale(hwnd, 20);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, dktheme::TextColor(state->theme));

    RECT currentSwatch{
        margin,
        top,
        margin + swatchWidth,
        top + cardHeight};
    DrawSwatch(hdc, currentSwatch, state->color, Hex(state->color), font);

    const dkcolor::CssNamedColor nearest =
        dkcolor::NearestCssNamedColor(state->color);

    const std::wstring nearestLabel = dkl10n::Text(L"tools.nearest_css");
    std::array<std::wstring, 5> lines{
        L"HEX  " + Hex(state->color),
        L"RGB  " + dkcolor::FormatColor(
            state->color, dkcolor::CopyFormat::Rgb, L""),
        L"HSL  " + dkcolor::FormatColor(
            state->color, dkcolor::CopyFormat::Hsl, L""),
        L"OKLCH  " + dkcolor::FormatColor(
            state->color, dkcolor::CopyFormat::Oklch, L""),
        nearestLabel + L": " +
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

    const int tonesTitleY = top + cardHeight + Scale(hwnd, 15);
    RECT tonesTitle{
        margin,
        tonesTitleY,
        Scale(hwnd, 740),
        tonesTitleY + rowHeight};
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
        DrawSwatch(hdc, rect, state->tones[index], Hex(state->tones[index]), font);
    }

    const int harmonyTitleY = swatchTop + smallSwatchHeight + Scale(hwnd, 14);
    RECT harmonyTitle{
        margin,
        harmonyTitleY,
        Scale(hwnd, 740),
        harmonyTitleY + rowHeight};
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
            hdc,
            rect,
            state->harmonies[index],
            Hex(state->harmonies[index]),
            font);
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
    DrawTextW(
        hdc,
        recentTitleText.c_str(),
        -1,
        &recentTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const int recentTop = recentTitleY + Scale(hwnd, 22);
    DrawLibrarySwatches(
        hwnd,
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
    DrawTextW(
        hdc,
        favoritesTitleText.c_str(),
        -1,
        &favoritesTitle,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const int favoritesTop = favoritesTitleY + Scale(hwnd, 22);
    DrawLibrarySwatches(
        hwnd,
        hdc,
        font,
        state->favoriteColors,
        state->favoriteRects.data(),
        state->favoriteRects.size(),
        favoritesTop);

    RECT exportLabel{
        margin + Scale(hwnd, 126),
        Scale(hwnd, 700),
        margin + Scale(hwnd, 246),
        Scale(hwnd, 728)};
    SetTextColor(hdc, dktheme::TextColor(state->theme));
    const std::wstring exportText = dkl10n::Text(L"tools.export");
    DrawTextW(
        hdc,
        exportText.c_str(),
        -1,
        &exportLabel,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT helpRect{
        margin,
        Scale(hwnd, 736),
        Scale(hwnd, 740),
        Scale(hwnd, 760)};
    SetTextColor(hdc, dktheme::MutedTextColor(state->theme));
    const std::wstring helpText = dkl10n::Text(L"tools.help");
    DrawTextW(
        hdc,
        helpText.c_str(),
        -1,
        &helpRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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
        if (PointInside(state->recentRects[index], point)) {
            SetColorInternal(
                hwnd,
                state,
                state->recentColors[index],
                true,
                false);
            return;
        }
    }

    for (std::size_t index = 0;
         index < state->favoriteColors.size() &&
         index < state->favoriteRects.size();
         ++index) {
        if (PointInside(state->favoriteRects[index], point)) {
            SetColorInternal(
                hwnd,
                state,
                state->favoriteColors[index],
                true,
                false);
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

        const std::array<HWND, 9> controls{
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

    case WM_COMMAND:
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

    case WM_LBUTTONDOWN:
        if (state != nullptr) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            for (std::size_t index = 0; index < state->toneRects.size(); ++index) {
                if (PointInside(state->toneRects[index], point)) {
                    if (state->onCopyColor != nullptr) {
                        state->onCopyColor(state->tones[index]);
                    }
                    return 0;
                }
            }

            for (std::size_t index = 0; index < state->harmonyRects.size(); ++index) {
                if (PointInside(state->harmonyRects[index], point)) {
                    if (state->onCopyColor != nullptr) {
                        state->onCopyColor(state->harmonies[index]);
                    }
                    return 0;
                }
            }

            HandleLibraryClick(hwnd, state, point);
        }
        return 0;

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
    dktheme::ThemeMode theme,
    ColorChangedCallback onColorChanged,
    CopyColorCallback onCopyColor,
    CopyFormatChangedCallback onCopyFormatChanged) {
    auto* state = new UtilityState();
    state->color = color;
    state->copyFormat = copyFormat;
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
