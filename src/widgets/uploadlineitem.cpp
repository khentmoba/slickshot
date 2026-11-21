#include "uploadlineitem.h"
#include "./ui_uploadlineitem.h"
#include "core/flameshotdaemon.h"
#include "tools/imgupload/imguploadermanager.h"
#include "utils/confighandler.h"
#include "utils/history.h"
#include "widgets/notificationwidget.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <QWidget>

void removeCacheFile(QString const& fullFileName)
{
    QFile file(fullFileName);
    if (file.exists()) {
        file.remove();
    }
}

UploadLineItem::UploadLineItem(QWidget* parent,
                               QPixmap const& preview,
                               QString const& timestamp,
                               QString const& url,
                               QString const& fullFileName,
                               HistoryFileName const& unpackFileName)
  : QWidget(parent)
  , ui(new Ui::UploadLineItem)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->imagePreview->setPixmap(preview);
    ui->uploadTimestamp->setText(timestamp);

    connect(ui->copyUrl, &QPushButton::clicked, this, [=, this]() {
        FlameshotDaemon::copyToClipboard(url);
    });

    connect(ui->openBrowser, &QPushButton::clicked, this, [=, this]() {
        const QUrl parsed(url);
        if (parsed.isValid() &&
            (parsed.scheme().compare(QLatin1String("https"),
                                     Qt::CaseInsensitive) == 0 ||
             parsed.scheme().compare(QLatin1String("http"),
                                     Qt::CaseInsensitive) == 0)) {
            QDesktopServices::openUrl(parsed);
        }
    });

    connect(ui->deleteImage, &QPushButton::clicked, this, [=, this]() {
        if (ConfigHandler().historyConfirmationToDelete() &&
            QMessageBox::No ==
              QMessageBox::question(
                this,
                tr("Confirm to delete"),
                tr("Are you sure you want to delete a screenshot from the "
                   "latest uploads and server?"),
                QMessageBox::Yes | QMessageBox::No)) {
            return;
        }

        ImgUploaderBase* imgUploaderBase =
          ImgUploaderManager(this).uploader(unpackFileName.type);
        if (imgUploaderBase != nullptr) {
            imgUploaderBase->deleteImage(unpackFileName.file,
                                         unpackFileName.token);
            imgUploaderBase->deleteLater();
        }

        removeCacheFile(fullFileName);
        emit requestedDeletion();
    });
}

UploadLineItem::~UploadLineItem()
{
    delete ui;
}
