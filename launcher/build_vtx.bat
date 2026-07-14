@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
cl vtx.c /Fe:vtx.exe /O2 /D_CRT_SECURE_NO_WARNINGS /link setupapi.lib
echo.
echo Build complete: vtx.exe
