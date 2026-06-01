@echo off
:: ════════════════════════════════════════════════════════
::  deploy_firmware.bat — Patch version + upload to Cerebro
:: ════════════════════════════════════════════════════════

set CEREBRO=YOUR_CEREBRO_IP:5005
set KEY=YOUR_OTA_KEY
set BIN=.pio\build\m5stack-cores3\firmware.bin
set SRC=src\main.cpp

if "%~1"=="" (
    for /f %%a in ('powershell -NoProfile -Command "Get-Date -Format 'yyyy.MM.dd.HHmm'"') do set VERSION=%%a
) else (
    set VERSION=%~1
)

echo.
echo  Patching FIRMWARE_VERSION to %VERSION% in main.cpp...

:: VBScript patcher — uses InStr not regex, no quote hell
set VBS=%TEMP%\kira_patch.vbs
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
echo  Rebuild in PlatformIO ^(Ctrl+Alt+B^) then press any key to upload.
pause

if not exist "%BIN%" (
    echo  ERROR: No firmware.bin found — build first
    pause
    exit /b 1
)

echo  Uploading %VERSION% to Cerebro...
curl -s -X POST "http://%CEREBRO%/ota/upload?robot_id=robot1&version=%VERSION%&key=%KEY%" --data-binary @"%BIN%"
echo.
curl -s -X POST "http://%CEREBRO%/ota/upload?robot_id=robot2&version=%VERSION%&key=%KEY%" --data-binary @"%BIN%"
echo.
echo  Done! Version: %VERSION%
echo.
pause
