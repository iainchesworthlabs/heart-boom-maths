# SPDX-License-Identifier: GPL-3.0-or-later
#
# Configure, build and test with the windows-msvc-release preset from any shell on Windows. It loads the Visual Studio
# developer environment first, because a plain shell has no compiler include paths. Run it from the repository root
# (or from a git worktree of it); each worktree keeps its own build directory.
#
#   pwsh tools/dev/test.ps1                 build and run every test
#   pwsh tools/dev/test.ps1 -Filter rng     run only the tests whose name matches
param(
    [string]$Preset = 'windows-msvc-release',
    [string]$Filter = ''
)

$ErrorActionPreference = 'Stop'
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$bat = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$root = (git rev-parse --show-toplevel).Trim()
Set-Location $root

$ctest = "ctest --test-dir build/$Preset --output-on-failure"
if ($Filter -ne '')
{
    $ctest = "$ctest -R $Filter"
}
cmd /c "`"$bat`" >nul && cmake --preset $Preset && cmake --build --preset $Preset && $ctest"
exit $LASTEXITCODE
