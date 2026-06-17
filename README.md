# sfall

[![License](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Dev Build](https://github.com/sfall-team/sfall/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/phobos2077/sfall/actions/workflows/build.yml)
[![GitHub Pages](https://github.com/sfall-team/sfall/actions/workflows/gh-pages.yml/badge.svg)](https://github.com/phobos2077/sfall/actions/workflows/gh-pages.yml)

A set of engine modifications for the classic game Fallout 2 in the form of a DLL, which modifies executable in memory without changing anything in EXE file itself.

**Engine modifications include:**
- Better support for modern operating systems
- Externalizing many settings like starting map and game time, skills, perks, critical hit tables, books, etc.
- Bug fixes
- Many additional features for users, such as item highlight button, party member control, etc.
- Extended scripting capabilities for modders (many new opcodes to control sfall features as well as previously unavailable vanilla engine functions)

Original author: **Timeslip**

Original description: A set of engine modifications for the classic game Fallout 2 by Interplay. Includes fixes for bugs in the original engine, allows Fallout to run correctly on modern operating systems, and adds additional features for modders.

## Installation (설치 방법)

### English
- The release package of **Unofficial-KR-Sfall** contains **only `ddraw.dll`**.
- Copy the `ddraw.dll` file and overwrite it in your Fallout 2 / Fallout Sonora root directory (where `fallout2.exe` resides).
- Ensure your Korean translation font files (e.g., `ChungjuKimSaeng.ttf`, `NanumSquareNeo-aLt.ttf`, `NanumSquareNeo-cBd.ttf`) are placed in `data\fonts\korean\`.

### Korean (한국어)
- **Unofficial-KR-Sfall** 배포 패키지에는 **`ddraw.dll` 파일만 포함**되어 있습니다.
- 빌드된 `ddraw.dll` 파일을 게임 루트 디렉터리(`fallout2.exe`가 있는 폴더)에 덮어씌웁니다.
- 한국어 출력용 폰트 파일들(예: `ChungjuKimSaeng.ttf`, `NanumSquareNeo-aLt.ttf`, `NanumSquareNeo-cBd.ttf`)이 `data\fonts\korean\` 경로에 있는지 확인해 주세요.

---

## Configuration (설정법)

To enable Korean rendering and load the translation correctly, you must edit two configuration files in your game directory:

### 1. `ddraw.ini` 설정 추가
Open `ddraw.ini` with a text editor, search for the `[Main]` section, and add the following block to configure the Korean Text Renderer, GDI face names, and rendering dimensions:

```ini
[Main]
; Enable the Korean Text Renderer (0 = disabled, 1 = enabled)
KoreanText=1
KoreanTextButtonThreshold=16

; 1. Small Font (Dialog text, log messages)
KoreanTextTextFontFile=data\fonts\korean\NanumSquareNeo-aLt.ttf
KoreanTextTextFontFace=NanumSquare Neo Light
KoreanTextTextFontHeight=10
KoreanTextTextCellWidth=10
KoreanTextTextRenderHeight=11
KoreanTextTextFontWeight=400

; 2. Large UI Font (Dialog headers, standard buttons)
KoreanTextFontFile=data\fonts\korean\ChungjuKimSaeng.ttf
KoreanTextFontFace=ChungjuKimSaeng TTF
KoreanTextFontHeight=17
KoreanTextCellWidth=19
KoreanTextRenderHeight=25
KoreanTextFontWeight=700

; 3. GNW UI Font (Main Menu titles, custom buttons)
KoreanTextGnwFontFile=data\fonts\korean\ChungjuKimSaeng.ttf
KoreanTextGnwFontFace=ChungjuKimSaeng TTF
KoreanTextGnwFontHeight=17
KoreanTextGnwCellWidth=19
KoreanTextGnwRenderHeight=25
KoreanTextGnwFontWeight=700
```

### 2. `fallout2.cfg` 설정 수정
Open `fallout2.cfg` with a text editor, look for the `[system]` section, and modify the `language` setting to `korean` so the engine loads assets from `data\text\korean\` instead of `data\text\english\`:

```ini
[system]
; Set the game assets language directory to Korean
language=korean
```

## Uninstallation

Delete `ddraw.dll`, `ddraw.ini`, and `sfall.dat` from your Fallout directory, and delete `sfall-mods.ini` from the `mods` folder.

## Usage

This mod is configured via the `ddraw.ini` and `sfall-mods.ini` files, which can be opened with any text editor. Details of every configerable option are included in those files. Where a comment refers to a DX scancode, the complete list of codes can be found at the link below:\
https://kippykip.com/b3ddocs/commands/scancodes.htm

In a default installation using an unmodified copy of `ddraw.ini`, the middle mouse button will be set to switch between weapons and the mouse wheel will be set to scroll through any menus that respond to the up/down arrow keys. Holding **Ctrl** and hitting numpad keys 0 to 6 (with Num Lock off) will adjust the game speed. Holding **left Ctrl** will let you move items between inventory lists by simply clicking on them. Pressing **left Shift** will highlight items on the ground, and holding the key will let you move an entire stack of items at once. The script extender and any engine fixes are also enabled. Most of the options that change gameplay in some way not originally intended by the developers are disabled.

For [__Wine__](https://www.winehq.org/) users:\
You need to set DLL overrides for `ddraw.dll` to __"native, builtin"__ in `winecfg` or use `WINEDLLOVERRIDES="ddraw=n,b"` to run Fallout from the command line. If you want to play alternative sound files, you'll also need to install GStreamer Good 32-bit plugins.

## Build Instructions

### Prerequisites:

* Visual Studio 2015 with **"Windows XP support for C++"** component. If you're using Visual Studio 2017/2019/2022, make sure to install **"VC++ 2015.3 v14.00 (v140) toolset for desktop"** component as well.
* [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812). You will also need `ddraw.lib` from DXSDK February 2010 and `dinput.lib` from DXSDK August 2007. Both files can be found in the [DirectX SDK Collection repo](https://github.com/NovaRain/DXSDK_Collection).
* [DirectX Runtime (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=8109). You can also install it from DirectX SDK installer.

### Steps:

1. Set up a `postbuild.cmd` using the template from the repo.
2. Open the solution file and build with the **ReleaseXP** configuration (this is the one used for sfall releases).
3. If everything is set up correctly, you should have a new sfall `ddraw.dll` in your Fallout 2 directory.

### Minimalist Setup:

If you don't need a full-fledged IDE, you can use Visual Studio Build Tools instead. Taking [Visual Studio 2017 Build Tools](https://aka.ms/vs/15/release/vs_buildtools.exe) for example:
1. Install using the command line:
   ```
   vs_BuildTools.exe --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Workload.MSBuildTools --add Microsoft.VisualStudio.Component.VC.140 --add Microsoft.VisualStudio.Component.WinXP --add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.WinXP --passive
   ```
2. Set up a `postbuild.cmd` using the template from the repo.
3. Use MSBuild in **x86 Native Tools Command Prompt** to build sfall:
   ```
   MSBuild.exe ddraw.sln /t:Clean;Build /p:Configuration=ReleaseXP /p:Platform=Win32
   ```

## Additional info

* [Changelog](CHANGELOG.md)
* [Scripting Documentation](https://sfall-team.github.io/sfall/)
* Fallout Engine IDA Database: [for IDA Pro 6.8](https://www.dropbox.com/s/tm0nyx0lnk4yui0/Fallout_1_and_2_IDA68.rar?dl=1 "Download from Dropbox") | [for IDA Pro 7.0](https://www.dropbox.com/s/61srq09pn8grfpu/Fallout_1_and_2_IDA70.rar?dl=1 "Download from Dropbox") (comments are in Russian)
* [Fallout 2 Reference Edition](https://github.com/alexbatalov/fallout2-re)
