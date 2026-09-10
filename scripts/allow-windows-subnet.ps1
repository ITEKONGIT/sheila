#Requires -RunAsAdministrator

[CmdletBinding()]
param(
    [ValidateRange(1, 65535)]
    [int]$Port = 18877,

    [string]$Executable = (Join-Path $PSScriptRoot "..\dist\windows\sSheila.exe")
)

$ruleName = "sSheila host - local subnet"
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable -ErrorAction Stop).Path
$existingRule = Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue

if ($null -eq $existingRule) {
    New-NetFirewallRule `
        -DisplayName $ruleName `
        -Description "Allows sSheila only from directly connected host subnets." `
        -Direction Inbound `
        -Action Allow `
        -Enabled True `
        -Profile Any `
        -Program $resolvedExecutable `
        -Protocol TCP `
        -LocalPort $Port `
        -RemoteAddress LocalSubnet | Out-Null
} else {
    Set-NetFirewallRule `
        -DisplayName $ruleName `
        -Enabled True `
        -Direction Inbound `
        -Action Allow `
        -Profile Any

    $existingRule | Get-NetFirewallPortFilter |
        Set-NetFirewallPortFilter -Protocol TCP -LocalPort $Port
    $existingRule | Get-NetFirewallAddressFilter |
        Set-NetFirewallAddressFilter -RemoteAddress LocalSubnet
    $existingRule | Get-NetFirewallApplicationFilter |
        Set-NetFirewallApplicationFilter -Program $resolvedExecutable
}

Write-Host "Windows Firewall now allows $resolvedExecutable on TCP $Port from LocalSubnet only."
