#pragma once

#include "utils/appupdate.h"

#include <QCryptographicHash>
#include <QFile>
#include <QObject>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class QUrl;

// Downloads, verifies and installs a release over the running MSI install.
// Owns itself: it deletes itself once the update is abandoned or handed to
// msiexec.
class AppUpdateInstaller : public QObject
{
    Q_OBJECT
public:
    // Installs in place when possible, otherwise opens the release page.
    static void openUpdate(const AppUpdateInfo& info);

    ~AppUpdateInstaller() override;

private:
    explicit AppUpdateInstaller(const AppUpdateInfo& info);

    void confirmAndDownload();
    void downloadChecksum();
    void downloadInstaller();
    void install();
    void cleanupInstallerFile();
    void fail(const QString& message);
    QNetworkReply* get(const QUrl& url);

    AppUpdateInfo m_info;
    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply;
    QProgressDialog* m_progress;
    std::unique_ptr<QFile> m_msiFile;
    QCryptographicHash m_hash;
    QString m_expectedSha256;
    QString m_msiPath;
    bool m_canceled;
    bool m_writeError;
};
