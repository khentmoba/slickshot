// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Yurii Puchkov & Contributors

#include "capturetoolobjects.h"

#define SEARCH_RADIUS_NEAR 3
#define SEARCH_RADIUS_FAR 5
#define SEARCH_RADIUS_TEXT_HANDICAP 5

CaptureToolObjects::CaptureToolObjects(QObject* parent)
  : QObject(parent)
{}

void CaptureToolObjects::append(const QPointer<CaptureTool>& captureTool)
{
    if (!captureTool.isNull()) {
        m_captureToolObjects.append(captureTool->copy(captureTool->parent()));
    }
}

void CaptureToolObjects::insert(int index,
                                const QPointer<CaptureTool>& captureTool)
{
    if (!captureTool.isNull() && index >= 0 &&
        index <= m_captureToolObjects.size()) {
        m_captureToolObjects.insert(index,
                                    captureTool->copy(captureTool->parent()));
    }
}

QPointer<CaptureTool> CaptureToolObjects::at(int index)
{
    if (index >= 0 && index < m_captureToolObjects.size()) {
        return m_captureToolObjects[index];
    }
    return nullptr;
}

void CaptureToolObjects::clear()
{
    m_captureToolObjects.clear();
}

QList<QPointer<CaptureTool>> CaptureToolObjects::captureToolObjects()
{
    return m_captureToolObjects;
}

int CaptureToolObjects::size() const
{
    return static_cast<int>(m_captureToolObjects.size());
}

void CaptureToolObjects::removeAt(int index)
{
    if (index >= 0 && index < m_captureToolObjects.size()) {
        m_captureToolObjects.removeAt(index);
    }
}

int CaptureToolObjects::find(const QPoint& pos, QSize captureSize)
{
    if (m_captureToolObjects.empty()) {
        return -1;
    }
    // first attempt to find at exact position
    int index = findWithRadius(pos, captureSize, SEARCH_RADIUS_NEAR);
    if (-1 == index) {
        // second attempt to find at position with radius
        index = findWithRadius(pos, captureSize, SEARCH_RADIUS_FAR);
    }
    return index;
}

int CaptureToolObjects::findWithRadius(const QPoint& pos,
                                       QSize captureSize,
                                       int radius)
{
    const QRect canvas(QPoint(0, 0), captureSize);
    QPixmap patch;
    for (int index = size() - 1; index >= 0; --index) {
        const QPointer<CaptureTool> toolItem = m_captureToolObjects.at(index);
        if (toolItem.isNull()) {
            continue;
        }
        int currentRadius = radius;
        if (toolItem->type() == CaptureTool::TYPE_TEXT) {
            if (currentRadius > SEARCH_RADIUS_NEAR) {
                // Text already has a big currentRadius and no need to search
                // with a bit bigger currentRadius than
                // SEARCH_RADIUS_TEXT_HANDICAP + SEARCH_RADIUS_NEAR
                continue;
            }

            // Text has spaces inside to need to take a bigger currentRadius for
            // text objects search
            currentRadius += SEARCH_RADIUS_TEXT_HANDICAP;
        }

        // Only the square around the click matters. Clip it to the canvas so
        // no pixel outside the image is ever read.
        const int side = (2 * currentRadius) + 1;
        const QRect area =
          QRect(pos - QPoint(currentRadius, currentRadius), QSize(side, side))
            .intersected(canvas);
        if (area.isEmpty()) {
            continue;
        }

        if (patch.size() != area.size()) {
            patch = QPixmap(area.size());
        }
        // QPixmap(size) can have no alpha channel. fill(transparent) gives it
        // one, so the patch starts fully transparent, not black.
        patch.fill(Qt::transparent);

        // Draw only this object, and only inside its own area, so objects
        // above it cannot leak into the result.
        {
            QPainter painter(&patch);
            painter.translate(-area.topLeft());
            toolItem->drawSearchArea(painter, patch);
        }
        const QImage image =
          patch.toImage().convertToFormat(QImage::Format_ARGB32);
        for (int row = 0; row < image.height(); ++row) {
            const QRgb* scan =
              reinterpret_cast<const QRgb*>(image.constScanLine(row));
            for (int column = 0; column < image.width(); ++column) {
                if (scan[column] != 0) {
                    // object was found, return it index (layer index)
                    return index;
                }
            }
        }
    }
    // no object at current pos found
    return -1;
}

void CaptureToolObjects::composeRange(QPixmap& pixmap, int first, int end) const
{
    const int last = qMin(end, size());
    for (int index = qMax(first, 0); index < last; ++index) {
        const QPointer<CaptureTool>& toolItem = m_captureToolObjects.at(index);
        if (toolItem.isNull()) {
            continue;
        }
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        toolItem->process(painter, pixmap);
    }
}

CaptureToolObjects& CaptureToolObjects::operator=(
  const CaptureToolObjects& other)
{
    // remove extra items for this if size is bigger
    while (this->m_captureToolObjects.size() >
           other.m_captureToolObjects.size()) {
        this->m_captureToolObjects.removeLast();
    }

    int count = 0;
    for (const auto& item : other.m_captureToolObjects) {
        QPointer<CaptureTool> itemCopy = item->copy(item->parent());
        if (count < this->m_captureToolObjects.size()) {
            this->m_captureToolObjects[count] = itemCopy;
        } else {
            this->m_captureToolObjects.append(itemCopy);
        }
        count++;
    }
    return *this;
}
