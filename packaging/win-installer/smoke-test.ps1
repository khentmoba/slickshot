#Requires -Version 5.1
<#
.SYNOPSIS
    Smoke-tests SlickShot Windows release artifacts.

.DESCRIPTION
    -Installer <setup.exe> -Version <x.y.z>
        Installs silently, installs again over the live install (the upgrade
        path), checks that uninstall refuses while SlickShot is running and
        leaves the install whole, then runs the registered uninstaller the way
        Windows Settings does. Fails if any file is left in the install folder
        (uninstall residue).

    -PortableZip <zip>
        Checks the .sha256sum sidecar: it must name the zip by bare filename and
        its hash must match the zip.

    The installer mode targets the real per-user install paths, so it only runs
    in CI (CI=true). Outside CI it requires -InstallDir, -ShortcutPath and
    -UninstallKey, which keeps a developer's own SlickShot install out of reach.
#>
[CmdletBinding()]
param(
    [string]$Installer,
    [string]$Version,
    [string]$PortableZip,
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'Programs\SlickShot'),
    [string]$ShortcutPath = (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\SlickShot.lnk'),
    [string]$UninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SlickShot',
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'

function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "SMOKE FAIL: $Message" }
    Write-Host "PASS: $Message"
}

function Test-PortableChecksum([string]$Zip) {
    $sidecar = "$Zip.sha256sum"
    Assert-Check (Test-Path -LiteralPath $Zip) 'portable zip exists'
    Assert-Check (Test-Path -LiteralPath $sidecar) 'checksum sidecar exists'

    $line = Get-Content -LiteralPath $sidecar -TotalCount 1
    # sha256sum writes '<hash>  <name>' in text mode and '<hash> *<name>' in binary mode.
    if ($line -match '^([0-9a-fA-F]{64}) [ *](.+?)\s*$') {
        $expectedHash = $Matches[1]
        $namedFile = $Matches[2]
    } else {
        throw "SMOKE FAIL: sidecar is not a sha256sum line: $line"
    }

    $zipName = Split-Path -Leaf $Zip
    Assert-Check ($namedFile -ceq $zipName) "sidecar names the bare filename (got '$namedFile')"
    $actualHash = (Get-FileHash -LiteralPath $Zip -Algorithm SHA256).Hash
    Assert-Check ($actualHash -ieq $expectedHash) 'sidecar hash matches the zip'
}

function Invoke-SilentInstaller([string]$Label) {
    $proc = Start-Process -FilePath $Installer -ArgumentList '/S' -Wait -PassThru
    Assert-Check ($proc.ExitCode -eq 0) "$Label exits 0 (got $($proc.ExitCode))"
}

function Assert-Installed {
    $app = Join-Path $InstallDir 'bin\flameshot.exe'
    Assert-Check (Test-Path -LiteralPath $app) 'app binary is installed'
    Assert-Check (Test-Path -LiteralPath (Join-Path $InstallDir 'uninstall.exe')) 'uninstaller is written'

    $fileVersion = (Get-Item -LiteralPath $app).VersionInfo.FileVersion
    Assert-Check ($fileVersion -eq $Version -or $fileVersion -eq "$Version.0") "flameshot.exe FileVersion is $Version (got '$fileVersion')"

    $reg = Get-ItemProperty -LiteralPath $UninstallKey -ErrorAction SilentlyContinue
    Assert-Check ($null -ne $reg -and $reg.DisplayVersion -eq $Version) "registry DisplayVersion is $Version"
    Assert-Check (Test-Path -LiteralPath $ShortcutPath) 'Start Menu shortcut exists'
}

function Start-RegisteredUninstall {
    $reg = Get-ItemProperty -LiteralPath $UninstallKey
    if ($reg.QuietUninstallString -notmatch '^"([^"]+)"\s*(.*)$') {
        throw "SMOKE FAIL: QuietUninstallString is not a quoted path: $($reg.QuietUninstallString)"
    }
    return Start-Process -FilePath $Matches[1] -ArgumentList $Matches[2] -PassThru
}

function Wait-ForRemoval([string]$Path, [int]$Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Test-Path -LiteralPath $Path) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
    }
    -not (Test-Path -LiteralPath $Path)
}

function Test-UninstallRefusedWhileRunning {
    # Stand-in for a running SlickShot: a read handle that denies writers keeps
    # bin\flameshot.exe locked the same way the running app does.
    $app = Join-Path $InstallDir 'bin\flameshot.exe'
    $lock = [System.IO.File]::Open($app, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $null = Start-RegisteredUninstall
        # The registered command hands its work to a copy of itself and returns
        # first, so wait a fixed grace period. A refusal leaves the install whole;
        # a removal finishes well inside the grace period.
        Start-Sleep -Seconds 10
        Assert-Check (Test-Path -LiteralPath (Join-Path $InstallDir 'bin\flameshot.exe')) 'refused uninstall left the app binary'
        Assert-Check (Test-Path -LiteralPath (Join-Path $InstallDir 'uninstall.exe')) 'refused uninstall left the uninstaller'
        Assert-Check (Test-Path -LiteralPath $UninstallKey) 'refused uninstall left the registry entry'
        Assert-Check (Test-Path -LiteralPath $ShortcutPath) 'refused uninstall left the Start Menu shortcut'
    } finally {
        $lock.Dispose()
    }
}

function Test-InstallerRoundTrip {
    Assert-Check (-not (Test-Path -LiteralPath $InstallDir)) 'install folder starts absent'

    Invoke-SilentInstaller 'first silent install'
    Assert-Installed

    Invoke-SilentInstaller 'second silent install over the live install (upgrade)'
    Assert-Installed

    Test-UninstallRefusedWhileRunning

    $null = Start-RegisteredUninstall
    if (-not (Wait-ForRemoval $InstallDir $TimeoutSeconds)) {
        $left = Get-ChildItem -LiteralPath $InstallDir -Recurse -Force |
            ForEach-Object { $_.FullName.Substring($InstallDir.Length + 1) }
        throw "SMOKE FAIL: uninstall left files in ${InstallDir}: $($left -join ', ')"
    }
    Write-Host "PASS: uninstall removed $InstallDir"
    Assert-Check (-not (Test-Path -LiteralPath $UninstallKey)) 'registry entry removed'
    Assert-Check (-not (Test-Path -LiteralPath $ShortcutPath)) 'Start Menu shortcut removed'
}

if ($PortableZip) {
    Test-PortableChecksum $PortableZip
} elseif ($Installer) {
    if (-not $Version) { throw 'SMOKE FAIL: -Version is required with -Installer' }

    $isolated = $PSBoundParameters.ContainsKey('InstallDir') -and
                $PSBoundParameters.ContainsKey('ShortcutPath') -and
                $PSBoundParameters.ContainsKey('UninstallKey')
    if (-not $isolated -and $env:CI -ne 'true') {
        throw 'SMOKE FAIL: refusing to touch the real per-user SlickShot install outside CI. Run in CI, or pass -InstallDir, -ShortcutPath and -UninstallKey.'
    }

    Assert-Check (Test-Path -LiteralPath $Installer) 'installer exists'
    Test-InstallerRoundTrip
} else {
    throw 'SMOKE FAIL: pass -Installer with -Version, or -PortableZip'
}

Write-Host 'SMOKE OK'
