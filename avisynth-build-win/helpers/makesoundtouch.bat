@echo off
setlocal

rem -----------------------------------------------------------------------
rem  Build SoundTouch static libs for use with AviSynth+ TimeStretch plugin
rem  Produces five variants:
rem    v141_xp  x86  ->  C:\avsplus_build_deps\xp-x86-32\lib\SoundTouch.lib
rem    v141_xp  x64  ->  C:\avsplus_build_deps\xp-x86-64\lib\SoundTouch.lib
rem    v143     x86  ->  C:\avsplus_build_deps\v143-x86-32\lib\SoundTouch.lib
rem    v143     x64  ->  C:\avsplus_build_deps\v143-x86-64\lib\SoundTouch.lib
rem    MinGW  ARM64  ->  C:\avsplus_build_deps\mingw-arm64\lib\libSoundTouch.a
rem                       (Windows on ARM64, cross-compiled with
rem                        llvm-mingw -- see LLVM_MINGW_ROOT below)
rem -----------------------------------------------------------------------

set SOURCE_ROOT=C:\avsplus-build-deps\soundtouch
set BUILD_ROOT=C:\avsplus-build-deps\soundtouch-build
set INST_XP_X86=C:\avsplus_build_deps\xp-x86-32
set INST_XP_X64=C:\avsplus_build_deps\xp-x86-64
set INST_V143_X86=C:\avsplus_build_deps\v143-x86-32
set INST_V143_X64=C:\avsplus_build_deps\v143-x86-64
set INST_ARM64=C:\avsplus_build_deps\mingw-arm64
set LLVM_MINGW_ROOT=C:\avsplus_build_deps\llvm-mingw-20261020-ucrt-x86_64
set TOOLCHAIN_FILE=%~dp0..\toolchains\llvm-mingw-aarch64.cmake
set BUILD_JOBS=8

rem --- Clone if not already present ---
if not exist "%SOURCE_ROOT%\.git" (
    git clone https://codeberg.org/soundtouch/soundtouch "%SOURCE_ROOT%"
    if errorlevel 1 goto :error
) else (
    echo SoundTouch source already present, skipping clone.
)

rem -----------------------------------------------------------------------
rem  BUILD 1/5 -- v141_xp  x86
rem -----------------------------------------------------------------------
echo.
echo [1/5] SoundTouch  v141_xp  Win32...
set BLD=%BUILD_ROOT%\xp-x86
if exist "%BLD%" rmdir /s /q "%BLD%" 2>nul
mkdir "%BLD%"
cmake -S "%SOURCE_ROOT%" -B "%BLD%" ^
  -G "Visual Studio 17 2022" -A Win32 -T "v141_xp" ^
  -DCMAKE_INSTALL_PREFIX="%INST_XP_X86%" ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if errorlevel 1 goto :error
cmake --build "%BLD%" --config Release -j %BUILD_JOBS% --target install
if errorlevel 1 goto :error

rem -----------------------------------------------------------------------
rem  BUILD 2/5 -- v141_xp  x64
rem -----------------------------------------------------------------------
echo.
echo [2/5] SoundTouch  v141_xp  x64...
set BLD=%BUILD_ROOT%\xp-x64
if exist "%BLD%" rmdir /s /q "%BLD%" 2>nul
mkdir "%BLD%"
cmake -S "%SOURCE_ROOT%" -B "%BLD%" ^
  -G "Visual Studio 17 2022" -A x64 -T "v141_xp" ^
  -DCMAKE_INSTALL_PREFIX="%INST_XP_X64%" ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if errorlevel 1 goto :error
cmake --build "%BLD%" --config Release -j %BUILD_JOBS% --target install
if errorlevel 1 goto :error

rem -----------------------------------------------------------------------
rem  BUILD 3/5 -- v143  x86
rem -----------------------------------------------------------------------
echo.
echo [3/5] SoundTouch  v143  Win32...
set BLD=%BUILD_ROOT%\v143-x86
if exist "%BLD%" rmdir /s /q "%BLD%" 2>nul
mkdir "%BLD%"
cmake -S "%SOURCE_ROOT%" -B "%BLD%" ^
  -G "Visual Studio 18 2026" -A Win32 -T "v143" ^
  -DCMAKE_INSTALL_PREFIX="%INST_V143_X86%" ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if errorlevel 1 goto :error
cmake --build "%BLD%" --config Release -j %BUILD_JOBS% --target install
if errorlevel 1 goto :error

rem -----------------------------------------------------------------------
rem  BUILD 4/5 -- v143  x64
rem -----------------------------------------------------------------------
echo.
echo [4/5] SoundTouch  v143  x64...
set BLD=%BUILD_ROOT%\v143-x64
if exist "%BLD%" rmdir /s /q "%BLD%" 2>nul
mkdir "%BLD%"
cmake -S "%SOURCE_ROOT%" -B "%BLD%" ^
  -G "Visual Studio 18 2026" -A x64 -T "v143" ^
  -DCMAKE_INSTALL_PREFIX="%INST_V143_X64%" ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if errorlevel 1 goto :error
cmake --build "%BLD%" --config Release -j %BUILD_JOBS% --target install
if errorlevel 1 goto :error

rem -----------------------------------------------------------------------
rem  BUILD 5/5 -- MinGW  ARM64 (Windows on ARM64, aarch64)
rem
rem  Cross-compiled on the x86_64 host with llvm-mingw.  Needs the
rem  Ninja generator -- the Visual Studio generator cannot
rem  cross-compile with a different compiler.
rem
rem  Two switches are needed:
rem    CMAKE_POLICY_VERSION_MINIMUM -- SoundTouch's CMakeLists.txt still
rem      says cmake_minimum_required(VERSION 3.1), which CMake 4.x
rem      refuses without this
rem    SOUNDSTRETCH=OFF -- the soundstretch command line utility is a
rem      GUI application for the MinGW build (it wants WinMain), and
rem      AviSynth+ needs only the static library
rem -----------------------------------------------------------------------
echo.
echo [5/5] SoundTouch  MinGW  ARM64 (aarch64)...
set BLD=%BUILD_ROOT%\mingw-arm64
if exist "%BLD%" rmdir /s /q "%BLD%" 2>nul
mkdir "%BLD%"
cmake -S "%SOURCE_ROOT%" -B "%BLD%" ^
  -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" ^
  -DAVS_LLVM_MINGW_ROOT="%LLVM_MINGW_ROOT%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%INST_ARM64%" ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
  -DSOUNDSTRETCH=OFF
if errorlevel 1 goto :error
cmake --build "%BLD%" -j %BUILD_JOBS% --target install
if errorlevel 1 goto :error

echo.
echo All SoundTouch builds complete.
echo.
echo  XP x86  : %INST_XP_X86%\lib\SoundTouch.lib
echo  XP x64  : %INST_XP_X64%\lib\SoundTouch.lib
echo  v143 x86: %INST_V143_X86%\lib\SoundTouch.lib
echo  v143 x64: %INST_V143_X64%\lib\SoundTouch.lib
echo  ARM64   : %INST_ARM64%\lib\libSoundTouch.a
goto :eof

:error
echo.
echo *** BUILD FAILED ***
exit /b 1
