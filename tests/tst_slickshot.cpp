// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QMap>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>
#include <QTest>
#include <QVersionNumber>
#include <QWheelEvent>
#include <QWidget>

#include "config/configwindow.h"
#include "config/shortcutswidget.h"
#include "core/flameshotdaemon.h"
#include "core/qguiappcurrentscreen.h"
#include "tools/arrow/arrowtool.h"
#include "tools/capturecontext.h"
#include "tools/capturetool.h"
#include "tools/invert/inverttool.h"
#include "tools/launcher/applauncherwidget.h"
#include "tools/pin/pinwidget.h"
#include "tools/pixelate/pixelatetool.h"
#include "tools/rectangle/rectangletool.h"
#include "utils/appupdate.h"
#include "utils/filenamehandler.h"
#include "utils/geometryutils.h"
#include "utils/history.h"
#include "utils/screengrabber.h"
#include "utils/screenshotsaver.h"
#include "utils/snippingtoolmanager.h"
#include "utils/valuehandler.h"
#include "widgets/capture/buttonhandler.h"
#include "widgets/capture/capturetoolbutton.h"
#include "widgets/capture/capturetoolobjects.h"
#include "widgets/capture/capturewidget.h"
#include "widgets/capture/magnifierwidget.h"
#include "widgets/capture/overlaymessage.h"
#include "widgets/capture/selectionwidget.h"
#include "widgets/panel/colorgrabwidget.h"
#if defined(Q_OS_WIN)
#include "utils/windowshdrcapture.h"
#include <QtCore/qfloat16.h>
#endif
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
    void testAppUpdateTrustedHosts();
    void testAppUpdateInstallScriptAndEncode();
    void testAppUpdateCanInstallInPlaceForPaths();
    void testFileNameHandlerParseFilename();
    void testProperScreenshotPathDottedDirectory();
    void testSelectionWidgetStaysStationary();
    void testRegionRejectsOutOfRangeScreen();
    void testOpenWithTempFileRemovalIsDeferred();
    void testButtonListNormalizeRemovesConsecutiveInvalid();
    void testSemanticVersionCompare();
    void testSnippingToolCommandMatch();
    void testMultiMonitorPhysicalRects();
    void testRegionMappingUsesScreenOriginAndDpr();
    void testCreatePinFallsBackToLocalWidget();
    void testCaptureToolButtonCheckableState();
    void testButtonHandlerMultiRowHitTesting();
    void testButtonHandlerContainsSingleRow();
    void testHistoryPreviewScalingUsesCrossMultiplication();
    void testMagnifierWidgetHiDpiAndRender();
    void testShortcutsWidgetHiddenOnConstruction();
    void testCaptureToolObjectsFindTopmost();
    void testSaveToClipboardMimeRoundTrip();
    void testComposeRangeSplitMatchesFull();
    void testGridTileMatchesPerPaintDots();
    void testGridTileCachePromotion();
    void testMagnifierDrawsWithinReach();
    void testOverlayMessageStaticHelpersSurviveDestruction();
    void testColorGrabMapsLogicalCursorToDevicePixel();
    void testConfigWheelLeavesUnfocusedComboUnchanged();
    void testPinWidgetAcceptsTouchEvents();
#if defined(Q_OS_WIN)
    void testWindowsHdrConvertScRgbToSdr();
#endif
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
    const QString base = QStringLiteral(
      "https://github.com/khentmoba/slickshot/releases/download/v0.2.0/");
    QJsonArray assets;
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64.zip" },
      { "browser_download_url", base + "SlickShot-0.2.0-win64.zip" },
    });
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64-setup.exe" },
      { "browser_download_url", base + "SlickShot-0.2.0-win64-setup.exe" },
    });
    assets.append(QJsonObject{
      { "name", "SlickShot-0.2.0-win64-setup.exe.sha256sum" },
      { "browser_download_url",
        base + "SlickShot-0.2.0-win64-setup.exe.sha256sum" },
    });

    const QJsonObject release{
        { "tag_name", "v0.2.0" },
        { "html_url",
          "https://github.com/khentmoba/slickshot/releases/tag/v0.2.0" },
        { "assets", assets },
    };

    const AppUpdateInfo info = AppUpdate::releaseInfo(release);
    QCOMPARE(info.version, QStringLiteral("0.2.0"));
    QCOMPARE(info.pageUrl,
             QStringLiteral(
               "https://github.com/khentmoba/slickshot/releases/tag/v0.2.0"));
    QCOMPARE(info.installerUrl,
             base + QStringLiteral("SlickShot-0.2.0-win64-setup.exe"));
    QCOMPARE(info.checksumUrl,
             base +
               QStringLiteral("SlickShot-0.2.0-win64-setup.exe.sha256sum"));

    // A tag without the leading 'v' and a release without installer assets.
    const QJsonObject minimal{
        { "tag_name", "0.3.0" },
        { "html_url",
          "https://github.com/khentmoba/slickshot/releases/tag/0.3.0" },
    };
    const AppUpdateInfo minimalInfo = AppUpdate::releaseInfo(minimal);
    QCOMPARE(minimalInfo.version, QStringLiteral("0.3.0"));
    QCOMPARE(minimalInfo.pageUrl,
             QStringLiteral(
               "https://github.com/khentmoba/slickshot/releases/tag/0.3.0"));
    QVERIFY(minimalInfo.installerUrl.isEmpty());
    QVERIFY(minimalInfo.checksumUrl.isEmpty());

    // Untrusted hosts must never become download or navigation targets.
    QJsonArray hostile = assets;
    hostile[1] = QJsonObject{
        { "name", "SlickShot-0.2.0-win64-setup.exe" },
        { "browser_download_url",
          "https://example.com/SlickShot-0.2.0-win64-setup.exe" },
    };
    QJsonObject hostileRelease = release;
    hostileRelease["assets"] = hostile;
    const AppUpdateInfo hostileInfo = AppUpdate::releaseInfo(hostileRelease);
    QVERIFY(hostileInfo.installerUrl.isEmpty());
    QVERIFY(hostileInfo.checksumUrl.isEmpty());
}

void SlickShotTests::testAppUpdateSha256()
{
    const QString valid = QString(64, QLatin1Char('A'));
    QCOMPARE(
      AppUpdate::parseSha256(valid + "  SlickShot-0.2.0-win64-setup.exe\n"),
      valid.toLower());
    QCOMPARE(AppUpdate::parseSha256("\\" + valid +
                                    " *dir\\SlickShot-0.2.0-win64-setup.exe"),
             valid.toLower());
    QCOMPARE(AppUpdate::parseSha256(QStringLiteral("not-a-sha256  file")),
             QString());
    QCOMPARE(AppUpdate::parseSha256(QString(63, QLatin1Char('a')) + "  file"),
             QString());

    // Strict input: exactly 64 hex digits followed by a separator. The digest
    // is always returned in lower case.
    const QString mixedCase =
      QString(32, QLatin1Char('a')) + QString(32, QLatin1Char('F'));
    QCOMPARE(AppUpdate::parseSha256(mixedCase + " *setup.exe"),
             QString(32, QLatin1Char('a')) + QString(32, QLatin1Char('f')));
    // No separator, a non-hex character, and a leading newline are malformed.
    QCOMPARE(AppUpdate::parseSha256(valid + "setup.exe"), QString());
    QCOMPARE(
      AppUpdate::parseSha256(valid.mid(1) + QLatin1Char('g') + "  setup.exe"),
      QString());
    QCOMPARE(AppUpdate::parseSha256(QLatin1Char('\n') + valid + "  setup.exe"),
             QString());

    // When an expected file name is supplied, the checksum must describe that
    // exact asset. Directory prefixes, case, the '*' binary marker and CRLF
    // endings are all tolerated; a different file name is rejected.
    const QString asset = QStringLiteral("SlickShot-0.2.0-win64-setup.exe");
    QCOMPARE(AppUpdate::parseSha256(valid + " *" + asset, asset),
             valid.toLower());
    QCOMPARE(AppUpdate::parseSha256(
               "\\" + valid + " *releases\\" + asset + "\r\n", asset),
             valid.toLower());
    QCOMPARE(AppUpdate::parseSha256(valid + "  " + asset, asset.toUpper()),
             valid.toLower());
    QCOMPARE(AppUpdate::parseSha256(valid + "  other.exe", asset), QString());
    QCOMPARE(AppUpdate::parseSha256(valid + "  " + asset + ".old", asset),
             QString());
}

void SlickShotTests::testAppUpdateTrustedHosts()
{
    // The redirect guard reuses isTrustedAssetUrl on every hop, so the trusted
    // set is the policy that matters for both the initial URL and redirects.
    QVERIFY(AppUpdate::isTrustedAssetUrl(
      QStringLiteral("https://github.com/o/r/releases/download/v1/a.exe")));
    QVERIFY(AppUpdate::isTrustedAssetUrl(
      QStringLiteral("https://objects.githubusercontent.com/asset?token=1")));
    QVERIFY(AppUpdate::isTrustedAssetUrl(
      QStringLiteral("https://github-releases.githubusercontent.com/asset")));
    QVERIFY(AppUpdate::isTrustedAssetUrl(
      QStringLiteral("https://release-assets.githubusercontent.com/asset")));

    // Arbitrary hosts, including lookalikes and plain HTTP, are never trusted
    // as a download or redirect target.
    QVERIFY(!AppUpdate::isTrustedAssetUrl(
      QStringLiteral("https://example.com/SlickShot-setup.exe")));
    QVERIFY(!AppUpdate::isTrustedAssetUrl(
      QStringLiteral("https://github.com.evil.test/a.exe")));
    QVERIFY(!AppUpdate::isTrustedAssetUrl(
      QStringLiteral("http://objects.githubusercontent.com/a.exe")));
    QVERIFY(!AppUpdate::isTrustedAssetUrl(QStringLiteral("not a url")));
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

    // Non-ASCII text confirms the command is encoded as UTF-16LE, not UTF-8.
    const QString unicode = QStringLiteral("caf\u00E9 \u2603");
    const QByteArray unicodeRaw =
      QByteArray::fromBase64(AppUpdate::encodeCommand(unicode).toLatin1());
    QCOMPARE(unicodeRaw.size(), unicode.size() * 2);
    QCOMPARE(QString::fromUtf16(
               reinterpret_cast<const char16_t*>(unicodeRaw.constData()),
               unicodeRaw.size() / 2),
             unicode);
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

    const QString installRoot =
      QStringLiteral("C:\\Users\\alice\\AppData\\Local\\Programs\\SlickShot");
    const QString installed = QStringLiteral(
      "C:\\Users\\alice\\AppData\\Local\\Programs\\SlickShot\\bin\\"
      "flameshot.exe");

    QVERIFY(
      AppUpdate::canInstallInPlaceForPaths(validInfo, installed, installRoot));
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
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      missingInstaller, installed, installRoot));
    AppUpdateInfo missingChecksum = validInfo;
    missingChecksum.checksumUrl.clear();
    QVERIFY(!AppUpdate::canInstallInPlaceForPaths(
      missingChecksum, installed, installRoot));
    QVERIFY(
      !AppUpdate::canInstallInPlaceForPaths(validInfo, QString(), installRoot));
    QVERIFY(
      !AppUpdate::canInstallInPlaceForPaths(validInfo, installed, QString()));
}

void SlickShotTests::testFileNameHandlerParseFilename()
{
    FileNameHandler handler;

    // Empty input falls back to the configured default pattern; it must be
    // expanded and must not leak raw strftime specifiers into a filename.
    const QString defaultName = handler.parseFilename(QString());
    QVERIFY(!defaultName.isEmpty());
    QVERIFY(!defaultName.contains(QLatin1Char('%')));

    // A trailing '%' is an incomplete specifier and must be removed.
    QCOMPARE(handler.parseFilename(QStringLiteral("shot%")),
             QStringLiteral("shot"));

    // Path and drive separators must never survive into a filename.
    const QString separators =
      handler.parseFilename(QStringLiteral("a/b\\c:d"));
    QVERIFY(!separators.contains(QLatin1Char('/')));
    QVERIFY(!separators.contains(QLatin1Char('\\')));
    QVERIFY(!separators.contains(QLatin1Char(':')));

    // Control characters are removed rather than written into a path.
    const QString control =
      handler.parseFilename(QStringLiteral("shot\nname\t\x01"
                                           "end"));
    for (int i = 0; i < control.size(); ++i) {
        const ushort code = control.at(i).unicode();
        QVERIFY2(!(code < 0x20 || (code >= 0x7f && code < 0xa0)),
                 qPrintable(QStringLiteral("control char at %1").arg(i)));
    }

#if defined(Q_OS_WIN)
    // Windows device names cannot be used as a filename, with or without an
    // extension. The exact replacement is an implementation detail, so only
    // require that the reserved name no longer appears verbatim.
    const QStringList reserved{
        QStringLiteral("CON"), QStringLiteral("PRN"),  QStringLiteral("AUX"),
        QStringLiteral("NUL"), QStringLiteral("COM1"), QStringLiteral("LPT9")
    };
    for (const QString& name : reserved) {
        const QString parsed = handler.parseFilename(name);
        QVERIFY2(parsed.compare(name, Qt::CaseInsensitive) != 0,
                 qPrintable(name));
        const QString withExtension = name + QStringLiteral(".txt");
        const QString parsedWithExtension =
          handler.parseFilename(withExtension);
        QVERIFY2(
          parsedWithExtension.compare(withExtension, Qt::CaseInsensitive) != 0,
          qPrintable(parsedWithExtension));
    }
#endif
}

void SlickShotTests::testProperScreenshotPathDottedDirectory()
{
    FileNameHandler handler;
    const QString dirPath =
      QDir::tempPath() + QStringLiteral("/slickshot_test.dotted");
    QVERIFY(QDir().mkpath(dirPath));
    const QString path = handler.properScreenshotPath(dirPath, QString());
    QVERIFY2(path.startsWith(dirPath), qPrintable(path));
    // The directory's own suffix must not become the screenshot's extension.
    QCOMPARE(QFileInfo(path).suffix(), QStringLiteral("png"));
    QDir(dirPath).removeRecursively();
}

void SlickShotTests::testSelectionWidgetStaysStationary()
{
    QWidget parent;
    parent.resize(400, 300);
    SelectionWidget selection(QColor(255, 0, 0), &parent);

    const QRect selectionRect(10, 20, 100, 50);
    selection.setGeometry(selectionRect);

    // The logical selection follows setGeometry(), but the transparent
    // overlay widget must stay pinned to the parent so a drag does not
    // invalidate the whole old and new child footprint.
    QCOMPARE(selection.geometry(), selectionRect);
    QCOMPARE(static_cast<QWidget*>(&selection)->geometry(), parent.rect());
}

void SlickShotTests::testRegionRejectsOutOfRangeScreen()
{
    Region region;
    const int screenCount = QGuiApplication::screens().size();
    QVERIFY(region.check(QStringLiteral("all")));
    QVERIFY(region.check(QStringLiteral("screen0")));
    QVERIFY(!region.check(QStringLiteral("screen%1").arg(screenCount)));
}

void SlickShotTests::testOpenWithTempFileRemovalIsDeferred()
{
    const QString dir =
      QDir::tempPath() + QStringLiteral("/SlickShot-test-openwith");
    QVERIFY(QDir().mkpath(dir));
    QString path;
    {
        QTemporaryFile file(dir + QStringLiteral("/deferred-XXXXXX.png"));
        file.setAutoRemove(false);
        QVERIFY(file.open());
        file.close();
        path = file.fileName();
    }
    QVERIFY(QFile::exists(path));

    AppLauncherWidget::scheduleTempFileRemoval(path, 50);
    QVERIFY(QFile::exists(path));
    QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(path), 2000);
}

void SlickShotTests::testButtonListNormalizeRemovesConsecutiveInvalid()
{
    const int arrow = static_cast<int>(CaptureTool::TYPE_ARROW);
    const int pencil = static_cast<int>(CaptureTool::TYPE_PENCIL);

    // 7, 25, -1 and 9999 are not in CaptureToolButton::iterableButtonTypes.
    // Consecutive invalid values used to skip entries during forward removal.
    QList<int> mixed{ 7, -1, arrow, 25, 7, pencil, -1 };
    QVERIFY(ButtonList::normalizeButtons(mixed));
    QCOMPARE(mixed, QList<int>({ arrow, pencil }));

    QList<int> edges{ -1, 7, pencil, arrow, 25, -1 };
    QVERIFY(ButtonList::normalizeButtons(edges));
    QCOMPARE(edges, QList<int>({ pencil, arrow }));

    QList<int> clean{ pencil, arrow };
    QVERIFY(!ButtonList::normalizeButtons(clean));
    QCOMPARE(clean, QList<int>({ pencil, arrow }));

    QList<int> allInvalid{ 7, 25, -1, 9999 };
    QVERIFY(ButtonList::normalizeButtons(allInvalid));
    QVERIFY(allInvalid.isEmpty());
}

void SlickShotTests::testSemanticVersionCompare()
{
    // SlickShot has no bespoke version-comparison helper: the update flow in
    // FlameshotDaemon compares Flameshot::getVersion() (APP_VERSION parsed as a
    // QVersionNumber) with QVersionNumber::fromString(release.version). This
    // test pins the QVersionNumber ordering that flow depends on, including
    // numeric segment comparison (0.9.0 < 0.10.0, not lexicographic).
    QVERIFY(QVersionNumber::fromString(QStringLiteral("0.9.0")) <
            QVersionNumber::fromString(QStringLiteral("0.10.0")));
    QVERIFY(QVersionNumber::fromString(QStringLiteral("0.10.0")) <
            QVersionNumber::fromString(QStringLiteral("0.10.1")));
    QVERIFY(!(QVersionNumber::fromString(QStringLiteral("1.0.0")) <
              QVersionNumber::fromString(QStringLiteral("0.99.0"))));

    // Mirror Flameshot::getVersion(): strip the leading 'v' from APP_VERSION
    // and parse the rest. This must yield a usable, non-null version.
    const QVersionNumber builtVersion = QVersionNumber::fromString(
      QStringLiteral(APP_VERSION).replace(QLatin1Char('v'), QString()));
    QVERIFY(!builtVersion.isNull());
    QVERIFY(builtVersion <
            QVersionNumber::fromString(QStringLiteral("9999.0")));
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

    // Surrounding whitespace and quote/argument variations are ignored.
    QVERIFY(SnippingToolManager::commandMatchesAppPath(
      QStringLiteral("  \"C:\\Program Files\\SlickShot\\flameshot.exe\" gui "
                     "\"%1\"  \n"),
      expected));
    QVERIFY(SnippingToolManager::commandMatchesAppPath(
      QStringLiteral("\"C:\\Program Files\\SlickShot\\flameshot.exe\"\tgui"),
      expected));
    QVERIFY(SnippingToolManager::commandMatchesAppPath(
      expected + QStringLiteral("  gui \"%1\""), expected));
    QVERIFY(SnippingToolManager::commandMatchesAppPath(
      QStringLiteral("\"C:\\Program Files\\SlickShot\\flameshot.exe\""),
      expected));

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

void SlickShotTests::testRegionMappingUsesScreenOriginAndDpr()
{
    QPixmap screenPixmap(2880, 1620);
    screenPixmap.setDevicePixelRatio(1.5);

    const QRect screen(1920, 0, 1920, 1080);
    const QRect requested(2000, 100, 200, 100);
    const GeometryUtils::RegionMapping mapping =
      GeometryUtils::mapRegionToScreen(requested, screen, screenPixmap);

    QVERIFY(mapping.valid);
    QCOMPARE(mapping.localLogical, QRect(80, 100, 200, 100));
    QCOMPARE(mapping.localPixmap, QRect(120, 150, 300, 150));
    QCOMPARE(mapping.globalLogical, requested);

    const GeometryUtils::RegionMapping outside =
      GeometryUtils::mapRegionToScreen(
        QRect(0, 0, 100, 100), screen, screenPixmap);
    QVERIFY(!outside.valid);
}

void SlickShotTests::testCreatePinFallsBackToLocalWidget()
{
#if defined(Q_OS_WIN)
    QPixmap pixmap(8, 8);
    pixmap.fill(Qt::red);
    FlameshotDaemon::createLocalPin(pixmap, QRect(20, 20, 8, 8));
    QVERIFY(FlameshotDaemon::hasLocalPins());

    const QList<QWidget*> windows = qApp->topLevelWidgets();
    for (QWidget* window : windows) {
        if (window->windowTitle() == QLatin1String("slickshot-pin")) {
            window->close();
        }
    }
    QTRY_VERIFY_WITH_TIMEOUT(!FlameshotDaemon::hasLocalPins(), 1000);
#else
    QSKIP("Local pin fallback is used on Windows with KDSingleApplication");
#endif
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

    // Group buttons into rows using their actual geometry. The number of
    // buttons per row depends on font metrics and DPI, so no fixed indices.
    QMap<int, QVector<CaptureToolButton*>> rows;
    for (CaptureToolButton* btn : buttons) {
        rows[btn->geometry().top()].append(btn);
    }
    QVERIFY2(rows.size() > 1, "Expected the toolbar to wrap into several rows");

    const QList<int> rowTops = rows.keys(); // QMap keys are sorted
    const QVector<CaptureToolButton*>& firstRow = rows.value(rowTops.first());
    const QVector<CaptureToolButton*>& lastRow = rows.value(rowTops.last());

    CaptureToolButton* firstRowRight = firstRow.first();
    for (CaptureToolButton* btn : firstRow) {
        if (btn->geometry().right() > firstRowRight->geometry().right()) {
            firstRowRight = btn;
        }
    }
    CaptureToolButton* lastRowLast = lastRow.last();

    const QPoint row1RightCenter = firstRowRight->geometry().center();
    const QPoint row2LastCenter = lastRowLast->geometry().center();
    QVERIFY(row1RightCenter.y() < row2LastCenter.y());

    // When the last row is shorter it is centered, so its right edge is
    // strictly left of the fuller first row's right edge.
    QRect firstRowBounds;
    QRect lastRowBounds;
    for (CaptureToolButton* btn : firstRow) {
        firstRowBounds = firstRowBounds.united(btn->geometry());
    }
    for (CaptureToolButton* btn : lastRow) {
        lastRowBounds = lastRowBounds.united(btn->geometry());
    }
    if (lastRowBounds.width() < firstRowBounds.width()) {
        QVERIFY(row1RightCenter.x() > row2LastCenter.x());
    }

    QVERIFY(handler.contains(buttons.first()->geometry().center()));
    QVERIFY(handler.contains(row1RightCenter));
    QVERIFY(handler.contains(row2LastCenter));
    QVERIFY(!handler.contains(QPoint(5, 5)));

    handler.hide();
    QVERIFY(!handler.isVisible());
}

void SlickShotTests::testButtonHandlerContainsSingleRow()
{
    QWidget host;
    host.resize(400, 300);

    ButtonHandler empty(&host);
    QVERIFY(!empty.contains(QPoint(10, 10)));
    QCOMPARE(empty.size(), size_t(0));

    QVector<CaptureToolButton*> buttons;
    buttons.append(new CaptureToolButton(CaptureTool::TYPE_ARROW, &host));
    buttons.append(new CaptureToolButton(CaptureTool::TYPE_COPY, &host));

    ButtonHandler handler(&host);
    handler.updateScreenRegions(host.rect());
    handler.setButtons(buttons);
    handler.updatePosition(QRect(50, 50, 200, 100));
    QCOMPARE(handler.size(), size_t(2));

    // Every button center is inside the toolbar; a point well outside the
    // selection is not.
    for (CaptureToolButton* btn : buttons) {
        QVERIFY(handler.contains(btn->geometry().center()));
    }
    QVERIFY(!handler.contains(QPoint(5, 5)));
}

void SlickShotTests::testHistoryPreviewScalingUsesCrossMultiplication()
{
    // The aspect-ratio decision is inline in History::save(), with no public
    // helper to call. Exercising it would write to the real user history
    // directory and History::history() prunes files there, so it is not a
    // deterministic offscreen unit test. Skipped intentionally.
    QSKIP("History preview scaling has no deterministic public helper and "
          "writes into the user's history directory.");
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

    // The constructor loads shortcuts and fills the table, so check the
    // loaded data rather than only the default QWidget visibility.
    auto* table = widget.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->columnCount(), 2);
    QVERIFY2(table->rowCount() > 0,
             "Shortcuts table should be populated on construction");
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* description = table->item(row, 0);
        QVERIFY(description != nullptr);
        QVERIFY(!description->text().isEmpty());
        QVERIFY(table->item(row, 1) != nullptr);
    }
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

void SlickShotTests::testOverlayMessageStaticHelpersSurviveDestruction()
{
    QWidget parent;
    OverlayMessage::init(&parent, QRect(0, 0, 100, 100));
    QVERIFY(OverlayMessage::instance() != nullptr);

    delete OverlayMessage::instance();
    QVERIFY(OverlayMessage::instance() == nullptr);

    // These used to dereference a dangling static pointer after destruction.
    OverlayMessage::push(QStringLiteral("message"));
    OverlayMessage::pop();
    OverlayMessage::setVisibility(false);
    QVERIFY(OverlayMessage::instance() == nullptr);
}

void SlickShotTests::testColorGrabMapsLogicalCursorToDevicePixel()
{
    QScreen* screen = QGuiAppCurrentScreen().currentScreen();
    QVERIFY(screen != nullptr);

    QPixmap pixmap(1000, 1000);
    pixmap.setDevicePixelRatio(2.0);
    ColorGrabWidget grabber(&pixmap);

    const QPoint logical = screen->geometry().topLeft() + QPoint(100, 100);
    const QPoint mapped = grabber.physicalPixelAt(logical);
    const qreal dpr = screen->devicePixelRatio();
    QCOMPARE(mapped, QPoint(qRound(100 * dpr), qRound(100 * dpr)));
}

void SlickShotTests::testConfigWheelLeavesUnfocusedComboUnchanged()
{
    ConfigWindow window;
    window.resize(320, 200);
    window.show();

    QComboBox* combo = nullptr;
    const auto combos = window.findChildren<QComboBox*>();
    for (QComboBox* candidate : combos) {
        if (candidate->count() > 1) {
            combo = candidate;
            break;
        }
    }
    QVERIFY(combo != nullptr);

    combo->clearFocus();
    const int before = combo->currentIndex();
    QWheelEvent wheel(QPointF(0, 0),
                      QPointF(0, 0),
                      QPoint(),
                      QPoint(0, 120),
                      Qt::NoButton,
                      Qt::NoModifier,
                      Qt::NoScrollPhase,
                      false);
    QApplication::sendEvent(combo, &wheel);
    QCOMPARE(combo->currentIndex(), before);
    // The filter ignores the wheel, so it must not stay accepted.
    QVERIFY(!wheel.isAccepted());
}

void SlickShotTests::testPinWidgetAcceptsTouchEvents()
{
    QPixmap pixmap(40, 40);
    PinWidget pin(pixmap, QRect(0, 0, 40, 40));
    QVERIFY(pin.testAttribute(Qt::WA_AcceptTouchEvents));
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

#if defined(Q_OS_WIN)
void SlickShotTests::testWindowsHdrConvertScRgbToSdr()
{
    auto refEncode = [](float scRgb, double sdrWhiteNits) -> quint8 {
        const float invScale = static_cast<float>(80.0 / sdrWhiteNits);
        const float normalized = scRgb * invScale;
        const float linear = normalized > 0.0f ? qMin(normalized, 1.0f) : 0.0f;
        const double l =
          static_cast<double>(static_cast<int>(linear * 65535.0f + 0.5f)) /
          65535.0;
        const double v =
          l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1.0 / 2.4) - 0.055;
        return static_cast<quint8>(qBound(0, qRound(v * 255.0), 255));
    };

    // Invalid inputs must safely return a null image.
    QVERIFY(
      WindowsHdrCapture::convertScRgbToSdr(nullptr, 32, 4, 4, 200.0).isNull());
    const quint16 dummy[4] = {};
    QVERIFY(WindowsHdrCapture::convertScRgbToSdr(
              reinterpret_cast<const uchar*>(dummy), 8, 1, 1, 0.0)
              .isNull());
    QVERIFY(WindowsHdrCapture::convertScRgbToSdr(
              reinterpret_cast<const uchar*>(dummy), 8, 0, 1, 200.0)
              .isNull());

    // Test representative FP16 bit patterns across two SDR white levels and
    // with row stride > width * 8 bytes.
    const std::vector<float> samples = {
        0.0f,  0.001f, 0.05f, 0.5f, 1.0f, 2.5f,
        5.0f,  10.0f,  -0.5f, std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()
    };
    const int width = static_cast<int>(samples.size());
    const int height = 2;
    const qsizetype bytesPerLine = static_cast<qsizetype>(width) * 8 + 16;
    std::vector<uchar> buffer(static_cast<size_t>(bytesPerLine) * height, 0xAB);

    for (int y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<qfloat16*>(buffer.data() + y * bytesPerLine);
        for (int x = 0; x < width; ++x) {
            const float f = samples[(x + y) % samples.size()];
            const qfloat16 h(f);
            row[x * 4 + 0] = h;
            row[x * 4 + 1] = qfloat16(f * 0.5f);
            row[x * 4 + 2] = qfloat16(f * 1.5f);
            row[x * 4 + 3] = qfloat16(1.0f);
        }
    }

    for (double sdrWhiteNits : { 80.0, 240.0 }) {
        const QImage out = WindowsHdrCapture::convertScRgbToSdr(
          buffer.data(), bytesPerLine, width, height, sdrWhiteNits);
        QCOMPARE(out.size(), QSize(width, height));
        QCOMPARE(out.format(), QImage::Format_RGB32);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float f = samples[(x + y) % samples.size()];
                const float rIn = static_cast<float>(qfloat16(f));
                const float gIn = static_cast<float>(qfloat16(f * 0.5f));
                const float bIn = static_cast<float>(qfloat16(f * 1.5f));
                const QRgb expected = qRgb(refEncode(rIn, sdrWhiteNits),
                                           refEncode(gIn, sdrWhiteNits),
                                           refEncode(bIn, sdrWhiteNits));
                QCOMPARE(out.pixel(x, y), expected);
            }
        }
    }
}
#endif

QTEST_MAIN(SlickShotTests)
#include "tst_slickshot.moc"
