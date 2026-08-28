@echo off
REM Start the UV-K5 V3 emulator (headless mode for CLI tools).
REM
REM   GDB stub  : tcp:1234
REM   QMP socket: tcp:127.0.0.1:4444
REM
REM Usage: run.bat [firmware.elf]

setlocal enabledelayedexpansion

set QEMU=%USERPROFILE%\qemu-build\qemu-system-arm.exe
set ELF=%1
if "%ELF%"=="" set ELF=%USERPROFILE%\uvk5-port\uvk5-sat\build\CW\nr7y.cw.elf
set FLASH=%USERPROFILE%\uvk5-port\sim\assets\flash.img
set QMP_PORT=4444

REM Kill any existing instance
taskkill /F /IM qemu-system-arm.exe 2>nul
timeout /t 1 /nobreak >nul

REM Start QEMU
"%QEMU%" -M "uv-k5-v3,flash-image=%FLASH%" ^
    -nographic -monitor none ^
    -qmp "tcp:127.0.0.1:%QMP_PORT%,server=on,wait=off" ^
    -kernel "%ELF%" ^
    -gdb tcp::1234
