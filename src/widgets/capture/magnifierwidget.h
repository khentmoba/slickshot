#pragma once

#include <QFont>
#include <QFontMetrics>
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

    // How far, in logical pixels, the loupe and its color readout reach from
    // the cursor. The capture window repaints only this box around the cursor.
    static constexpr int reach = 240;

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

    // Cached readout font/metrics, sampled patch and color label, reused
    // across mouse moves instead of being rebuilt on every paint.
    QFont m_readoutFont;
    QFontMetrics m_readoutMetrics;
    QPoint m_lastCenterPx;
    QImage m_cachedPatch;
    QColor m_cachedCenterColor;
    QString m_cachedLabel;
    QColor m_cachedLabelColor;
    bool m_patchValid = false;

    QImage samplePatch(const QPoint& centerPx, QColor& centerColor);
    void drawColorReadout(QPainter& painter,
                          const QPointF& drawPos,
                          const QColor& pixelColor);
    void drawLoupe(QPainter& painter, bool circle);
    void drawMagnifier(QPainter& painter);
    void drawMagnifierCircle(QPainter& painter);
};
