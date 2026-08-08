#!/usr/bin/env pwsh
#
# Configure, build, and run only the container tests (CTest label "containers").
# Thin wrapper around run-tests.ps1 -Label containers.
#
# Usage:
#   scripts/run-container-tests.ps1
#   scripts/run-container-tests.ps1 -NoBuild
#
[CmdletBinding()]
param(
    [string]$BuildDir = "out/build/tests",
    [switch]$NoBuild,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CTestArgs
)

& "$PSScriptRoot/run-tests.ps1" `
    -BuildDir $BuildDir `
    -Label "containers" `
    -NoBuild:$NoBuild `
    @CTestArgs

exit $LASTEXITCODE
