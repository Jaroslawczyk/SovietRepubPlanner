@echo off
rem Offline planner check: builds tools\plan_test.exe, runs it (args: width height population [house ids...]) and draws plan.png
setlocal
cd /d "%~dp0\.."
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist build\tobj mkdir build\tobj
cl /nologo /O2 /MT /EHsc /utf-8 /std:c++17 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Fobuild\tobj\ /Febuild\plan_test.exe ^
  tools\plan_test.cpp src\catalog.cpp src\planner.cpp user32.lib >build\plan_test_build.txt || (type build\plan_test_build.txt & exit /b 1)
cd build
.\plan_test.exe %*
python ..\tools\plan_draw.py plan.txt plan.png
