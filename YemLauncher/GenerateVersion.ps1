#------------------------------------------------------------------------------------------------------------------
# Version numbers (build is auto-incremented).
#------------------------------------------------------------------------------------------------------------------
$versionMajor = "1"
$versionMinor = "0"
$versionPatch = "0"

#------------------------------------------------------------------------------------------------------------------
# Paths
#------------------------------------------------------------------------------------------------------------------
$buildFile = "$PSScriptRoot\buildnumber.txt"
$template  = "$PSScriptRoot\version.template"
$verFile   = "$PSScriptRoot\version.h"

#------------------------------------------------------------------------------------------------------------------
# Get the build number. If read, increment it.
#------------------------------------------------------------------------------------------------------------------
try
{
    $versionBuild  = (Select-String -Path $verFile -Pattern '^\s*#define\s+VER_BUILD\s+(\d+)' | ForEach-Object { [int]$_.Matches[0].Groups[1].Value })
    $versionBuild += 1
}
catch
{
    $versionBuild = 1
}

#------------------------------------------------------------------------------------------------------------------
# Generate version strings.
#------------------------------------------------------------------------------------------------------------------
$year            = (Get-Date).Year
$copyrightYear   = if ($year -gt 2026) { "2026-$year" } else { $year }
$copyrightStr    = '"' + '© ' + $copyrightYear + ' Marc Klaver"'
$versionFile     = '"'  + $versionMajor + '.' + $versionMinor + '.' + $versionPatch + '.' + $versionBuild + '"'
$versionFileWide = 'L"' + $versionMajor + '.' + $versionMinor + '.' + $versionPatch + '.' + $versionBuild + '"'
$versionProduct  = '"'  + $versionMajor + '.' + $versionMinor + '.' + $versionPatch + '"'

#------------------------------------------------------------------------------------------------------------------
# Load template
#------------------------------------------------------------------------------------------------------------------
$content = Get-Content $template -Raw

#------------------------------------------------------------------------------------------------------------------
# Replace placeholders
#------------------------------------------------------------------------------------------------------------------
$content = $content -replace '___MAJOR_NUMBER___',      $versionMajor
$content = $content -replace '___MINOR_NUMBER___',      $versionMinor    
$content = $content -replace '___PATCH_NUMBER___',      $versionPatch
$content = $content -replace '___BUILD_NUMBER___',      $versionBuild
$content = $content -replace '___YEAR___',              $year
$content = $content -replace '___VERSION_FILE___',      $versionFile
$content = $content -replace '___VERSION_FILE_WIDE___', $versionFileWide
$content = $content -replace '___VERSION_PRODUCT___',   $versionProduct
$content = $content -replace '___COPYRIGHT___',         $copyrightStr

#------------------------------------------------------------------------------------------------------------------
# Write final header (UTF‑8 BOM to avoid © issues)
#------------------------------------------------------------------------------------------------------------------
$content | Out-File $verFile -Encoding UTF8BOM
