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

#include <shellapi.h>

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
    // browser_download_url normally points at github.com; GitHub may redirect
    // asset downloads to these hosts, so allow them as download origins too.
    return host == QLatin1String("github.com") ||
           host == QLatin1String("objects.githubusercontent.com") ||
           host == QLatin1String("github-releases.githubusercontent.com") ||
           host == QLatin1String("release-assets.githubusercontent.com");
}

AppUpdateInfo AppUpdate::releaseInfo(const QJsonObject& release)
{
    AppUpdateInfo info;
    info.version = release["tag_name"].toString();
    if (info.version.startsWith(QLatin1Char('v'))) {
        info.version.remove(0, 1);
    }

    // Never open a release page handed to us by an untrusted response unless it
    // is an HTTPS page on GitHub.
    const QString pageUrl = release["html_url"].toString();
    const QUrl page(pageUrl);
    if (page.isValid() &&
        page.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) ==
          0 &&
        page.host().compare(QLatin1String("github.com"), Qt::CaseInsensitive) ==
          0) {
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

QString AppUpdate::parseSha256(const QString& checksumFile)
{
    // sha256sum writes "<digest> *<name>" (the '*' marks binary mode), with a
    // leading '\' when the file name contains backslashes. Accept only that
    // exact shape: 64 hex digits followed by spaces/tabs and an optional '*'.
    // Allow a single leading UTF-8 BOM, which is otherwise not part of the
    // line.
    QString line = checksumFile;
    if (line.startsWith(QChar(0xFEFF))) {
        line.remove(0, 1);
    }
    static const QRegularExpression digest(
      QStringLiteral("^\\\\?([0-9a-fA-F]{64})[ \\t]+\\*?"));
    const QRegularExpressionMatch match = digest.match(line);
    return match.hasMatch() ? match.captured(1).toLower() : QString();
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
             "try {\n"
             "  Wait-Process -Id %1 -Timeout 120 -ErrorAction Stop\n"
             "} catch {\n"
             "  ('Timed out waiting for SlickShot to exit: ' + $_) | "
             "Add-Content -LiteralPath $log\n"
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

bool AppUpdate::startHidden(const QString& program, const QString& arguments)
{
#if defined(Q_OS_WIN)
    const std::wstring file = program.toStdWString();
    const std::wstring params = arguments.toStdWString();

    SHELLEXECUTEINFOW info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    info.lpFile = file.c_str();
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    return ShellExecuteExW(&info) != FALSE;
#else
    Q_UNUSED(program);
    Q_UNUSED(arguments);
    return false;
#endif
}
