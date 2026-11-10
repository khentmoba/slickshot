#include "appupdate.h"

#include <QCoreApplication>
#include <QDir>
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
      assets, QStringLiteral("^SlickShot-[0-9A-Za-z._+-]+-win64\\.msi$"));
    info.checksumUrl = assetUrl(
      assets,
      QStringLiteral("^SlickShot-[0-9A-Za-z._+-]+-win64\\.msi\\.sha256sum$"));
    return info;
}

bool AppUpdate::canInstallInPlace(const AppUpdateInfo& info)
{
#if defined(Q_OS_WIN)
    if (info.installerUrl.isEmpty() || info.checksumUrl.isEmpty()) {
        return false;
    }
    const QString appDir =
      QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) +
      QDir::separator();
    const QString programFiles =
      QDir::toNativeSeparators(qEnvironmentVariable("ProgramFiles")) +
      QDir::separator();
    return programFiles.length() > 1 &&
           appDir.startsWith(programFiles, Qt::CaseInsensitive);
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
                                 const QString& msiPath,
                                 const QString& appPath,
                                 const QString& expectedSha256)
{
    // Single-quoted PowerShell literals only need ' doubled.
    const auto literal = [](const QString& value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
        return QStringLiteral("'%1'").arg(escaped);
    };
    return QStringLiteral(
             "Wait-Process -Id %1 -ErrorAction SilentlyContinue\n"
             "$hash = Get-FileHash -LiteralPath %2 -Algorithm SHA256 "
             "-ErrorAction SilentlyContinue\n"
             "if (-not $hash -or $hash.Hash -ine %3) {\n"
             "  Start-Process -FilePath explorer.exe -ArgumentList %5\n"
             "  exit 1\n"
             "}\n"
             "Start-Process -FilePath msiexec.exe -ArgumentList @('/i', %4, "
             "'/passive', '/norestart') -Wait\n"
             "Start-Process -FilePath explorer.exe -ArgumentList %5\n")
      .arg(QString::number(pid),
           literal(msiPath),
           literal(expectedSha256),
           literal(QStringLiteral("\"%1\"").arg(msiPath)),
           literal(QStringLiteral("\"%1\"").arg(appPath)));
}

QString AppUpdate::encodeCommand(const QString& script)
{
    const QByteArray utf16(reinterpret_cast<const char*>(script.utf16()),
                           script.size() * 2);
    return QString::fromLatin1(utf16.toBase64());
}

bool AppUpdate::runElevated(const QString& program, const QString& arguments)
{
#if defined(Q_OS_WIN)
    const std::wstring file = program.toStdWString();
    const std::wstring params = arguments.toStdWString();

    SHELLEXECUTEINFOW info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    info.lpVerb = L"runas";
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
