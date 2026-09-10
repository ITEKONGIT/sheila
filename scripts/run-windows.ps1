[CmdletBinding()]
param(
    [ValidateRange(1, 65535)]
    [int]$Port = 18877,

    [string]$DataDirectory = (Join-Path $PSScriptRoot "..\data")
)

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$executable = Join-Path $projectRoot "dist\windows\sSheila.exe"

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "sSheila is not built at $executable"
}

Push-Location $projectRoot
try {
    & $executable --port $Port --data-dir $DataDirectory
} finally {
    Pop-Location
}
