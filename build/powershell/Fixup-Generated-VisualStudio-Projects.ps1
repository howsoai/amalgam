#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Fixup the CMake generated VisualStudio project files

.DESCRIPTION
    This script edits all of the CMake generated VisualStudio project files with
    updates that are not available through CMake.
    
    The first case is because CMake itself does not allow for setting config-specific
    settings on a PropertyGroup (set_property for VS_GLOBAL settings does not accept
    generator expressions). Therefore, there is no way to set those settings in CMake.

    The second case is issues where complier/linker flags should set the right behavior
    but for some reason... don't. This could be because of a CMake bug or user
    error when we set the flags (setting opposing flags possibly).

    Both of these cases are issues and are currently handled in this script post CMake
    generate.

.EXAMPLE
    Fixup-Generated-VisualStudio-Projects.ps1
#>

# Source helper functions:
. $PSScriptRoot/Helper-Functions.ps1

# Stop if errors:
$ErrorActionPreference="Stop"

function Fixup-Generated-VisualStudio-Projects {

    [CmdletBinding()]
    param(
        [Parameter()]
        [string]$Preset = "amd64-windows-vs"
    )

    $OS = Get-OS
    Write-Host "OS: $OS"

    # VS generator only supported on windows:
    if($OS.equals("windows")) {

        $nl = [Environment]::NewLine
        Write-Host "Gathering VS project files..."
        $ProjectFiles = (Get-ChildItem "out/build/$Preset/amalgam-*.vcxproj") | Select-Object -ExpandProperty FullName | Out-String -Stream | Select-String -Pattern "(app|sharedlib|objlib)"
        foreach($ProjFile in $ProjectFiles) {

            # Read proj in as one string for simple replacements:
            $ProjFileContents = Get-Content -Path $ProjFile -Encoding UTF8 -Raw

            # Properties in "PropertyGroup" for a specific config (debug, release) cannot be set through CMake:
            $ProjFileContents = $ProjFileContents -ireplace "(<PropertyGroup Condition=.*debug.*/>$nl)", "`$1    <UseDebugLibraries>true</UseDebugLibraries>$nl"
            $ProjFileContents = $ProjFileContents -ireplace "(<PropertyGroup Condition=.*release.*/>$nl)", "`$1    <WholeProgramOptimization>true</WholeProgramOptimization>$nl"

            # Properties in "ProjectReference" can't be set through CMake:
            $ProjFileContents = $ProjFileContents -ireplace '(<LinkLibraryDependencies>)(false)', '$1true'

            # For some reason, setting /ZI for debug builds does not correctly set EditAndContinue. It is unknown why this
            # true, possibly a CMake bug, VS bug in reading order of options, or we incorrectly set the compiler/linker options.
            $re = [regex]'<DebugInformationFormat>ProgramDatabase'
            $ProjFileContents = $re.Replace($ProjFileContents, '<DebugInformationFormat>EditAndContinue', 1)

            # Write file back out:
            Set-Content -Path $ProjFile -Value $ProjFileContents -Encoding UTF8
        }

        Write-Host "Fix-up completed, edited $($ProjectFiles.length) files"
    } else {
        Write-Host "Visual Studio generation (and fixup) only supported on windows, nothing to do"
    }
}

Fixup-Generated-VisualStudio-Projects @args

exit 0