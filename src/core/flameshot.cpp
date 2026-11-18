// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "flameshot.h"
#include "core/flameshotdaemon.h"
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
#include "qhotkey.h"
#endif

#if defined(Q_OS_MACOS)
#include <QWindow>
#include <objc/message.h>

namespace {

constexpr long NSApplicationActivationPolicyRegular = 0;
constexpr long NSApplicationActivationPolicyAccessory = 1;

void setActivationPolicy(long policy)
{
    auto sharedApp = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
    auto setPolicy = reinterpret_cast<void (*)(id, SEL, long)>(objc_msgSend);
    id nsApp = sharedApp(reinterpret_cast<id>(objc_getClass("NSApplication")),
                         sel_registerName("sharedApplication"));
    setPolicy(nsApp, sel_registerName("setActivationPolicy:"), policy);
}

void setActivationPolicyRegular()
{
    setActivationPolicy(NSApplicationActivationPolicyRegular);
}

void setActivationPolicyAccessory()
{
    setActivationPolicy(NSApplicationActivationPolicyAccessory);
}

constexpr const char* visibleInDockProperty = "_visibleInDock";

// NSMainMenuWindowLevel (24) + 2: above the menu bar, its status items and
// the Dock, but below pop-up menus so Qt menus remain visible.
constexpr long captureWindowLevel = 26;

// Qt::WindowStaysOnTopHint maps to NSFloatingWindowLevel, which is below the
// menu bar and the Dock, so they would cover parts of the capture overlay.
void raiseAboveMenuBar(QWidget* widget)
{
    auto getWindow = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
    auto setLevel = reinterpret_cast<void (*)(id, SEL, long)>(objc_msgSend);
    auto nsView = reinterpret_cast<id>(widget->winId());
    id nsWindow = getWindow(nsView, sel_registerName("window"));
    if (nsWindow != nullptr) {
        setLevel(nsWindow, sel_registerName("setLevel:"), captureWindowLevel);
    }
}

} // namespace

#include <CoreGraphics/CoreGraphics.h>
#endif

#include "config/cacheutils.h"
#include "config/configresolver.h"
#include "config/configwindow.h"
#include "core/qguiappcurrentscreen.h"
#include "utils/abstractlogger.h"
#include "utils/confighandler.h"
#include "utils/geometryutils.h"
#include "utils/screengrabber.h"
#include "utils/screenshotsaver.h"
#include "widgets/capture/capturewidget.h"
#include "widgets/capturelauncher.h"
#include "widgets/infowindow.h"

#ifdef ENABLE_IMGUR
#include "tools/imgupload/imguploadermanager.h"
#include "tools/imgupload/storages/imguploaderbase.h"
#include "widgets/imguploaddialog.h"
#include "widgets/uploadhistory.h"
#endif

#include <QApplication>
#include <QBuffer>
#include <QDebug>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

namespace {

bool coversWholeScreen(const QRect& region)
{
    for (const QScreen* screen : QGuiApplication::screens()) {
        // getLastRegion() stores logical virtual-desktop coordinates.
        if (region.contains(screen->geometry().adjusted(1, 1, -1, -1))) {
            return true;
        }
    }
    return false;
}

QRect regionInDesktopPixmap(const QRect& region, const QPixmap& pixmap)
{
    QList<QRect> logicalScreens;
    QList<qreal> screenDprs;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        logicalScreens.append(screen->geometry());
        screenDprs.append(screen->devicePixelRatio());
    }

#if defined(Q_OS_WIN)
    // windowsScreenshot() composes each monitor at its physical size.
    return GeometryUtils::logicalToPhysicalDesktop(
      region,
      logicalScreens,
      ScreenGrabber::computePhysicalScreenRects(logicalScreens, screenDprs));
#else
    // macOS and the Unix paths composite into one uniform-DPR pixmap.
    QRect desktopLogical;
    for (const QRect& geometry : logicalScreens) {
        desktopLogical = desktopLogical.united(geometry);
    }
    const QRect local =
      region.translated(-desktopLogical.topLeft())
        .intersected(
          QRect(0, 0, desktopLogical.width(), desktopLogical.height()));
    if (local.isEmpty()) {
        return {};
    }
    return GeometryUtils::logicalToPixmap(
             local,
             GeometryUtils::effectiveDesktopPixelRatio(pixmap, desktopLogical))
      .translated(desktopLogical.topLeft());
#endif
}

} // namespace

Flameshot::Flameshot()
  : m_haveExternalWidget(false)
  , m_captureWindow(nullptr)
#if (defined(Q_OS_MACOS) || defined(Q_OS_WIN))
  , m_HotkeyScreenshotCapture(nullptr)
  , m_HotkeyScreenshotCaptureInstant(nullptr)
  , m_HotkeyInstantDirect(nullptr)
#endif
#if (defined(Q_OS_MACOS) && ENABLE_IMGUR)
  , m_HotkeyScreenshotHistory(nullptr)
#endif
{
    QString StyleSheet = CaptureButton::globalStyleSheet();
    qApp->setStyleSheet(StyleSheet);

#if defined(Q_OS_MACOS)
    // Request Screen Recording permission via the proper CoreGraphics API
    if (!CGPreflightScreenCaptureAccess()) {
        CGRequestScreenCaptureAccess();
    }
#endif
#if (defined(Q_OS_MACOS) || defined(Q_OS_WIN))
    // Set global shortcuts for MacOS or Windows
    m_HotkeyScreenshotCapture = new QHotkey(
      QKeySequence(ConfigHandler().shortcut("TAKE_SCREENSHOT")), true, this);
    QObject::connect(m_HotkeyScreenshotCapture,
                     &QHotkey::activated,
                     qApp,
                     [this]() { gui(); });
    const QKeySequence instantSeq(
      ConfigHandler().shortcut("TAKE_SCREENSHOT_INSTANT"));
    if (!instantSeq.isEmpty()) {
        m_HotkeyScreenshotCaptureInstant = new QHotkey(instantSeq, true, this);
        QObject::connect(m_HotkeyScreenshotCaptureInstant,
                         &QHotkey::activated,
                         qApp,
                         [this]() {
                             CaptureRequest req(CaptureRequest::GRAPHICAL_MODE);
                             req.addTask(CaptureRequest::COPY);
                             req.addTask(CaptureRequest::ACCEPT_ON_SELECT);
                             gui(req);
                         });
    }

    // No-overlay capture: grab the full desktop (or the last region) directly
    // and apply the export tasks without showing the editor.
    const QKeySequence directSeq(
      ConfigHandler().shortcut("TAKE_SCREENSHOT_NO_OVERLAY"));
    if (!directSeq.isEmpty()) {
        m_HotkeyInstantDirect = new QHotkey(directSeq, true, this);
        QObject::connect(
          m_HotkeyInstantDirect, &QHotkey::activated, qApp, [this]() {
              CaptureRequest req(CaptureRequest::FULLSCREEN_MODE);
              req.addTask(CaptureRequest::COPY);
              if (ConfigHandler().instantCaptureSave()) {
                  req.addSaveTask();
              }
              instantCapture(req);
          });
    }
#endif
#if (defined(Q_OS_MACOS) && ENABLE_IMGUR)
    m_HotkeyScreenshotHistory = new QHotkey(
      QKeySequence(ConfigHandler().shortcut("SCREENSHOT_HISTORY")), true, this);
    QObject::connect(m_HotkeyScreenshotHistory,
                     &QHotkey::activated,
                     qApp,
                     [this]() { history(); });
#endif
}

Flameshot* Flameshot::instance()
{
    static Flameshot c;
    return &c;
}

CaptureWidget* Flameshot::gui(const CaptureRequest& req)
{
    if (!resolveAnyConfigErrors()) {
        return nullptr;
    }

    CaptureRequest request = req;
    if (request.captureMode() == CaptureRequest::GRAPHICAL_MODE &&
        request.initialSelection().isNull() &&
        ConfigHandler().saveLastRegion()) {
        const QRect lastRegion = getLastRegion();
        if (!ConfigHandler().ignoreWholeScreenLastRegion() ||
            !coversWholeScreen(lastRegion)) {
            request.setInitialSelection(lastRegion);
        }
    }

#if defined(Q_OS_MACOS)
    // This is required on MacOS because of Mission Control. If you'll switch to
    // another Desktop you cannot take a new screenshot from the tray, you have
    // to switch back to the Flameshot Desktop manually. It is not obvious and a
    // large number of users are confused and report a bug.
    if (m_captureWindow != nullptr) {
        m_captureWindow->close();
        delete m_captureWindow;
        m_captureWindow = nullptr;
    }
#endif

    if (nullptr == m_captureWindow) {
        if (m_guiOpening) {
            emit captureFailed();
            return nullptr;
        }
        m_guiOpening = true;
        // Close any lingering modal widgets without blocking the UI thread
        QWidget* modalWidget = nullptr;
        for (int attempt = 0; attempt < 25; ++attempt) {
            modalWidget = qApp->activeModalWidget();
            if (nullptr == modalWidget) {
                break;
            }
            modalWidget->close();
            modalWidget->deleteLater();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents,
                                            20);
        }
        if (qApp->activeModalWidget() != nullptr) {
            m_guiOpening = false;
            QMessageBox::warning(
              nullptr, tr("Error"), tr("Unable to close active modal widgets"));
            return nullptr;
        }

        m_captureWindow = new CaptureWidget(request);
        m_guiOpening = false;

#ifdef Q_OS_WIN
        m_captureWindow->show();
        m_captureWindow->activateCaptureWindow();
#elif defined(Q_OS_MACOS)
        if (ConfigHandler().useNativeFullscreen()) {
            m_captureWindow->showFullScreen();
        } else {
            m_captureWindow->show();
            raiseAboveMenuBar(m_captureWindow);
        }
        m_captureWindow->activateWindow();
        m_captureWindow->raise();
#else
        m_captureWindow->showFullScreen();
//        m_captureWindow->show(); // For CaptureWidget Debugging under Linux
#endif
#if !defined(DISABLE_UPDATE_CHECKER)
        if (FlameshotDaemon::instance()) {
            FlameshotDaemon::instance()->showUpdateNotificationIfAvailable(
              m_captureWindow);
        }
#endif
        return m_captureWindow;
    } else {
        emit captureFailed();
        return nullptr;
    }
}

void Flameshot::screen(CaptureRequest req, const int screenNumber)
{
    if (!resolveAnyConfigErrors()) {
        return;
    }

    bool ok = false;
    QPixmap p;
    QRect geometry;

    if (screenNumber < 0) {
        ScreenGrabber grabber;
        p = grabber.grabEntireDesktop(ok);
        if (ok) {
            QScreen* selectedScreen = grabber.getSelectedScreen();
            if (selectedScreen) {
                geometry = ScreenGrabber().screenGeometry(selectedScreen);
            } else {
                ok = false;
            }
        }
    } else if (screenNumber >= qApp->screens().count()) {
        AbstractLogger() << QObject::tr(
          "Requested screen exceeds screen count");
        ok = false;
    } else {
        // Specific screen number provided - use grabScreen to bypass selector
        QScreen* screen = qApp->screens()[screenNumber];
        p = ScreenGrabber().grabScreen(screen, ok);
        if (ok) {
            geometry = ScreenGrabber().screenGeometry(screen);
        }
    }

    if (ok) {
        QRect selection = geometry;
        const QRect requested = req.initialSelection();
        if (!requested.isNull()) {
            const GeometryUtils::RegionMapping mapping =
              GeometryUtils::mapRegionToScreen(requested, geometry, p);
            if (mapping.valid) {
                selection = mapping.globalLogical;
                p = p.copy(mapping.localPixmap);
            } else {
                ok = false;
            }
        }
        if (ok) {
            if (req.tasks() & CaptureRequest::PIN) {
                req.addPinTask(selection);
            }
            exportCapture(p, selection, req);
            return;
        }
    }
    emit captureFailed();
}

void Flameshot::full(const CaptureRequest& req)
{
    if (!resolveAnyConfigErrors()) {
        return;
    }

    bool ok = true;
    QPixmap p(ScreenGrabber().grabFullDesktop(ok));
    if (ok) {
        QRect selection; // `flameshot full` does not support region selection
        exportCapture(p, selection, req);
    } else {
        emit captureFailed();
    }
}

void Flameshot::instantCapture(const CaptureRequest& req)
{
    if (!resolveAnyConfigErrors()) {
        return;
    }

    bool ok = false;
    QPixmap capture = ScreenGrabber().grabFullDesktop(ok);
    if (!ok || capture.isNull()) {
        emit captureFailed();
        return;
    }

    QRect selection;
    if (ConfigHandler().instantCaptureUseLastRegion()) {
        const QRect lastRegion = getLastRegion();
        if (!lastRegion.isNull() &&
            (!ConfigHandler().ignoreWholeScreenLastRegion() ||
             !coversWholeScreen(lastRegion))) {
            const QRect crop = regionInDesktopPixmap(lastRegion, capture);
            if (crop.isEmpty()) {
                emit captureFailed();
                return;
            }
            capture = capture.copy(crop);
            selection = lastRegion;
        }
    }

    exportCapture(capture, selection, req);
}

void Flameshot::launcher()
{
    if (!resolveAnyConfigErrors()) {
        return;
    }

    if (m_launcherWindow == nullptr) {
        m_launcherWindow = new CaptureLauncher();
    }
    m_launcherWindow->show();
#if defined(Q_OS_MACOS)
    showDockIcon(m_launcherWindow);
#endif
}

void Flameshot::config()
{
    if (!resolveAnyConfigErrors()) {
        return;
    }

    if (m_configWindow == nullptr) {
        m_configWindow = new ConfigWindow();
        m_configWindow->show();
        // Call show() first, otherwise the correct geometry cannot be fetched
        // for fitting and centering the window on the screen
        QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
        m_configWindow->fitToScreen(currentScreen->availableGeometry());
        QRect position = m_configWindow->frameGeometry();
        position.moveCenter(currentScreen->availableGeometry().center());
        m_configWindow->move(position.topLeft());
#if defined(Q_OS_MACOS)
        showDockIcon(m_configWindow);
#endif
    }
}

void Flameshot::info()
{
    if (m_infoWindow == nullptr) {
        m_infoWindow = new InfoWindow();
#if defined(Q_OS_MACOS)
        showDockIcon(m_infoWindow);
#endif
    }
}

#ifdef ENABLE_IMGUR
void Flameshot::history()
{
    static UploadHistory* historyWidget = nullptr;
    if (historyWidget == nullptr) {
        historyWidget = new UploadHistory;
        historyWidget->loadHistory();
        connect(historyWidget, &QObject::destroyed, this, []() {
            historyWidget = nullptr;
        });
    }

    historyWidget->show();
    // Call show() first, otherwise the correct geometry cannot be fetched
    // for centering the window on the screen
    QRect position = historyWidget->frameGeometry();
    QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
    position.moveCenter(currentScreen->availableGeometry().center());
    historyWidget->move(position.topLeft());

#if defined(Q_OS_MACOS)
    showDockIcon(historyWidget);
#endif
}
#endif

#if defined(Q_OS_MACOS)
void Flameshot::onWindowVisibilityChanged(QWindow::Visibility newVisibility)
{
    auto* qw = qobject_cast<QWindow*>(sender());
    if (!qw) {
        return;
    }

    if (newVisibility == QWindow::Hidden) {
        qw->setProperty(visibleInDockProperty, false);
        --m_dockIconVisibleCount;
        if (m_dockIconVisibleCount == 0) {
            setActivationPolicyAccessory();
        }
    } else {
        bool windowTrackedInDock = qw->property(visibleInDockProperty).toBool();
        if (!windowTrackedInDock) {
            qw->setProperty(visibleInDockProperty, true);
            ++m_dockIconVisibleCount;
            setActivationPolicyRegular();
        }
    }
}

void Flameshot::showDockIcon(QWidget* w)
{
    QWindow* qw = w->windowHandle();
    if (!qw) {
        return;
    }

    connect(qw,
            &QWindow::visibilityChanged,
            this,
            &Flameshot::onWindowVisibilityChanged);
}
#endif

void Flameshot::openSavePath()
{
    QString savePath = ConfigHandler().savePath();
    if (!savePath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    }
}

QVersionNumber Flameshot::getVersion()
{
    return QVersionNumber::fromString(
      QStringLiteral(APP_VERSION).replace("v", ""));
}

void Flameshot::setOrigin(Origin origin)
{
    m_origin = origin;
}

Flameshot::Origin Flameshot::origin()
{
    return m_origin;
}

/**
 * @brief Prompt the user to resolve config errors if necessary.
 * @return Whether errors were resolved.
 */
bool Flameshot::resolveAnyConfigErrors()
{
    bool resolved = true;
    if (ConfigHandler::hasUnresolvedConfigErrors()) {
        auto* resolver = new ConfigResolver();
        QObject::connect(
          resolver, &ConfigResolver::rejected, [resolver, &resolved]() {
              resolved = false;
              resolver->deleteLater();
              if (origin() == CLI) {
                  exit(1);
              }
          });
        QObject::connect(
          resolver, &ConfigResolver::accepted, [resolver, &resolved]() {
              resolved = true;
              resolver->close();
              resolver->deleteLater();
              // Ensure that the dialog is closed before starting capture
              qApp->processEvents();
          });
        resolver->exec();
        qApp->processEvents();
    }
    return resolved;
}

void Flameshot::requestCapture(const CaptureRequest& request)
{
    if (!resolveAnyConfigErrors()) {
        return;
    }

    switch (request.captureMode()) {
        case CaptureRequest::FULLSCREEN_MODE:
            QTimer::singleShot(request.delay(),
                               [this, request] { full(request); });
            break;
        case CaptureRequest::SCREEN_MODE: {
            int&& number = request.data().toInt();
            QTimer::singleShot(request.delay(), [this, request, number]() {
                screen(request, number);
            });
            break;
        }
        case CaptureRequest::GRAPHICAL_MODE: {
            QTimer::singleShot(
              request.delay(), this, [this, request]() { gui(request); });
            break;
        }
        default:
            emit captureFailed();
            break;
    }
}

void Flameshot::exportCapture(const QPixmap& capture,
                              const QRect& selection,
                              const CaptureRequest& req)
{
    using CR = CaptureRequest;
    int tasks = req.tasks(), mode = req.captureMode();
    QString path = req.path();

    // The help text promises geometry output is suppressed when raw is set.
    if ((tasks & CR::PRINT_GEOMETRY) && !(tasks & CR::PRINT_RAW)) {
        QTextStream(stdout)
          << selection.width() << "x" << selection.height() << "+"
          << selection.x() << "+" << selection.y() << "\n";
    }

    if (tasks & CR::PRINT_RAW) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        capture.save(&buffer, "PNG");
        if (QFile file; file.open(stdout, QIODevice::WriteOnly)) {
            file.write(byteArray);
            file.close();
        }
    }

    if (tasks & CR::SAVE) {
        if (req.path().isEmpty()) {
            saveToFilesystemGUI(capture);
        } else {
            saveToFilesystem(capture, path);
        }
    }

    if (tasks & CR::COPY) {
        FlameshotDaemon::copyToClipboard(capture);
    }

    if (tasks & CR::PIN) {
        // The screen path stores the actual cropped selection when a pin is
        // requested. Fall back to `selection` for modes that do not set one.
        QRect pinGeometry = req.pinWindowGeometry();
        if (pinGeometry.isNull()) {
            pinGeometry = selection;
        }
        FlameshotDaemon::createPin(capture, pinGeometry);
        if (mode == CR::SCREEN_MODE || mode == CR::FULLSCREEN_MODE) {
            AbstractLogger::info()
              << QObject::tr("Full screen screenshot pinned to screen");
        }
    }

#ifdef ENABLE_IMGUR
    if (tasks & CR::UPLOAD) {
        if (!ConfigHandler().uploadWithoutConfirmation()) {
            auto* dialog = new ImgUploadDialog();
            if (dialog->exec() == QDialog::Rejected) {
                return;
            }
        }

        ImgUploaderBase* widget = ImgUploaderManager().uploader(capture);
        widget->show();
        widget->activateWindow();
        // NOTE: lambda can't capture 'this' because it might be destroyed later
        CR::ExportTask uploadTasks = static_cast<CR::ExportTask>(tasks);
        QObject::connect(
          widget, &ImgUploaderBase::uploadOk, [=, this](const QUrl& url) {
              if (ConfigHandler().copyURLAfterUpload()) {
                  if (!(uploadTasks & CR::COPY)) {
                      FlameshotDaemon::copyToClipboard(
                        url.toString(), tr("URL copied to clipboard."));
                  }
                  widget->showPostUploadDialog();
              }
          });
    }
#endif

    if (!(tasks & CR::UPLOAD)) {
        emit captureTaken(capture);
    }
}

void Flameshot::setExternalWidget(bool b)
{
    m_haveExternalWidget = b;
}
bool Flameshot::haveExternalWidget()
{
    return m_haveExternalWidget;
}

// STATIC ATTRIBUTES
Flameshot::Origin Flameshot::m_origin = Flameshot::DAEMON;
