# sfall (한글판)

[![License](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Dev Build](https://github.com/sfall-team/sfall/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/phobos2077/sfall/actions/workflows/build.yml)
[![GitHub Pages](https://github.com/sfall-team/sfall/actions/workflows/gh-pages.yml/badge.svg)](https://github.com/phobos2077/sfall/actions/workflows/gh-pages.yml)

본 프로젝트는 클래식 게임 Fallout 2를 위한 엔진 수정 DLL 패키지(sfall)의 비공식 한국어 지원 포크입니다. EXE 파일 자체를 변경하지 않고 메모리 상에서 실행 파일을 직접 수정하여 작동하며, 한국어 출력 및 다양한 편의성 기능을 제공합니다.

**엔진 수정 사항 및 주요 기능:**
- 최신 운영체제(Windows 10/11 등) 환경에서의 안정적인 실행 및 호환성 지원
- 시작 맵, 게임 시간, 기술(Skill), 퍽(Perk), 치명타 테이블, 서적 등 다양한 시스템 설정을 외부 파일로 커스터마이징 가능
- 바닐라 엔진의 버그 수정
- 아이템 강조 표시(Shift 키), 동료 직접 제어 기능 등 사용자 편의성 기능 대거 추가
- 모더를 위한 스크립팅 기능 확장 (sfall 기능을 제어하는 다양한 신규 Opcode 및 바닐라 엔진의 미공개 내부 함수 사용 가능)

원작자: **Timeslip**

원문 설명: Interplay사 클래식 게임 Fallout 2의 엔진 수정 툴셋입니다. 원본 엔진의 버그 수정, 최신 운영체제에서의 정상 실행 지원, 모더들을 위한 다양한 추가 기능들을 탑재하고 있습니다.

---

## 설치 방법 (Installation)

- **Unofficial-KR-Sfall** 배포 패키지에는 **`ddraw.dll` 파일과 `LangFont.ini` 파일**이 함께 포함되어 있습니다.
- `ddraw.dll` 파일과 `LangFont.ini` 파일을 게임 루트 디렉터리(`fallout2.exe`가 있는 폴더)에 그대로 복사 및 덮어씌웁니다.

### 참고: 기존(순정) sfall 설치 방법
- [sfall 공식 릴리즈 아카이브](https://sourceforge.net/projects/sfall/files/)에서 `sfall_*.7z` 파일을 다운로드합니다.
- `ddraw.dll`를 Fallout 기본 디렉터리(`fallout2.exe`가 있는 폴더)에 압축 해제합니다. 이전 버전에서 업데이트하는 경우, Fallout의 `data\scripts\` 디렉터리에서 `gl_highlighting.int` 및 `gl_partycontrol.int` 파일을 삭제하십시오.
---

## 설정법 (Configuration)

한글 폰트 렌더링 및 번역 데이터를 정상적으로 불러오기 위해서는 게임 디렉터리에 있는 설정 파일을 다음과 같이 준비해야 합니다:

### 1. `LangFont.ini` 설정
`LangFont.ini` 파일을 `ddraw.dll`과 같은 게임 루트 디렉터리(`fallout2.exe`가 있는 폴더)에 둡니다. `ddraw.ini`에는 한글 폰트 설정을 추가하지 않습니다.

Windows에 설치된 폰트를 사용하려면 `Name`에 폰트 이름을 그대로 입력합니다. 예를 들어 `돋움`, `맑은 고딕`, `궁서`, `바탕`처럼 한글 폰트명도 사용할 수 있습니다. 별도 폰트 파일을 함께 사용하려면 선택적으로 `File`에 게임 폴더 기준 상대 경로 또는 절대 경로를 입력합니다.

```ini
[TEXT]
; 작은 글꼴 (대화문, 로그 메시지)
Name=돋움
Size=11
Weight=400

[BUTTON]
; 큰 UI 글꼴 (대화 제목, 일반 버튼)
Name=돋움
Size=19
Weight=5000

[GNW]
; GNW UI 글꼴 (메인 메뉴 제목, 커스텀 버튼)
Name=돋움
Size=19
Weight=5000

[MISC]
; 한글 텍스트 렌더러 사용 여부 (0 = 끔, 1 = 켬)
Enable=1
ButtonThreshold=16
```

예를 들어 게임 폴더 밖에 있는 폰트 파일을 직접 지정하려면 다음처럼 절대 경로를 사용할 수 있습니다:

```ini
[TEXT]
Name=맑은 고딕
File=C:\Windows\Fonts\malgun.ttf
```

### 2. `fallout2.cfg` 설정 수정
텍스트 편집기로 `fallout2.cfg` 파일을 열어 `[system]` 섹션의 `language` 설정을 `korean`으로 변경합니다. 이렇게 하면 엔진이 기존 `data\text\english\` 대신 `data\text\korean\` 폴더의 번역 데이터를 불러옵니다:

```ini
[system]
; 게임 데이터 언어 폴더를 korean으로 설정
language=korean
```

---

## 제거 방법 (Uninstallation)

Fallout 설치 디렉터리에서 `ddraw.dll`, `LangFont.ini`, `sfall.dat` 파일을 삭제하고, `mods` 폴더 내의 `sfall-mods.ini` 파일을 삭제하십시오.

---

## 사용 방법 (Usage)

이 모드는 텍스트 편집기(메모장 등)로 열 수 있는 `ddraw.ini`, `LangFont.ini` 및 `sfall-mods.ini` 파일을 통해 설정할 수 있습니다. 한글 폰트 렌더러 설정은 `LangFont.ini`에서만 읽습니다. 각 설정 옵션에 대한 상세한 설명은 해당 파일들의 주석에 적혀 있습니다. 주석에서 DX 스캔코드(scancode)를 참조하는 경우, 전체 코드 목록은 아래 링크에서 확인할 수 있습니다:
https://kippykip.com/b3ddocs/commands/scancodes.htm

수정되지 않은 기본 `ddraw.ini` 파일을 사용하는 초기 상태에서는 마우스 가운데 버튼으로 무기 전환이 가능하며, 마우스 휠을 통해 위/아래 방향키가 연동되는 모든 메뉴를 스크롤할 수 있습니다. **Ctrl**을 누른 상태에서 숫자 키패드 0~6(Num Lock 해제 상태)을 누르면 게임 속도가 조절됩니다. **왼쪽 Ctrl**을 누른 채로 인벤토리 아이템을 클릭하면 인벤토리 목록 간에 아이템을 한 번에 이동할 수 있습니다. **왼쪽 Shift**를 누르면 바닥에 있는 아이템들이 강조 표시되며, 키를 꾹 누르고 있으면 한 번에 아이템 묶음 전체를 이동할 수 있습니다. 스크립트 확장 엔진 및 각종 엔진 수정 기능들도 기본적으로 활성화되어 있습니다. 개발자가 원래 의도하지 않은 방식으로 게임 플레이를 변경하는 대부분의 비바닐라적 옵션들은 기본적으로 비활성화되어 있습니다.

[__Wine__](https://www.winehq.org/) (리눅스/맥) 사용자 참고 사항:
`winecfg`에서 `ddraw.dll`에 대한 DLL 재정의(DLL overrides)를 **"네이티브, 내장(native, builtin)"**으로 설정하거나, 커맨드 라인에서 `WINEDLLOVERRIDES="ddraw=n,b"` 환경변수를 주어 실행해야 합니다. 만약 다른 사운드 파일을 재생하려면 GStreamer Good 32비트 플러그인도 함께 설치해야 합니다.

---

## 빌드 방법 (Build Instructions)

### 필수 요구사항 (Prerequisites)
* **"C++용 Windows XP 지원"** 구성요소가 포함된 Visual Studio 2015. 만약 Visual Studio 2017/2019/2022 버전을 사용 중이라면 **"VC++ 2015.3 v14.00 (v140) 데스크톱용 도구 세트"** 구성요소도 추가로 설치해야 합니다.
* [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812). 또한, 구버전 DirectX SDK의 `ddraw.lib` 및 `dinput.lib` 파일이 추가로 필요합니다. DirectX SDK April 2007의 `Lib\x86\ddraw.lib`와 `Lib\x86\dinput.lib`로 빌드할 수 있으며, 이 파일들은 [DirectX SDK Collection 저장소](https://github.com/NovaRain/DXSDK_Collection)에서도 다운로드할 수 있습니다.
* [DirectX Runtime (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=8109). DirectX SDK 설치 관리자를 통해서도 설치할 수 있습니다.

### 빌드 단계 (Steps)
1. 저장소 템플릿을 사용하여 `postbuild.cmd` 파일을 설정합니다.
2. Visual Studio로 솔루션 파일(`ddraw.sln`)을 열고 **ReleaseXP** 구성(sfall 배포판 릴리즈 빌드 구성)으로 빌드합니다.
3. 빌드가 정상 완료되면 Fallout 2 게임 디렉터리에 새로운 sfall `ddraw.dll` 파일이 복사/생성됩니다.

### 커맨드 라인 빌드 환경 (Minimalist Setup)
Full IDE(Visual Studio)가 필요하지 않은 경우, Visual Studio Build Tools를 사용할 수 있습니다. (예: Visual Studio 2017 Build Tools 기준)
1. 명령 프롬프트(CMD)를 통해 아래 명령어로 필요한 도구를 설치합니다:
   ```cmd
   vs_BuildTools.exe --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Workload.MSBuildTools --add Microsoft.VisualStudio.Component.VC.140 --add Microsoft.VisualStudio.Component.WinXP --add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.WinXP --passive
   ```
2. 저장소 템플릿을 사용하여 `postbuild.cmd` 파일을 설정합니다.
3. **x86 Native Tools Command Prompt** 창을 열고 MSBuild를 실행하여 빌드합니다:
   ```cmd
   MSBuild.exe ddraw.sln /t:Clean;Build /p:Configuration=ReleaseXP /p:Platform=Win32
   ```

---

## 추가 정보 (Additional info)

* [변경 사항 (Changelog)](CHANGELOG.md)
* [스크립팅 가이드 문서 (Scripting Documentation)](https://sfall-team.github.io/sfall/)
* Fallout 엔진 IDA 데이터베이스: [IDA Pro 6.8용](https://www.dropbox.com/s/tm0nyx0lnk4yui0/Fallout_1_and_2_IDA68.rar?dl=1) | [IDA Pro 7.0용](https://www.dropbox.com/s/61srq09pn8grfpu/Fallout_1_and_2_IDA70.rar?dl=1) (주석은 러시아어로 작성되어 있습니다)
* [Fallout 2 레퍼런스 에디션 (Reference Edition)](https://github.com/alexbatalov/fallout2-re)
