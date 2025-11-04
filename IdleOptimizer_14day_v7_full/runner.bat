@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0"

set "LOG=run_log.txt"
echo Running IdleOptimizer... (logging to %LOG%)

(
  echo === IdleOptimizer run started %date% %time% ===
  .\IdleOptimizer.exe
  echo === Exit code !errorlevel! ===
) > "%LOG%" 2>&1

echo Done. Opening log...
start "" notepad.exe "%LOG%"
popd
