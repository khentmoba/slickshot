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
  , m_hash(QCryptographicHash::Sha256)
  , m_canceled(false)
  , m_writeError(false)
{}

AppUpdateInstaller::~AppUpdateInstaller()
{
    m_msiFile.reset();
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

    m_msiFile = std::make_unique<QFile>(m_msiPath);
    if (!m_msiFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = m_msiFile->errorString();
        m_msiFile.reset();
        fail(tr("Could not save the update to %1.\n\n%2")
               .arg(QDir::toNativeSeparators(m_msiPath), err));
        return;
    }
    m_hash.reset();
    m_writeError = false;

    m_progress->setLabelText(
      tr("Downloading SlickShot %1...").arg(m_info.version));
    m_progress->setRange(0, 0);

    QNetworkReply* reply = get(QUrl(m_info.installerUrl));
    const auto drainChunk = [this, reply]() {
        if (!m_msiFile || m_writeError) {
            return;
        }
        while (reply->bytesAvailable() > 0) {
            const QByteArray chunk = reply->read(65536);
            if (chunk.isEmpty()) {
                break;
            }
            m_hash.addData(chunk);
            if (m_msiFile->write(chunk) != chunk.size()) {
                m_writeError = true;
                reply->abort();
                break;
            }
        }
    };

    connect(reply, &QNetworkReply::readyRead, this, drainChunk);
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, drainChunk] {
        drainChunk();
        m_reply = nullptr;
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        reply->deleteLater();

        QString writeErrorString;
        if (m_msiFile) {
            if (m_writeError || !m_msiFile->flush()) {
                m_writeError = true;
                writeErrorString = m_msiFile->errorString();
            }
            m_msiFile->close();
            m_msiFile.reset();
        }

        if (m_writeError) {
            cleanupInstallerFile();
            fail(tr("Could not save the update to %1.\n\n%2")
                   .arg(QDir::toNativeSeparators(m_msiPath), writeErrorString));
            return;
        }
        if (!ok) {
            cleanupInstallerFile();
            fail(tr("Could not download the update.\n\n%1").arg(error));
            return;
        }
        const QString digest = QString::fromLatin1(m_hash.result().toHex());
        if (digest != m_expectedSha256) {
            cleanupInstallerFile();
            fail(tr("The downloaded update failed its checksum. The update was "
                    "not installed."));
            return;
        }
        install();
    });
}

void AppUpdateInstaller::install()
{
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
        cleanupInstallerFile();
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

void AppUpdateInstaller::cleanupInstallerFile()
{
    m_msiFile.reset();
    if (!m_msiPath.isEmpty()) {
        QFile::remove(m_msiPath);
    }
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
