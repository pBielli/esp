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

REM Genera firmwares.json
echo Generazione firmwares.json...
powershell -ExecutionPolicy Bypass -Command "$firmwares = @(); Get-ChildItem 'docs\bin\*' -Directory | ForEach-Object { $board = $_.Name; Get-ChildItem ('{0}\*.bin' -f $_.FullName) | ForEach-Object { $filename = $_.Name; $version = $filename -replace '^v(.*)_firmware\.bin$','$1'; $firmwares += @{board=$board;version=$version;filename=$filename;url=($board+'/'+$filename)} } }; @{firmwares=$firmwares} | ConvertTo-Json -Depth 3 | Set-Content 'docs\bin\firmwares.json' -Encoding UTF8"

echo Fatto.
