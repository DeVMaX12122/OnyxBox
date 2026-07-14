@echo off
REM Apply all OnyxBox patches to a VirtualBox source tree.
REM Run from the root of the cloned VirtualBox repository.
REM
REM Usage:
REM   cd C:\path\to\virtualbox
REM   C:\path\to\OnyxBox\patches\apply_all.bat

setlocal enabledelayedexpansion
set PATCH_DIR=%~dp0

echo ========================================
echo  Applying OnyxBox patches...
echo ========================================
echo.

for %%p in ("%PATCH_DIR%*.patch") do (
    echo [APPLY] %%~nxp
    git apply "%%p"
    if !errorlevel! neq 0 (
        echo [FAILED] %%~nxp - manual intervention may be needed
        echo.
    ) else (
        echo [OK] %%~nxp
    )
)

echo.
echo ========================================
echo  Done. All patches applied.
echo  Verify with: git diff --stat
echo ========================================
pause
