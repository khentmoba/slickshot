#pragma once

#include <QImage>
#include <QPixmap>
#include <QWidget>

class MagnifierWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MagnifierWidget(const QPixmap& p,
                             const QColor& c,
                             bool isSquare,
                             QWidget* parent = nullptr);

    static QPoint physicalPixelAt(const QPoint& logicalPos,
                                  qreal dpr,
                                  const QSize& physicalSize);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    const int m_magPixels = 8;
    const int m_magOffset = 16;
    const int magZoom = 10;
    const int m_pixels = 2 * m_magPixels + 1;
    bool m_square;
    QColor m_color;
    QColor m_borderColor;
    QPixmap m_screenshot;
    QImage m_screenshotImage;

    void ensureScreenshotImage();
    QImage samplePatch(const QPoint& centerPx) const;
    void drawColorReadout(QPainter& painter,
                          const QPointF& drawPos,
                          const QColor& pixelColor);
    void drawLoupe(QPainter& painter, bool circle);
    void drawMagnifier(QPainter& painter);
    void drawMagnifierCircle(QPainter& painter);
};
