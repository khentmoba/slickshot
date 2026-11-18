// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QRect>

class QPixmap;

namespace GeometryUtils {

struct RegionMapping
{
    QRect localLogical;
    QRect localPixmap;
    QRect globalLogical;
    bool valid{ false };
};

QRect logicalToPixmap(const QRect& rect, qreal dpr);
QRect pixmapToLogical(const QRect& rect, qreal dpr);
QRect logicalRectForPixmap(const QRect& rect, const QPixmap& pixmap);
qreal effectiveDesktopPixelRatio(const QPixmap& pixmap,
                                 const QRect& desktopLogical);
QRect logicalToPhysicalDesktop(const QRect& logicalRegion,
                               const QList<QRect>& logicalScreens,
                               const QList<QRect>& physicalScreens);
RegionMapping mapRegionToScreen(const QRect& requested,
                                const QRect& screenLogical,
                                const QPixmap& pixmap);

} // namespace GeometryUtils
