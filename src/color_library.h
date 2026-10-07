#pragma once

#include <windows.h>

#include <vector>

namespace dkcolorlib {

constexpr std::size_t kMaxRecentColors = 8;
constexpr std::size_t kMaxFavoriteColors = 8;

std::vector<COLORREF> LoadRecentColors();
std::vector<COLORREF> LoadFavoriteColors();

enum class FavoriteToggleResult {
    Added,
    Removed,
    LimitReached,
};

void AddRecentColor(COLORREF color);
FavoriteToggleResult ToggleFavoriteColor(COLORREF color);
bool IsFavoriteColor(COLORREF color);
void ClearRecentColors();

} // namespace dkcolorlib
