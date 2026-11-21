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
constexpr int kTransferTimeoutMs = 30000;
constexpr int kMaxRedirects = 5;
constexpr qint64 kMaxChecksumBytes = 64 * 1024;
constexpr qint64 kMaxInstallerBytes = 512LL * 1024 * 1024;
}

QPointer<AppUpdateInstaller> AppUpdateInstaller::s_activeInstaller;

void AppUpdateInstaller::openUpdate(const AppUpdateInfo& info)
{
    if (s_activeInstaller) {
        if (s_activeInstaller->m_progress) {
            s_activeInstaller->m_progress->raise();
            s_activeInstaller->m_progress->activateWindow();
        }
        return;
    }

    // Even if releaseInfo() already filtered these, re-check at the download
    // boundary so a caller can never make us fetch untrusted URLs.
    if (!AppUpdate::isTrustedAssetUrl(info.installerUrl) ||
        !AppUpdate::isTrustedAssetUrl(info.checksumUrl) ||
        !AppUpdate::canInstallInPlace(info)) {
        if (AppUpdate::isTrustedReleasePageUrl(info.pageUrl)) {
            QDesktopServices::openUrl(QUrl(info.pageUrl));
        }
        return;
    }
    auto* installer = new AppUpdateInstaller(info);
    s_activeInstaller = installer;
    installer->start();
}

AppUpdateInstaller::AppUpdateInstaller(const AppUpdateInfo& info)
  : QObject(nullptr)
  , m_info(info)
  , m_network(new QNetworkAccessManager(this))
  , m_reply(nullptr)
  , m_progress(nullptr)
  , m_hash(QCryptographicHash::Sha256)
  , m_downloadedBytes(0)
  , m_canceled(false)
  , m_writeError(false)
  , m_redirectBlocked(false)
  , m_sizeExceeded(false)
{}

AppUpdateInstaller::~AppUpdateInstaller()
{
    if (s_activeInstaller == this) {
        s_activeInstaller = nullptr;
    }
    m_installerFile.reset();
    delete m_progress;
}

void AppUpdateInstaller::start()
{
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
    if (!AppUpdate::isTrustedAssetUrl(m_info.checksumUrl)) {
        fail(tr("The update checksum URL is not trusted. The update was not "
                "installed."));
        return;
    }
    m_redirectBlocked = false;
    m_sizeExceeded = false;
    QNetworkReply* reply = get(QUrl(m_info.checksumUrl));
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (reply->bytesAvailable() > kMaxChecksumBytes) {
            m_sizeExceeded = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_reply = nullptr;
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QByteArray raw = reply->read(kMaxChecksumBytes + 1);
        if (raw.size() > kMaxChecksumBytes) {
            m_sizeExceeded = true;
        }
        const QString content = QString::fromUtf8(raw);
        const QString error = reply->errorString();
        reply->deleteLater();

        if (m_redirectBlocked) {
            fail(tr("The update was redirected to an untrusted server. The "
                    "update was not installed."));
            return;
        }
        if (m_sizeExceeded) {
            fail(tr("The update checksum exceeded the maximum allowed size. "
                    "The update was not installed."));
            return;
        }
        if (!ok) {
            fail(
              tr("Could not download the update checksum.\n\n%1").arg(error));
            return;
        }
        // The checksum must describe the exact installer asset we are about to
        // download, not some other file served alongside it.
        const QString installerName = QUrl(m_info.installerUrl).fileName();
        m_expectedSha256 = AppUpdate::parseSha256(content, installerName);
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
    if (!AppUpdate::isTrustedAssetUrl(m_info.installerUrl)) {
        fail(tr("The update installer URL is not trusted. The update was not "
                "installed."));
        return;
    }
    const QString tempTemplate =
      QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("SlickShot-update-XXXXXX"));
    m_tempDir = std::make_unique<QTemporaryDir>(tempTemplate);
    if (!m_tempDir->isValid()) {
        const QString err = m_tempDir->errorString();
        m_tempDir.reset();
        fail(tr("Could not create a temporary folder for the update.\n\n%1")
               .arg(err));
        return;
    }
    m_installerPath =
      m_tempDir->filePath(QStringLiteral("SlickShot-update-setup.exe"));

    m_installerFile = std::make_unique<QFile>(m_installerPath);
    if (!m_installerFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = m_installerFile->errorString();
        m_installerFile.reset();
        cleanupInstallerFile();
        fail(tr("Could not save the update to %1.\n\n%2")
               .arg(QDir::toNativeSeparators(m_installerPath), err));
        return;
    }
    m_hash.reset();
    m_downloadedBytes = 0;
    m_writeError = false;
    m_redirectBlocked = false;
    m_sizeExceeded = false;

    m_progress->setLabelText(
      tr("Downloading SlickShot %1...").arg(m_info.version));
    m_progress->setRange(0, 0);

    QNetworkReply* reply = get(QUrl(m_info.installerUrl));
    const auto drainChunk = [this, reply]() {
        if (!m_installerFile || m_writeError || m_sizeExceeded) {
            return;
        }
        while (reply->bytesAvailable() > 0) {
            const QByteArray chunk = reply->read(65536);
            if (chunk.isEmpty()) {
                break;
            }
            m_downloadedBytes += chunk.size();
            if (m_downloadedBytes > kMaxInstallerBytes) {
                m_sizeExceeded = true;
                reply->abort();
                break;
            }
            m_hash.addData(chunk);
            if (m_installerFile->write(chunk) != chunk.size()) {
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
            [this, reply](qint64 received, qint64 total) {
                if (total > kMaxInstallerBytes ||
                    received > kMaxInstallerBytes) {
                    m_sizeExceeded = true;
                    reply->abort();
                    return;
                }
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
        if (m_installerFile) {
            if (m_writeError || !m_installerFile->flush()) {
                m_writeError = true;
                writeErrorString = m_installerFile->errorString();
            }
            m_installerFile->close();
            m_installerFile.reset();
        }

        if (m_redirectBlocked) {
            cleanupInstallerFile();
            fail(tr("The update was redirected to an untrusted server. The "
                    "update was not installed."));
            return;
        }
        if (m_sizeExceeded) {
            cleanupInstallerFile();
            fail(tr("The update download exceeded the maximum allowed size. "
                    "The update was not installed."));
            return;
        }
        if (m_writeError) {
            cleanupInstallerFile();
            fail(tr("Could not save the update to %1.\n\n%2")
                   .arg(QDir::toNativeSeparators(m_installerPath),
                        writeErrorString));
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
    const QString powershell = AppUpdate::powershellExecutablePath();
    if (powershell.isEmpty()) {
        cleanupInstallerFile();
        fail(tr("SlickShot was not updated because Windows PowerShell could "
                "not be located."));
        return;
    }

    const QString script = AppUpdate::installScript(
      QCoreApplication::applicationPid(),
      QDir::toNativeSeparators(m_installerPath),
      m_expectedSha256,
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    const QString arguments =
      QStringLiteral("-NoProfile -NonInteractive -WindowStyle Hidden "
                     "-ExecutionPolicy Bypass -EncodedCommand ") +
      AppUpdate::encodeCommand(script);
    if (!AppUpdate::startHidden(powershell, arguments)) {
        cleanupInstallerFile();
        fail(tr("SlickShot was not updated because the update script could "
                "not start."));
        return;
    }

    // Hand ownership of the temporary directory to the updater script so our
    // destructor does not delete the installer before PowerShell runs it.
    if (m_tempDir) {
        m_tempDir->setAutoRemove(false);
    }

    // The script waits for this process to exit, runs the installer silently,
    // and starts SlickShot again.
    m_progress->hide();
    QCoreApplication::quit();
}

void AppUpdateInstaller::cleanupInstallerFile()
{
    m_installerFile.reset();
    if (!m_installerPath.isEmpty()) {
        QFile::remove(m_installerPath);
    }
    if (m_tempDir) {
        m_tempDir->remove();
        m_tempDir.reset();
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
                         QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setMaximumRedirectsAllowed(kMaxRedirects);
    request.setTransferTimeout(kTransferTimeoutMs);
    QNetworkReply* reply = m_network->get(request);
    // Approve every redirect explicitly before Qt sends a request to the
    // target host so an untrusted server is never contacted.
    connect(reply,
            &QNetworkReply::redirected,
            this,
            [this, reply](const QUrl& redirectUrl) {
                if (AppUpdate::isTrustedAssetUrl(redirectUrl.toString())) {
                    emit reply->redirectAllowed();
                } else {
                    m_redirectBlocked = true;
                    reply->abort();
                }
            });
    m_reply = reply;
    return reply;
}
