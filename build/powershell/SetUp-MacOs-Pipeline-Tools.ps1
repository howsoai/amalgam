#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Setup pipeline tools for macos

.DESCRIPTION
    This script sets up the tools needed for pipeline builds not covered by
    the build machines/containers on macos.

.PARAMETER Arch
    Specifies the arch for the current platform.

.EXAMPLE
    SetUp-MacOs-Pipeline-Tools.ps1 -Arch amd64
#>

# Source helper functions:
. $PSScriptRoot/Helper-Functions.ps1

# Stop if errors:
$ErrorActionPreference="Stop"

# Build all:
function SetUp-MacOs-Pipeline-Tools {

    [CmdletBinding()]
    param(
        [Parameter()]
        [string]$Arch = "amd64"
    )

    $OS = Get-OS
    Write-Host "OS: $OS"
    
    if ($OS.equals("macos")) {

        # Hack to get the arm64 lib downloaded for cross compile
        if ($Arch.equals("arm64")) {
            Invoke-NativeCommand -Cmd "brew" -Arguments @("uninstall", "--ignore-dependencies", "libomp")
            Invoke-NativeCommand -Cmd "brew" -Arguments @("cleanup", "-s")
            Invoke-NativeCommand -Cmd "rm"   -Arguments @("-rf", "$(brew --cache)")
            Invoke-NativeCommand -Cmd "brew" -Arguments @("fetch", "--force", "--bottle-tag=arm64_big_sur", "libomp")
            Invoke-NativeCommand -Cmd "brew" -Arguments @("install", "$(brew --cache --bottle-tag=arm64_big_sur libomp)")
        } else {
            Invoke-NativeCommand -Cmd "brew" -Arguments @("install", "libomp")
        }
        Invoke-NativeCommand -Cmd "brew" -Arguments @("install", "ninja")

        Invoke-NativeCommand -Cmd "brew"  -Arguments @("list", "--versions", "libomp")
        Invoke-NativeCommand -Cmd "ninja" -Arguments @("--version")

    } else {
        Write-Host "Not macos, nothing to do"
    }
}

SetUp-MacOs-Pipeline-Tools @args

exit 0