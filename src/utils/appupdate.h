#pragma once

#include <QString>

class QJsonObject;

// The parts of the latest GitHub release that the update flow needs.
struct AppUpdateInfo
{
    QString version; // "15.0.1", without the leading "v"
    QString
      pageUrl; // release page, opened when in-place install is not possible
    QString installerUrl; // SlickShot-<version>-win64.msi, empty if the release
                          // lacks it
    QString checksumUrl;  // SlickShot-<version>-win64.msi.sha256sum
};

namespace AppUpdate {

AppUpdateInfo releaseInfo(const QJsonObject& release);

// True only for a non-portable copy installed under Program Files or
// %LocalAppData%\Programs on Windows, which the MSI can replace.
bool canInstallInPlace(const AppUpdateInfo& info);

// Testable path predicate underlying canInstallInPlace().
bool canInstallInPlaceForPaths(const AppUpdateInfo& info,
                               const QString& appDir,
                               const QString& programFiles,
                               const QString& localPrograms,
                               bool isPortable = false);

// Returns the first 64-hex-digit digest of a sha256sum file, lowercase, or
// empty.
QString parseSha256(const QString& checksumFile);

// PowerShell that waits for pid to exit, verifies msiPath's SHA-256, installs
// silently, and restarts appPath.
QString installScript(qint64 pid,
                      const QString& msiPath,
                      const QString& appPath,
                      const QString& expectedSha256);

// Base64 of UTF-16LE text, the form powershell -EncodedCommand expects.
QString encodeCommand(const QString& script);

// Starts a program elevated (UAC). Returns false if it did not start, including
// when the user declines.
bool runElevated(const QString& program, const QString& arguments);

} // namespace AppUpdate
