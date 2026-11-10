// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QTest>
#include <QVersionNumber>
#include <QWidget>

#include "config/shortcutswidget.h"
#include "tools/capturetool.h"
#include "utils/appupdate.h"
#include "utils/screengrabber.h"
#include "utils/snippingtoolmanager.h"
#include "widgets/capture/buttonhandler.h"
#include "widgets/capture/capturetoolbutton.h"
#include "widgets/capture/magnifierwidget.h"
#if !defined(DISABLE_UPDATE_CHECKER)
#include "widgets/updatenotificationwidget.h"
#endif

class SlickShotTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAppUpdateReleaseInfo();
    void testAppUpdateSha256();
    void testAppUpdateInstallScriptAndEncode();
    void testAppUpdateCanInstallInPlaceForPaths();
    void testSemanticVersionCompare();
    void testSnippingToolCommandMatch();
    void testMultiMonitorPhysicalRects();
    void testCaptureToolButtonCheckableState();
    void testButtonHandlerMultiRowHitTesting();
    void testMagnifierWidgetHiDpiAndRender();
    void testShortcutsWidgetHiddenOnConstruction();
#if !defined(DISABLE_UPDATE_CHECKER)
    void testUpdateNotificationWidget();
#endif
};

void SlickShotTests::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void SlickShotTests::testAppUpdateReleaseInfo()
{
    QJsonArray assets;
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64.zip" },
      { "browser_download_url",
        "https://example.com/SlickShot-0.2.0-win64.zip" },
    });
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64.msi" },
      { "browser_download_url",
        "https://example.com/SlickShot-0.2.0-win64.msi" },
    });
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64.msi.sha256sum" },
      { "browser_download_url",
        "https://example.com/SlickShot-0.2.0-win64.msi.sha256sum" },
    });

    const QJsonObject release{
        { "tag_name", "v0.2.0" },
        { "html_url", "https://example.com/releases/v0.2.0" },
        { "assets", assets },
    };

    const AppUpdateInfo info = AppUpdate::releaseInfo(release);
    QCOMPARE(info.version, QStringLiteral("0.2.0"));
    QCOMPARE(info.pageUrl,
             QStringLiteral("https://example.com/releases/v0.2.0"));
    QCOMPARE(info.installerUrl,
             QStringLiteral("https://example.com/SlickShot-0.2.0-win64.msi"));
    QCOMPARE(info.checksumUrl,
             QStringLiteral(
               "https://example.com/SlickShot-0.2.0-win64.msi.sha256sum"));
}

void SlickShotTests::testAppUpdateSha256()
{
    const QString valid = QString(64, QLatin1Char('A'));
    QCOMPARE(AppUpdate::parseSha256(valid + "  SlickShot-0.2.0-win64.msi\n"),
             valid.toLower());
    QCOMPARE(
      AppUpdate::parseSha256("\\" + valid + " *dir\\SlickShot-0.2.0-win64.msi"),
      valid.toLower());
    QCOMPARE(AppUpdate::parseSha256(QStringLiteral("not-a-sha256  file")),
             QString());
    QCOMPARE(AppUpdate::parseSha256(QString(63, QLatin1Char('a')) + "  file"),
             QString());
}

void SlickShotTests::testAppUpdateInstallScriptAndEncode()
{
    const QString script = AppUpdate::installScript(
      1234,
      QStringLiteral("C:\\Temp\\O'Connor\\SlickShot-update.msi"),
      QStringLiteral("C:\\Program Files\\SlickShot\\flameshot.exe"),
      QString(64, QLatin1Char('b')));

    QVERIFY(script.contains(QStringLiteral("Wait-Process -Id 1234")));
    QVERIFY(script.contains(QStringLiteral("O''Connor")));
    QVERIFY(script.contains(QStringLiteral("Get-FileHash")));
    QVERIFY(script.contains(QStringLiteral("msiexec.exe")));
    QVERIFY(script.contains(QStringLiteral("Remove-Item -LiteralPath")));
    QVERIFY(script.contains(QStringLiteral("explorer.exe")));

    const QString encoded = AppUpdate::encodeCommand(QStringLiteral("abc"));
    const QByteArray raw = QByteArray::fromBase64(encoded.toLatin1());
    QCOMPARE(raw.size(), 6);
    QCOMPARE(
      QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()),
                         raw.size() / 2),
      QStringLiteral("abc"));
}

void SlickShotTests::testAppUpdateCanInstallInPlaceForPaths()
{
    AppUpdateInfo validInfo{
        QStringLiteral("0.2.0"),
        QStringLiteral("https://example.com/release"),
        QStringLiteral("https://example.com/app.msi"),
        QStringLiteral("https://example.com/app.msi.sha256sum"),
    };

    const QString programFiles = QStringLiteral("C:\\Program Files");
    const QString localPrograms =
      QStringLiteral("C:\\Users\\alice\\AppData\\Local\\Programs");

    QVERIFY(AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral("C:\\Program Files\\SlickShot"),
      programFiles,
      localPrograms,
      false));
    QVERIFY(AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral("C:/Users/alice/AppData/Local/Programs/SlickShot"),
      programFiles,
      localPrograms,
      false));
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral("C:\\Program Files (x86)\\SlickShot"),
      programFiles,
      localPrograms,
      false));
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral("C:\\Users\\alice\\Desktop\\SlickShot"),
      programFiles,
      localPrograms,
      false));
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral("C:\\Program Files\\SlickShot"),
      programFiles,
      localPrograms,
      true));

    AppUpdateInfo missingMsi = validInfo;
    missingMsi.installerUrl.clear();
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      missingMsi,
      QStringLiteral("C:\\Program Files\\SlickShot"),
      programFiles,
      localPrograms,
      false));
}

void SlickShotTests::testSemanticVersionCompare()
{
    QVERIFY(QVersionNumber::fromString(QStringLiteral("0.9.0")) <
            QVersionNumber::fromString(QStringLiteral("0.10.0")));
    QVERIFY(QVersionNumber::fromString(QStringLiteral("0.10.0")) <
            QVersionNumber::fromString(QStringLiteral("0.10.1")));
    QVERIFY(!(QVersionNumber::fromString(QStringLiteral("1.0.0")) <
              QVersionNumber::fromString(QStringLiteral("0.99.0"))));
}

void SlickShotTests::testSnippingToolCommandMatch()
{
    const QString expected =
      QStringLiteral("C:\\Program Files\\SlickShot\\flameshot.exe");

    QVERIFY(SnippingToolManager::commandMatchesAppPath(
      QStringLiteral(
        "\"C:\\Program Files\\SlickShot\\flameshot.exe\" gui \"%1\""),
      expected));
    QVERIFY(SnippingToolManager::commandMatchesAppPath(
      QStringLiteral("\"c:/program files/slickshot/flameshot.exe\""),
      expected));
    QVERIFY(SnippingToolManager::commandMatchesAppPath(expected, expected));

    // Must not match a stale install in a different folder just because it
    // contains "flameshot" or "slickshot".
    QVERIFY(!SnippingToolManager::commandMatchesAppPath(
      QStringLiteral("\"C:\\Old\\SlickShot\\flameshot.exe\" gui \"%1\""),
      expected));
    QVERIFY(!SnippingToolManager::commandMatchesAppPath(
      QStringLiteral("\"C:\\Program Files\\SlickShot2\\flameshot.exe\""),
      expected));
}

void SlickShotTests::testMultiMonitorPhysicalRects()
{
    // Case 1: Two side-by-side monitors with mixed DPI (1.0 + 1.5)
    {
        const QList<QRect> geoms = {
            QRect(0, 0, 1920, 1080),
            QRect(1920, 0, 2560, 1440),
        };
        const QList<qreal> dprs = { 1.0, 1.5 };
        const QList<QRect> phys =
          ScreenGrabber::computePhysicalScreenRects(geoms, dprs);
        QCOMPARE(phys.size(), 2);
        QCOMPARE(phys[0], QRect(0, 0, 1920, 1080));
        QCOMPARE(phys[1], QRect(1920, 0, 3840, 2160));
    }

    // Case 2: Two stacked monitors on the left + one monitor on the right.
    // Right monitor must start at x = 1920 (NOT double-counted at x = 3840).
    {
        const QList<QRect> geoms = {
            QRect(0, 0, 1920, 1080),
            QRect(0, 1080, 1920, 1080),
            QRect(1920, 0, 2560, 1440),
        };
        const QList<qreal> dprs = { 1.0, 1.0, 1.5 };
        const QList<QRect> phys =
          ScreenGrabber::computePhysicalScreenRects(geoms, dprs);
        QCOMPARE(phys.size(), 3);
        QCOMPARE(phys[0], QRect(0, 0, 1920, 1080));
        QCOMPARE(phys[1], QRect(0, 1080, 1920, 1080));
        QCOMPARE(phys[2], QRect(1920, 0, 3840, 2160));
    }

    // Case 3: Negative-coordinate secondary monitor to the left of primary
    {
        const QList<QRect> geoms = {
            QRect(0, 0, 2560, 1440),
            QRect(-1920, 0, 1920, 1080),
        };
        const QList<qreal> dprs = { 1.25, 1.0 };
        const QList<QRect> phys =
          ScreenGrabber::computePhysicalScreenRects(geoms, dprs);
        QCOMPARE(phys[1], QRect(0, 0, 1920, 1080));
        QCOMPARE(phys[0], QRect(1920, 0, 3200, 1800));
    }
}

void SlickShotTests::testCaptureToolButtonCheckableState()
{
    QWidget host;
    auto* arrowBtn = new CaptureToolButton(CaptureTool::TYPE_ARROW, &host);
    auto* copyBtn = new CaptureToolButton(CaptureTool::TYPE_COPY, &host);

    QVERIFY(arrowBtn->isCheckable());
    QVERIFY(!arrowBtn->isChecked());
    arrowBtn->setChecked(true);
    QVERIFY(arrowBtn->isChecked());
    arrowBtn->setChecked(false);
    QVERIFY(!arrowBtn->isChecked());

    QVERIFY(!copyBtn->isCheckable());
    copyBtn->setChecked(true);
    QVERIFY(!copyBtn->isChecked());

    // Color state is per-instance, not shared via static state
    arrowBtn->setColor(QColor(2, 132, 199));
    copyBtn->setColor(QColor(200, 30, 30));
    QVERIFY(arrowBtn->styleSheet() != copyBtn->styleSheet());
}

void SlickShotTests::testButtonHandlerMultiRowHitTesting()
{
    QWidget host;
    host.resize(420, 420);

    const QList<CaptureTool::Type> types = {
        CaptureTool::TYPE_PENCIL,    CaptureTool::TYPE_DRAWER,
        CaptureTool::TYPE_ARROW,     CaptureTool::TYPE_SELECTION,
        CaptureTool::TYPE_RECTANGLE, CaptureTool::TYPE_CIRCLE,
        CaptureTool::TYPE_MARKER,    CaptureTool::TYPE_TEXT,
        CaptureTool::TYPE_PIXELATE,  CaptureTool::TYPE_INVERT,
        CaptureTool::TYPE_UNDO,      CaptureTool::TYPE_REDO,
        CaptureTool::TYPE_COPY,      CaptureTool::TYPE_SAVE,
    };

    QVector<CaptureToolButton*> buttons;
    for (CaptureTool::Type t : types) {
        auto* btn = new CaptureToolButton(t, &host);
        buttons.append(btn);
    }

    ButtonHandler handler(&host);
    handler.updateScreenRegions(host.rect());
    handler.setButtons(buttons);
    handler.updatePosition(QRect(40, 40, 320, 120));
    handler.show();

    // First row wraps around ~9 buttons on 420px width; button 8 (row 1 right)
    // is further right than button 13 (row 2 last).
    const QPoint row1RightCenter = buttons[7]->geometry().center();
    const QPoint row2LastCenter = buttons.last()->geometry().center();
    QVERIFY(row1RightCenter.y() < row2LastCenter.y());
    QVERIFY(row1RightCenter.x() > row2LastCenter.x());

    QVERIFY(handler.contains(buttons.first()->geometry().center()));
    QVERIFY(handler.contains(row1RightCenter));
    QVERIFY(handler.contains(row2LastCenter));
    QVERIFY(!handler.contains(QPoint(5, 5)));

    handler.hide();
    QVERIFY(!handler.isVisible());
}

void SlickShotTests::testMagnifierWidgetHiDpiAndRender()
{
    QCOMPARE(
      MagnifierWidget::physicalPixelAt(QPoint(30, 10), 2.0, QSize(100, 100)),
      QPoint(60, 20));
    QCOMPARE(
      MagnifierWidget::physicalPixelAt(QPoint(-5, 999), 2.0, QSize(100, 100)),
      QPoint(0, 99));

    QPixmap hidpi(100, 100);
    hidpi.fill(Qt::red);
    {
        QPainter p(&hidpi);
        p.fillRect(50, 0, 50, 100, QColor(2, 132, 199));
    }
    hidpi.setDevicePixelRatio(2.0);

    QWidget parent;
    parent.resize(50, 50);

    MagnifierWidget circle(hidpi, QColor(2, 132, 199), false, &parent);
    MagnifierWidget square(hidpi, QColor(2, 132, 199), true, &parent);

    QPixmap outCircle(50, 50);
    circle.render(&outCircle);
    QVERIFY(!outCircle.isNull());

    QPixmap outSquare(50, 50);
    square.render(&outSquare);
    QVERIFY(!outSquare.isNull());
}

void SlickShotTests::testShortcutsWidgetHiddenOnConstruction()
{
    ShortcutsWidget widget;
    QVERIFY(!widget.isVisible());
}

#if !defined(DISABLE_UPDATE_CHECKER)
void SlickShotTests::testUpdateNotificationWidget()
{
    QWidget parent;
    parent.resize(800, 600);

    AppUpdateInfo info{ QStringLiteral("0.2.0"),
                        QStringLiteral(
                          "https://example.invalid/releases/v0.2.0"),
                        {},
                        {} };
    auto* banner = new UpdateNotificationWidget(&parent, info);
    auto* label = banner->findChild<QLabel*>();
    QVERIFY(label != nullptr);
    QVERIFY(label->text().contains(QStringLiteral("SlickShot")));
    QVERIFY(label->text().contains(QStringLiteral("0.2.0")));

    AppUpdateInfo newer{ QStringLiteral("0.3.0"),
                         QStringLiteral(
                           "https://example.invalid/releases/v0.3.0"),
                         {},
                         {} };
    banner->setUpdateInfo(newer);
    QVERIFY(label->text().contains(QStringLiteral("0.3.0")));

    banner->show();
    QVERIFY(banner->isVisibleTo(&parent));

    banner->hide();
    QTRY_VERIFY_WITH_TIMEOUT(!banner->isVisibleTo(&parent), 1000);
}
#endif

QTEST_MAIN(SlickShotTests)
#include "tst_slickshot.moc"
