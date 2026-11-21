// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QUrl>
#include <QWidget>

class QNetworkReply;
class QNetworkAccessManager;
class QHBoxLayout;
class QVBoxLayout;
class QLabel;
class LoadSpinner;
class QPushButton;
class QUrl;
class NotificationWidget;

class ImgUploaderBase : public QWidget
{
    Q_OBJECT
public:
    explicit ImgUploaderBase(const QPixmap& capture, QWidget* parent = nullptr);

    LoadSpinner* spinner();

    const QUrl& imageURL();
    void setImageURL(const QUrl&);
    const QPixmap& pixmap();
    void setPixmap(const QPixmap&);
    void setInfoLabelText(const QString&);

    NotificationWidget* notification();
    virtual void deleteImage(const QString& fileName,
                             const QString& deleteToken) = 0;
    virtual void upload() = 0;

signals:
    void uploadOk(const QUrl& url);
    void deleteOk();

public slots:
    void showPostUploadDialog();

private slots:
    void startDrag();
    void openURL();
    void copyURL();
    void copyImage();
    void deleteCurrentImage();
    void saveScreenshotToFilesystem();

private:
    QPixmap m_pixmap;

    QVBoxLayout* m_vLayout{ nullptr };
    QHBoxLayout* m_hLayout{ nullptr };
    // loading
    QLabel* m_infoLabel{ nullptr };
    LoadSpinner* m_spinner{ nullptr };
    // uploaded
    QPushButton* m_openUrlButton{ nullptr };
    QPushButton* m_openDeleteUrlButton{ nullptr };
    QPushButton* m_copyUrlButton{ nullptr };
    QPushButton* m_toClipboardButton{ nullptr };
    QPushButton* m_saveToFilesystemButton{ nullptr };
    QUrl m_imageURL;
    NotificationWidget* m_notification{ nullptr };

public:
    QString m_currentImageName;
};
