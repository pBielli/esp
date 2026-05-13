@echo off
setlocal enabledelayedexpansion

REM Legge versione da Config.h, board e progetto da user_config.h
for /f "tokens=3" %%a in ('findstr /b /c:"#define FIRMWARE_VERSION" firmware\include\Config.h') do set "VER=%%~a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define DEFAULT_FIRMWARE_BOARD" firmware\include\user_config.h') do set "BOARD=%%~a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define DEFAULT_FIRMWARE_PROJECT" firmware\include\user_config.h') do set "PROJECT=%%~a"

if "%VER%"=="" (echo ERRORE: versione non trovata & exit /b 1)
if "%BOARD%"=="" (echo ERRORE: board non trovata & exit /b 1)
if "%PROJECT%"=="" (echo ERRORE: progetto non trovato & exit /b 1)

echo Versione: %VER%  Board: %BOARD%  Progetto: %PROJECT%

REM Scansiona firmware\.pio\build\ per firmware.bin
for /d %%d in (firmware\.pio\build\*) do (
    if exist "%%d\firmware.bin" (
        set "BD=%%~nxd"
        set "DEST=docs\bin\!BD!\%PROJECT%"
        if not exist "!DEST!" mkdir "!DEST!"
        copy "%%d\firmware.bin" "!DEST!\v%VER%_firmware.bin" >nul
        echo Copiato: %%d\firmware.bin -^> !DEST!\v%VER%_firmware.bin

        REM Genera file latest (JSON senza estensione) nella cartella progetto
        powershell -ExecutionPolicy Bypass -Command "$url='https://pbielli.github.io/esp/bin/!BD!/%PROJECT%/v%VER%_firmware.bin'; @{version='%VER%';url=$url;board='!BD!';project='%PROJECT%'} | ConvertTo-Json -Compress | Set-Content '!DEST!\latest' -Encoding UTF8"
        echo Generato: !DEST!\latest
    )
)

REM Genera firmwares.json (con progetto)
echo Generazione firmwares.json...
powershell -ExecutionPolicy Bypass -Command "$firmwares = @(); Get-ChildItem 'docs\bin\*' -Directory | ForEach-Object { $board = $_.Name; Get-ChildItem $_.FullName -Directory | ForEach-Object { $project = $_.Name; Get-ChildItem ('{0}\*.bin' -f $_.FullName) | ForEach-Object { $filename = $_.Name; $version = $filename -replace '^v(.*)_firmware\.bin$','$1'; $firmwares += @{board=$board;project=$project;version=$version;filename=$filename;url=($board+'/'+$project+'/'+$filename)} } } }; @{firmwares=$firmwares} | ConvertTo-Json -Depth 3 | Set-Content 'docs\bin\firmwares.json' -Encoding UTF8"

echo Fatto.
