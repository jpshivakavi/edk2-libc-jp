@echo off
setlocal EnableExtensions

REM Stage Python 3.12.13 UEFI runtime tree (PREFIX relative to fsN:\EFI)
REM
REM   <OutFolder>\EFI\bin\Python312.efi
REM   <OutFolder>\EFI\lib\python3.12\
REM   <OutFolder>\EFI\stdlib\etc\

set TOOL_CHAIN_TAG=%1
set TARGET=%2
set ARCH=%3
set OUT_FOLDER=%4

if "%TOOL_CHAIN_TAG%"=="" goto usage
if "%TARGET%"=="" goto usage
if "%ARCH%"=="" goto usage
if "%OUT_FOLDER%"=="" goto usage
goto continue

:usage
echo.
echo Create Python 3.12 EFI package (Iteration 1).
echo.
echo Usage: %0 ^<ToolChain^> ^<Target^> ^<Architecture^> ^<OutFolder^>
echo   ToolChain     e.g. GCC, VS2019, VS2022
echo   Target        e.g. NOOPT, DEBUG, RELEASE
echo   Architecture  e.g. X64
echo   OutFolder     destination directory for the EFI\ tree
echo.
exit /b 1

:continue
if "%WORKSPACE%"=="" (
  echo WORKSPACE is not set.
  exit /b 1
)

if "%EDK2_LIBC_PATH%"=="" (
  echo Warning: EDK2_LIBC_PATH not set; using WORKSPACE.
  set "EDK2_LIBC_PATH=%WORKSPACE%"
)

set "PYTHON_SRC=%EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13"
set "BUILD_DIR=%WORKSPACE%\Build\AppPkg\%TARGET%_%TOOL_CHAIN_TAG%\%ARCH%"
set "PYTHON_BIN=%BUILD_DIR%\Python312.efi"

if not exist "%PYTHON_BIN%" (
  if exist "%BUILD_DIR%\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312_MIN\DEBUG\Python312.efi" (
    set "PYTHON_BIN=%BUILD_DIR%\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312_MIN\DEBUG\Python312.efi"
  ) else if exist "%BUILD_DIR%\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312\DEBUG\Python312.efi" (
    set "PYTHON_BIN=%BUILD_DIR%\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312\DEBUG\Python312.efi"
  ) else if exist "%BUILD_DIR%\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312\OUTPUT\Python312.efi" (
    set "PYTHON_BIN=%BUILD_DIR%\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312\OUTPUT\Python312.efi"
  ) else if exist "%BUILD_DIR%\AppPkg\Applications\Python\Python-3.12.13\Python312_MIN\DEBUG\Python312.efi" (
    set "PYTHON_BIN=%BUILD_DIR%\AppPkg\Applications\Python\Python-3.12.13\Python312_MIN\DEBUG\Python312.efi"
  ) else if exist "%BUILD_DIR%\AppPkg\Applications\Python\Python-3.12.13\Python312\DEBUG\Python312.efi" (
    set "PYTHON_BIN=%BUILD_DIR%\AppPkg\Applications\Python\Python-3.12.13\Python312\DEBUG\Python312.efi"
  ) else if exist "%BUILD_DIR%\AppPkg\Applications\Python\Python-3.12.13\Python312\OUTPUT\Python312.efi" (
    set "PYTHON_BIN=%BUILD_DIR%\AppPkg\Applications\Python\Python-3.12.13\Python312\OUTPUT\Python312.efi"
  ) else (
    goto error
  )
)

if not exist "%PYTHON_SRC%\Lib\" (
  echo Python Lib\ not found at %PYTHON_SRC%\Lib
  exit /b 1
)

mkdir "%OUT_FOLDER%\EFI\bin" 2>nul
mkdir "%OUT_FOLDER%\EFI\lib\python3.12" 2>nul
mkdir "%OUT_FOLDER%\EFI\lib\python3.12\lib-dynload" 2>nul
mkdir "%OUT_FOLDER%\EFI\stdlib\etc" 2>nul

copy /Y "%PYTHON_BIN%" "%OUT_FOLDER%\EFI\bin\Python312.efi" >nul
xcopy "%PYTHON_SRC%\Lib\*" "%OUT_FOLDER%\EFI\lib\python3.12\" /Y /S /I /Q >nul
if exist "%PYTHON_SRC%\PyMod-3.12.13\Lib\" (
  xcopy "%PYTHON_SRC%\PyMod-3.12.13\Lib\*" "%OUT_FOLDER%\EFI\lib\python3.12\" /Y /S /I /Q >nul
)
set "READLINE_VENDOR=%PYTHON_SRC%\PyMod-3.12.13\Modules\readline"
if exist "%READLINE_VENDOR%\pyreadline\" (
  if exist "%READLINE_VENDOR%\readline.py" (
    copy /Y "%READLINE_VENDOR%\readline.py" "%OUT_FOLDER%\EFI\lib\python3.12\readline.py" >nul
    xcopy "%READLINE_VENDOR%\pyreadline\*" "%OUT_FOLDER%\EFI\lib\python3.12\pyreadline\" /Y /S /I /Q >nul
    echo Staged pyreadline from %READLINE_VENDOR%
  )
) else (
  echo Warning: %READLINE_VENDOR% missing; import readline will fail.
)
if exist "%EDK2_LIBC_PATH%\StdLib\Efi\StdLib\etc\" (
  xcopy "%EDK2_LIBC_PATH%\StdLib\Efi\StdLib\etc\*" "%OUT_FOLDER%\EFI\stdlib\etc\" /Y /S /I /Q >nul
)

echo.
echo Python 3.12 EFI package ready at %OUT_FOLDER%\EFI\
echo   bin\Python312.efi
echo   lib\python3.12\
echo   stdlib\etc\
echo.
echo UEFI Shell: fs0: then cd EFI\bin then Python312.efi
exit /b 0

:error
echo Failed to create Python EFI package.
echo Python312.efi not found under Build\AppPkg\%TARGET%_%TOOL_CHAIN_TAG%\%ARCH%\
echo Build with: build -a %ARCH% -b %TARGET% -t %TOOL_CHAIN_TAG% -p AppPkg\AppPkg.dsc -D BUILD_PYTHON312
exit /b 1
