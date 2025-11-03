@echo off
cd /d "%~dp0"
IdleOptimizer.exe > run_log.txt 2>&1
notepad run_log.txt
