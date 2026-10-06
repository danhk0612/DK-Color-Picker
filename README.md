# DK Color Picker

Windows 10/11용 초경량 화면 색상 추출기입니다.

ColorPick과 같은 빠른 사용 흐름을 목표로 하지만, 기존 프로그램의 코드나 자산을 사용하지 않는 독립적인 MIT 구현입니다.

## 현재 구현

- 네이티브 C++20 + Win32 API
- 설치가 필요 없는 단일 EXE
- 트레이 상주
- 기본 전역 단축키 Ctrl+Alt+C
- 가상 데스크톱 전체 화면 캡처 후 화면 정지
- 다중 모니터 좌표 지원
- Per-Monitor DPI Awareness V2
- 픽셀 확대경과 HEX/RGB 미리보기
- 클릭한 픽셀의 HEX 값을 클립보드에 복사
- Esc 또는 우클릭으로 색 추출 취소
- 트레이 메뉴에서 색 추출 / Windows 자동 시작 / 종료
- HKCU Run 기반 사용자별 자동 시작
- 중복 실행 방지
- 정적 MSVC 런타임 사용으로 별도 런타임 설치 없이 실행

## 사용법

1. DKColorPicker.exe를 실행합니다.
2. 프로그램은 창을 띄우지 않고 트레이에 상주합니다.
3. Ctrl+Alt+C를 누릅니다.
4. 확대경으로 원하는 픽셀을 확인하고 클릭합니다.
5. #RRGGBB 형식의 값이 클립보드에 복사됩니다.
6. 종료하려면 트레이 아이콘을 우클릭하고 종료를 선택합니다.

Windows 시작 시 자동 실행은 트레이 메뉴에서 켜거나 끌 수 있습니다. 관리자 권한은 필요하지 않습니다.

## 빌드

요구 사항:

- Windows 10/11 x64
- Visual Studio 2022 Build Tools 또는 Visual Studio 2022
- CMake 3.23 이상

PowerShell:

    cmake -S . -B build -A x64
    cmake --build build --config Release

결과 파일:

    build\Release\DKColorPicker.exe

## 개발 방향

현재 v0.1은 상주 프로그램과 색 추출 코어를 먼저 검증하는 단계입니다. 이후에는 아래 기능을 순차적으로 추가합니다.

- 1x1~9x9 평균 추출
- 확대 배율 조절
- HEX/RGB/HSL/HSV/HWB/CMYK/LAB/OKLCH 등 복사 형식
- 사용자 정의 복사 템플릿
- 최근 색 / 즐겨찾기
- 명암 단계와 조화 배색
- WCAG 대비 검사
- CSS/JSON/Tailwind/GIMP 팔레트 출력
- 테마 및 다국어
- 단축키와 세부 설정 UI

기능 동작의 참고 대상은 공개된 ColorPick 설명이지만, 본 프로젝트는 별도의 독립 구현이며 원 프로그램과 제휴 또는 연관되어 있지 않습니다.

## 라이선스

MIT License. 자세한 내용은 LICENSE를 확인하세요.
