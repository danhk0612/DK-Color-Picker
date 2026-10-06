#include "localization.h"

#include <windows.h>

#include <array>
#include <string>
#include <unordered_map>

namespace dkl10n {
namespace {

struct Entry {
    const wchar_t* key;
    const wchar_t* ko;
    const wchar_t* en;
};

constexpr std::array<Entry, 44> kEntries{{
    {L"app.title", L"DK Color Picker", L"DK Color Picker"},
    {L"tray.pick", L"색 추출", L"Pick color"},
    {L"tray.tools", L"색상 도구 열기", L"Open color tools"},
    {L"tray.zoom", L"확대 배율", L"Magnification"},
    {L"tray.average", L"평균 추출", L"Average sampling"},
    {L"tray.copy_format", L"복사 형식", L"Copy format"},
    {L"tray.hotkey", L"전역 단축키", L"Global hotkey"},
    {L"tray.auto_start", L"Windows 시작 시 자동 실행", L"Start with Windows"},
    {L"tray.always_open", L"색 추출 후 항상 색상 도구 열기", L"Always open color tools after picking"},
    {L"tray.theme", L"테마", L"Theme"},
    {L"tray.language", L"언어", L"Language"},
    {L"tray.exit", L"종료", L"Exit"},
    {L"tray.template_edit", L"사용자 템플릿 편집...", L"Edit custom template..."},
    {L"theme.system", L"시스템", L"System"},
    {L"theme.light", L"라이트", L"Light"},
    {L"theme.dark", L"다크", L"Dark"},
    {L"language.ko", L"한국어", L"Korean"},
    {L"language.en", L"영어", L"English"},
    {L"tools.title", L"DK Color Picker - 색상 도구", L"DK Color Picker - Color Tools"},
    {L"tools.apply", L"적용", L"Apply"},
    {L"tools.favorite_add", L"즐겨찾기 추가", L"Add favorite"},
    {L"tools.favorite_remove", L"즐겨찾기 제거", L"Remove favorite"},
    {L"tools.clear_recent", L"최근 목록 비우기", L"Clear recent"},
    {L"tools.copy_format", L"복사 형식", L"Copy format"},
    {L"tools.tones", L"톤 단계 — 클릭하여 적용", L"Tone steps — click to apply"},
    {L"tools.harmony", L"조화 배색 — 보색 / 유사색 -30° / 유사색 +30° / 삼각 -120° / 삼각 +120°", L"Harmony — complementary / analogous -30° / +30° / triadic -120° / +120°"},
    {L"tools.recent", L"최근 색상 — 최대 20개, 클릭하여 적용", L"Recent colors — up to 20, click to apply"},
    {L"tools.favorites", L"즐겨찾기 — 최대 20개, 클릭하여 적용", L"Favorites — up to 20, click to apply"},
    {L"tools.export", L"즐겨찾기 내보내기:", L"Export favorites:"},
    {L"tools.help", L"내보내기 대상은 즐겨찾기입니다. 창의 X 버튼은 트레이로 숨깁니다.", L"Exports use favorites. Closing this window hides it to the tray."},
    {L"tools.nearest_css", L"가장 가까운 CSS 색상", L"Nearest CSS color"},
    {L"tools.contrast", L"WCAG 대비", L"WCAG contrast"},
    {L"dialog.invalid_color", L"색상 형식을 확인해주세요.\n#RGB, #RRGGBB, rgb(r,g,b), CSS 색상 이름을 사용할 수 있습니다.", L"Check the color format.\nSupported: #RGB, #RRGGBB, rgb(r,g,b), and CSS color names."},
    {L"dialog.no_favorites", L"내보낼 즐겨찾기 색상이 없습니다.", L"There are no favorite colors to export."},
    {L"dialog.export_failed", L"팔레트 파일을 저장하지 못했습니다.", L"Could not save the palette file."},
    {L"dialog.export_done", L"즐겨찾기 팔레트를 저장했습니다.", L"Favorite palette saved."},
    {L"dialog.hotkey_failed", L"선택한 전역 단축키를 등록하지 못했습니다.\n다른 프로그램에서 이미 사용 중일 수 있습니다.", L"Could not register that global hotkey.\nAnother program may already be using it."},
    {L"dialog.autostart_failed", L"자동 시작 설정을 변경하지 못했습니다.", L"Could not change the startup setting."},
    {L"dialog.capture_failed", L"화면을 캡처하지 못했습니다.", L"Could not capture the screen."},
    {L"template.title", L"DK Color Picker - 사용자 템플릿", L"DK Color Picker - Custom Template"},
    {L"template.heading", L"사용자 정의 복사 템플릿", L"Custom copy template"},
    {L"template.save", L"저장", L"Save"},
    {L"template.cancel", L"취소", L"Cancel"},
    {L"picker.hint", L"휠/± 확대 · 1/3/5/7/9 평균 · 방향키 이동 · Enter 선택", L"Wheel/± zoom · 1/3/5/7/9 average · arrows move · Enter select"},
}};

Language g_language = Language::Korean;
std::unordered_map<std::wstring, std::wstring> g_overrides;

std::wstring ExecutableDirectory() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }

    std::wstring path(buffer.data(), length);
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

const Entry* FindEntry(const wchar_t* key) {
    for (const Entry& entry : kEntries) {
        if (wcscmp(entry.key, key) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

void LoadOverrides() {
    g_overrides.clear();
    const std::wstring path = ExternalLocalePath(g_language);

    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return;
    }

    for (const Entry& entry : kEntries) {
        std::array<wchar_t, 2048> buffer{};
        const DWORD count = GetPrivateProfileStringW(
            L"Strings",
            entry.key,
            L"",
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            path.c_str());

        if (count > 0) {
            g_overrides.emplace(entry.key, std::wstring(buffer.data(), count));
        }
    }
}

} // namespace

void SetLanguage(Language language) {
    g_language = language;
    LoadOverrides();
}

Language GetLanguage() {
    return g_language;
}

const wchar_t* LanguageCode(Language language) {
    return language == Language::English ? L"en" : L"ko";
}

std::wstring Text(const wchar_t* key) {
    const auto overrideIt = g_overrides.find(key);
    if (overrideIt != g_overrides.end()) {
        return overrideIt->second;
    }

    const Entry* entry = FindEntry(key);
    if (entry == nullptr) {
        return key;
    }

    return g_language == Language::English ? entry->en : entry->ko;
}

std::wstring ExternalLocalePath(Language language) {
    const std::wstring directory = ExecutableDirectory();
    if (directory.empty()) {
        return {};
    }

    return directory + L"\\locales\\" + LanguageCode(language) + L".ini";
}

} // namespace dkl10n
