#include "uploadhistory.h"
#include "./ui_uploadhistory.h"
#include "tools/imgupload/imguploadermanager.h"
#include "utils/history.h"
#include "widgets/uploadlineitem.h"

#include <QDateTime>
#include <QFileInfo>
#include <QPixmap>
#include <QPushButton>

void scaleThumbnail(QPixmap& pixmap)
{
    if (pixmap.height() / HISTORYPIXMAP_MAX_PREVIEW_HEIGHT >=
        pixmap.width() / HISTORYPIXMAP_MAX_PREVIEW_WIDTH) {
        pixmap = pixmap.scaledToHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT,
                                       Qt::SmoothTransformation);
    } else {
        pixmap = pixmap.scaledToWidth(HISTORYPIXMAP_MAX_PREVIEW_WIDTH,
                                      Qt::SmoothTransformation);
    }
}

void clearHistoryLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
}

UploadHistory::UploadHistory(QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::UploadHistory)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void UploadHistory::loadHistory()
{
    clearHistoryLayout(ui->historyContainer);

    History history = History();
    QList<QString> historyFiles = history.history();

    if (historyFiles.isEmpty()) {
        setEmptyMessage();
    } else {
        for (const auto& fileName : historyFiles) {
            addLine(history.path(), fileName);
        }
    }
}

void UploadHistory::setEmptyMessage()
{
    auto* buttonEmpty = new QPushButton;
    buttonEmpty->setText(tr("Screenshots history is empty"));
    buttonEmpty->setMinimumSize(1, HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    connect(
      buttonEmpty, &QPushButton::clicked, this, [=, this]() { this->close(); });
    ui->historyContainer->addWidget(buttonEmpty);
}

void UploadHistory::addLine(const QString& path, const QString& fileName)
{
    QString fullFileName = path + fileName;

    History history;
    HistoryFileName unpackFileName = history.unpackFileName(fileName);

    QString url = ImgUploaderManager(this).url() + unpackFileName.file;

    // load pixmap
    QPixmap pixmap;
    pixmap.load(fullFileName, "png");
    scaleThumbnail(pixmap);

    // get file info
    auto fileInfo = QFileInfo(fullFileName);
    QString lastModified =
      fileInfo.lastModified().toString("yyyy-MM-dd\nhh:mm:ss");

    auto* line = new UploadLineItem(
      this, pixmap, lastModified, url, fullFileName, unpackFileName);

    connect(line, &UploadLineItem::requestedDeletion, this, [=, this]() {
        ui->historyContainer->removeWidget(line);
        line->hide();
        line->deleteLater();
        if (ui->historyContainer->count() == 0) {
            setEmptyMessage();
        }
    });

    ui->historyContainer->addWidget(line);
}

UploadHistory::~UploadHistory()
{
    delete ui;
}
