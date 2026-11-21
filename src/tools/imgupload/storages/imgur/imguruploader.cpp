// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "imguruploader.h"
#include "utils/abstractlogger.h"
#include "utils/confighandler.h"
#include "utils/filenamehandler.h"
#include "utils/history.h"
#include "widgets/loadspinner.h"
#include "widgets/notificationwidget.h"

#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QShortcut>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace {

// A hidden sidecar next to the history thumbnails maps a random, opaque id to
// the Imgur deletehash. Keeping the token out of the (shareable) history
// filename prevents leaking the deletion capability when a thumbnail is shared.
constexpr char kDeleteTokenFile[] = ".imgur_tokens";

QString deleteTokenFilePath()
{
    return QDir(History().path()).filePath(QLatin1String(kDeleteTokenFile));
}

QString readDeleteToken(const QString& id)
{
    if (id.isEmpty()) {
        return {};
    }
    QSettings store(deleteTokenFilePath(), QSettings::IniFormat);
    return store.value(QStringLiteral("tokens/") + id).toString();
}

void writeDeleteToken(const QString& id, const QString& token)
{
    if (id.isEmpty() || token.isEmpty()) {
        return;
    }
    QSettings store(deleteTokenFilePath(), QSettings::IniFormat);
    store.setValue(QStringLiteral("tokens/") + id, token);
    store.sync();
}

// The Imgur "link" comes from the network and is untrusted. Only keep a plain
// basename, rejecting traversal, separators and control characters.
QString safeImageName(const QString& path)
{
    const QString base = QFileInfo(path).fileName();
    if (base.isEmpty() || base == QLatin1String(".") ||
        base == QLatin1String("..") || base.contains(QLatin1String("..")) ||
        base.contains(QLatin1Char('-'))) {
        return {};
    }
    for (QChar c : base) {
        const ushort code = c.unicode();
        if (code < 0x20 || (code >= 0x7f && code < 0xa0) ||
            c == QLatin1Char('/') || c == QLatin1Char('\\')) {
            return {};
        }
    }
    return base;
}

} // namespace

ImgurUploader::ImgurUploader(const QPixmap& capture, QWidget* parent)
  : ImgUploaderBase(capture, parent)
{
    m_NetworkAM = new QNetworkAccessManager(this);
    connect(m_NetworkAM,
            &QNetworkAccessManager::finished,
            this,
            &ImgurUploader::handleReply);
}

void ImgurUploader::handleReply(QNetworkReply* reply)
{
    reply->deleteLater();
    spinner()->deleteLater();
    m_currentImageName.clear();
    constexpr qint64 kMaxImgurReplyBytes = 1024 * 1024;
    const QByteArray payload = reply->read(kMaxImgurReplyBytes + 1);
    if (payload.size() > kMaxImgurReplyBytes) {
        setInfoLabelText(tr("Upload response exceeded size limit."));
        new QShortcut(Qt::Key_Escape, this, SLOT(close()));
        return;
    }
    QJsonDocument response = QJsonDocument::fromJson(payload);
    QJsonObject json = response.object();
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject data = json[QStringLiteral("data")].toObject();
        const QUrl imageUrl(data[QStringLiteral("link")].toString());
        if (!imageUrl.isValid() ||
            (imageUrl.scheme().compare(QLatin1String("https"),
                                       Qt::CaseInsensitive) != 0 &&
             imageUrl.scheme().compare(QLatin1String("http"),
                                       Qt::CaseInsensitive) != 0)) {
            setInfoLabelText(tr("Invalid upload URL returned by server."));
            new QShortcut(Qt::Key_Escape, this, SLOT(close()));
            return;
        }
        setImageURL(imageUrl);

        const QString deleteToken =
          data[QStringLiteral("deletehash")].toString();

        // Only use a safe basename from the untrusted link, and carry an opaque
        // id in the filename instead of the deletehash itself.
        const QString opaqueId = QUuid::createUuid().toString(QUuid::Id128);
        QString imageName = safeImageName(imageURL().path());
        if (imageName.isEmpty()) {
            imageName = opaqueId + QStringLiteral(".png");
        }

        // Keep the deletion token in a sidecar, never in the shared filename.
        writeDeleteToken(opaqueId, deleteToken);

        // save image to history
        History history;
        m_currentImageName = history.packFileName("imgur", opaqueId, imageName);
        history.save(pixmap(), m_currentImageName);

        emit uploadOk(imageURL());
    } else {
        QString status;
        if (json.contains(QStringLiteral("errors")) &&
            json.value(QStringLiteral("errors")).isArray()) {
            QJsonArray errorsArray =
              json.value(QStringLiteral("errors")).toArray();
            if (!errorsArray.isEmpty() && errorsArray.at(0).isObject()) {
                QJsonObject errorObj = errorsArray.at(0).toObject();
                status = errorObj.value(QStringLiteral("code")).toString() +
                         " - " +
                         errorObj.value(QStringLiteral("status")).toString();
            }
        }

        setInfoLabelText(reply->errorString() + "\n" + status);
    }
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void ImgurUploader::upload()
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    pixmap().save(&buffer, "PNG");

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("title"), QStringLiteral(""));
    QString description = FileNameHandler().parsedPattern();
    urlQuery.addQueryItem(QStringLiteral("description"), description);

    QUrl url(QStringLiteral("https://api.imgur.com/3/image"));
    url.setQuery(urlQuery);
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");
    request.setRawHeader("Authorization",
                         QStringLiteral("Client-ID %1")
                           .arg(ConfigHandler().uploadClientSecret())
                           .toUtf8());

    m_NetworkAM->post(request, byteArray);
}

void ImgurUploader::deleteImage(const QString& fileName,
                                const QString& deleteToken)
{
    Q_UNUSED(fileName)
    // New entries carry an opaque id in the filename; look the real token up
    // in the sidecar. Older entries embedded the token directly, so fall back
    // to the passed value when there is no sidecar entry for the id.
    QString token = deleteToken;
    const QString stored = readDeleteToken(deleteToken);
    if (!stored.isEmpty()) {
        token = stored;
    }

    // The token is untrusted input from a filename/JSON, so percent-encode it
    // before placing it in the URL path.
    const QByteArray encoded = QUrl::toPercentEncoding(token);
    const bool successful = QDesktopServices::openUrl(
      QUrl(QStringLiteral("https://imgur.com/delete/%1")
             .arg(QString::fromLatin1(encoded))));
    if (!successful) {
        if (notification() != nullptr) {
            notification()->showMessage(tr("Unable to open the URL."));
        } else {
            AbstractLogger::error() << tr("Unable to open the URL.");
        }
    }

    emit deleteOk();
}
