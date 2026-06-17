@echo off
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VS_PATH%" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)

echo Using Developer Command Prompt from: %VS_PATH%
call "%VS_PATH%" x86

echo Compiling ddraw.dll...
cl.exe /O2 /EHsc /LD /Fe"ddraw.dll" ddraw_proxy.cpp /link /def:ddraw_proxy.def user32.lib gdi32.lib
echo Done.
