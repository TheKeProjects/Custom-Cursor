# Writes the changelog.txt published next to version.txt (read by the update window and the
# "What's New" dialog) from the in-app changelog in Source\Services\App\ChangelogData.cpp: every
# version up to the one in Resources\Version.h, newest first, in the old changelog.txt format so
# editors several versions behind see everything they missed:
#
#   3.0.1                  (or "2.1.0 | 21/09/26" when the entry has a date)
#   - change
#   - change
#
#   3.0.0
#   ...
param(
    [string]$ProjectDir = (Split-Path -Parent $PSScriptRoot),
    [string]$Output = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'Custom Cursor-Cs Source Code\Custom-Cursor\changelog.txt')
)

$ErrorActionPreference = 'Stop'

function Parse-Version([string]$v) {
    $parts = ($v -replace '[^0-9.]', '').Split('.') | ForEach-Object { [int]$_ }
    while ($parts.Count -lt 3) { $parts += 0 }
    return [version]::new($parts[0], $parts[1], $parts[2])
}

$versionHeader = Get-Content (Join-Path $ProjectDir 'Resources\Version.h') -Raw
$released = Parse-Version ([regex]::Match($versionHeader, 'APP_VERSION_STRING\s+"([^"]+)"').Groups[1].Value)

$source = Get-Content (Join-Path $ProjectDir 'Source\Services\App\ChangelogData.cpp') -Raw -Encoding UTF8
$lines = New-Object System.Collections.Generic.List[string]
$entryRe = [regex]'(?s)\{\s*L"([^"]+)",\s*L"((?:[^"\\]|\\.)*)",\s*\{(.*?)\}\s*\}'
$stringRe = [regex]'L"((?:[^"\\]|\\.)*)"'
foreach ($m in $entryRe.Matches($source)) {
    $version = $m.Groups[1].Value
    if ((Parse-Version $version) -gt $released) { continue }   # not released yet
    if ($lines.Count) { $lines.Add('') }
    $label = $m.Groups[2].Value -replace '\\"', '"' -replace '\\\\', '\'
    if ($label) { $lines.Add("$version $label") } else { $lines.Add($version) }
    foreach ($s in $stringRe.Matches($m.Groups[3].Value)) {
        $lines.Add(($s.Groups[1].Value -replace '\\"', '"' -replace '\\\\', '\'))
    }
}

[System.IO.File]::WriteAllText($Output, ($lines -join "`n") + "`n", [System.Text.UTF8Encoding]::new($false))
Write-Host "Wrote $Output (versions up to $released)"
