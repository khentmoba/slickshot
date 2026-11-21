#pragma once

#include <QTimer>
#include <QWidget>

class SidePanelWidget;
class OverlayMessage;

class ColorGrabWidget : public QWidget
{
    Q_OBJECT
public:
    ColorGrabWidget(QPixmap* p, QWidget* parent = nullptr);
    ~ColorGrabWidget() override;

    void startGrabbing();

    QColor color();

    // Maps a global logical cursor position to a pixel in the screenshot.
    QPoint physicalPixelAt(const QPoint& globalPos) const;

signals:
    void colorUpdated(const QColor& color);
    void colorGrabbed(const QColor& color);
    void grabAborted();

private:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* e) override;
    void showEvent(QShowEvent* event) override;

    QPoint cursorPos() const;
    QColor getColorAtPoint(const QPoint& point) const;
    void setExtraZoomActive(bool active);
    void setMagnifierActive(bool active);
    void updateWidget();
    void finalize();

    const QImage& cachedImage() const;

    QPixmap m_pixmap;
    mutable QImage m_cachedImage;
    QImage m_previewImage;
    QColor m_color;
    QTimer m_extraZoomTimer;

    bool m_mousePressReceived;
    bool m_extraZoomActive;
    bool m_magnifierActive;
    bool m_grabActive{ false };
};
