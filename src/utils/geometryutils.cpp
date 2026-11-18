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

qreal effectiveDesktopPixelRatio(const QPixmap& pixmap,
                                 const QRect& desktopLogical)
{
    if (pixmap.isNull() || desktopLogical.isEmpty()) {
        return 1.0;
    }
    if (pixmap.devicePixelRatio() > 1.0) {
        return pixmap.devicePixelRatio();
    }
    // windowsScreenshot returns physical pixels with a DPR of 1.0, so derive
    // the ratio from the pixmap size relative to the logical desktop.
    const qreal ratioX =
      qreal(pixmap.width()) / qreal(desktopLogical.width());
    const qreal ratioY =
      qreal(pixmap.height()) / qreal(desktopLogical.height());
    return qMax(1.0, qMax(ratioX, ratioY));
}

QRect logicalToPhysicalDesktop(const QRect& logicalRegion,
                               const QList<QRect>& logicalScreens,
                               const QList<QRect>& physicalScreens)
{
    QRect result;
    if (logicalRegion.isEmpty() ||
        logicalScreens.size() != physicalScreens.size()) {
        return result;
    }

    for (int i = 0; i < logicalScreens.size(); ++i) {
        const QRect& logical = logicalScreens.at(i);
        const QRect& physical = physicalScreens.at(i);
        if (logical.isEmpty() || physical.isEmpty()) {
            continue;
        }
        const QRect part = logicalRegion.intersected(logical);
        if (part.isEmpty()) {
            continue;
        }
        const qreal scaleX =
          qreal(physical.width()) / qreal(logical.width());
        const qreal scaleY =
          qreal(physical.height()) / qreal(logical.height());
        const QRect mapped(
          physical.x() + qRound((part.x() - logical.x()) * scaleX),
          physical.y() + qRound((part.y() - logical.y()) * scaleY),
          qRound(part.width() * scaleX),
          qRound(part.height() * scaleY));
        result = result.isEmpty() ? mapped : result.united(mapped);
    }
    return result;
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
