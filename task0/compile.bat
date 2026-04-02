@echo off
REM ============================================================================
REM 矩阵乘法 - 编译脚本 (Windows)
REM ============================================================================

setlocal enabledelayedexpansion

REM Get script directory (task0 folder)
set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

echo ========================================
echo Matrix Multiplication - Build Script
echo ========================================
echo Source: %SCRIPT_DIR%\src
echo Output: %SCRIPT_DIR%\bin
echo Obj:    %SCRIPT_DIR%\obj
echo ========================================

REM MKL paths (short names to avoid spaces)
set "MKL_INCLUDE=C:\PROGRA~2\Intel\oneAPI\mkl\2025.3\include"
set "MKL_LIB=C:\PROGRA~2\Intel\oneAPI\mkl\2025.3\lib"

REM Check g++ compiler
where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: g++ compiler not found
    exit /b 1
)

REM Create directories
if not exist "%SCRIPT_DIR%\bin" mkdir "%SCRIPT_DIR%\bin"
if not exist "%SCRIPT_DIR%\obj" mkdir "%SCRIPT_DIR%\obj"

echo.
echo [1/2] Building matrix_all_mkl.exe (with MKL)...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
if exist "%MKL_LIB%\mkl_rt.lib" (
    cl /Od /EHsc /DUSE_MKL /I"%MKL_INCLUDE%" /Fe:"%SCRIPT_DIR%\bin\matrix_all_mkl.exe" /Fo:"%SCRIPT_DIR%\obj\matrix_all_mkl.obj" "%SCRIPT_DIR%\src\matrix_mul_all_mkl.cpp" "%MKL_LIB%\mkl_rt.lib"
    if %ERRORLEVEL% neq 0 (
        echo Build FAILED!
    ) else (
        echo Build SUCCESS: bin\matrix_all_mkl.exe
    )
) else (
    echo Error: MKL library not found
)

echo.
echo [2/2] Building matrix_ikj_opt.exe (-O3 -march=native)...
g++ -O3 -march=native -o "%SCRIPT_DIR%\bin\matrix_ikj_opt.exe" "%SCRIPT_DIR%\src\matrix_mul_ikj_opt.cpp"
if %ERRORLEVEL% neq 0 (
    echo Build FAILED!
) else (
    echo Build SUCCESS: bin\matrix_ikj_opt.exe
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Executables:
dir /b "%SCRIPT_DIR%\bin\*.exe" 2>nul
echo.
echo Usage:
echo   %SCRIPT_DIR%\bin\matrix_all_mkl.exe 4 1024
echo   %SCRIPT_DIR%\bin\matrix_ikj_opt.exe 1024
echo.
echo Parameters: M, K, N in range [512, 2048]
echo.
