// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QTest>
#include <QVersionNumber>
#include <QWidget>

#include "config/shortcutswidget.h"
#include "tools/arrow/arrowtool.h"
#include "tools/capturecontext.h"
#include "tools/capturetool.h"
#include "tools/invert/inverttool.h"
#include "tools/pixelate/pixelatetool.h"
#include "tools/rectangle/rectangletool.h"
#include "utils/appupdate.h"
#include "utils/screengrabber.h"
#include "utils/screenshotsaver.h"
#include "utils/snippingtoolmanager.h"
#include "widgets/capture/buttonhandler.h"
#include "widgets/capture/capturetoolbutton.h"
#include "widgets/capture/capturetoolobjects.h"
#include "widgets/capture/capturewidget.h"
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
    void testCaptureToolObjectsFindTopmost();
    void testSaveToClipboardMimeRoundTrip();
    void testComposeRangeSplitMatchesFull();
    void testGridTileMatchesPerPaintDots();
    void testGridTileCachePromotion();
    void testMagnifierDrawsWithinReach();
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
      { "name", "SlickShot-0.2.0-win64-setup.exe" },
      { "browser_download_url",
        "https://example.com/SlickShot-0.2.0-win64-setup.exe" },
    });
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64-setup.exe.sha256sum" },
      { "browser_download_url",
        "https://example.com/SlickShot-0.2.0-win64-setup.exe.sha256sum" },
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
             QStringLiteral("https://example.com/SlickShot-0.2.0-win64-setup.exe"));
    QCOMPARE(info.checksumUrl,
             QStringLiteral(
               "https://example.com/SlickShot-0.2.0-win64-setup.exe.sha256sum"));
}

void SlickShotTests::testAppUpdateSha256()
{
    const QString valid = QString(64, QLatin1Char('A'));
    QCOMPARE(AppUpdate::parseSha256(valid + "  SlickShot-0.2.0-win64-setup.exe\n"),
             valid.toLower());
    QCOMPARE(
      AppUpdate::parseSha256("\\" + valid + " *dir\\SlickShot-0.2.0-win64-setup.exe"),
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
      QStringLiteral("C:\\Temp\\O'Connor\\SlickShot-update-setup.exe"),
      QString(64, QLatin1Char('b')),
      QStringLiteral("C:\\Users\\O'Connor\\AppData\\Local\\Programs\\"
                     "SlickShot\\bin\\flameshot.exe"));

    QVERIFY(script.contains(QStringLiteral("Wait-Process -Id 1234")));
    QVERIFY(script.contains(QStringLiteral("O''Connor")));
    QVERIFY(script.contains(QStringLiteral("[System.IO.FileShare]::Read")));
    QVERIFY(script.contains(QStringLiteral("ComputeHash")));
    QVERIFY(script.contains(QStringLiteral("'/S'")));
    QVERIFY(script.contains(QStringLiteral("Start-Process -FilePath $app")));
    QVERIFY(!script.contains(QStringLiteral("msiexec")));
    QVERIFY(!script.contains(QStringLiteral("%")));

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
    const AppUpdateInfo validInfo{
        QStringLiteral("0.2.0"),
        QStringLiteral("https://example.com/release"),
        QStringLiteral("https://example.com/SlickShot-0.2.0-win64-setup.exe"),
        QStringLiteral(
          "https://example.com/SlickShot-0.2.0-win64-setup.exe.sha256sum"),
    };

    const QString installRoot = QStringLiteral(
      "C:\\Users\\alice\\AppData\\Local\\Programs\\SlickShot");
    const QString installed = QStringLiteral(
      "C:\\Users\\alice\\AppData\\Local\\Programs\\SlickShot\\bin\\"
      "flameshot.exe");

    QVERIFY(AppUpdate::canInstallInPlaceForPaths(validInfo, installed,
                                                 installRoot));
    QVERIFY(AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral(
        "C:/Users/alice/AppData/Local/Programs/slickshot/bin/flameshot.exe"),
      installRoot));
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral("C:\\Program Files\\SlickShot\\bin\\flameshot.exe"),
      installRoot));
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      validInfo,
      QStringLiteral(
        "C:\\Users\\alice\\AppData\\Local\\Programs\\SlickShot2\\bin\\"
        "flameshot.exe"),
      installRoot));

    AppUpdateInfo missingInstaller = validInfo;
    missingInstaller.installerUrl.clear();
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(missingInstaller, installed,
                                                  installRoot));
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

namespace {

void addInvertObject(CaptureToolObjects& objects,
                     QObject* owner,
                     const QPoint& from,
                     const QPoint& to)
{
    CaptureContext ctx;
    ctx.color = Qt::red;
    ctx.toolSize = 4;
    ctx.mousePos = from;
    auto* tool = new InvertTool(owner);
    tool->drawStart(ctx);
    tool->drawMove(to);
    objects.append(tool);
}

} // namespace

void SlickShotTests::testCaptureToolObjectsFindTopmost()
{
    QObject owner;
    CaptureToolObjects objects(&owner);
    addInvertObject(objects, &owner, QPoint(100, 100), QPoint(200, 200));
    addInvertObject(objects, &owner, QPoint(300, 300), QPoint(400, 400));
    addInvertObject(objects, &owner, QPoint(150, 150), QPoint(180, 180));
    addInvertObject(objects, &owner, QPoint(1900, 1040), QPoint(1919, 1079));
    addInvertObject(objects, &owner, QPoint(0, 0), QPoint(4, 4));

    const QSize canvas(1920, 1080);
    QCOMPARE(objects.find(QPoint(120, 120), canvas), 0);
    QCOMPARE(objects.find(QPoint(160, 160), canvas), 2);
    QCOMPARE(objects.find(QPoint(110, 110), canvas), 0);
    QCOMPARE(objects.find(QPoint(97, 150), canvas), 0);
    QCOMPARE(objects.find(QPoint(350, 350), canvas), 1);
    QCOMPARE(objects.find(QPoint(250, 250), canvas), -1);
    QCOMPARE(objects.find(QPoint(50, 50), canvas), -1);
    QCOMPARE(objects.find(QPoint(1915, 1075), canvas), 3);
    // The search square runs past the bottom-right corner here. Pixels
    // outside the canvas must not count as hits on the top object.
    QCOMPARE(objects.find(QPoint(1921, 1081), canvas), 3);
    QCOMPARE(objects.find(QPoint(2, 2), canvas), 4);
}

void SlickShotTests::testComposeRangeSplitMatchesFull()
{
    QObject owner;
    CaptureToolObjects objects(&owner);
    CaptureContext ctx;
    ctx.color = Qt::red;
    ctx.toolSize = 4;

    auto* pixelate = new PixelateTool(&owner);
    ctx.mousePos = QPoint(40, 40);
    pixelate->drawStart(ctx);
    pixelate->drawMove(QPoint(200, 160));
    objects.append(pixelate);

    auto* rectangle = new RectangleTool(&owner);
    ctx.mousePos = QPoint(60, 60);
    rectangle->drawStart(ctx);
    rectangle->drawMove(QPoint(120, 100));
    objects.append(rectangle);

    auto* arrow = new ArrowTool(&owner);
    ctx.mousePos = QPoint(10, 150);
    arrow->drawStart(ctx);
    arrow->drawMove(QPoint(180, 20));
    objects.append(arrow);

    QPixmap original(256, 192);
    original.fill(QColor(30, 90, 150));
    {
        QPainter painter(&original);
        painter.fillRect(QRect(0, 0, 128, 96), QColor(200, 40, 40));
    }

    QPixmap full = original;
    objects.composeRange(full, 0, objects.size());
    const QImage expected = full.toImage();

    // Dragging composes the objects below the moved one once, then the rest
    // on top. Every split point must match a single full pass.
    for (int split = 0; split <= objects.size(); ++split) {
        QPixmap partial = original;
        objects.composeRange(partial, 0, split);
        objects.composeRange(partial, split, objects.size());
        QVERIFY(partial.toImage() == expected);
    }
}

void SlickShotTests::testGridTileMatchesPerPaintDots()
{
    const QColor color(255, 255, 255, 100);
    const QPoint topLeft(13, 9);
    // Far edge of the selection, deliberately off the grid lines
    const QPointF farEdge(200.6, 150.4);
    const QSizeF canvas(240, 160);

    struct Case
    {
        qreal dpr;
        int gridSize;
    };
    // Even step, HiDPI even step, and a fractional step (5 px grid at 2x)
    const Case cases[] = { { 1.0, 10 }, { 2.0, 10 }, { 2.0, 5 } };
    for (const Case& c : cases) {
        const qreal scale = c.dpr;
        const QSize pixels = (canvas * c.dpr).toSize();

        // Reference: draw every dot directly, as a paint would
        QImage expected(pixels, QImage::Format_ARGB32_Premultiplied);
        expected.setDevicePixelRatio(c.dpr);
        expected.fill(Qt::transparent);
        {
            QPainter painter(&expected);
            painter.setPen(color);
            painter.setBrush(QBrush(color));
            const qreal step = c.gridSize / scale;
            const qreal radius = 1 * scale;
            for (int y = topLeft.y(); y < farEdge.y(); y += step) {
                for (int x = topLeft.x(); x < farEdge.x(); x += step) {
                    painter.drawEllipse(x, y, radius, radius);
                }
            }
        }

        QImage actual(pixels, QImage::Format_ARGB32_Premultiplied);
        actual.setDevicePixelRatio(c.dpr);
        actual.fill(Qt::transparent);
        {
            QPainter painter(&actual);
            const QImage tile = CaptureWidget::renderGridTile(
              farEdge - topLeft, c.dpr, scale, c.gridSize, color);
            const int margin = CaptureWidget::gridTileMargin(scale);
            painter.drawImage(topLeft - QPoint(margin, margin), tile);
        }
        QVERIFY(actual == expected);
    }
}

void SlickShotTests::testGridTileCachePromotion()
{
    using Cache = CaptureGridTileCache;
    Cache cache;
    Cache::Key key{ QPointF(200, 150), 1.0, 1.0, 10, qRgb(255, 255, 255) };

    QVERIFY(Cache::supportsDevicePixelRatio(1.0, 1.0));
    QVERIFY(Cache::supportsDevicePixelRatio(2.0, 2.0));
    QVERIFY(!Cache::supportsDevicePixelRatio(1.25, 1.25));
    QVERIFY(!Cache::supportsDevicePixelRatio(1.5, 1.5));
    QVERIFY(!Cache::supportsDevicePixelRatio(2.0, 1.0));

    QVERIFY(cache.request(key) == Cache::Request::DrawDirect);
    QVERIFY(cache.tile().isNull());
    QVERIFY(cache.request(key) == Cache::Request::BuildTile);

    QImage tile(8, 8, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::white);
    cache.setTile(tile);
    QVERIFY(cache.request(key) == Cache::Request::UseCachedTile);
    QVERIFY(!cache.tile().isNull());

    Cache::Key changed = key;
    changed.limit.setX(changed.limit.x() + 1);
    QVERIFY(cache.request(changed) == Cache::Request::DrawDirect);
    QVERIFY(cache.tile().isNull());
    QVERIFY(cache.request(changed) == Cache::Request::BuildTile);

    cache.setTile(tile);
    cache.clear();
    QVERIFY(cache.tile().isNull());
    QVERIFY(cache.request(changed) == Cache::Request::DrawDirect);
    QVERIFY(cache.tile().isNull());
}

void SlickShotTests::testMagnifierDrawsWithinReach()
{
    if (QGuiApplication::platformName() != QLatin1String("offscreen")) {
        QSKIP("Moves the cursor; runs only on the offscreen platform.");
    }
    // The capture window repaints only a box of MagnifierWidget::reach around
    // the cursor, so everything the loupe draws has to stay inside that box.
    QPixmap shot(400, 300);
    shot.fill(Qt::darkGray);
    QWidget parent;
    parent.resize(1280, 720);
    MagnifierWidget magnifier(shot, QColor(2, 132, 199), true, &parent);
    magnifier.resize(1280, 720);

    const QPoint cursor(600, 360);
    QCursor::setPos(parent.mapToGlobal(cursor));
    QImage image(1280, 720, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    magnifier.render(&image, QPoint(), QRegion(), QWidget::DrawChildren);

    QRect painted;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) != 0) {
                painted = painted.united(QRect(x, y, 1, 1));
            }
        }
    }
    QVERIFY(!painted.isNull());
    const int reach = MagnifierWidget::reach;
    const QRect box(cursor - QPoint(reach, reach), QSize(2 * reach, 2 * reach));
    QCOMPARE(box.united(painted), box);
}

void SlickShotTests::testSaveToClipboardMimeRoundTrip()
{
    if (QGuiApplication::platformName() != QLatin1String("offscreen")) {
        QSKIP("Uses the clipboard; runs only on the offscreen platform.");
    }
    QPixmap capture(64, 48);
    capture.fill(QColor(12, 34, 56));

    saveToClipboardMime(capture, QStringLiteral("png"));
    const QMimeData* pngData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(pngData != nullptr);
    QVERIFY(pngData->hasFormat(QStringLiteral("image/png")));
    const QImage png =
      QImage::fromData(pngData->data(QStringLiteral("image/png")), "PNG");
    QCOMPARE(png.size(), QSize(64, 48));
    QCOMPARE(png.pixelColor(10, 10), QColor(12, 34, 56));

    saveToClipboardMime(capture, QStringLiteral("jpeg"));
    const QMimeData* jpegData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(jpegData != nullptr);
    QVERIFY(jpegData->hasFormat(QStringLiteral("image/jpeg")));
    const QImage jpeg =
      QImage::fromData(jpegData->data(QStringLiteral("image/jpeg")), "JPEG");
    QCOMPARE(jpeg.size(), QSize(64, 48));
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
