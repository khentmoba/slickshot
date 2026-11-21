#include "colorgrabwidget.h"
#include "core/qguiappcurrentscreen.h"
#include "widgets/capture/overlaymessage.h"
#include "widgets/panel/sidepanelwidget.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <stdexcept>

// Width (= height) and zoom level of the widget before the user clicks
#define WIDTH1 77
#define ZOOM1 11
// Width (= height) and zoom level of the widget after the user clicks
#define WIDTH2 165
#define ZOOM2 15

// NOTE: WIDTH1(2) should be divisible by ZOOM1(2) for best precision.
//       WIDTH1 should be odd so the cursor can be centered on a pixel.

ColorGrabWidget::ColorGrabWidget(QPixmap* p, QWidget* parent)
  : QWidget(parent)
  , m_mousePressReceived(false)
  , m_extraZoomActive(false)
  , m_magnifierActive(false)
{
    if (p == nullptr) {
        throw std::logic_error("Pixmap must not be null");
    }
    m_pixmap = *p;
    setAttribute(Qt::WA_DeleteOnClose);
    // We don't need this widget to receive mouse events because we use
    // eventFilter on other objects that do
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_QuitOnClose, false);
    // On Windows: don't activate the widget so CaptureWidget remains active
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(Qt::BypassWindowManagerHint | Qt::WindowStaysOnTopHint |
                   Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setMouseTracking(true);

    m_extraZoomTimer.setSingleShot(true);
    m_extraZoomTimer.setInterval(250);
    connect(&m_extraZoomTimer, &QTimer::timeout, this, [this]() {
        if (!m_extraZoomActive) {
            return;
        }
        if (!isVisible()) {
            show();
        } else {
            updateWidget();
        }
    });
}

ColorGrabWidget::~ColorGrabWidget()
{
    finalize();
}

void ColorGrabWidget::startGrabbing()
{
    if (m_grabActive) {
        return;
    }
    m_grabActive = true;
    // NOTE: grabMouse() would prevent move events being received
    // With this method we just need to make sure that mouse press and release
    // events get consumed before they reach their target widget.
    // This is undone in finalize() / the destructor.
    qApp->setOverrideCursor(Qt::CrossCursor);
    qApp->installEventFilter(this);
    OverlayMessage::pushKeyMap(
      { { tr("Enter or Left Click"), tr("Accept color") },
        { tr("Hold Left Click"), tr("Precisely select color") },
        { tr("Space or Right Click"), tr("Toggle magnifier") },
        { tr("Esc"), tr("Cancel") } });
}

QColor ColorGrabWidget::color()
{
    return m_color;
}

bool ColorGrabWidget::eventFilter(QObject*, QEvent* event)
{
    // Consume shortcut events and handle key presses from whole app
    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::Shortcut) {
        QKeySequence key = event->type() == QEvent::KeyPress
                             ? static_cast<QKeyEvent*>(event)->key()
                             : static_cast<QShortcutEvent*>(event)->key();
        if (key == Qt::Key_Escape) {
            emit grabAborted();
            finalize();
        } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            emit colorGrabbed(m_color);
            finalize();
        } else if (key == Qt::Key_Space && !m_extraZoomActive) {
            setMagnifierActive(!m_magnifierActive);
        }
        return true;
    } else if (event->type() == QEvent::MouseMove) {
        // NOTE: This relies on the fact that CaptureWidget tracks mouse moves

        if (m_extraZoomActive && !geometry().contains(cursorPos())) {
            setExtraZoomActive(false);
            return true;
        }
        if (!m_extraZoomActive && !m_magnifierActive) {
            // This fixes an issue when the mouse leaves the zoom area before
            // the widget even appears.
            hide();
        }
        if (!m_extraZoomActive) {
            // Update only before the user clicks the mouse, after the mouse
            // press the widget remains static.
            updateWidget();
        }

        // Hide overlay message when cursor is over it
        OverlayMessage* overlayMsg = OverlayMessage::instance();
        if (overlayMsg != nullptr) {
            overlayMsg->setVisibility(
              !overlayMsg->geometry().contains(cursorPos()));
        }

        m_color = getColorAtPoint(cursorPos());
        emit colorUpdated(m_color);
        return true;
    } else if (event->type() == QEvent::MouseButtonPress) {
        m_mousePressReceived = true;
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->buttons() == Qt::RightButton) {
            setMagnifierActive(!m_magnifierActive);
        } else if (e->buttons() == Qt::LeftButton) {
            setExtraZoomActive(true);
        }
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (!m_mousePressReceived) {
            // Do not consume event if it corresponds to the mouse press that
            // triggered the color grabbing in the first place. This prevents
            // focus issues in the capture widget when the color grabber is
            // closed.
            return false;
        }
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton && m_extraZoomActive) {
            emit colorGrabbed(getColorAtPoint(cursorPos()));
            finalize();
        }
        return true;
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        return true;
    }
    return false;
}

void ColorGrabWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (!painter.isActive())
        return;
    painter.drawImage(QRectF(0, 0, width(), height()), m_previewImage);
}

void ColorGrabWidget::showEvent(QShowEvent*)
{
    updateWidget();
}

QPoint ColorGrabWidget::cursorPos() const
{
    return QCursor::pos(QGuiAppCurrentScreen().currentScreen());
}

/// @note The screenshot is in device pixels while the cursor is in logical
/// coordinates. Without this conversion the sampled color is off by the
/// scaling factor (e.g. at 200% the color of 50,50 is returned for 100,100).
QPoint ColorGrabWidget::physicalPixelAt(const QPoint& globalPos) const
{
    QPoint point = globalPos;
    if (QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen()) {
        const qreal dpr = currentScreen->devicePixelRatio();
        if (dpr > 0.0) {
            point =
              QPoint((globalPos.x() - currentScreen->geometry().x()) * dpr,
                     (globalPos.y() - currentScreen->geometry().y()) * dpr);
        }
    }
    // QPixmap::copy() takes device pixels; clamp to the screenshot's physical
    // extent so edge cursors cannot sample outside the captured image.
    // QPixmap::size() is already in device pixels.
    const QSize physicalSize = m_pixmap.size();
    if (!physicalSize.isEmpty()) {
        point.setX(qBound(0, point.x(), physicalSize.width() - 1));
        point.setY(qBound(0, point.y(), physicalSize.height() - 1));
    }
    return point;
}

const QImage& ColorGrabWidget::cachedImage() const
{
    if (m_cachedImage.isNull() && !m_pixmap.isNull()) {
        m_cachedImage = m_pixmap.toImage();
    }
    return m_cachedImage;
}

/// @note The point is in screen coordinates.
QColor ColorGrabWidget::getColorAtPoint(const QPoint& p) const
{
    if (m_extraZoomActive && geometry().contains(p)) {
        const QPoint point = mapFromGlobal(p);
        // we divide coordinate-wise to avoid rounding to nearest
        const QPoint samplePt(point.x() / ZOOM2, point.y() / ZOOM2);
        if (!m_previewImage.isNull() && m_previewImage.rect().contains(samplePt)) {
            return m_previewImage.pixel(samplePt);
        }
    }
    const QImage& img = cachedImage();
    if (img.isNull()) {
        return QColor();
    }
    const QPoint point = physicalPixelAt(p);
    if (!img.rect().contains(point)) {
        return QColor();
    }
    return img.pixel(point);
}

void ColorGrabWidget::setExtraZoomActive(bool active)
{
    m_extraZoomActive = active;
    m_extraZoomTimer.stop();
    if (!active && !m_magnifierActive) {
        hide();
    } else if (active) {
        m_extraZoomTimer.start();
    }
}

void ColorGrabWidget::setMagnifierActive(bool active)
{
    m_magnifierActive = active;
    setVisible(active);
}

void ColorGrabWidget::updateWidget()
{
    int width = m_extraZoomActive ? WIDTH2 : WIDTH1;
    float zoom = m_extraZoomActive ? ZOOM2 : ZOOM1;
    // Set window size and move its center to the mouse cursor
    QRect rect(0, 0, width, width);

    auto realCursorPos = cursorPos();
    auto adjustedCursorPos = physicalPixelAt(realCursorPos);

    // DPR conversion is handled by physicalPixelAt() on all platforms.

    rect.moveCenter(cursorPos());
    setGeometry(rect);
    // Store a zoomed-in section around the cursor from the cached CPU image.
    QRect sourceRect(0, 0, width / zoom, width / zoom);
    sourceRect.moveCenter(adjustedCursorPos);
    const QImage& img = cachedImage();
    m_previewImage = img.isNull() ? QImage() : img.copy(sourceRect);
    // Repaint
    update();
}

void ColorGrabWidget::finalize()
{
    m_extraZoomTimer.stop();
    if (!m_grabActive) {
        return;
    }
    m_grabActive = false;
    if (qApp != nullptr) {
        qApp->removeEventFilter(this);
        qApp->restoreOverrideCursor();
    }
    OverlayMessage::pop();
    close();
}
