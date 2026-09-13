# Charge l'environnement de compilation dans la session courante.
#
#   . .\scripts\dev-env.ps1
#
# Rend disponibles cl.exe, cmake.exe, ninja.exe et les DLL Qt. Les chemins sont
# découverts, pas codés en dur : vswhere pour Visual Studio, glob pour Qt.
# Surcharges possibles : $env:QT_DIR, $env:VS_DIR.

$ErrorActionPreference = 'Stop'

function Find-VisualStudio {
    if ($env:VS_DIR -and (Test-Path $env:VS_DIR)) { return $env:VS_DIR }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe introuvable : Visual Studio n'est pas installé." }
    $path = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $path) { throw "Aucune installation Visual Studio avec les outils C++ x64." }
    return $path
}

function Find-Qt {
    if ($env:QT_DIR -and (Test-Path "$env:QT_DIR\bin\qmake.exe")) { return $env:QT_DIR }
    $candidates = Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
        ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
        Where-Object { $_.Name -like 'msvc*_64' -and (Test-Path "$($_.FullName)\bin\qmake.exe") }
    if (-not $candidates) { throw "Aucun Qt 6 MSVC x64 trouvé sous C:\Qt. Définir `$env:QT_DIR." }
    # Version la plus récente : tri sur le nom du dossier parent.
    $newest = $candidates | Sort-Object { [version]$_.Parent.Name } | Select-Object -Last 1
    return $newest.FullName
}

$vsDir = Find-VisualStudio
$qtDir = Find-Qt

# vcvars64 ne s'exécute que dans cmd.exe : on récupère l'environnement produit.
$vcvars = Join-Path $vsDir 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat introuvable dans $vsDir." }

& "$env:ComSpec" /c "call `"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
    }
}

# CMake et Ninja fournis par Visual Studio : inutile de les installer séparément.
$cmakeBin = Join-Path $vsDir 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$ninjaBin = Join-Path $vsDir 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
foreach ($dir in @($cmakeBin, $ninjaBin, "$qtDir\bin")) {
    if ((Test-Path $dir) -and ($env:PATH -notlike "*$dir*")) {
        $env:PATH = "$dir;$env:PATH"
    }
}

$env:QT_DIR = $qtDir
$env:CMAKE_PREFIX_PATH = $qtDir

Write-Host "Visual Studio : $vsDir"
Write-Host "Qt            : $qtDir"
Write-Host "cl            : $((Get-Command cl -ErrorAction SilentlyContinue).Source)"
Write-Host "cmake         : $((Get-Command cmake -ErrorAction SilentlyContinue).Source)"
Write-Host "ninja         : $((Get-Command ninja -ErrorAction SilentlyContinue).Source)"
