@echo off
:: ════════════════════════════════════════════════════════
::  Project Cortex — deploy_firmware.bat
::  Patches FIRMWARE_VERSION, prompts rebuild, uploads
::  to Cerebro for OTA distribution to both robots.
::
::  Usage: deploy_firmware.bat          (auto date version)
::         deploy_firmware.bat 1.2.3    (manual version)
::
::  Put this file in your m5stack\ folder
:: ════════════════════════════════════════════════════════

set CEREBRO=192.168.50.188:5005
set KEY=kira_ota_2024
set BIN=.pio\build\m5stack-cores3\firmware.bin
set SRC=src\main.cpp

:: Auto-version from date if not provided
if "%~1"=="" (
    for /f %%a in ('powershell -NoProfile -Command "Get-Date -Format 'yyyy.MM.dd.HHmm'"') do set VERSION=%%a
) else (
    set VERSION=%~1
)

echo.
echo  ══════════════════════════════════════════
echo   Project Cortex — Firmware Deploy
echo  ══════════════════════════════════════════
echo  Version : %VERSION%
echo  Cerebro : %CEREBRO%
echo.
echo  Patching FIRMWARE_VERSION in main.cpp...

:: VBScript patcher — uses InStr, no regex quote hell
set VBS=%TEMP%\cortex_patch.vbs
(
    echo Dim fso, f, c, p1, p2
    echo Set fso = CreateObject^("Scripting.FileSystemObject"^)
    echo Set f = fso.OpenTextFile^("%SRC:\=\\%", 1^)
    echo c = f.ReadAll : f.Close
    echo p1 = InStr^(c, "#define FIRMWARE_VERSION " ^& Chr^(34^)^)
    echo If p1 ^> 0 Then
    echo     p1 = p1 + Len^("#define FIRMWARE_VERSION " ^& Chr^(34^)^)
    echo     p2 = InStr^(p1, c, Chr^(34^)^)
    echo     c = Left^(c, p1-1^) ^& "%VERSION%" ^& Mid^(c, p2^)
    echo End If
    echo Set f = fso.OpenTextFile^("%SRC:\=\\%", 2^)
    echo f.Write c : f.Close
    echo WScript.Echo "  Patched to %VERSION%"
) > "%VBS%"
cscript //NoLogo //E:VBScript "%VBS%"

if errorlevel 1 (
    echo  Patch failed — manually set FIRMWARE_VERSION to "%VERSION%" in main.cpp
)

echo.
echo  Now rebuild in PlatformIO ^(Ctrl+Alt+B^)
echo  then press any key to upload to Cerebro.
echo.
pause

if not exist "%BIN%" (
    echo  ERROR: firmware.bin not found — did you rebuild?
    pause
    exit /b 1
)

echo  Uploading to Cerebro...
echo.
curl -s -X POST "http://%CEREBRO%/ota/upload?robot_id=robot1&version=%VERSION%&key=%KEY%" --data-binary @"%BIN%"
echo.
curl -s -X POST "http://%CEREBRO%/ota/upload?robot_id=robot2&version=%VERSION%&key=%KEY%" --data-binary @"%BIN%"
echo.
echo  ══════════════════════════════════════════
echo   Done! Both robots update on next boot.
echo   Version: %VERSION%
echo  ══════════════════════════════════════════
echo.
pause
