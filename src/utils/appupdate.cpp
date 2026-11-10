#include "appupdate.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

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

AppUpdateInfo AppUpdate::releaseInfo(const QJsonObject& release)
{
    AppUpdateInfo info;
    info.version = release["tag_name"].toString();
    if (info.version.startsWith(QLatin1Char('v'))) {
        info.version.remove(0, 1);
    }
    info.pageUrl = release["html_url"].toString();

    const QJsonArray assets = release["assets"].toArray();
    info.installerUrl = assetUrl(
      assets, QStringLiteral("^SlickShot-[0-9A-Za-z._+-]+-win64-setup\\.exe$"));
    info.checksumUrl =
      assetUrl(assets,
               QStringLiteral(
                 "^SlickShot-[0-9A-Za-z._+-]+-win64-setup\\.exe\\.sha256sum$"));
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
    // sha256sum writes "<digest> *<name>", with a leading "\" when the name
    // contains backslashes.
    static const QRegularExpression digest(
      QStringLiteral("^\\\\?([0-9a-fA-F]{64})\\s"));
    const QRegularExpressionMatch match = digest.match(checksumFile);
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
             "Wait-Process -Id %1 -ErrorAction SilentlyContinue\n"
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
