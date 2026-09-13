# Configure, compile et teste.
#
#   .\scripts\build.ps1                 Debug, avec les tests
#   .\scripts\build.ps1 -Config Release
#   .\scripts\build.ps1 -Clean

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')] [string] $Config = 'Debug',
    [switch] $Clean,
    [switch] $NoTests
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

. (Join-Path $PSScriptRoot 'dev-env.ps1')

$buildDir = Join-Path $root 'build\msvc'
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "`nNettoyage de $buildDir"
    Remove-Item $buildDir -Recurse -Force
}

Push-Location $root
try {
    Write-Host "`n=== Configuration ==="
    cmake --preset msvc
    if ($LASTEXITCODE -ne 0) { throw "Échec de la configuration CMake." }

    Write-Host "`n=== Compilation ($Config) ==="
    cmake --build --preset $Config.ToLower()
    if ($LASTEXITCODE -ne 0) { throw "Échec de la compilation." }

    if (-not $NoTests) {
        Write-Host "`n=== Tests ($Config) ==="
        ctest --preset $Config.ToLower()
        if ($LASTEXITCODE -ne 0) { throw "Des tests ont échoué." }
    }

    Write-Host "`nBinaires : $buildDir\$Config"
}
finally {
    Pop-Location
}
