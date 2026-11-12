
#include "screengrabber.h"
#include "core/qguiappcurrentscreen.h"
#include "utils/abstractlogger.h"
#include "utils/confighandler.h"
#include "utils/monitorpreview.h"
#include "utils/systemnotification.h"

#include <QApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <numeric>

#ifdef FLAMESHOT_DEBUG_CAPTURE
#include <QDebug>
#endif

#if !(defined(Q_OS_MACOS) || defined(Q_OS_WIN))
#include "request.h"
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDir>
#include <QUrl>
#include <QUuid>
#endif

bool ScreenGrabber::m_monitorSelectionActive = false;

ScreenGrabber::ScreenGrabber(QObject* parent)
  : QObject(parent)
  , m_selectedMonitor(-1)
  , m_highlightedMonitorPreview(-1)
  , m_monitorSelectionLoop(nullptr)
  , m_userCancelled(false)
{
    // Increase image allocation limit for large screenshots
    // (multi-monitor/high-DPI) Default is 128MB, set to 1GB to handle 4K+
    // multi-monitor setups
    QImageReader::setAllocationLimit(1024);
}

ScreenGrabber::PortalStatus ScreenGrabber::freeDesktopPortal(
  QPixmap& res,
  QString& errorDetail)
{

#if !(defined(Q_OS_MACOS) || defined(Q_OS_WIN))
    auto* connectionInterface = QDBusConnection::sessionBus().interface();
    auto service = QStringLiteral("org.freedesktop.portal.Desktop");

    if (!connectionInterface->isServiceRegistered(service)) {
        errorDetail =
          tr("Could not locate the `org.freedesktop.portal.Desktop` service");
        return PortalStatus::Unavailable;
    }

    QDBusInterface screenshotInterface(
      service,
      QStringLiteral("/org/freedesktop/portal/desktop"),
      QStringLiteral("org.freedesktop.portal.Screenshot"));

    // unique token
    QString token =
      QUuid::createUuid().toString().remove('-').remove('{').remove('}');

    // premake interface
    auto* request = new OrgFreedesktopPortalRequestInterface(
      service,
      "/org/freedesktop/portal/desktop/request/" +
        QDBusConnection::sessionBus().baseService().remove(':').replace('.',
                                                                        '_') +
        "/" + token,
      QDBusConnection::sessionBus(),
      this);

    QEventLoop loop;

    const auto onPortalResponse = [&res, &loop, this](uint status,
                                                      const QVariantMap& map) {
        if (status == 0) {
            // Parse this as URI to handle unicode properly
            QUrl uri = map.value("uri").toString();
            QString uriString = uri.toLocalFile();
            res = QPixmap(uriString);
            QFile imgFile(uriString);
            imgFile.remove();
        }
        loop.quit();
    };

    // prevent racy situations and listen before calling screenshot
    QMetaObject::Connection conn = QObject::connect(
      request, &org::freedesktop::portal::Request::Response, onPortalResponse);

    bool timedOut = false;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(15000); // 15 second timeout

    QObject::connect(&timeout, &QTimer::timeout, &loop, [&loop, &timedOut]() {
        timedOut = true;
        loop.quit();
    });
    timeout.start();

    // Build a non-empty parent_window handle. xdg-desktop-portal-gnome
    // (>= 46) rejects an empty string with "Failed to associate portal
    // window with parent window ''" and the Screenshot request fails.
    // On X11 we pass a real x11:<hex> handle from an offscreen QWidget.
    // On Wayland we fall back to an empty string inside a wayland: prefix
    // (xdg-desktop-portal-gnome treats unknown handles as no-parent and
    // proceeds, instead of rejecting outright).
    QString parentWindow;
    QWidget parentDummy;
    parentDummy.setAttribute(Qt::WA_DontShowOnScreen, true);
    parentDummy.resize(1, 1);
    parentDummy.show();
    if (QGuiApplication::platformName() == QLatin1String("wayland")) {
        parentWindow = QStringLiteral("wayland:");
    } else {
        parentWindow =
          QStringLiteral("x11:0x%1").arg(parentDummy.winId(), 0, 16);
    }

    // Async so the UI stays responsive. The timeout below only runs inside
    // loop.exec(), so a blocking call would wait on the portal without it.
    bool rejected = false;
    QString rejectionMessage;
    auto* watcher = new QDBusPendingCallWatcher(
      screenshotInterface.asyncCall(
        QStringLiteral("Screenshot"),
        parentWindow,
        QMap<QString, QVariant>({ { "handle_token", QVariant(token) },
                                  { "interactive", QVariant(false) } })),
      &loop);
    QObject::connect(watcher,
                     &QDBusPendingCallWatcher::finished,
                     &loop,
                     [&](QDBusPendingCallWatcher* call) {
                         if (call->isError()) {
                             // No backend provides
                             // org.freedesktop.portal.Screenshot (or the portal
                             // rejected the request outright); the Response
                             // signal will never arrive, so stop waiting
                             // instead of timing out.
                             rejected = true;
                             rejectionMessage = call->error().message();
                             loop.quit();
                         }
                         call->deleteLater();
                     });

    loop.exec();
    timeout.stop();
    QObject::disconnect(conn);
    if (rejected) {
        request->deleteLater();
        errorDetail =
          tr("The `org.freedesktop.portal.Screenshot` request failed: %1")
            .arg(rejectionMessage);
        return PortalStatus::Unavailable;
    }
    // Don't wait for the close reply; it would block the UI thread.
    request->Close();
    request->deleteLater();

    if (timedOut) {
        errorDetail =
          tr("The xdg-desktop-portal backend did not respond "
             "If you are on wayland make sure an xdg-desktop-portal backend "
             "for your desktop is "
             "installed and properly configured.\n \n"
             "If on X11 enable Legacy X11 method in the General Settings");
        return PortalStatus::Failed;
    }

    if (res.isNull()) {
        return PortalStatus::Failed;
    }

#ifdef FLAMESHOT_DEBUG_CAPTURE
    qDebug() << tr("FreeDesktop portal screenshot size: %1x%2, DPR: %3")
                  .arg(res.width())
                  .arg(res.height())
                  .arg(res.devicePixelRatio());
#endif
    return PortalStatus::Success;
#else
    Q_UNUSED(res)
    Q_UNUSED(errorDetail)
    return PortalStatus::Failed;
#endif
}

QPixmap ScreenGrabber::unixScreenshot(bool& ok)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QPixmap screenshot;

    if (!m_info.waylandDetected() && ConfigHandler().useX11LegacyScreenshot()) {
        screenshot = x11LegacyScreenshot();
        ok = !screenshot.isNull();
        if (!ok) {
            AbstractLogger::error() << tr("Unable to capture screen");
        }
        return screenshot;
    }

    QString portalError;
    const PortalStatus status = freeDesktopPortal(screenshot, portalError);
    ok = status == PortalStatus::Success;

    if (status == PortalStatus::Unavailable && !m_info.waylandDetected()) {
        // The portal cannot take screenshots here, which is common on X11
        // window managers such as i3 or XMonad, so take the native X11
        // grab instead.
        AbstractLogger::info(AbstractLogger::Stderr | AbstractLogger::LogFile)
          << tr("Screenshot portal unavailable, using direct X11 capture");
        screenshot = x11LegacyScreenshot();
        ok = !screenshot.isNull();
    }

    if (!ok) {
        if (!portalError.isEmpty()) {
            AbstractLogger::error() << portalError;
        }
        AbstractLogger::error() << tr("Unable to capture screen");
    }

    return screenshot;
#else
    ok = false;
    return QPixmap();
#endif
}

QPixmap ScreenGrabber::selectMonitorAndCrop(const QPixmap& fullScreenshot,
                                            bool& ok)
{
    ok = true;
#if defined(Q_OS_MACOS)
    // Avoid showing additional top-level monitor selection UI on macOS
    // Only screenshot the monitor where the tray activated the screenshot
    return cropToMonitor(fullScreenshot, 0);
#else

    // If there's only one monitor, skip selection
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.size() == 1) {
        return cropToMonitor(fullScreenshot, 0);
    }

    // Capture Active Monitor: auto-select monitor under cursor
    if (ConfigHandler().captureActiveMonitor()) {
        if (m_info.waylandDetected()) {
            AbstractLogger::error()
              << tr("Capture Active Monitor is not supported on Wayland due to "
                    "Wayland security model.");
            ok = false;
            return QPixmap();
        }

        QGuiAppCurrentScreen screenFinder;
        QScreen* cursorScreen = screenFinder.currentScreen();
        int monitorIndex = screens.indexOf(cursorScreen);
        if (monitorIndex >= 0) {
            m_selectedMonitor = monitorIndex;
            return cropToMonitor(fullScreenshot, monitorIndex);
        }
        // Fall through to manual selection if screen lookup fails
    }

    if (m_monitorSelectionActive) {
        AbstractLogger::error()
          << tr("Screenshot already in progress, please wait for the current "
                "screenshot to complete");
        ok = false;
        return QPixmap();
    }

    m_monitorSelectionActive = true;
    m_selectedMonitor = -1;
    m_userCancelled = false;
    QWidget* container = createMonitorPreviews(fullScreenshot);

    // Wait for user to select a monitor
    QEventLoop loop;
    m_monitorSelectionLoop = &loop;
    loop.exec();
    m_monitorSelectionLoop = nullptr;

    delete container;
    m_monitorPreviews.clear();
    m_highlightedMonitorPreview = -1;
    m_monitorSelectionActive = false;

    if (m_selectedMonitor >= 0) {
        return cropToMonitor(fullScreenshot, m_selectedMonitor);
    } else {
        ok = false;
        if (m_userCancelled) {
            AbstractLogger::info() << tr("Screenshot cancelled");
        }
        return fullScreenshot;
    }
#endif
}

QPixmap ScreenGrabber::grabEntireDesktop(bool& ok, int preSelectedMonitor)
{
    ok = true;
    int wid = 0;
    QPixmap screenshot;

#if defined(Q_OS_MACOS)
    QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
    if (!currentScreen) {
        AbstractLogger::error() << tr("Unable to get current screen");
        ok = false;
        return QPixmap();
    }
    m_selectedMonitor = QGuiApplication::screens().indexOf(currentScreen);
    if (m_selectedMonitor < 0) {
        AbstractLogger::error() << tr("Unable to get current screen");
        ok = false;
        return QPixmap();
    }
    const QRect geom = currentScreen->geometry();
    screenshot = currentScreen->grabWindow(
      wid, geom.x(), geom.y(), geom.width(), geom.height());
    screenshot.setDevicePixelRatio(currentScreen->devicePixelRatio());
    return screenshot;

#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    screenshot = unixScreenshot(ok);
    if (!ok) {
        return QPixmap();
    }
#elif defined(Q_OS_WIN)
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        ok = false;
        return QPixmap();
    }
    int targetMonitor = -1;
    if (preSelectedMonitor >= 0 && preSelectedMonitor < screens.size()) {
        targetMonitor = preSelectedMonitor;
    } else if (screens.size() == 1) {
        targetMonitor = 0;
    } else if (ConfigHandler().captureActiveMonitor()) {
        QScreen* cursorScreen = QGuiAppCurrentScreen().currentScreen();
        targetMonitor = screens.indexOf(cursorScreen);
        if (targetMonitor < 0) {
            targetMonitor = 0;
        }
    }
    if (targetMonitor >= 0) {
        QScreen* target = screens[targetMonitor];
        m_selectedMonitor = targetMonitor;
        screenshot = target->grabWindow(wid);
        ok = !screenshot.isNull();
        if (ok) {
            screenshot.setDevicePixelRatio(target->devicePixelRatio());
        }
        return screenshot;
    }
    screenshot = windowsScreenshot(wid);
#endif

    // If monitor was pre-selected skip UI and crop directly
    if (preSelectedMonitor >= 0) {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (preSelectedMonitor < screens.size()) {
            m_selectedMonitor = preSelectedMonitor;
            return cropToMonitor(screenshot, preSelectedMonitor);
        }
    }

    return selectMonitorAndCrop(screenshot, ok);
}

QPixmap ScreenGrabber::grabFullDesktop(bool& ok)
{
    ok = true;
    QPixmap screenshot;

#if defined(Q_OS_MACOS)
    // On macOS, composite all screens into a single pixmap.
    const QList<QScreen*> screens = QGuiApplication::screens();
    QRect totalGeom;
    for (QScreen* s : screens) {
        totalGeom = totalGeom.united(s->geometry());
    }
    qreal maxDpr = 1.0;
    for (QScreen* s : screens) {
        maxDpr = qMax(maxDpr, s->devicePixelRatio());
    }
    screenshot = QPixmap(qRound(totalGeom.width() * maxDpr),
                         qRound(totalGeom.height() * maxDpr));
    screenshot.setDevicePixelRatio(maxDpr);
    screenshot.fill(Qt::black);
    QPainter painter(&screenshot);
    for (QScreen* s : screens) {
        QRect geom = s->geometry();
        QPixmap p = s->grabWindow(0);
        QPoint offset = geom.topLeft() - totalGeom.topLeft();
        painter.drawPixmap(offset, p);
    }
    painter.end();
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    screenshot = unixScreenshot(ok);
#elif defined(Q_OS_WIN)
    screenshot = windowsScreenshot(0);
#endif

    return screenshot;
}

QRect ScreenGrabber::screenGeometry(QScreen* screen)
{
    QRect geometry = screen->geometry();
    if (m_info.waylandDetected()) {
        QPoint topLeft(0, 0);
        geometry.moveTo(geometry.topLeft() - topLeft);
    }
    return geometry;
}

QPixmap ScreenGrabber::grabScreen(QScreen* screen, bool& ok)
{
    QPixmap p;
    QRect geometry = screenGeometry(screen);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QList<QScreen*> screens = QGuiApplication::screens();
    int screenIndex = screens.indexOf(screen);

    p = grabEntireDesktop(ok, screenIndex);
#else
    ok = true;
    return screen->grabWindow(
      0, geometry.x(), geometry.y(), geometry.width(), geometry.height());
#endif
    return p;
}

QRect ScreenGrabber::desktopGeometry()
{
    QRect geometry;

    for (QScreen* const screen : QGuiApplication::screens()) {
        QRect scrRect = screen->geometry();
#if !defined(Q_OS_WIN)
        // https://doc.qt.io/qt-6/highdpi.html#device-independent-screen-geometry
        qreal dpr = screen->devicePixelRatio();
        scrRect.moveTo(QPointF(scrRect.x() / dpr, scrRect.y() / dpr).toPoint());
#endif
        geometry = geometry.united(scrRect);
    }
    return geometry;
}

QScreen* ScreenGrabber::getSelectedScreen() const
{
    const QList<QScreen*> screens = QGuiApplication::screens();

    if ((m_selectedMonitor < 0) || (m_selectedMonitor >= screens.size())) {
        return nullptr;
    }

    return screens[m_selectedMonitor];
}

QWidget* ScreenGrabber::createMonitorPreviews(const QPixmap& fullScreenshot)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    m_monitorPreviews.clear();
    m_highlightedMonitorPreview = -1;

#ifdef FLAMESHOT_DEBUG_CAPTURE
    qDebug() << tr("=== All Screen Information ===");
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* s = screens[i];
        qDebug() << tr("Screen %1: %2").arg(i).arg(s->name());
        qDebug() << tr("  Logical geometry: %1x%2+%3+%4")
                      .arg(s->geometry().width())
                      .arg(s->geometry().height())
                      .arg(s->geometry().x())
                      .arg(s->geometry().y());
        qDebug() << tr("  DPR: %1").arg(s->devicePixelRatio());
    }
#endif

    QWidget* monitorPreviews = new QWidget(
      nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    monitorPreviews->setAttribute(Qt::WA_TranslucentBackground);
    monitorPreviews->setStyleSheet(
      "QWidget { background-color: transparent; }");
    monitorPreviews->installEventFilter(this); // For ESC key handling
    monitorPreviews->setFocusPolicy(Qt::StrongFocus);

    QHBoxLayout* containerLayout = new QHBoxLayout(monitorPreviews);
    containerLayout->setSpacing(20);
    containerLayout->setContentsMargins(20, 20, 20, 20);

    // Build list of screen indices sorted by X position (left to right)
    QList<int> sortedIndices;
    for (int i = 0; i < screens.size(); ++i) {
        sortedIndices.append(i);
    }
    std::sort(
      sortedIndices.begin(), sortedIndices.end(), [&screens](int a, int b) {
          return screens[a]->geometry().x() < screens[b]->geometry().x();
      });

    for (int i : sortedIndices) {
        QScreen* screen = screens[i];

        QPixmap cropped = cropToMonitor(fullScreenshot, i);
        QPixmap thumbnail = cropped.scaled(
          400, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        thumbnail.setDevicePixelRatio(1.0);

        MonitorPreview* preview =
          new MonitorPreview(i, screen, thumbnail, monitorPreviews);

        connect(preview,
                &MonitorPreview::monitorSelected,
                this,
                [this](int index) { selectMonitor(index); });

        m_monitorPreviews.append(preview);
        containerLayout->addWidget(preview);
    }

    int initialPreviewIndex = 0;
    QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
    int currentMonitorIndex = screens.indexOf(currentScreen);
    int currentPreviewIndex = previewIndexForMonitor(currentMonitorIndex);
    if (currentPreviewIndex >= 0) {
        initialPreviewIndex = currentPreviewIndex;
    }
    setHighlightedMonitorPreview(initialPreviewIndex);

    monitorPreviews->setLayout(containerLayout);
    monitorPreviews->adjustSize();

    QScreen* primaryScreen = QGuiApplication::primaryScreen();
    QRect screenGeometry = primaryScreen->geometry();
    QPoint center = screenGeometry.center();
    monitorPreviews->move(center.x() - monitorPreviews->width() / 2,
                          center.y() - monitorPreviews->height() / 2);

    monitorPreviews->show();
    monitorPreviews->raise();
    monitorPreviews->activateWindow();
    monitorPreviews->setFocus(Qt::ActiveWindowFocusReason);
    return monitorPreviews;
}

void ScreenGrabber::cancelMonitorSelection()
{
    m_selectedMonitor = -1;
    m_userCancelled = true;
    if (m_monitorSelectionLoop) {
        m_monitorSelectionLoop->quit();
    }
}

void ScreenGrabber::moveHighlightedMonitorPreview(int offset)
{
    if (m_monitorPreviews.isEmpty()) {
        return;
    }

    int nextPreviewIndex = m_highlightedMonitorPreview;
    if (nextPreviewIndex < 0) {
        nextPreviewIndex = 0;
    } else {
        nextPreviewIndex += offset;
    }

    setHighlightedMonitorPreview(nextPreviewIndex);
}

int ScreenGrabber::previewIndexForMonitor(int monitorIndex) const
{
    for (int i = 0; i < m_monitorPreviews.size(); ++i) {
        if (m_monitorPreviews[i]->monitorIndex() == monitorIndex) {
            return i;
        }
    }

    return -1;
}

void ScreenGrabber::selectHighlightedMonitorPreview()
{
    if (m_highlightedMonitorPreview < 0 ||
        m_highlightedMonitorPreview >= m_monitorPreviews.size()) {
        return;
    }

    selectMonitor(
      m_monitorPreviews[m_highlightedMonitorPreview]->monitorIndex());
}

void ScreenGrabber::selectMonitor(int monitorIndex)
{
    m_selectedMonitor = monitorIndex;
    if (m_monitorSelectionLoop) {
        m_monitorSelectionLoop->quit();
    }
}

void ScreenGrabber::setHighlightedMonitorPreview(int previewIndex)
{
    if (m_monitorPreviews.isEmpty()) {
        m_highlightedMonitorPreview = -1;
        return;
    }

    const int previewCount = m_monitorPreviews.size();
    int normalizedIndex = previewIndex % previewCount;
    if (normalizedIndex < 0) {
        normalizedIndex += previewCount;
    }

    for (int i = 0; i < previewCount; ++i) {
        m_monitorPreviews[i]->setSelected(i == normalizedIndex);
    }
    m_highlightedMonitorPreview = normalizedIndex;
}

bool ScreenGrabber::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Close) {
        cancelMonitorSelection();
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
            case Qt::Key_Escape:
                cancelMonitorSelection();
                return true;
            case Qt::Key_Left:
            case Qt::Key_Up:
            case Qt::Key_Backtab:
                moveHighlightedMonitorPreview(-1);
                return true;
            case Qt::Key_Right:
            case Qt::Key_Down:
            case Qt::Key_Tab:
                moveHighlightedMonitorPreview(1);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space:
                selectHighlightedMonitorPreview();
                return true;
            default:
                if (keyEvent->key() >= Qt::Key_1 &&
                    keyEvent->key() <= Qt::Key_9) {
                    int monitorIndex = keyEvent->key() - Qt::Key_1;
                    if (previewIndexForMonitor(monitorIndex) >= 0) {
                        selectMonitor(monitorIndex);
                    }
                    return true;
                }
        }
    }
    return QObject::eventFilter(obj, event);
}

QPixmap ScreenGrabber::cropToMonitor(const QPixmap& fullScreenshot,
                                     int monitorIndex)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (monitorIndex >= screens.size()) {
        return fullScreenshot;
    }

    QScreen* targetScreen = screens[monitorIndex];
    QRect targetGeometry = targetScreen->geometry();
    qreal targetDpr = targetScreen->devicePixelRatio();

    // Calculate total logical dimensions and minimum coordinates
    int minX = INT_MAX, minY = INT_MAX;
    int maxX = INT_MIN, maxY = INT_MIN;

    for (QScreen* screen : screens) {
        QRect geo = screen->geometry();
        minX = qMin(minX, geo.x());
        minY = qMin(minY, geo.y());
        maxX = qMax(maxX, geo.x() + geo.width());
        maxY = qMax(maxY, geo.y() + geo.height());
    }

    int totalLogicalWidth = maxX - minX;
    int totalLogicalHeight = maxY - minY;

#ifdef FLAMESHOT_DEBUG_CAPTURE
    qDebug() << tr("Total logical dimensions: %1x%2 (min: %3,%4)")
                  .arg(totalLogicalWidth)
                  .arg(totalLogicalHeight)
                  .arg(minX)
                  .arg(minY);
    qDebug() << tr("Screenshot dimensions: %1x%2")
                  .arg(fullScreenshot.width())
                  .arg(fullScreenshot.height());
#endif

    int cropX, cropY, cropWidth, cropHeight;

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // Linux (both X11 and Wayland via freedesktop portal):
    // Use logical coordinate-based cropping since portal returns full
    // desktop
    qreal screenshotScaleX = (qreal)fullScreenshot.width() / totalLogicalWidth;
    qreal screenshotScaleY =
      (qreal)fullScreenshot.height() / totalLogicalHeight;

#ifdef FLAMESHOT_DEBUG_CAPTURE
    qDebug() << tr("Screenshot scale factors: X=%1 Y=%2")
                  .arg(screenshotScaleX)
                  .arg(screenshotScaleY);
#endif

    cropX = qRound((targetGeometry.x() - minX) * screenshotScaleX);
    cropY = qRound((targetGeometry.y() - minY) * screenshotScaleY);
    cropWidth = qRound(targetGeometry.width() * screenshotScaleX);
    cropHeight = qRound(targetGeometry.height() * screenshotScaleY);
#else
    // Windows: Calculate physical pixel positions for mixed DPI using shared
    // multi-monitor layout math (handles stacked monitors without double-count)
    QList<QRect> logicalGeoms;
    QList<qreal> dprs;
    logicalGeoms.reserve(screens.size());
    dprs.reserve(screens.size());
    for (QScreen* screen : screens) {
        logicalGeoms.append(screen->geometry());
        dprs.append(screen->devicePixelRatio());
    }
    const QList<QRect> physRects =
      computePhysicalScreenRects(logicalGeoms, dprs);
    const QRect targetPhys = physRects[monitorIndex];
    cropX = targetPhys.x();
    cropY = targetPhys.y();
    cropWidth = targetPhys.width();
    cropHeight = targetPhys.height();

#ifdef FLAMESHOT_DEBUG_CAPTURE
    qDebug() << tr("Calculated crop position for mixed DPI: X=%1 Y=%2")
                  .arg(cropX)
                  .arg(cropY);
#endif
#endif

    QRect cropRect(cropX, cropY, cropWidth, cropHeight);

#ifdef FLAMESHOT_DEBUG_CAPTURE
    qDebug() << tr("Screen %1: %2").arg(monitorIndex).arg(targetScreen->name());
    qDebug() << tr("  Logical geometry: %1x%2+%3+%4 DPR: %5")
                  .arg(targetGeometry.width())
                  .arg(targetGeometry.height())
                  .arg(targetGeometry.x())
                  .arg(targetGeometry.y())
                  .arg(targetDpr);
    qDebug() << tr("  Crop rect in screenshot: %1x%2+%3+%4")
                  .arg(cropRect.width())
                  .arg(cropRect.height())
                  .arg(cropRect.x())
                  .arg(cropRect.y());
#endif

    // Ensure crop rect is within bounds
    cropRect = cropRect.intersected(
      QRect(0, 0, fullScreenshot.width(), fullScreenshot.height()));

    if (cropRect.isEmpty()) {
        AbstractLogger::warning()
          << tr("Crop rect is empty, returning full screenshot");
        return fullScreenshot;
    }

    QPixmap cropped = fullScreenshot.copy(cropRect);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // Linux: May need rescaling if scale factors don't match
    if (qAbs(screenshotScaleX - targetDpr) > 0.01) {
        int targetPhysicalWidth = qRound(targetGeometry.width() * targetDpr);
        int targetPhysicalHeight = qRound(targetGeometry.height() * targetDpr);
        cropped = cropped.scaled(targetPhysicalWidth,
                                 targetPhysicalHeight,
                                 Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);
#ifdef FLAMESHOT_DEBUG_CAPTURE
        qDebug() << tr("Scaling screenshot to: %1 %2")
                      .arg(targetPhysicalWidth)
                      .arg(targetPhysicalHeight);
#endif
    }
#endif
    // Cropped region should be at target monitor's native DPR
    cropped.setDevicePixelRatio(targetDpr);

    return cropped;
}

QList<QRect> ScreenGrabber::computePhysicalScreenRects(
  const QList<QRect>& logicalGeoms,
  const QList<qreal>& dprs)
{
    const int n = logicalGeoms.size();
    QList<QRect> result(n);
    if (n == 0 || dprs.size() != n) {
        return result;
    }

    QList<int> physW(n), physH(n), physX(n, 0), physY(n, 0);
    for (int i = 0; i < n; ++i) {
        physW[i] = qRound(logicalGeoms[i].width() * dprs[i]);
        physH[i] = qRound(logicalGeoms[i].height() * dprs[i]);
    }

    // Compute physical X in left-to-right topological order
    QList<int> orderX(n);
    std::iota(orderX.begin(), orderX.end(), 0);
    std::sort(orderX.begin(), orderX.end(), [&](int a, int b) {
        if (logicalGeoms[a].x() != logicalGeoms[b].x()) {
            return logicalGeoms[a].x() < logicalGeoms[b].x();
        }
        return logicalGeoms[a].y() < logicalGeoms[b].y();
    });

    for (int idx = 0; idx < n; ++idx) {
        const int i = orderX[idx];
        const QRect& gi = logicalGeoms[i];
        bool hasOverlapLeft = false;
        bool hasAnyLeft = false;
        int maxOverlapRight = 0;
        int maxAnyRight = 0;
        int partialX = -1;

        for (int p = 0; p < idx; ++p) {
            const int j = orderX[p];
            const QRect& gj = logicalGeoms[j];
            const bool overlapsY =
              gj.y() < gi.y() + gi.height() && gj.y() + gj.height() > gi.y();
            if (gj.x() + gj.width() <= gi.x()) {
                const int right = physX[j] + physW[j];
                if (overlapsY) {
                    maxOverlapRight =
                      hasOverlapLeft ? qMax(maxOverlapRight, right) : right;
                    hasOverlapLeft = true;
                }
                maxAnyRight = hasAnyLeft ? qMax(maxAnyRight, right) : right;
                hasAnyLeft = true;
            } else if (overlapsY && gj.x() < gi.x()) {
                partialX = qMax(partialX,
                                physX[j] + qRound((gi.x() - gj.x()) * dprs[j]));
            }
        }

        if (partialX >= 0) {
            physX[i] = partialX;
        } else if (hasOverlapLeft) {
            physX[i] = maxOverlapRight;
        } else if (hasAnyLeft) {
            physX[i] = maxAnyRight;
        } else {
            physX[i] = 0;
        }
    }

    // Compute physical Y in top-to-bottom topological order
    QList<int> orderY(n);
    std::iota(orderY.begin(), orderY.end(), 0);
    std::sort(orderY.begin(), orderY.end(), [&](int a, int b) {
        if (logicalGeoms[a].y() != logicalGeoms[b].y()) {
            return logicalGeoms[a].y() < logicalGeoms[b].y();
        }
        return logicalGeoms[a].x() < logicalGeoms[b].x();
    });

    for (int idx = 0; idx < n; ++idx) {
        const int i = orderY[idx];
        const QRect& gi = logicalGeoms[i];
        bool hasOverlapAbove = false;
        bool hasAnyAbove = false;
        int maxOverlapBottom = 0;
        int maxAnyBottom = 0;
        int partialY = -1;

        for (int p = 0; p < idx; ++p) {
            const int j = orderY[p];
            const QRect& gj = logicalGeoms[j];
            const bool overlapsX =
              gj.x() < gi.x() + gi.width() && gj.x() + gj.width() > gi.x();
            if (gj.y() + gj.height() <= gi.y()) {
                const int bottom = physY[j] + physH[j];
                if (overlapsX) {
                    maxOverlapBottom =
                      hasOverlapAbove ? qMax(maxOverlapBottom, bottom) : bottom;
                    hasOverlapAbove = true;
                }
                maxAnyBottom =
                  hasAnyAbove ? qMax(maxAnyBottom, bottom) : bottom;
                hasAnyAbove = true;
            } else if (overlapsX && gj.y() < gi.y()) {
                partialY = qMax(partialY,
                                physY[j] + qRound((gi.y() - gj.y()) * dprs[j]));
            }
        }

        if (partialY >= 0) {
            physY[i] = partialY;
        } else if (hasOverlapAbove) {
            physY[i] = maxOverlapBottom;
        } else if (hasAnyAbove) {
            physY[i] = maxAnyBottom;
        } else {
            physY[i] = 0;
        }
    }

    for (int i = 0; i < n; ++i) {
        result[i] = QRect(physX[i], physY[i], physW[i], physH[i]);
    }
    return result;
}

QPixmap ScreenGrabber::windowsScreenshot(int wid)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return QPixmap();
    }

    QList<QRect> logicalGeoms;
    QList<qreal> dprs;
    QList<QPixmap> screenPixmaps;
    logicalGeoms.reserve(screens.size());
    dprs.reserve(screens.size());
    screenPixmaps.reserve(screens.size());

    for (QScreen* screen : screens) {
        logicalGeoms.append(screen->geometry());
        dprs.append(screen->devicePixelRatio());
        QPixmap screenPixmap = screen->grabWindow(wid);
        screenPixmap.setDevicePixelRatio(1.0);
        screenPixmaps.append(screenPixmap);
    }

    const QList<QRect> physRects =
      computePhysicalScreenRects(logicalGeoms, dprs);

    int canvasWidth = 0;
    int canvasHeight = 0;
    for (int i = 0; i < screens.size(); ++i) {
        canvasWidth =
          qMax(canvasWidth, physRects[i].x() + screenPixmaps[i].width());
        canvasHeight =
          qMax(canvasHeight, physRects[i].y() + screenPixmaps[i].height());
    }

    // Composite all screens onto canvas
    QPixmap desktop(canvasWidth, canvasHeight);
    desktop.fill(Qt::black);

    QPainter painter(&desktop);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    for (int i = 0; i < screens.size(); ++i) {
        painter.drawPixmap(physRects[i].topLeft(), screenPixmaps[i]);
    }
    painter.end();

    return desktop;
}

QPixmap ScreenGrabber::x11LegacyScreenshot()
{
    const QList<QScreen*> screens = QGuiApplication::screens();

    if (screens.isEmpty()) {
        return QPixmap();
    }

    if (screens.size() == 1) {
        QScreen* screen = screens.first();
        QPixmap p = screen->grabWindow(0);
        p.setDevicePixelRatio(screen->devicePixelRatio());
        return p;
    }

    // Composite all screens using logical geometry.
    // On i3 (tested) DPR is uniform so we don't need the per-screen
    // physical pixel math that the Windows backend does. Not sure if this is
    // true for other DE's like xmonad.
    QRect totalGeom;
    for (QScreen* s : screens) {
        totalGeom = totalGeom.united(s->geometry());
    }

    qreal dpr = screens.first()->devicePixelRatio();
    QPixmap desktop(qRound(totalGeom.width() * dpr),
                    qRound(totalGeom.height() * dpr));
    desktop.setDevicePixelRatio(dpr);
    desktop.fill(Qt::black);

    QPainter painter(&desktop);
    for (QScreen* s : screens) {
        QPixmap p = s->grabWindow(0);
        QPoint offset = s->geometry().topLeft() - totalGeom.topLeft();
        painter.drawPixmap(offset, p);
    }
    painter.end();

    return desktop;
}
