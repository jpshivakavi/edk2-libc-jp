@echo off
REM Phase V2 proof: MSVC sees pyconfig.h under /DUEFI_MSVC_64 (Python312.inf X64 MSFT flags).
setlocal
set ROOT=%~dp0..
set INC=%ROOT%\Include
set INCINT=%ROOT%\Include\internal
set OBJ=%TEMP%\verify_pyconfig_msft_%RANDOM%.obj

if not exist "%INC%\pyconfig.h" (
  echo ERROR: %INC%\pyconfig.h missing. Run srcprep.py in Python-3.12.13 first.
  exit /b 1
)

where cl >nul 2>&1
if errorlevel 1 (
  echo ERROR: cl not on PATH. Open "x64 Native Tools Command Prompt for VS 2022" or run edksetup.bat first.
  exit /b 1
)

echo === V2 MSVC verify: cl /DUEFI_MSVC_64 ===
cl /nologo /c /W4 /DUEFI_MSVC_64 /DUEFI_C_SOURCE ^
  /I"%INC%" /I"%INCINT%" ^
  "%~dp0verify_pyconfig_sizes.c" /Fo"%OBJ%"
set ERR=%ERRORLEVEL%
if exist "%OBJ%" del /q "%OBJ%"
if %ERR% neq 0 (
  echo FAILED: V2 MSVC pyconfig verify
  exit /b %ERR%
)
echo OK: V2 MSVC pyconfig verify passed
exit /b 0
