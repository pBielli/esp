@echo off
setlocal enabledelayedexpansion

REM Legge la versione da firmware/include/Config.h
for /f "tokens=3" %%a in ('findstr /b /c:"#define FIRMWARE_VERSION" firmware\include\Config.h') do (
    set "VER=%%~a"
)

if "%VER%"=="" (
    echo ERRORE: versione non trovata in firmware\include\Config.h
    exit /b 1
)

echo Versione firmware: %VER%

REM Scansiona firmware\.pio\build\ per firmware.bin
for /d %%d in (firmware\.pio\build\*) do (
    if exist "%%d\firmware.bin" (
        set "BOARD=%%~nxd"
        set "DEST=docs\bin\!BOARD!"
        if not exist "!DEST!" mkdir "!DEST!"
        copy "%%d\firmware.bin" "!DEST!\v%VER%_firmware.bin" >nul
        echo Copiato: %%d\firmware.bin -^> !DEST!\v%VER%_firmware.bin
    )
)

echo Fatto.
