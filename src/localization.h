#pragma once

#include <string>

namespace dkl10n {

enum class Language : int {
    Korean = 0,
    English = 1,
};

void SetLanguage(Language language);
Language GetLanguage();
const wchar_t* LanguageCode(Language language);
std::wstring Text(const wchar_t* key);
std::wstring ExternalLocalePath(Language language);

} // namespace dkl10n
