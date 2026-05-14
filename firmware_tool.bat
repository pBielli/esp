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
echo  4. Full auto: incr ^> build ^> copy ^> push ^> verify
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

REM Leggi versione corrente
for /f "tokens=3" %%a in ('findstr /b /c:"#define FIRMWARE_VERSION" firmware\include\Config.h') do set "CUR_VER=%%~a"
if "!CUR_VER!"=="" ( echo ERRORE: versione non trovata & pause & exit /b 1 )
echo Versione corrente: !CUR_VER!

REM Incrementa patch
for /f "tokens=1-3 delims=." %%a in ("!CUR_VER!") do (
    set /a "NEW_PATCH=%%c+1"
    set "NEW_VER=%%a.%%b.!NEW_PATCH!"
)
echo Nuova versione: !NEW_VER!

REM Riscrivi Config.h
powershell -ExecutionPolicy Bypass -Command ^
"(Get-Content 'firmware\include\Config.h') -replace '#define FIRMWARE_VERSION \"[^\"]+\"','#define FIRMWARE_VERSION \"!NEW_VER!\"' | Set-Content 'firmware\include\Config.h' -Encoding UTF8"
if %errorlevel% neq 0 ( echo ERRORE aggiornamento versione & pause & exit /b 1 )
echo [1/4] Versione aggiornata a !NEW_VER!.

REM Build
echo [2/4] Compilazione...
pushd "%PIO_DIR%"
"%PIO%" run -e %PIOENV%
set "BUILD_ERR=%errorlevel%"
popd
if %BUILD_ERR% neq 0 ( echo ERRORE build & pause & exit /b 1 )
echo [2/4] Build completato.

REM Copy
echo [3/4] Copia firmware...
call copy_firmware.bat
if %errorlevel% neq 0 ( echo ERRORE copy & pause & exit /b 1 )
echo [3/4] Copia completata.

REM Push
echo [4/4] Push su GitHub...
git add .
git commit -m "firmware v!NEW_VER!"
git push -u origin %GH_BRANCH%
if %errorlevel% neq 0 ( echo ERRORE push & pause & exit /b 1 )
echo [4/4] Push completato.

REM Avvia serial monitor in finestra separata
echo.
echo Avvio serial monitor su %MONITOR_PORT% (baud %MONITOR_BAUD%)...
start "Serial Monitor" cmd /c ""%PIO%" device monitor --port %MONITOR_PORT% --baud %MONITOR_BAUD% & pause"
echo.

REM Wait for GitHub Pages update
echo ===== Verifica GitHub Pages =====
echo Attendo che la versione !NEW_VER! sia disponibile su:
echo %ROOT_URL%/generic/%PIOENV%/info.json
echo.
echo Premere CTRL+C per interrompere la verifica.
echo.

set "INFO_URL=%ROOT_URL%/generic/%PIOENV%/info.json"
set "WAIT_SEC=60"
set "ATTEMPTS=30"

:verify_loop
echo [%date% %time%] Verifico...
for /f "usebackq delims=" %%r in (`powershell -ExecutionPolicy Bypass -Command "try { $r = Invoke-WebRequest -Uri '%INFO_URL%' -UseBasicParsing -TimeoutSec 30; Write-Output $r.Content } catch { Write-Output '' }"`) do set "BODY=%%r"

if "!BODY!"=="" (
    echo   Risposta vuota o errore — riprovo tra !WAIT_SEC!s
) else (
    echo   Risposta: !BODY!
    echo !BODY! | findstr "!NEW_VER!" >nul
    if !errorlevel! equ 0 (
        echo.
        echo ===== SUCCESSO — Versione !NEW_VER! rilevata su GitHub Pages =====
        echo.
        echo OTA URL: %INFO_URL%
        echo.
        echo Il serial monitor e' aperto in un'altra finestra.
        echo Chiudilo manualmente quando hai finito.
        pause
        goto menu
    ) else (
        echo   Versione !NEW_VER! non ancora presente — riprovo tra !WAIT_SEC!s
    )
)

set /a "ATTEMPTS-=1"
if !ATTEMPTS! leq 0 (
    echo.
    echo ===== TIMEOUT — !NEW_VER! non rilevata dopo 30 tentativi =====
    pause
    goto menu
)
timeout /t !WAIT_SEC! /nobreak >nul
goto verify_loop
