# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compile every core source file (and optionally the tests) with Clang and the strict warning set that the Apple and
# Linux CI builds use, without linking. GCC-style warnings that MSVC does not report show up here before CI finds them.
# Needs a configured windows-msvc-release build (for the generated headers) and the LLVM that ships with Visual Studio.
#
#   pwsh tools/dev/clang-check.ps1            core sources
#   pwsh tools/dev/clang-check.ps1 -Tests     core sources and tests
param(
    [switch]$Tests
)

$ErrorActionPreference = 'Stop'
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$bat = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$root = (git rev-parse --show-toplevel).Trim()
Set-Location $root

$flags = '-std=c++23 -fsyntax-only --target=x86_64-pc-windows-msvc -Wall -Wextra -Wpedantic -Werror -Wshadow -Wcast-align ' +
    '-Wunused -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wimplicit-fallthrough -Wformat=2 ' +
    '-Wnon-virtual-dtor -Woverloaded-virtual -Wold-style-cast'
$generated = "build/windows-msvc-release/libs/core/generated"
$testGenerated = "build/windows-msvc-release/libs/core/tests/generated"
$vcpkg = "build/windows-msvc-release/vcpkg_installed/x64-windows/include"

$files = Get-ChildItem -Path libs/core/src -Filter *.cpp
if ($Tests)
{
    $files += Get-ChildItem -Path libs/core/tests -Filter *.cpp
}

$failed = 0
foreach ($file in $files)
{
    $cmd = "`"$bat`" >nul && clang++ $flags -Ilibs/core/include -Ilibs/core/tests -I$generated -I$testGenerated " +
        "-isystem $vcpkg `"$($file.FullName)`""
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0)
    {
        $failed++
    }
}
if ($failed -gt 0)
{
    Write-Error "$failed file(s) have Clang warnings."
    exit 1
}
Write-Host 'Clang strict check passed.'
