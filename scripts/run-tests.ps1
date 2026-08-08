#!/usr/bin/env pwsh
#
# Configure, build, and run the test suite.
#
# Usage:
#   scripts/run-tests.ps1                 # configure + build + run all tests
#   scripts/run-tests.ps1 -NoBuild        # skip configure/build, just run
#   scripts/run-tests.ps1 -Label containers   # run only tests with that label
#   scripts/run-tests.ps1 -- -R SListIterator  # pass extra args straight to ctest
#
[CmdletBinding()]
param(
    # Build directory used for the test configuration.
    [string]$BuildDir = "out/build/tests",

    # Restrict the run to tests carrying this CTest label (e.g. "containers").
    [string]$Label,

    # Skip the configure/build step and run the existing binaries as-is.
    [switch]$NoBuild,

    # Anything after `--` is forwarded verbatim to ctest.
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CTestArgs
)

# Note: we deliberately do NOT set $ErrorActionPreference = "Stop". In Windows
# PowerShell, native tools (cmake/ctest) writing to stderr would otherwise be
# turned into terminating errors. We gate on $LASTEXITCODE instead.

# Run from the repo root regardless of where the script is invoked from.
$RepoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $RepoRoot
try
{
    if (-not $NoBuild)
    {
        cmake -S . -B $BuildDir -DCPP_CORE_SYSTEMS_BUILD_TESTS=ON
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

        cmake --build $BuildDir
        if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
    }

    $args = @("--test-dir", $BuildDir, "--output-on-failure")
    if ($Label) { $args += @("-L", $Label) }
    if ($CTestArgs) { $args += $CTestArgs }

    ctest @args
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)" }
}
finally
{
    Pop-Location
}
