// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "inverttool.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>

InvertTool::InvertTool(QObject* parent)
  : AbstractTwoPointTool(parent)
{}

QIcon InvertTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "invert.svg");
}

QString InvertTool::name() const
{
    return tr("Invert");
}

CaptureTool::Type InvertTool::type() const
{
    return CaptureTool::TYPE_INVERT;
}

QString InvertTool::description() const
{
    return tr("Set Inverter as the paint tool");
}

QRect InvertTool::boundingRect() const
{
    return QRect(points().first, points().second).normalized();
}

CaptureTool* InvertTool::copy(QObject* parent)
{
    auto* tool = new InvertTool(parent);
    copyParams(this, tool);
    return tool;
}

void InvertTool::invalidateCache()
{
    m_effectValid = false;
    m_cachedEffect = QImage();
}

void InvertTool::drawMove(const QPoint& p)
{
    AbstractTwoPointTool::drawMove(p);
    invalidateCache();
}

void InvertTool::drawMoveWithAdjustment(const QPoint& p)
{
    AbstractTwoPointTool::drawMoveWithAdjustment(p);
    invalidateCache();
}

void InvertTool::move(const QPoint& pos)
{
    AbstractTwoPointTool::move(pos);
    invalidateCache();
}

void InvertTool::process(QPainter& painter, const QPixmap& pixmap)
{
    const qreal pixelRatio =
      pixmap.devicePixelRatio() > 0 ? pixmap.devicePixelRatio() : 1.0;
    const QRect logicalBounds(QPoint(0, 0),
                              (QSizeF(pixmap.size()) / pixelRatio).toSize());
    const QRect selection = boundingRect().intersected(logicalBounds);
    if (selection.isEmpty()) {
        return;
    }

    const EffectKey key{ selection, pixmap.cacheKey(), pixelRatio };
    if (!m_effectValid || !(m_effectKey == key)) {
        const QRect selectionScaled =
          QRect(qRound(selection.x() * pixelRatio),
                qRound(selection.y() * pixelRatio),
                qRound(selection.width() * pixelRatio),
                qRound(selection.height() * pixelRatio))
            .intersected(pixmap.rect());
        if (selectionScaled.isEmpty()) {
            return;
        }
        QImage img = pixmap.copy(selectionScaled).toImage();
        img.invertPixels();
        m_cachedEffect = img;
        m_effectKey = key;
        m_effectValid = true;
    }

    painter.drawImage(selection, m_cachedEffect);
}

void InvertTool::drawSearchArea(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    painter.fillRect(boundingRect(), QBrush(Qt::black));
}

void InvertTool::paintMousePreview(QPainter& painter,
                                   const CaptureContext& context)
{
    Q_UNUSED(context)
    Q_UNUSED(painter)
}

void InvertTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
