@echo off
REM ============================================================================
REM MPI 矩阵乘法 - 编译脚本 (Windows)
REM 需要: MSVC 编译器 + MS-MPI SDK
REM ============================================================================

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

set "MPI_INCLUDE=D:\Microsoft SDKs\MPI\Include"
set "MPI_LIB=D:\Microsoft SDKs\MPI\Lib\x64"
set "MPI_BIN=D:\Microsoft MPI\Bin"

echo ========================================
echo MPI Matrix Multiplication - Build Script
echo ========================================
echo Source: %SCRIPT_DIR%\src
echo Output: %SCRIPT_DIR%\bin
echo ========================================

REM Check MSVC compiler
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: MSVC compiler not found. Install Visual Studio 2022 with C++ workload.
    exit /b 1
)

REM Check MPI headers
if not exist "%MPI_INCLUDE%\mpi.h" (
    echo Error: MPI headers not found at %MPI_INCLUDE%
    exit /b 1
)

REM Create directories
if not exist "%SCRIPT_DIR%\bin" mkdir "%SCRIPT_DIR%\bin"

echo.
echo [1/1] Building mpi_matrix_mul.exe...
cl /O2 /EHsc /I"%MPI_INCLUDE%" /Fe:"%SCRIPT_DIR%\bin\mpi_matrix_mul.exe" "%SCRIPT_DIR%\src\mpi_matrix_mul.c" "%MPI_LIB%\msmpi.lib"
if %ERRORLEVEL% neq 0 (
    echo Build FAILED!
) else (
    echo Build SUCCESS: bin\mpi_matrix_mul.exe
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Usage:
echo   mpiexec -n ^<procs^> %SCRIPT_DIR%\bin\mpi_matrix_mul.exe ^<m^> ^<n^> ^<k^>
echo.
echo Examples:
echo   mpiexec -n 4 %SCRIPT_DIR%\bin\mpi_matrix_mul.exe 128
echo   mpiexec -n 8 %SCRIPT_DIR%\bin\mpi_matrix_mul.exe 512
echo   mpiexec -n 4 %SCRIPT_DIR%\bin\mpi_matrix_mul.exe 256 256 256
echo.
echo Note: Minimum 2 processes required (1 master + 1 worker)
echo.
