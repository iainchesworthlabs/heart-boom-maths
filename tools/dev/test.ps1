# SPDX-License-Identifier: GPL-3.0-or-later
#
# Configure, build and test with the windows-msvc-release preset from any shell on Windows. It loads the Visual Studio
# developer environment first, because a plain shell has no compiler include paths. Run it from the repository root
# (or from a git worktree of it); each worktree keeps its own build directory.
#
#   pwsh tools/dev/test.ps1                 build and run every test
#   pwsh tools/dev/test.ps1 -Filter rng     run only the tests whose name matches
#   pwsh tools/dev/test.ps1 -Perf           build with the performance tests and run only those (label perf, one at a time),
#                                           then print the summary; the ordinary run never builds or runs them
param(
    [string]$Preset = 'windows-msvc-release',
    [string]$Filter = '',
    [switch]$Perf
)

$ErrorActionPreference = 'Stop'
if ($Perf -and $Preset -eq 'windows-msvc-release')
{
    $Preset = 'windows-msvc-release-perf'
}
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$bat = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$root = (git rev-parse --show-toplevel).Trim()
Set-Location $root

# The game's tests start the real program, which needs the Qt libraries on PATH on Windows.
if ($env:ICL_QT_ROOT)
{
    $qtBin = Join-Path $env:ICL_QT_ROOT 'bin'
    if (Test-Path $qtBin)
    {
        $env:PATH = "$qtBin;$env:PATH"
    }
}

$ctest = "ctest --test-dir build/$Preset --output-on-failure"
if ($Perf)
{
    # One test at a time on a quiet machine, and the results of an earlier run gone, so the summary is of this run.
    $ctest = "$ctest -L perf -j 1 -V"
    Remove-Item -Recurse -Force "build/$Preset/perf-results" -ErrorAction SilentlyContinue
}
if ($Filter -ne '')
{
    $ctest = "$ctest -R $Filter"
}
cmd /c "`"$bat`" >nul && cmake --preset $Preset && cmake --build --preset $Preset && $ctest"
$code = $LASTEXITCODE
if ($Perf)
{
    python tools/ci/perf_report.py --results "build/$Preset/perf-results"
    if ($code -eq 0)
    {
        $code = $LASTEXITCODE
    }
}
exit $code
