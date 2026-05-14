@echo off
setlocal enabledelayedexpansion

for /f "tokens=3" %%a in ('findstr /b /c:"#define FIRMWARE_VERSION" firmware\include\Config.h') do set "VER=%%~a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define DEFAULT_FIRMWARE_BOARD" firmware\include\user_config.h') do set "BOARD=%%~a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define DEFAULT_FIRMWARE_PROJECT" firmware\include\user_config.h') do set "PROJECT=%%~a"

if "%VER%"=="" set "ERR=1"
if "%BOARD%"=="" set "ERR=1"
if "%PROJECT%"=="" set "ERR=1"
if defined ERR (
    echo ERRORE: parametri mancanti
    echo VER=[%VER%] BOARD=[%BOARD%] PROJECT=[%PROJECT%]
    exit /b 1
)

echo Versione: %VER%  Board: %BOARD%  Progetto: %PROJECT%

set "ALL_OK=1"
set "SRC=firmware\.pio\build\%BOARD%\firmware.bin"
set "DEST=docs\bin\%PROJECT%\%BOARD%"

if not exist "%SRC%" (
    echo [ERRORE] Source mancante: %SRC%
    exit /b 1
)
if not exist "%DEST%" mkdir "%DEST%"

copy /Y "%SRC%" "%DEST%\v%VER%_firmware.bin" >nul
if %errorlevel% neq 0 goto :copy_fail

for %%s in ("%SRC%") do set "SRC_SIZE=%%~zs"
for %%s in ("%DEST%\v%VER%_firmware.bin") do set "DST_SIZE=%%~zs"
if "!SRC_SIZE!"=="!DST_SIZE!" (
    echo [OK] %PROJECT%/%BOARD%/v%VER%_firmware.bin  (!DST_SIZE! bytes)
    goto :info_gen
)
echo [ERRORE] Dimensione non corrisponde: !SRC_SIZE! -^> !DST_SIZE!
set "ALL_OK=0"
goto :info_gen

:copy_fail
echo [ERRORE] Copia fallita: %SRC% -^> %DEST%\v%VER%_firmware.bin
set "ALL_OK=0"

:info_gen
powershell -ExecutionPolicy Bypass -Command "$url='https://pbielli.github.io/esp/bin/%PROJECT%/%BOARD%/v%VER%_firmware.bin'; @{version='%VER%';url=$url;board='%BOARD%';project='%PROJECT%'} | ConvertTo-Json -Compress | Set-Content '%DEST%\info.json' -Encoding UTF8"
if %errorlevel% neq 0 (
    echo [ERRORE] info.json non generato
    set "ALL_OK=0"
    goto :fw_json
)
findstr /c:"%VER%" "%DEST%\info.json" >nul
if %errorlevel% equ 0 (
    echo [OK] info.json generato (versione %VER%)
    goto :fw_json
)
echo [ERRORE] info.json non contiene la versione %VER%
set "ALL_OK=0"

:fw_json
echo.
echo Generazione firmwares.json...
powershell -ExecutionPolicy Bypass -Command "$firmwares = @(); Get-ChildItem 'docs\bin\*' -Directory | Where-Object { $_.Name -ne 'index.html' } | ForEach-Object { $project = $_.Name; Get-ChildItem $_.FullName -Directory | ForEach-Object { $board = $_.Name; Get-ChildItem ('{0}\*.bin' -f $_.FullName) | ForEach-Object { $filename = $_.Name; $version = $filename -replace '^v(.*)_firmware\.bin$','$1'; $firmwares += @{project=$project;board=$board;version=$version;filename=$filename;url=($project+'/'+$board+'/'+$filename)} } } }; @{firmwares=$firmwares} | ConvertTo-Json -Depth 3 | Set-Content 'docs\bin\firmwares.json' -Encoding UTF8"
if %errorlevel% neq 0 (
    echo [ERRORE] firmwares.json non generato
    set "ALL_OK=0"
    goto :done
)
findstr /c:"%VER%" "docs\bin\firmwares.json" >nul
if %errorlevel% equ 0 (
    echo [OK] firmwares.json aggiornato (versione %VER%)
    goto :done
)
echo [ERRORE] firmwares.json non contiene la versione %VER%
set "ALL_OK=0"

:done
echo.
if "%ALL_OK%"=="1" (
    echo ===== TUTTO OK =====
    exit /b 0
)
echo ===== ERRORI RILEVATI =====
exit /b 1
