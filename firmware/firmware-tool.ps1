#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Strumento per build, flash USB e flash OTA del firmware ESP8266.
.DESCRIPTION
    Comandi:
      build       - Compila il firmware
      flash-usb   - Flash via seriale (USB)
      flash-ota   - Flash via OTA (WiFi)
      monitor     - Apre il monitor seriale
      list        - Elenca gli ambienti disponibili
.PARAMETER Command
    Comando da eseguire (build|flash-usb|flash-ota|monitor|list)
.PARAMETER Env
    Ambiente PlatformIO (default: wemos_d1_r1)
.PARAMETER Port
    Porta seriale (es. COM3)
.PARAMETER Hostname
    IP o hostname dell'ESP per OTA
    .PARAMETER BinFile
    Percorso del file .bin per flash-ota (opzionale, usa l'ultimo build)
.PARAMETER OtaPassword
    Password OTA (default: admin)
.PARAMETER SkipBuild
    Salta la build prima del flash
.EXAMPLE
    .\firmware-tool.ps1 build
    .\firmware-tool.ps1 build -e nodemcu_1_0
    .\firmware-tool.ps1 flash-usb -p COM3
    .\firmware-tool.ps1 flash-ota -h 192.168.1.100
    .\firmware-tool.ps1 flash-ota -h esp-device.local
    .\firmware-tool.ps1 monitor -p COM3
#>

param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'flash-usb', 'flash-ota', 'monitor', 'list')]
    [string]$Command,

    [Parameter()]
    [Alias('e')]
    [string]$Env,

    [Parameter()]
    [Alias('p')]
    [string]$Port,

    [Parameter()]
    [Alias('h')]
    [string]$Hostname,

    [Parameter()]
    [Alias('b')]
    [string]$BinFile,

    [Parameter()]
    [Alias('a')]
    [string]$OtaPassword = 'admin',

    [Parameter()]
    [switch]$SkipBuild
)

# ── Configurazione ──────────────────────────────────────────────
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PioExe = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
$PythonExe = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
$EspotaPy = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif8266\tools\espota.py"
$EspToolPy = "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\espytool.py"

$FirmwareVersion = "1.0.1"

$Environments = @{
    'wemos_d1_r1' = @{ Board = 'Wemos D1 R1' }
    'nodemcu_1_0' = @{ Board = 'NodeMCU 1.0' }
}
$DefaultEnv = 'wemos_d1_r1'

# ── Funzioni helper ─────────────────────────────────────────────
function Write-Step($msg) {
    Write-Host "`n>>> $msg" -ForegroundColor Cyan
}

function Write-Ok($msg) {
    Write-Host "OK: $msg" -ForegroundColor Green
}

function Write-Err($msg) {
    Write-Host "ERR: $msg" -ForegroundColor Red
}

function Write-Info($msg) {
    Write-Host "    $msg" -ForegroundColor Gray
}

function Get-Pio {
    if (-not (Test-Path $PioExe)) { throw "pio.exe non trovato: $PioExe" }
    return $PioExe
}

function Get-ValidEnv {
    param($requested)
    if (-not $requested) { return $DefaultEnv }
    if ($Environments.ContainsKey($requested)) { return $requested }
    $match = $Environments.Keys | Where-Object { $_ -like "$requested*" } | Select-Object -First 1
    if ($match) { return $match }
    throw "Ambiente '$requested' non trovato. Usa 'list'."
}

function Find-SerialPort {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) { throw "Nessuna porta seriale trovata." }
    if ($ports.Count -eq 1) { return $ports[0] }
    Write-Host "Porte seriali:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $ports.Count; $i++) { Write-Host "  [$i] $($ports[$i])" }
    $choice = Read-Host "Seleziona numero porta"
    if ($choice -match '^\d+$' -and [int]$choice -lt $ports.Count) { return $ports[[int]$choice] }
    throw "Selezione non valida."
}

function Find-BuildBin($envName) {
    $p = "$ProjectDir\.pio\build\$envName\firmware.bin"
    if (Test-Path $p) { return $p }
    return $null
}

# ── Comandi ─────────────────────────────────────────────────────
function Invoke-Build {
    param([string]$envName)

    Write-Step "Build firmware per $envName ($($Environments[$envName].Board))"

    $pio = Get-Pio
    & $pio run -e $envName --project-dir "$ProjectDir"
    if ($LASTEXITCODE -ne 0) { throw "Build fallita." }

    $bin = Find-BuildBin $envName
    if ($bin) { Write-Ok "Firmware: $bin" } else { Write-Err "firmware.bin non trovato" }
}

function Invoke-FlashUsb {
    param([string]$envName, [string]$port)

    Write-Step "Flash USB: $envName su $port"

    if (-not $SkipBuild) { Invoke-Build -envName $envName }

    if (-not $port) { $port = Find-SerialPort }
    Write-Info "Porta: $port"

    $pio = Get-Pio
    & $pio run -e $envName -t upload --upload-port $port --project-dir "$ProjectDir"
    if ($LASTEXITCODE -ne 0) { throw "Flash USB fallito." }
    Write-Ok "Flashing completato su $port"
}

function Invoke-FlashOta {
    param([string]$envName, [string]$hostname, [string]$binFile, [string]$otaPassword)

    Write-Step "Flash OTA: $envName -> $hostname"

    if (-not $hostname) {
        $hostname = Read-Host "Hostname/IP dell'ESP"
    }

    if (-not $binFile) {
        if (-not $SkipBuild) { Invoke-Build -envName $envName }
        $binFile = Find-BuildBin $envName
    }

    if (-not $binFile -or -not (Test-Path $binFile)) {
        throw "File .bin non trovato. Specifica -b o esegui build prima."
    }

    Write-Info "Bin:     $binFile"
    Write-Info "Target:  $hostname"

    if (-not (Test-Path $EspotaPy)) {
        throw "espota.py non trovato: $EspotaPy"
    }

    & $PythonExe $EspotaPy --help 2>&1 | Out-Null
    $usePort = $false
    if ($LASTEXITCODE -eq 0) { $usePort = $true }

    # espota.py -i <ip> -f <firmware.bin> [-p <port>] [-a <password>]
    $espotaArgs = @(
        "-i", $hostname,
        "-f", $binFile,
        "-a", $otaPassword
    )
    if ($usePort) {
        $espotaArgs += @("-p", "8266")
    }

    Write-Info "Eseguo: $PythonExe $EspotaPy $($espotaArgs -join ' ')"
    & $PythonExe $EspotaPy @espotaArgs

    if ($LASTEXITCODE -ne 0) {
        throw "Flash OTA fallito. Verifica ESP acceso, connesso e raggiungibile."
    }

    Write-Ok "OTA completato con successo!"
}

function Invoke-Monitor {
    param([string]$envName, [string]$port)

    Write-Step "Monitor seriale: $envName"

    if (-not $port) { $port = Find-SerialPort }
    Write-Info "Porta: $port (Ctrl+C per uscire)"

    $pio = Get-Pio
    & $pio device monitor --port $port --baud 115200 --project-dir "$ProjectDir"
}

function Invoke-List {
    Write-Host "`nAmbienti disponibili:`n" -ForegroundColor Green
    foreach ($key in $Environments.Keys | Sort-Object) {
        Write-Host "  $key" -ForegroundColor Cyan
        Write-Host "       Board: $($Environments[$key].Board)" -ForegroundColor Gray
        Write-Host ""
    }
    Write-Host "Esempi:`n" -ForegroundColor Green
    Write-Host "  Build:          .\firmware-tool.ps1 build" -ForegroundColor White
    Write-Host "  Build (custom): .\firmware-tool.ps1 build -e nodemcu_1_0" -ForegroundColor White
    Write-Host "  Flash USB:      .\firmware-tool.ps1 flash-usb -p COM3" -ForegroundColor White
    Write-Host "  Flash OTA:      .\firmware-tool.ps1 flash-ota -h esp-device.local" -ForegroundColor White
    Write-Host "  Flash OTA:      .\firmware-tool.ps1 flash-ota -h 192.168.1.100" -ForegroundColor White
    Write-Host "  Monitor:        .\firmware-tool.ps1 monitor -p COM3" -ForegroundColor White
    Write-Host "  Aiuto:          .\firmware-tool.ps1 list`n" -ForegroundColor White
}

# ── Main ────────────────────────────────────────────────────────
try {
    switch ($Command) {
        'build'     { Invoke-Build -envName (Get-ValidEnv $Env) }
        'flash-usb' { Invoke-FlashUsb -envName (Get-ValidEnv $Env) -port $Port }
        'flash-ota' { Invoke-FlashOta -envName (Get-ValidEnv $Env) -hostname $Hostname -binFile $BinFile -otaPassword $OtaPassword }
        'monitor'   { Invoke-Monitor -envName (Get-ValidEnv $Env) -port $Port }
        'list'      { Invoke-List }
        default     { Invoke-List }
    }
} catch {
    Write-Err $_.Exception.Message
    exit 1
}
