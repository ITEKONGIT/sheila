#Requires -RunAsAdministrator

[CmdletBinding()]
param(
    [ValidateRange(1, 65535)]
    [int]$Port = 18877,

    [string]$DataDirectory = (Join-Path $PSScriptRoot "..\data")
)

$ErrorActionPreference = "Stop"
$taskName = "sSheila Host"
$taskPath = "\sSheila\"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$runner = (Resolve-Path (Join-Path $PSScriptRoot "run-windows.ps1")).Path
$executable = (Resolve-Path (Join-Path $projectRoot "dist\windows\sSheila.exe")).Path
$powerShell = (Get-Process -Id $PID).Path
$resolvedDataDirectory = [System.IO.Path]::GetFullPath($DataDirectory)

New-Item -ItemType Directory -Path $resolvedDataDirectory -Force | Out-Null

$arguments = '-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File "{0}" -Port {1} -DataDirectory "{2}"' -f `
    $runner, $Port, $resolvedDataDirectory
$action = New-ScheduledTaskAction `
    -Execute $powerShell `
    -Argument $arguments `
    -WorkingDirectory $projectRoot
$trigger = New-ScheduledTaskTrigger -AtStartup
$principal = New-ScheduledTaskPrincipal `
    -UserId "SYSTEM" `
    -LogonType ServiceAccount `
    -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet `
    -StartWhenAvailable `
    -RestartCount 999 `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -MultipleInstances IgnoreNew `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries

Register-ScheduledTask `
    -TaskName $taskName `
    -TaskPath $taskPath `
    -Description "Starts the private sSheila host at Windows startup and restarts it after failure." `
    -Action $action `
    -Trigger $trigger `
    -Principal $principal `
    -Settings $settings `
    -Force | Out-Null

& (Join-Path $PSScriptRoot "allow-windows-subnet.ps1") -Port $Port

$listener = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($null -ne $listener) {
    $listenerProcess = Get-Process -Id $listener.OwningProcess -ErrorAction Stop
    if (-not [string]::Equals($listenerProcess.Path, $executable, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "TCP port $Port is occupied by $($listenerProcess.Path); the sSheila startup task was installed but could not be started."
    }
    Write-Host "An existing sSheila process is already healthy; the startup task will take over after the next restart."
} else {
    Start-ScheduledTask -TaskName $taskName -TaskPath $taskPath
}

$ready = $false
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    try {
        $health = Invoke-RestMethod "http://127.0.0.1:$Port/api/v1/health" -TimeoutSec 2
        if ($health.status -eq "ok") {
            $ready = $true
            break
        }
    } catch {
        Start-Sleep -Milliseconds 500
    }
}

if (-not $ready) {
    throw "The startup task was installed, but sSheila did not become healthy on TCP port $Port."
}

Write-Host "sSheila autostart is installed and running."
Write-Host "Data will continue from: $resolvedDataDirectory"
Write-Host "Open: http://127.0.0.1:$Port"
