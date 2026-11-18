@echo off
rem Builds build\dinput8.dll (x64). Run from any shell; needs Visual Studio (Build Tools) with the C++ x64 tools and git.
setlocal
cd /d "%~dp0"

rem Visual Studio: found with vswhere (any edition / version), else the path used during development
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" ( echo ERROR: Visual Studio C++ x64 tools not found. & exit /b 1 )
call "%VCVARS%" >nul || exit /b 1

rem Dear ImGui: fetched at the commit the mod was developed with
set IMGUI=third_party\imgui
set IMGUI_COMMIT=77fe4588e6af934b4167b91312174ca3fbb0d71a
if not exist %IMGUI%\imgui.cpp (
  git clone https://github.com/ocornut/imgui %IMGUI% || exit /b 1
  git -C %IMGUI% checkout %IMGUI_COMMIT% || exit /b 1
)

if not exist build\obj mkdir build\obj
cl /nologo /O2 /MT /EHsc /utf-8 /std:c++17 /W3 /LD /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
  /I %IMGUI% /I %IMGUI%\backends /Fobuild\obj\ /Febuild\dinput8.dll ^
  src\main.cpp src\i18n.cpp src\engine.cpp src\catalog.cpp src\planner.cpp src\ui.cpp ^
  %IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp ^
  %IMGUI%\backends\imgui_impl_win32.cpp %IMGUI%\backends\imgui_impl_dx11.cpp ^
  /link /DEF:src\dinput8.def d3d11.lib dxgi.lib user32.lib d3dcompiler.lib ole32.lib windowscodecs.lib || exit /b 1
echo OK: build\dinput8.dll
