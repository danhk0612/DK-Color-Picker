#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace dkcolorlib {

constexpr std::size_t kMaxRecentColors = 20;
constexpr std::size_t kMaxFavoriteColors = 20;

enum class PaletteExportFormat {
    CssVariables = 0,
    Json,
    Tailwind,
    GimpGpl,
};

std::vector<COLORREF> LoadRecentColors();
std::vector<COLORREF> LoadFavoriteColors();

void AddRecentColor(COLORREF color);
bool ToggleFavoriteColor(COLORREF color);
bool IsFavoriteColor(COLORREF color);
void ClearRecentColors();

std::wstring ExportPaletteText(
    const std::vector<COLORREF>& colors,
    PaletteExportFormat format);

} // namespace dkcolorlib
