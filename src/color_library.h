#pragma once

#include <windows.h>

#include <vector>

namespace dkcolorlib {

constexpr std::size_t kMaxRecentColors = 20;
constexpr std::size_t kMaxFavoriteColors = 20;

std::vector<COLORREF> LoadRecentColors();
std::vector<COLORREF> LoadFavoriteColors();

void AddRecentColor(COLORREF color);
bool ToggleFavoriteColor(COLORREF color);
bool IsFavoriteColor(COLORREF color);
void ClearRecentColors();

} // namespace dkcolorlib
