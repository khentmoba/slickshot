// SPDX-License-Identifier: GPL-3.0-or-later

#include "geometryutils.h"

#include <QPixmap>

#include <cmath>

namespace GeometryUtils {

QRect logicalToPixmap(const QRect& rect, qreal dpr)
{
    if (!rect.isValid() || dpr <= 0) {
        return {};
    }
    const qreal left = rect.left() * dpr;
    const qreal top = rect.top() * dpr;
    const qreal right = (rect.left() + rect.width()) * dpr;
    const qreal bottom = (rect.top() + rect.height()) * dpr;
    const QPoint topLeft(static_cast<int>(std::floor(left)),
                         static_cast<int>(std::floor(top)));
    const QPoint bottomRight(static_cast<int>(std::ceil(right)),
                             static_cast<int>(std::ceil(bottom)));
    return QRect(
      topLeft,
      QSize(bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y()));
}

QRect pixmapToLogical(const QRect& rect, qreal dpr)
{
    if (!rect.isValid() || dpr <= 0) {
        return {};
    }
    const qreal left = rect.left() / dpr;
    const qreal top = rect.top() / dpr;
    const qreal right = (rect.left() + rect.width()) / dpr;
    const qreal bottom = (rect.top() + rect.height()) / dpr;
    const QPoint topLeft(static_cast<int>(std::floor(left)),
                         static_cast<int>(std::floor(top)));
    const QPoint bottomRight(static_cast<int>(std::ceil(right)),
                             static_cast<int>(std::ceil(bottom)));
    return QRect(
      topLeft,
      QSize(bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y()));
}

QRect logicalRectForPixmap(const QRect& rect, const QPixmap& pixmap)
{
    return logicalToPixmap(rect, pixmap.devicePixelRatio());
}

RegionMapping mapRegionToScreen(const QRect& requested,
                                const QRect& screenLogical,
                                const QPixmap& pixmap)
{
    RegionMapping mapping;
    if (requested.isNull() || screenLogical.isEmpty() || pixmap.isNull()) {
        return mapping;
    }

    const QRect localLogical =
      requested.translated(-screenLogical.topLeft())
        .intersected(
          QRect(0, 0, screenLogical.width(), screenLogical.height()));
    if (localLogical.isEmpty()) {
        return mapping;
    }

    mapping.localLogical = localLogical;
    mapping.localPixmap = logicalRectForPixmap(localLogical, pixmap);
    mapping.globalLogical = localLogical.translated(screenLogical.topLeft());
    mapping.valid = mapping.localPixmap.isValid();
    return mapping;
}

} // namespace GeometryUtils
