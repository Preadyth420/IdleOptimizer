@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "EXE_NAME=IdleOptimizer.exe"
set "CANDIDATES=out\build\x64-Debug out\build\Debug out\build\x64-Release out\build\Release build\x64-Debug build\Debug build\x64-Release build\Release"

pushd "%~dp0"
echo [RunWithLog] Working from: %CD%

rem --- find exe ---
set "EXE="
for %%D in (%CANDIDATES%) do (
  if exist "%%~D\%EXE_NAME%" (
    set "EXE=%%~D\%EXE_NAME%"
    goto :found
  )
)
if not defined EXE (
  for /r %%F in ("%EXE_NAME%") do (
    set "EXE=%%~fF"
    goto :found
  )
)
echo [RunWithLog] ERROR: Could not find %EXE_NAME% under common build folders.
pause
exit /b 1

:found
for %%F in ("%EXE%") do set "EXEDIR=%%~dpF"
echo [RunWithLog] Found EXE: "%EXE%"
echo [RunWithLog] Using work dir: "%EXEDIR%"

pushd "%EXEDIR%"

rem --- ensure config.json next to exe ---
set "CFG_SOURCE="
if exist "config.json" (
  set "CFG_SOURCE=existing EXE\config.json"
) else (
  if exist "%~dp0config.json" (
    copy /Y "%~dp0config.json" "config.json" >nul
    set "CFG_SOURCE=repo\config.json -> EXE\config.json"
  ) else if exist "%~dp0config.example.json" (
    copy /Y "%~dp0config.example.json" "config.json" >nul
    set "CFG_SOURCE=repo\config.example.json -> EXE\config.json"
  )
)
if exist "config.json" (
  for %%Z in ("config.json") do set "CFGSIZE=%%~zZ"
  echo [RunWithLog] Using config.json  size=!CFGSIZE!  source: !CFG_SOURCE!
) else (
  echo [RunWithLog] WARNING: No config.json; running with defaults.
)

rem --- locale-proof timestamp (sanitize everything that breaks paths) ---
set "STAMP=%date%_%time%"
set "STAMP=%STAMP: =0%"
set "STAMP=%STAMP:/=%"
set "STAMP=%STAMP:\=%"
set "STAMP=%STAMP::=%"
set "STAMP=%STAMP:.=%"
set "STAMP=%STAMP:,=%"

if not exist "logs" mkdir "logs" >nul 2>&1
set "LOG=logs\run_!STAMP!.txt"
echo [RunWithLog] Logging to: "!LOG!"

rem --- run and capture stdout+stderr ---
(
  echo === IdleOptimizer run started %date% %time% ===
  echo EXE: %EXE%
  if exist "config.json" echo CONFIG: %CD%\config.json
  .\%EXE_NAME%
  echo === Exit code !errorlevel! ===
) > "!LOG!" 2>&1

if exist "!LOG!" (
  echo [RunWithLog] Done. Opening log...
  start "" notepad.exe "!LOG!"
) else (
  echo [RunWithLog] ERROR: Log file was not created. Check folder permissions.
  pause
)

popd
popd
exit /b 0
