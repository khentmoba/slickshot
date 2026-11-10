#include "appupdateinstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr int kProgressSteps = 1000;
}

void AppUpdateInstaller::openUpdate(const AppUpdateInfo& info)
{
    if (!AppUpdate::canInstallInPlace(info)) {
        if (!info.pageUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(info.pageUrl));
        }
        return;
    }
    auto* installer = new AppUpdateInstaller(info);
    installer->confirmAndDownload();
}

AppUpdateInstaller::AppUpdateInstaller(const AppUpdateInfo& info)
  : QObject(nullptr)
  , m_info(info)
  , m_network(new QNetworkAccessManager(this))
  , m_reply(nullptr)
  , m_progress(nullptr)
  , m_canceled(false)
{}

AppUpdateInstaller::~AppUpdateInstaller()
{
    delete m_progress;
}

void AppUpdateInstaller::confirmAndDownload()
{
    const auto answer = QMessageBox::question(
      nullptr,
      tr("Update SlickShot"),
      tr("Install SlickShot %1 now?\n\nSlickShot will close, install the "
         "update, and start again. Windows will ask for permission to install.")
        .arg(m_info.version));
    if (answer != QMessageBox::Yes) {
        deleteLater();
        return;
    }

    m_progress =
      new QProgressDialog(tr("Preparing update..."), tr("Cancel"), 0, 0);
    m_progress->setWindowTitle(tr("Update SlickShot"));
    m_progress->setWindowModality(Qt::ApplicationModal);
    m_progress->setMinimumDuration(0);
    m_progress->setAutoReset(false);
    m_progress->setAutoClose(false);
    connect(m_progress, &QProgressDialog::canceled, this, [this] {
        m_canceled = true;
        if (m_reply) {
            m_reply->abort();
        }
    });
    m_progress->show();
    downloadChecksum();
}

void AppUpdateInstaller::downloadChecksum()
{
    QNetworkReply* reply = get(QUrl(m_info.checksumUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_reply = nullptr;
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString content = QString::fromUtf8(reply->readAll());
        const QString error = reply->errorString();
        reply->deleteLater();

        if (!ok) {
            fail(
              tr("Could not download the update checksum.\n\n%1").arg(error));
            return;
        }
        m_expectedSha256 = AppUpdate::parseSha256(content);
        if (m_expectedSha256.isEmpty()) {
            fail(tr(
              "The update checksum is invalid. The update was not installed."));
            return;
        }
        downloadInstaller();
    });
}

void AppUpdateInstaller::downloadInstaller()
{
    const QString dirPath =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      QStringLiteral("/SlickShot-update");
    QDir().mkpath(dirPath);
    m_msiPath = QDir(dirPath).filePath(QStringLiteral("SlickShot-update.msi"));

    m_progress->setLabelText(
      tr("Downloading SlickShot %1...").arg(m_info.version));
    m_progress->setRange(0, 0);

    QNetworkReply* reply = get(QUrl(m_info.installerUrl));
    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            [this](qint64 received, qint64 total) {
                if (total > 0) {
                    m_progress->setRange(0, kProgressSteps);
                    m_progress->setValue(
                      static_cast<int>(received * kProgressSteps / total));
                }
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_reply = nullptr;
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        const QByteArray msi = ok ? reply->readAll() : QByteArray();
        reply->deleteLater();

        if (!ok) {
            fail(tr("Could not download the update.\n\n%1").arg(error));
            return;
        }
        const QString digest = QString::fromLatin1(
          QCryptographicHash::hash(msi, QCryptographicHash::Sha256).toHex());
        if (digest != m_expectedSha256) {
            fail(tr("The downloaded update failed its checksum. The update was "
                    "not installed."));
            return;
        }
        install(msi);
    });
}

void AppUpdateInstaller::install(const QByteArray& msi)
{
    QFile file(m_msiPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        file.write(msi) != msi.size()) {
        fail(tr("Could not save the update to %1.\n\n%2")
               .arg(QDir::toNativeSeparators(m_msiPath), file.errorString()));
        return;
    }
    file.close();

    const QString script = AppUpdate::installScript(
      QCoreApplication::applicationPid(),
      QDir::toNativeSeparators(m_msiPath),
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
      m_expectedSha256);
    const QString arguments =
      QStringLiteral("-NoProfile -NonInteractive -WindowStyle Hidden "
                     "-ExecutionPolicy Bypass -EncodedCommand ") +
      AppUpdate::encodeCommand(script);
    if (!AppUpdate::runElevated(QStringLiteral("powershell.exe"), arguments)) {
        fail(
          tr("SlickShot was not updated because the Windows permission prompt "
             "was declined or could not start."));
        return;
    }

    // The elevated script waits for this process to exit, runs msiexec, then
    // restarts SlickShot.
    m_progress->hide();
    QCoreApplication::quit();
}

void AppUpdateInstaller::fail(const QString& message)
{
    // hide(), not close(): closing a QProgressDialog emits canceled().
    if (m_progress) {
        m_progress->hide();
    }
    if (!m_canceled) {
        QMessageBox::warning(nullptr, tr("Update SlickShot"), message);
    }
    deleteLater();
}

QNetworkReply* AppUpdateInstaller::get(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->get(request);
    return m_reply;
}
