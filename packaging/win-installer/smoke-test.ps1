#Requires -Version 5.1
<#
.SYNOPSIS
    Smoke-tests SlickShot Windows release artifacts.

.DESCRIPTION
    -Installer <setup.exe> -Version <x.y.z>
        Installs silently, installs again over the live install (the upgrade
        path), checks that a marker config file in the Roaming config location
        survives that upgrade, checks that uninstall refuses while SlickShot is
        running and leaves the install whole, then runs the registered
        uninstaller the way Windows Settings does. Fails if any file is left in
        the install folder (uninstall residue).

    -PortableZip <zip>
        Checks the .sha256sum sidecar (bare filename and hash), extracts the zip
        to a temporary folder, checks that it contains bin\flameshot.exe and a
        populated translations folder if one is present, then cleans up.

    The installer mode targets the real per-user install and config paths, so it
    only runs in CI (CI=true). Outside CI it requires -InstallDir,
    -ShortcutPath, -UninstallKey and -ConfigDir, which keeps a developer's own
    SlickShot install and settings out of reach.
#>
[CmdletBinding()]
param(
    [string]$Installer,
    [string]$Version,
    [string]$PortableZip,
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'Programs\SlickShot'),
    [string]$ShortcutPath = (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\SlickShot.lnk'),
    [string]$UninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SlickShot',
    # Installer builds keep config in user scope at
    # %APPDATA%\flameshot\flameshot.ini (the directory name is inherited).
    [string]$ConfigDir = (Join-Path $env:APPDATA 'flameshot'),
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'

function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "SMOKE FAIL: $Message" }
    Write-Host "PASS: $Message"
}

$script:IntegrationKeys = @(
    'HKCU:\Software\Classes\ms-screenclip',
    'HKCU:\Software\Classes\SlickShot.ms-screenclip',
    'HKCU:\Software\SlickShot',
    'HKCU:\Software\Microsoft\Windows\CurrentVersion\App Paths\SnippingTool.exe',
    'HKCU:\Software\Microsoft\Windows\CurrentVersion\App Paths\ScreenClippingHost.exe',
    'HKCU:\Software\Microsoft\Windows\CurrentVersion\App Paths\flameshot.exe'
)

$script:IntegrationValues = @(
    @{ Path = 'HKCU:\Control Panel\Keyboard'; Name = 'PrintScreenKeyForSnippingEnabled' },
    @{ Path = 'HKCU:\Software\RegisteredApplications'; Name = 'SlickShot' },
    @{ Path = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'; Name = 'SlickShot' },
    @{ Path = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'; Name = 'Flameshot' }
)

function Set-WindowsIntegrationMarkers {
    if ($env:CI -ne 'true') {
        Write-Host 'SKIP: integration registry markers only run in CI'
        return
    }
    foreach ($key in $script:IntegrationKeys) {
        New-Item -Path $key -Force | Out-Null
        New-ItemProperty -Path $key -Name 'SlickShotSmokeTest' -Value 'present' -PropertyType String -Force | Out-Null
    }
    foreach ($value in $script:IntegrationValues) {
        New-Item -Path $value.Path -Force | Out-Null
        New-ItemProperty -Path $value.Path -Name $value.Name -Value 'slickshot-smoke' -PropertyType String -Force | Out-Null
    }
}

function Assert-WindowsIntegrationRemoved {
    if ($env:CI -ne 'true') { return }
    foreach ($key in $script:IntegrationKeys) {
        Assert-Check (-not (Test-Path -LiteralPath $key)) "registry key removed: $key"
    }
    foreach ($value in $script:IntegrationValues) {
        $item = Get-ItemProperty -LiteralPath $value.Path -ErrorAction SilentlyContinue
        $present = $null -ne $item -and $null -ne $item.PSObject.Properties[$value.Name]
        Assert-Check (-not $present) "registry value removed: $($value.Name)"
    }
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

function Test-PortableContents([string]$Zip) {
    # CPack may or may not add a top-level directory to the archive, so look for
    # the executable under any bin\ directory in the extraction.
    $temp = Join-Path ([System.IO.Path]::GetTempPath()) ("slickshot-smoke-" + [guid]::NewGuid().ToString('N'))
    New-Item -Path $temp -ItemType Directory -Force | Out-Null
    try {
        Expand-Archive -LiteralPath $Zip -DestinationPath $temp -Force

        $exe = Get-ChildItem -LiteralPath $temp -Recurse -File -Filter 'flameshot.exe' |
            Where-Object { $_.DirectoryName -match '[\\/]bin$' } |
            Select-Object -First 1
        Assert-Check ($null -ne $exe) 'portable archive contains bin\flameshot.exe'

        $translations = Get-ChildItem -LiteralPath $temp -Recurse -Directory -Filter 'translations' |
            Select-Object -First 1
        if ($null -ne $translations) {
            $entries = @(Get-ChildItem -LiteralPath $translations.FullName -Force)
            Assert-Check ($entries.Count -gt 0) 'portable archive translations folder is populated'
        }
    } finally {
        Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
    }
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
        $proc = Start-RegisteredUninstall
        # The registered command hands its work to a copy of itself and exits
        # first. Wait on the launcher, then give the child time to attempt the
        # removal so a broken lock guard cannot pass before the deletion runs.
        Wait-Process -Id $proc.Id -Timeout $TimeoutSeconds -ErrorAction SilentlyContinue
        $proc.Refresh()
        Assert-Check $proc.HasExited "registered uninstall process exited within $TimeoutSeconds seconds"
        Start-Sleep -Seconds 5
        Assert-Check (Test-Path -LiteralPath (Join-Path $InstallDir 'bin\flameshot.exe')) 'refused uninstall left the app binary'
        Assert-Check (Test-Path -LiteralPath (Join-Path $InstallDir 'uninstall.exe')) 'refused uninstall left the uninstaller'
        Assert-Check (Test-Path -LiteralPath $UninstallKey) 'refused uninstall left the registry entry'
        Assert-Check (Test-Path -LiteralPath $ShortcutPath) 'refused uninstall left the Start Menu shortcut'
        if ($env:CI -eq 'true') {
            Assert-Check (Test-Path -LiteralPath 'HKCU:\Software\Classes\ms-screenclip') 'refused uninstall left Snipping Tool registration'
        }
    } finally {
        $lock.Dispose()
    }
}

function Test-InstallerRoundTrip {
    Assert-Check (-not (Test-Path -LiteralPath $InstallDir)) 'install folder starts absent'

    Invoke-SilentInstaller 'first silent install'
    Assert-Installed

    # Installer builds use QSettings UserScope, so settings live in Roaming and
    # must not be removed by the installer's RMDir of the install folder.
    $configFile = Join-Path $ConfigDir 'flameshot.ini'
    $configExisted = Test-Path -LiteralPath $configFile
    $marker = "slickshot-smoke-$([guid]::NewGuid().ToString('N'))"
    if (-not (Test-Path -LiteralPath $ConfigDir)) {
        New-Item -Path $ConfigDir -ItemType Directory -Force | Out-Null
    }
    Set-Content -LiteralPath $configFile -Value "; SlickShot smoke-test marker: $marker" -Encoding UTF8 -Force
    Assert-Check (Test-Path -LiteralPath $configFile) 'marker config file written before upgrade install'

    Invoke-SilentInstaller 'second silent install over the live install (upgrade)'
    Assert-Installed
    Assert-Check (Test-Path -LiteralPath $configFile) 'config file still exists after the upgrade install'
    Assert-Check ((Get-Content -LiteralPath $configFile -Raw) -match [regex]::Escape($marker)) 'config marker survived the upgrade install'

    Set-WindowsIntegrationMarkers
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
    Assert-WindowsIntegrationRemoved

    # Do not leave a marker behind in an isolated developer config directory.
    if (-not $configExisted) {
        Remove-Item -LiteralPath $configFile -Force -ErrorAction SilentlyContinue
    }
}

if ($PortableZip) {
    Test-PortableChecksum $PortableZip
    Test-PortableContents $PortableZip
} elseif ($Installer) {
    if (-not $Version) { throw 'SMOKE FAIL: -Version is required with -Installer' }

    $isolated = $PSBoundParameters.ContainsKey('InstallDir') -and
                $PSBoundParameters.ContainsKey('ShortcutPath') -and
                $PSBoundParameters.ContainsKey('UninstallKey') -and
                $PSBoundParameters.ContainsKey('ConfigDir')
    if (-not $isolated -and $env:CI -ne 'true') {
        throw 'SMOKE FAIL: refusing to touch the real per-user SlickShot install outside CI. Run in CI, or pass -InstallDir, -ShortcutPath, -UninstallKey and -ConfigDir.'
    }

    Assert-Check (Test-Path -LiteralPath $Installer) 'installer exists'
    Test-InstallerRoundTrip
} else {
    throw 'SMOKE FAIL: pass -Installer with -Version, or -PortableZip'
}

Write-Host 'SMOKE OK'
