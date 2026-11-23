@echo off
setlocal
cd /d "%~dp0"

rem Build the complete 32-bit Advanced Pawn Compiler set used by Pawno.
if not exist bin mkdir bin
if not exist build mkdir build
if not exist build\pawnc mkdir build\pawnc
if not exist build\pawndisasm mkdir build\pawndisasm

where cl >nul 2>nul
if not errorlevel 1 goto have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_vs
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto no_vs
if not exist "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" goto no_vs
echo Found Visual Studio: %VSDIR%
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
where cl >nul 2>nul
if errorlevel 1 goto no_vs

:have_cl
cl 2>&1 | findstr /i /r /c:"for x86" /c:"for 80x86" >nul
if errorlevel 1 (
  echo [ERROR] An x86 MSVC compiler is required. Run from a plain cmd window
  echo         or the x86 Native Tools Command Prompt.
  exit /b 1
)

rem CMake generates this header from version.h.in.
(
  echo #define VERSION_MINOR 11
  echo #define VERSION_MAJOR 3
  echo #define VERSION_BUILD 0
  echo #define VERSION_STR "3.11.0"
  echo #define VERSION_INT 779
) > build\version.h

set "COMMON=/nologo /O2 /Oi /GL /GS- /MT /DNDEBUG /DHAVE_STDINT_H /DHAVE_INTTYPES_H /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /D_CRT_DISABLE_PERFCRIT_LOCKS /I source\compiler /I build"
if "%APC_PROFILE%"=="1" set "COMMON=%COMMON% /DPC_TIMING"
set "COMPILER_SOURCES=source\compiler\hashtable\wrap_hashtable.c source\compiler\libpawnc.c source\compiler\lstring.c source\compiler\memfile.c source\compiler\sc1.c source\compiler\sc2.c source\compiler\sc3.c source\compiler\sc4.c source\compiler\sc5.c source\compiler\sc6.c source\compiler\sc7.c source\compiler\sci18n.c source\compiler\sclist.c source\compiler\scmemfil.c source\compiler\scstate.c source\compiler\scvars.c"

echo [1/3] Building bin\pawnc.dll ...
cl %COMMON% /DNO_MAIN /DPAWNC_DLL /LD /Fo:build\pawnc\ %COMPILER_SOURCES% source\compiler\libpawnc.def /link /LTCG /OUT:bin\pawnc.dll /IMPLIB:build\pawnc.lib user32.lib
if errorlevel 1 goto build_failed

echo [2/3] Building bin\pawncc.exe ...
cl %COMMON% source\compiler\pawncc.c build\pawnc.lib /Fe:bin\pawncc.exe /link /LTCG /MACHINE:X86
if errorlevel 1 goto build_failed

echo [3/3] Building bin\pawndisasm.exe ...
cl %COMMON% /Fo:build\pawndisasm\ source\compiler\pawndisasm.c source\amx\amxdbg.c /Fe:bin\pawndisasm.exe /link /LTCG /MACHINE:X86
if errorlevel 1 goto build_failed

echo.
echo Advanced Pawn Compiler 3.11.0 build complete. Pawno files:
echo   bin\pawncc.exe
echo   bin\pawnc.dll
echo   bin\pawndisasm.exe
echo Copy all three files together into the Pawno folder.
exit /b 0

:build_failed
echo.
echo [ERROR] Build failed. See the compiler output above.
exit /b 1

:no_vs
echo [ERROR] Visual Studio C++ tools were not found.
echo Install "Desktop development with C++" in Visual Studio Installer.
exit /b 1
