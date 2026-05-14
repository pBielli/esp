@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "PIO=C:\Users\Pat\.platformio\penv\Scripts\pio.exe"
set "PIOENV=wemos_d1_r1"
set "PIO_DIR=firmware"
set "MONITOR_PORT=COM5"
set "MONITOR_BAUD=115200"
set "GH_USER=pbielli"
set "GH_REPO=esp"
set "GH_BRANCH=main"
set "ROOT_URL=https://%GH_USER%.github.io/%GH_REPO%/bin"

:menu
cls
echo ============================================
echo       FIRMWARE TOOL
echo ============================================
echo.
echo  1. Build firmware (%PIOENV%)
echo  2. Copy firmware to docs/
echo  3. Push to GitHub
echo  4. Full auto: flash ^> incr ^> build ^> copy ^> push ^> verify
echo  5. Exit
echo.
set /p "CH=Seleziona [1-5]: "
if "%CH%"=="1" goto build
if "%CH%"=="2" goto copy
if "%CH%"=="3" goto push
if "%CH%"=="4" goto auto
if "%CH%"=="5" exit /b 0
goto menu

:build
echo.
echo [Build] Avvio compilazione...
pushd "%PIO_DIR%"
"%PIO%" run -e %PIOENV%
set "BUILD_ERR=%errorlevel%"
popd
if %BUILD_ERR% neq 0 ( echo [Build] ERRORE & pause & exit /b 1 )
echo [Build] Completato.
pause
goto menu

:copy
echo.
echo [Copy] Copio firmware in docs/...
call copy_firmware.bat
if %errorlevel% neq 0 ( echo [Copy] ERRORE & pause & exit /b 1 )
echo [Copy] Completato.
pause
goto menu

:push
echo.
echo [Push] Commit e push su GitHub...
git add .
git commit -m "firmware update"
git push -u origin %GH_BRANCH%
if %errorlevel% neq 0 ( echo [Push] ERRORE & pause & exit /b 1 )
echo [Push] Completato.
pause
goto menu

:auto
echo.
echo ===== FULL AUTO =====
echo.

REM ── Step pre: leggi versione corrente ──
for /f "tokens=3" %%a in ('findstr /b /c:"#define FIRMWARE_VERSION" firmware\include\Config.h') do set "CUR_VER=%%~a"
if "!CUR_VER!"=="" ( echo ERRORE: versione non trovata & pause & exit /b 1 )
echo Versione corrente: !CUR_VER!

REM ── Step 0: USB flash del firmware ATTUALE ──
echo.
echo [0/5] Flash USB del firmware v!CUR_VER! su %MONITOR_PORT%...
pushd "%PIO_DIR%"
"%PIO%" run -e %PIOENV% -t upload --upload-port %MONITOR_PORT%
set "FLASH_ERR=%errorlevel%"
popd
if !FLASH_ERR! neq 0 (
    echo [ERRORE] Flash USB fallito
    pause & exit /b 1
)
echo [0/5] Flash USB completato (v!CUR_VER!).
echo.

REM ── Step 1: incrementa versione ──
for /f "tokens=1-3 delims=." %%a in ("!CUR_VER!") do (
    set /a "NEW_PATCH=%%c+1"
    set "NEW_VER=%%a.%%b.!NEW_PATCH!"
)
echo [1/5] Nuova versione: !NEW_VER!

REM Riscrivi solo la riga FIRMWARE_VERSION in Config.h
set "TMP_FILE=%TEMP%\Config_%RANDOM%.h"
(
    for /f "usebackq delims=" %%a in ("firmware\include\Config.h") do (
        set "LINE=%%a"
        if "!LINE:#define FIRMWARE_VERSION=!"=="!LINE!" (
            echo(!LINE!
        ) else (
            echo #define FIRMWARE_VERSION "!NEW_VER!"
        )
    )
) > "!TMP_FILE!"
move /Y "!TMP_FILE!" "firmware\include\Config.h" >nul
if %errorlevel% neq 0 ( echo ERRORE aggiornamento versione & pause & exit /b 1 )
echo [1/5] Versione aggiornata a !NEW_VER!.

REM ── Step 2: build ──
echo [2/5] Compilazione...
pushd "%PIO_DIR%"
"%PIO%" run -e %PIOENV%
set "BUILD_ERR=%errorlevel%"
popd
if %BUILD_ERR% neq 0 ( echo ERRORE build & pause & exit /b 1 )
echo [2/5] Build completato.

REM ── Step 3: copy ──
echo [3/5] Copia firmware...
call copy_firmware.bat
if %errorlevel% neq 0 ( echo ERRORE copy & pause & exit /b 1 )
echo [3/5] Copia completata.

REM ── Step 4: push ──
echo [4/5] Push su GitHub...
git add .
git commit -m "firmware v!NEW_VER!"
git push -u origin %GH_BRANCH%
if %errorlevel% neq 0 ( echo ERRORE push & pause & exit /b 1 )
echo [4/5] Push completato.

REM ── Step 5: verifica + serial monitor inline ──
echo [5/5] Verifica GitHub Pages + serial monitor
echo.
echo Premere CTRL+C per interrompere.
echo.

set "INFO_URL=%ROOT_URL%/generic/%PIOENV%/info.json"
set "WAIT_SEC=30"
set "ATTEMPTS=20"

REM Configura porta seriale
mode %MONITOR_PORT%: BAUD=%MONITOR_BAUD% PARITY=N DATA=8 STOP=1 >nul 2>&1

:verify_loop

REM Legge seriale inline (output recente della scheda)
for /f "delims=" %%s in ('powershell -NoP -C "try{$p=New-Object IO.Ports.SerialPort %MONITOR_PORT%,%MONITOR_BAUD%,None,8,One;$p.Open();$p.ReadTimeout=800;while(($s=$p.ReadLine())-ne$null){Write-Output $s}}catch{}" 2^>nul') do (
    echo [SCHEDA] %%s
)

REM Verifica GitHub Pages
for /f "usebackq delims=" %%r in (`powershell -NoP -C "try{$r=Invoke-WebRequest -Uri '%INFO_URL%' -UseBasicParsing -TimeoutSec 30;Write-Output $r.Content}catch{}" 2^>nul`) do set "BODY=%%r"

if not "!BODY!"=="" (
    echo [OTA] !BODY!
    echo !BODY! | findstr "!NEW_VER!" >nul
    if !errorlevel! equ 0 (
        echo.
        echo ===== SUCCESSO — Versione !NEW_VER! rilevata su GitHub Pages =====
        echo OTA URL: %INFO_URL%
        echo.
        pause
        goto menu
    )
)

set /a "ATTEMPTS-=1"
if !ATTEMPTS! leq 0 (
    echo ===== TIMEOUT — !NEW_VER! non rilevata dopo 20 tentativi =====
    pause
    goto menu
)
timeout /t !WAIT_SEC! /nobreak >nul
goto verify_loop
