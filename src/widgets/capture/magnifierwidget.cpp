// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "magnifierwidget.h"

#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

MagnifierWidget::MagnifierWidget(const QPixmap& p,
                                 const QColor& c,
                                 bool isSquare,
                                 QWidget* parent)
  : QWidget(parent)
  , m_square(isSquare)
  , m_color(c)
  , m_borderColor(c)
  , m_screenshot(p)
{
    if (parent) {
        setFixedSize(parent->width(), parent->height());
    }
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_color.setAlpha(130);
}

QPoint MagnifierWidget::physicalPixelAt(const QPoint& logicalPos,
                                        qreal dpr,
                                        const QSize& physicalSize)
{
    if (physicalSize.isEmpty()) {
        return { 0, 0 };
    }
    const qreal scale = dpr > 0.0 ? dpr : 1.0;
    const int px =
      qBound(0, qRound(logicalPos.x() * scale), physicalSize.width() - 1);
    const int py =
      qBound(0, qRound(logicalPos.y() * scale), physicalSize.height() - 1);
    return { px, py };
}

void MagnifierWidget::ensureScreenshotImage()
{
    if (m_screenshotImage.isNull() && !m_screenshot.isNull()) {
        m_screenshotImage = m_screenshot.toImage();
        m_screenshotImage.setDevicePixelRatio(1.0);
    }
}

QImage MagnifierWidget::samplePatch(const QPoint& centerPx) const
{
    QImage patch(m_pixels, m_pixels, QImage::Format_ARGB32);
    patch.fill(Qt::black);
    if (m_screenshotImage.isNull()) {
        return patch;
    }

    const int topLeftX = centerPx.x() - m_magPixels;
    const int topLeftY = centerPx.y() - m_magPixels;
    const QRect srcRect = QRect(topLeftX, topLeftY, m_pixels, m_pixels)
                            .intersected(m_screenshotImage.rect());
    if (!srcRect.isEmpty()) {
        QPainter patchPainter(&patch);
        patchPainter.drawImage(srcRect.x() - topLeftX,
                               srcRect.y() - topLeftY,
                               m_screenshotImage,
                               srcRect.x(),
                               srcRect.y(),
                               srcRect.width(),
                               srcRect.height());
    }
    return patch;
}

void MagnifierWidget::drawColorReadout(QPainter& painter,
                                       const QPointF& drawPos,
                                       const QColor& pixelColor)
{
    const QString label = QStringLiteral("%1  %2,%3,%4")
                            .arg(pixelColor.name(QColor::HexRgb).toUpper())
                            .arg(pixelColor.red())
                            .arg(pixelColor.green())
                            .arg(pixelColor.blue());

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);

    const QFontMetrics fm(font);
    const int swatchSize = 10;
    const int padX = 8;
    const int padY = 5;
    const int gap = 6;
    const int pillW = padX * 2 + swatchSize + gap + fm.horizontalAdvance(label);
    const int pillH = qMax(swatchSize, fm.height()) + padY * 2;

    const qreal loupeHalf = (m_pixels * magZoom) / 2.0;
    qreal pillX = drawPos.x() - pillW / 2.0;
    pillX = qBound(4.0, pillX, static_cast<qreal>(width() - pillW - 4));

    qreal pillY = drawPos.y() + loupeHalf + 8.0;
    if (pillY + pillH > height() - 4) {
        pillY = drawPos.y() - loupeHalf - 8.0 - pillH;
    }

    const QRectF pillRect(pillX, pillY, pillW, pillH);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(24, 24, 27, 235));
    painter.setPen(QPen(QColor(255, 255, 255, 45), 1.0));
    painter.drawRoundedRect(pillRect, 6.0, 6.0);

    const QRectF swatchRect(
      pillX + padX, pillY + (pillH - swatchSize) / 2.0, swatchSize, swatchSize);
    painter.setBrush(pixelColor);
    painter.setPen(QPen(QColor(255, 255, 255, 160), 1.0));
    painter.drawRoundedRect(swatchRect, 2.0, 2.0);

    painter.setPen(QColor(244, 244, 245));
    const QRectF textRect(swatchRect.right() + gap,
                          pillY,
                          pillW - padX * 2 - swatchSize - gap,
                          pillH);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, label);
    painter.restore();
}

void MagnifierWidget::paintEvent(QPaintEvent*)
{
    ensureScreenshotImage();
    if (m_screenshotImage.isNull()) {
        return;
    }
    QPainter p(this);
    if (!p.isActive()) {
        return;
    }
    if (m_square) {
        drawMagnifier(p);
    } else {
        drawMagnifierCircle(p);
    }
}

void MagnifierWidget::drawMagnifierCircle(QPainter& painter)
{
    drawLoupe(painter, true);
}

void MagnifierWidget::drawMagnifier(QPainter& painter)
{
    drawLoupe(painter, false);
}

void MagnifierWidget::drawLoupe(QPainter& painter, bool circle)
{
    const QPoint translated = mapFromGlobal(QCursor::pos());
    const qreal dpr = m_screenshot.devicePixelRatio();
    const QPoint centerPx =
      physicalPixelAt(translated, dpr, m_screenshotImage.size());
    const QImage patch = samplePatch(centerPx);
    const QColor centerColor = m_screenshotImage.pixelColor(centerPx);

    const qreal loupeSize = m_pixels * magZoom;
    const qreal loupeHalf = loupeSize / 2.0;

    qreal drawPosX = translated.x() + m_magOffset + loupeHalf;
    if (drawPosX > width() - loupeHalf - 4) {
        drawPosX = translated.x() - m_magOffset - loupeHalf;
    }
    qreal drawPosY = translated.y() + m_magOffset + loupeHalf;
    if (drawPosY > height() - loupeHalf - 34) {
        drawPosY = translated.y() - m_magOffset - loupeHalf;
    }
    const QPointF drawPos(drawPosX, drawPosY);

    const QRectF loupeRect(
      drawPos.x() - loupeHalf, drawPos.y() - loupeHalf, loupeSize, loupeSize);
    const QRectF crossHairTop(drawPos.x() + magZoom * (-0.5),
                              drawPos.y() - magZoom * (m_magPixels + 0.5),
                              magZoom,
                              magZoom * m_magPixels);
    const QRectF crossHairRight(drawPos.x() + magZoom * 0.5,
                                drawPos.y() + magZoom * (-0.5),
                                magZoom * m_magPixels,
                                magZoom);
    const QRectF crossHairBottom(drawPos.x() + magZoom * (-0.5),
                                 drawPos.y() + magZoom * 0.5,
                                 magZoom,
                                 magZoom * m_magPixels);
    const QRectF crossHairLeft(drawPos.x() - magZoom * (m_magPixels + 0.5),
                               drawPos.y() + magZoom * (-0.5),
                               magZoom * m_magPixels,
                               magZoom);

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    if (circle) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addEllipse(drawPos, loupeHalf, loupeHalf);
        painter.setClipPath(path);
        painter.drawImage(loupeRect, patch);

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const auto& rect :
             { crossHairTop, crossHairRight, crossHairBottom, crossHairLeft }) {
            painter.fillRect(rect, m_color);
        }
        painter.setClipping(false);

        QPen pen(m_borderColor);
        pen.setWidth(4);
        painter.setPen(pen);
        painter.drawEllipse(drawPos, loupeHalf, loupeHalf);
    } else {
        const QRectF crossHairBorder = loupeRect.adjusted(-1, -1, 1, 1);
        painter.fillRect(crossHairBorder, m_borderColor);
        painter.drawImage(loupeRect, patch);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const auto& rect :
             { crossHairTop, crossHairRight, crossHairBottom, crossHairLeft }) {
            painter.fillRect(rect, m_color);
        }
    }

    painter.restore();
    drawColorReadout(painter, drawPos, centerColor);
}
