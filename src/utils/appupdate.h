#pragma once

#include <QString>

class QJsonObject;

// The parts of the latest GitHub release that the update flow needs.
struct AppUpdateInfo
{
    QString version; // "15.0.1", without the leading "v"
    QString
      pageUrl; // release page, opened when in-place install is not possible
    QString installerUrl; // SlickShot-<version>-win64-setup.exe, empty if the
                          // release lacks it
    QString checksumUrl;  // SlickShot-<version>-win64-setup.exe.sha256sum
};

namespace AppUpdate {

AppUpdateInfo releaseInfo(const QJsonObject& release);

// True only for an HTTPS URL hosted on the hosts GitHub uses for release
// assets. Used both when parsing release metadata and before downloading.
bool isTrustedAssetUrl(const QString& url);

// True only for an HTTPS release page URL belonging to SlickShot on GitHub.
bool isTrustedReleasePageUrl(const QString& url);

// True when appPath is inside installRoot and the release has both the
// installer and its checksum.
bool canInstallInPlaceForPaths(const AppUpdateInfo& info,
                               const QString& appPath,
                               const QString& installRoot);

// True on Windows for a non-portable copy installed per user in
// %LOCALAPPDATA%\Programs\SlickShot, the only location the installer updates.
bool canInstallInPlace(const AppUpdateInfo& info);

// Returns the first 64-hex-digit digest of a sha256sum file, lowercase, or
// empty. When expectedFileName is not empty, the file name recorded in the
// checksum line must match it (ignoring any directory prefix and case) or an
// empty string is returned. This stops a checksum for one asset from being
// used to validate a different download.
QString parseSha256(const QString& checksumFile,
                    const QString& expectedFileName = QString());

// PowerShell that waits for pid to exit, checks installerPath against
// expectedSha256 while holding the file open so it cannot change, runs the
// installer silently, and starts appPath again.
QString installScript(qint64 pid,
                      const QString& installerPath,
                      const QString& expectedSha256,
                      const QString& appPath);

// Base64 of UTF-16LE text, the form powershell -EncodedCommand expects.
QString encodeCommand(const QString& script);

// Full path to Windows PowerShell inside System32, or empty if unavailable.
QString powershellExecutablePath();

// Starts a program without showing a window. Returns false if it did not start.
bool startHidden(const QString& program, const QString& arguments);

} // namespace AppUpdate
