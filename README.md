# DK Color Picker

Windows 10/11용 초경량 화면 색상 추출기입니다.

ColorPick과 같은 빠른 사용 흐름을 목표로 하지만, 기존 프로그램의 코드나 자산을 사용하지 않는 독립적인 MIT 구현입니다.

## 현재 구현

- 네이티브 C++20 + Win32 API
- 설치가 필요 없는 단일 EXE
- 트레이 상주
- 전역 단축키
  - 기본값: Ctrl+Alt+C
  - Ctrl+Shift+C / Alt+Shift+C / Ctrl+Alt+P / F8 선택 가능
- 가상 데스크톱 전체 화면 캡처 후 화면 정지
- 다중 모니터 좌표 지원
- Per-Monitor DPI Awareness V2
- 8x / 12x / 16x / 24x / 32x / 48x 확대
- 1x1 / 3x3 / 5x5 / 7x7 / 9x9 평균 색상 추출
- 평균 영역과 현재 픽셀을 확대경에 구분 표시
- 방향키로 1픽셀 단위 이동
- 마우스 휠 또는 +/- 키로 확대 배율 변경
- 숫자 1/3/5/7/9 키로 평균 범위 변경
- Enter 또는 Space로 선택
- 픽커가 화면 가장자리에서도 가상 데스크톱 내부에 유지되도록 배치
- HEX / RGB 미리보기
- 복사 형식 선택
  - HEX
  - RGB
  - HSL
  - HSV
  - HWB
  - CMYK
  - CIELAB
  - OKLCH
  - 사용자 정의 템플릿
- 사용자 정의 복사 템플릿 편집 창
- Esc 또는 우클릭으로 색 추출 취소
- 트레이 메뉴에서 색 추출 / 확대 배율 / 평균 추출 / 복사 형식 / 전역 단축키 / 자동 시작 / 종료 설정
- 설정을 %LOCALAPPDATA%\DKColorPicker\settings.ini에 저장
- HKCU Run 기반 사용자별 자동 시작
- 중복 실행 방지
- 정적 MSVC 런타임 사용으로 별도 런타임 설치 없이 실행

## 사용법

1. DKColorPicker.exe를 실행합니다.
2. 프로그램은 창을 띄우지 않고 트레이에 상주합니다.
3. 기본 단축키 Ctrl+Alt+C를 누릅니다.
4. 확대경으로 원하는 위치를 확인합니다.
5. 필요하면 다음 조작을 사용합니다.
   - 방향키: 1픽셀 이동
   - 마우스 휠 또는 +/-: 확대 배율 변경
   - 1 / 3 / 5 / 7 / 9: 평균 추출 범위 변경
   - Enter / Space / 좌클릭: 색상 선택
   - Esc / 우클릭: 취소
6. 트레이의 **복사 형식** 메뉴에서 원하는 출력 형식을 선택합니다.
7. 선택한 색상 값이 해당 형식으로 클립보드에 복사됩니다.

## 사용자 정의 템플릿

트레이에서 **복사 형식 → 사용자 템플릿 편집...** 을 선택합니다.

기본 템플릿:

    {hex} / {rgb}

전체 형식 자리표시자:

    {hex}
    {rgb}
    {hsl}
    {hsv}
    {hwb}
    {cmyk}
    {lab}
    {oklch}

구성요소 자리표시자:

    {r} {g} {b}
    {hsl_h} {hsl_s} {hsl_l}
    {hsv_h} {hsv_s} {hsv_v}
    {hwb_h} {hwb_w} {hwb_b}
    {cmyk_c} {cmyk_m} {cmyk_y} {cmyk_k}
    {lab_l} {lab_a} {lab_b}
    {oklch_l} {oklch_c} {oklch_h}

예:

    color: {hex}; rgb: {r}, {g}, {b}

복사 형식을 **사용자 템플릿**으로 선택하면 위 템플릿을 확장한 결과가 클립보드에 복사됩니다.

## 설정 저장

가벼운 INI 파일만 사용합니다.

    %LOCALAPPDATA%\DKColorPicker\settings.ini

저장 항목:

- 확대 배율
- 평균 추출 범위
- 전역 단축키 프리셋
- 복사 형식
- 사용자 정의 템플릿

자동 시작은 별도로 현재 사용자 HKCU Run 항목을 사용합니다.

## 빌드

요구 사항:

- Windows 10/11 x64
- Visual Studio 2022 Build Tools 또는 그 이상
- CMake 3.23 이상

PowerShell:

    cmake -S . -B build -A x64
    cmake --build build --config Release

결과 파일:

    build\Release\DKColorPicker.exe

## 개발 방향

T01~T03에서 상주 프로그램, 픽커 품질, 색상 형식과 복사 템플릿을 구현했습니다. 이후에는 아래 기능을 순차적으로 추가합니다.

- 현재 색상 및 색상 도구 UI
- 직접 색상 입력
- CSS 이름 색상
- 명암 단계와 조화 배색
- WCAG 대비 검사
- 최근 색 / 즐겨찾기
- CSS/JSON/Tailwind/GIMP 팔레트 출력
- 테마 및 다국어

자세한 순서는 ROADMAP.md를 확인하세요.

기능 동작의 참고 대상은 공개된 ColorPick 설명이지만, 본 프로젝트는 별도의 독립 구현이며 원 프로그램과 제휴 또는 연관되어 있지 않습니다.

## 라이선스

MIT License. 자세한 내용은 LICENSE를 확인하세요.
