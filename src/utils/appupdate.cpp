#include "appupdate.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>

#if defined(Q_OS_WIN)
#include <windows.h>

#include <string>
#endif

namespace {

QString assetUrl(const QJsonArray& assets, const QString& pattern)
{
    const QRegularExpression name(pattern);
    for (const QJsonValue value : assets) {
        const QJsonObject asset = value.toObject();
        if (name.match(asset["name"].toString()).hasMatch()) {
            return asset["browser_download_url"].toString();
        }
    }
    return {};
}

QString toSlashes(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(path);
}

} // namespace

bool AppUpdate::isTrustedAssetUrl(const QString& value)
{
    const QUrl url(value);
    if (!url.isValid()) {
        return false;
    }
    if (url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) !=
        0) {
        return false;
    }
    const QString host = url.host().toLower();
    if (host == QLatin1String("github.com")) {
        return url.path().startsWith(
          QLatin1String("/khentmoba/slickshot/releases/"), Qt::CaseInsensitive);
    }
    // GitHub redirects release asset downloads to these CDN hosts.
    return host == QLatin1String("objects.githubusercontent.com") ||
           host == QLatin1String("github-releases.githubusercontent.com") ||
           host == QLatin1String("release-assets.githubusercontent.com");
}

bool AppUpdate::isTrustedReleasePageUrl(const QString& value)
{
    const QUrl page(value);
    return page.isValid() &&
           page.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) ==
             0 &&
           page.host().compare(QLatin1String("github.com"),
                               Qt::CaseInsensitive) == 0 &&
           page.path().startsWith(
             QLatin1String("/khentmoba/slickshot/releases/"),
             Qt::CaseInsensitive);
}

AppUpdateInfo AppUpdate::releaseInfo(const QJsonObject& release)
{
    AppUpdateInfo info;
    info.version = release["tag_name"].toString();
    if (info.version.startsWith(QLatin1Char('v'))) {
        info.version.remove(0, 1);
    }

    // Never open a release page handed to us by an untrusted response unless it
    // is an HTTPS SlickShot release page on GitHub.
    const QString pageUrl = release["html_url"].toString();
    if (isTrustedReleasePageUrl(pageUrl)) {
        info.pageUrl = pageUrl;
    }

    const QJsonArray assets = release["assets"].toArray();
    const QString installer = assetUrl(
      assets, QStringLiteral("^SlickShot-[0-9A-Za-z._+-]+-win64-setup\\.exe$"));
    const QString checksum =
      assetUrl(assets,
               QStringLiteral(
                 "^SlickShot-[0-9A-Za-z._+-]+-win64-setup\\.exe\\.sha256sum$"));
    // Only accept asset URLs we are willing to download from. When either is
    // untrusted, leave both empty so the caller falls back to the release page.
    if (isTrustedAssetUrl(installer) && isTrustedAssetUrl(checksum)) {
        info.installerUrl = installer;
        info.checksumUrl = checksum;
    }
    return info;
}

bool AppUpdate::canInstallInPlaceForPaths(const AppUpdateInfo& info,
                                          const QString& appPath,
                                          const QString& installRoot)
{
    if (info.installerUrl.isEmpty() || info.checksumUrl.isEmpty() ||
        appPath.isEmpty() || installRoot.isEmpty()) {
        return false;
    }
    const QString root = toSlashes(installRoot) + QLatin1Char('/');
    return toSlashes(appPath).startsWith(root, Qt::CaseInsensitive);
}

bool AppUpdate::canInstallInPlace(const AppUpdateInfo& info)
{
#if defined(Q_OS_WIN) && !defined(USE_PORTABLE_CONFIG)
    const QString appDir = QCoreApplication::applicationDirPath();
    if (QFileInfo::exists(
          QDir(appDir).filePath(QStringLiteral("flameshot.ini")))) {
        // A portable copy keeps its config next to the exe. The installer
        // removes the install folder, so never replace a portable copy.
        return false;
    }
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) {
        return false;
    }
    const QString installRoot =
      QDir(localAppData).filePath(QStringLiteral("Programs/SlickShot"));
    return canInstallInPlaceForPaths(
      info, QCoreApplication::applicationFilePath(), installRoot);
#else
    Q_UNUSED(info);
    return false;
#endif
}

QString AppUpdate::parseSha256(const QString& checksumFile,
                               const QString& expectedFileName)
{
    // sha256sum writes "<digest> *<name>" (the '*' marks binary mode), with a
    // leading '\' when the file name contains backslashes. Accept only that
    // exact shape: 64 hex digits followed by spaces/tabs, an optional '*',
    // then the file name. Allow a single leading UTF-8 BOM, which is otherwise
    // not part of the line.
    QString line = checksumFile;
    if (line.startsWith(QChar(0xFEFF))) {
        line.remove(0, 1);
    }
    static const QRegularExpression digest(
      QStringLiteral("^\\\\?([0-9a-fA-F]{64})[ \\t]+\\*?(.+)$"));
    const QRegularExpressionMatch match = digest.match(line);
    if (!match.hasMatch()) {
        return QString();
    }

    if (!expectedFileName.isEmpty()) {
        // Only the base name matters: sha256sum may record a path prefix, and
        // Windows paths may use either separator.
        const QString recorded = match.captured(2);
        const QString recordedBase =
          recorded.section(QLatin1Char('/'), -1).section(QLatin1Char('\\'), -1);
        if (recordedBase.trimmed().compare(expectedFileName,
                                           Qt::CaseInsensitive) != 0) {
            return QString();
        }
    }
    return match.captured(1).toLower();
}

QString AppUpdate::installScript(qint64 pid,
                                 const QString& installerPath,
                                 const QString& expectedSha256,
                                 const QString& appPath)
{
    // Single-quoted PowerShell literals only need ' doubled.
    const auto literal = [](const QString& value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
        return QStringLiteral("'") + escaped + QStringLiteral("'");
    };
    return QStringLiteral(
             "$ErrorActionPreference = 'Stop'\n"
             "$log = Join-Path $env:TEMP 'SlickShot-update.log'\n"
             "$installer = %2\n"
             "$app = %4\n"
             "# Bound the wait so a hung SlickShot cannot block the update.\n"
             "$proc = Get-Process -Id %1 -ErrorAction SilentlyContinue\n"
             "if ($null -ne $proc) {\n"
             "  $exited = $true\n"
             "  try {\n"
             "    $exited = $proc.WaitForExit(120000)\n"
             "  } catch [System.InvalidOperationException] {\n"
             "    $exited = $true\n"
             "  }\n"
             "  if (-not $exited) {\n"
             "    'Timed out waiting for SlickShot to exit; aborting update.' "
             "| Add-Content -LiteralPath $log\n"
             "    Remove-Item -LiteralPath $installer -Force "
             "-ErrorAction SilentlyContinue\n"
             "    Remove-Item -LiteralPath (Split-Path -Parent $installer) "
             "-Force -ErrorAction SilentlyContinue\n"
             "    exit 1\n"
             "  }\n"
             "}\n"
             "try {\n"
             "  # Hold the installer open until it exits. Other processes "
             "cannot\n"
             "  # write or delete it meanwhile, so the bytes hashed here are "
             "the\n"
             "  # bytes that run.\n"
             "  $stream = [System.IO.File]::Open($installer, "
             "[System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, "
             "[System.IO.FileShare]::Read)\n"
             "  try {\n"
             "    $digest = [BitConverter]::ToString("
             "[System.Security.Cryptography.SHA256]::Create()"
             ".ComputeHash($stream)).Replace('-', '')\n"
             "    if ($digest -ine %3) { throw 'checksum mismatch' }\n"
             "    $setup = Start-Process -FilePath $installer "
             "-ArgumentList '/S' -Wait -PassThru\n"
             "    if ($setup.ExitCode -ne 0) { "
             "throw ('installer exited with ' + $setup.ExitCode) }\n"
             "  } finally {\n"
             "    $stream.Dispose()\n"
             "  }\n"
             "} catch {\n"
             "  ($_ | Out-String) | Add-Content -LiteralPath $log\n"
             "}\n"
             "Remove-Item -LiteralPath $installer -Force "
             "-ErrorAction SilentlyContinue\n"
             "Remove-Item -LiteralPath (Split-Path -Parent $installer) "
             "-Force -ErrorAction SilentlyContinue\n"
             "Start-Process -FilePath $app\n")
      .arg(QString::number(pid),
           literal(installerPath),
           literal(expectedSha256),
           literal(appPath));
}

QString AppUpdate::encodeCommand(const QString& script)
{
    const QByteArray utf16(reinterpret_cast<const char*>(script.utf16()),
                           script.size() * 2);
    return QString::fromLatin1(utf16.toBase64());
}

QString AppUpdate::powershellExecutablePath()
{
#if defined(Q_OS_WIN)
    wchar_t sysDir[MAX_PATH] = {};
    const UINT len = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return QDir::toNativeSeparators(
      QDir(QString::fromWCharArray(sysDir, static_cast<int>(len)))
        .filePath(QStringLiteral("WindowsPowerShell/v1.0/powershell.exe")));
#else
    return {};
#endif
}

bool AppUpdate::startHidden(const QString& program, const QString& arguments)
{
#if defined(Q_OS_WIN)
    if (program.isEmpty()) {
        return false;
    }
    const std::wstring appName = program.toStdWString();
    std::wstring cmdLine = L"\"" + appName + L"\"";
    if (!arguments.isEmpty()) {
        cmdLine += L" ";
        cmdLine += arguments.toStdWString();
    }

    STARTUPINFOW si{};
    si.cb = static_cast<DWORD>(sizeof(si));
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    const BOOL started = CreateProcessW(appName.c_str(),
                                        cmdLine.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        CREATE_NO_WINDOW,
                                        nullptr,
                                        nullptr,
                                        &si,
                                        &pi);
    if (!started) {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    Q_UNUSED(program);
    Q_UNUSED(arguments);
    return false;
#endif
}
