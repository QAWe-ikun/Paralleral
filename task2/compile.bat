 @echo off
  REM ============================================================================
  REM MPI Collective Matrix Multiplication - Build Script
  REM ============================================================================

  REM Get script directory
  for %%i in (%~dp0) do set SCRIPT_DIR=%%~fi

  echo ========================================
  echo MPI Collective Matrix Multiplication - Build Script
  echo ========================================
  echo Source: %SCRIPT_DIR%\src
  echo Output: %SCRIPT_DIR%\bin
  echo Obj:    %SCRIPT_DIR%\obj
  echo ========================================

  REM Setup MSVC environment
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

  where cl >nul 2>&1
  if %ERRORLEVEL% neq 0 (
      echo Error: MSVC compiler not found
      echo Please install Visual Studio Build Tools
      exit /b 1
  )

  REM Create directories
  if not exist "%SCRIPT_DIR%\bin" mkdir "%SCRIPT_DIR%\bin"
  if not exist "%SCRIPT_DIR%\obj" mkdir "%SCRIPT_DIR%\obj"

  echo.
  echo [1/4] Building mpi_collective_mat_mul.exe...

  cl /O2 /EHsc /I"D:\Microsoft SDKs\MPI\Include" /Fo"%SCRIPT_DIR%\obj\mpi_collective_mat_mul.obj" /Fe"%SCRIPT_DIR%\bin\mpi_collective_mat_mul.exe" "%SCRIPT_DIR%\src\mpi_collective_mat_mul.cpp" "D:\Microsoft SDKs\MPI\Lib\x64\msmpi.lib"

  if %ERRORLEVEL% neq 0 (
      echo Build FAILED!
  ) else (
      echo Build SUCCESS: bin\mpi_collective_mat_mul.exe
  )

  echo.
  echo [2/4] Building mpi_col_distrib_mat_mul.exe...

  if exist "%SCRIPT_DIR%\bin\mpi_col_distrib_mat_mul.exe" (
      echo Deleting old mpi_col_distrib_mat_mul.exe...
      del /Q "%SCRIPT_DIR%\bin\mpi_col_distrib_mat_mul.exe"
  )
  cl /O2 /EHsc /I"D:\Microsoft SDKs\MPI\Include" /Fo"%SCRIPT_DIR%\obj\mpi_col_distrib_mat_mul.obj" /Fe"%SCRIPT_DIR%\bin\mpi_col_distrib_mat_mul.exe" "%SCRIPT_DIR%\src\mpi_col_distrib_mat_mul.cpp" "D:\Microsoft SDKs\MPI\Lib\x64\msmpi.lib"

  if %ERRORLEVEL% neq 0 (
      echo Build FAILED!
  ) else (
      echo Build SUCCESS: bin\mpi_col_distrib_mat_mul.exe
  )

  echo.
  echo [3/4] Building mpi_2d_block_mat_mul.exe...

  cl /O2 /EHsc /I"D:\Microsoft SDKs\MPI\Include" /Fo"%SCRIPT_DIR%\obj\mpi_2d_block_mat_mul.obj" /Fe"%SCRIPT_DIR%\bin\mpi_2d_block_mat_mul.exe" "%SCRIPT_DIR%\src\mpi_2d_block_mat_mul.cpp" "D:\Microsoft SDKs\MPI\Lib\x64\msmpi.lib"

  if %ERRORLEVEL% neq 0 (
      echo Build FAILED!
  ) else (
      echo Build SUCCESS: bin\mpi_2d_block_mat_mul.exe
  )

  echo.
  echo [4/4] Building serial_mat_mul.exe...

  cl /O2 /EHsc /Fo"%SCRIPT_DIR%\obj\serial_mat_mul.obj" /Fe"%SCRIPT_DIR%\bin\serial_mat_mul.exe" "%SCRIPT_DIR%\src\serial_mat_mul.cpp"

  if %ERRORLEVEL% neq 0 (
      echo Build FAILED!
  ) else (
      echo Build SUCCESS: bin\serial_mat_mul.exe
  )

  echo.
  echo ========================================
  echo Build Complete
  echo ========================================