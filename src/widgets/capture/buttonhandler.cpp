// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "buttonhandler.h"
#include "utils/globalvalues.h"

#include <QPoint>
#include <QScreen>

// ButtonHandler is a handler for every active button. It makes easier to
// manipulate the buttons as a unit.

ButtonHandler::ButtonHandler(const QVector<CaptureToolButton*>& v,
                             QObject* parent)
  : QObject(parent)
{
    setButtons(v);
    init();
}

ButtonHandler::ButtonHandler(QObject* parent)
  : QObject(parent)
{
    init();
}

void ButtonHandler::hide()
{
    for (CaptureToolButton* b : m_vectorButtons) {
        b->hide();
    }
}

void ButtonHandler::show()
{
    if (m_vectorButtons.isEmpty() || m_vectorButtons.first()->isVisible()) {
        return;
    }
    for (CaptureToolButton* b : m_vectorButtons) {
        b->animatedShow();
    }
}

bool ButtonHandler::isVisible() const
{
    bool ret = true;
    for (const CaptureToolButton* b : m_vectorButtons) {
        if (!b->isVisible()) {
            ret = false;
            break;
        }
    }
    return ret;
}

bool ButtonHandler::buttonsAreInside() const
{
    return m_buttonsAreInside;
}

size_t ButtonHandler::size() const
{
    return m_vectorButtons.size();
}

// updatePosition updates the position of the buttons around the
// selection area. Ignores the sides blocked by the end of the screen.
// updatePosition updates the position of the buttons as a modern floating toolbar
// docked right below (or above/inside) the selection area, respecting screen bounds.
void ButtonHandler::updatePosition(const QRect& selection)
{
    resetRegionTrack();
    const int vecLength = m_vectorButtons.size();
    if (vecLength == 0) {
        return;
    }
    // Copy of the selection area for internal modifications
    m_selection = intersectWithAreas(selection);
    updateBlockedSides();

    // Determine available screen bounds containing the selection
    QRect screenBounds;
    for (const QRect& rect : m_screenRegions) {
        if (rect.intersects(selection)) {
            screenBounds = screenBounds.united(rect);
        }
    }
    if (screenBounds.isEmpty() && !m_screenRegions.isEmpty()) {
        screenBounds = m_screenRegions.first();
    }

    const int buttonWidth = m_buttonExtendedSize;
    const int buttonHeight = m_buttonBaseSize;
    const int gap = m_separator;
    const int margin = m_separator * 2;

    // Calculate how many buttons can fit in a horizontal toolbar row
    int maxAvailableWidth = screenBounds.isValid() ? screenBounds.width() - (2 * margin) : selection.width();
    int buttonsPerRow = qMax(1, (maxAvailableWidth + gap) / (buttonWidth + gap));
    if (buttonsPerRow > vecLength) {
        buttonsPerRow = vecLength;
    }

    int rows = (vecLength + buttonsPerRow - 1) / buttonsPerRow;
    int totalToolbarHeight = rows * buttonHeight + (rows - 1) * gap;

    // Check if toolbar fits below the selection
    int toolbarY;
    if (!m_blockedBotton && (m_selection.bottom() + margin + totalToolbarHeight <= screenBounds.bottom())) {
        toolbarY = m_selection.bottom() + margin;
        m_buttonsAreInside = false;
    } else if (!m_blockedTop && (m_selection.top() - margin - totalToolbarHeight >= screenBounds.top())) {
        // Place above selection
        toolbarY = m_selection.top() - margin - totalToolbarHeight;
        m_buttonsAreInside = false;
    } else {
        // Dock inside selection near the bottom
        toolbarY = m_selection.bottom() - margin - totalToolbarHeight;
        if (toolbarY < m_selection.top()) {
            toolbarY = m_selection.top() + margin;
        }
        m_buttonsAreInside = true;
    }

    int buttonIdx = 0;
    for (int r = 0; r < rows && buttonIdx < vecLength; ++r) {
        int countInRow = qMin(buttonsPerRow, vecLength - buttonIdx);
        int rowWidth = countInRow * buttonWidth + (countInRow - 1) * gap;

        // Center horizontally relative to selection center, then clamp to screen bounds
        int startX = m_selection.center().x() - (rowWidth / 2);
        if (screenBounds.isValid()) {
            if (startX + rowWidth > screenBounds.right() - margin) {
                startX = screenBounds.right() - margin - rowWidth;
            }
            if (startX < screenBounds.left() + margin) {
                startX = screenBounds.left() + margin;
            }
        }

        int curY = toolbarY + r * (buttonHeight + gap);
        for (int i = 0; i < countInRow && buttonIdx < vecLength; ++i) {
            CaptureToolButton* btn = m_vectorButtons[buttonIdx++];
            btn->move(startX + i * (buttonWidth + gap), curY);
        }
    }
}

int ButtonHandler::calculateShift(int elements, bool reverse) const
{
    int shift = 0;
    if (elements % 2 == 0) {
        shift = m_buttonExtendedSize * (elements / 2) - (m_separator / 2);
    } else {
        shift =
          m_buttonExtendedSize * ((elements - 1) / 2) + m_buttonBaseSize / 2;
    }
    if (!reverse) {
        shift -= m_buttonBaseSize;
    }

    return shift;
}
// horizontalPoints is an auxiliary method for the button position computation.
// starts from a known center and keeps adding elements horizontally
// and returns the computed positions.
QVector<QPoint> ButtonHandler::horizontalPoints(const QPoint& center,
                                                const int elements,
                                                const bool leftToRight) const
{
    QVector<QPoint> res;
    // Distance from the center to start adding buttons
    int shift = calculateShift(elements, leftToRight);

    int x = leftToRight ? center.x() - shift : center.x() + shift;
    QPoint i(x, center.y());
    while (elements > res.length()) {
        res.append(i);
        leftToRight ? i.setX(i.x() + m_buttonExtendedSize)
                    : i.setX(i.x() - m_buttonExtendedSize);
    }
    return res;
}

// verticalPoints is an auxiliary method for the button position computation.
// starts from a known center and keeps adding elements vertically
// and returns the computed positions.
QVector<QPoint> ButtonHandler::verticalPoints(const QPoint& center,
                                              const int elements,
                                              const bool upToDown) const
{
    QVector<QPoint> res;
    // Distance from the center to start adding buttons
    int shift = calculateShift(elements, upToDown);

    int y = upToDown ? center.y() - shift : center.y() + shift;
    QPoint i(center.x(), y);
    while (elements > res.length()) {
        res.append(i);
        upToDown ? i.setY(i.y() + m_buttonExtendedSize)
                 : i.setY(i.y() - m_buttonExtendedSize);
    }
    return res;
}

QRect ButtonHandler::intersectWithAreas(const QRect& rect)
{
    QRect res;
    for (const QRect& r : m_screenRegions) {
        QRect temp = rect.intersected(r);
        if (temp.height() * temp.width() > res.height() * res.width()) {
            res = temp;
        }
    }
    return res;
}

void ButtonHandler::init()
{
    m_separator = GlobalValues::buttonBaseSize() / 4;
}

void ButtonHandler::resetRegionTrack()
{
    m_buttonsAreInside = false;
}

void ButtonHandler::updateBlockedSides()
{
    QRegion screenRegion{};
    for (const QRect& rect : m_screenRegions) {
        screenRegion += rect;
    }

    const int EXTENSION = m_separator * 2 + m_buttonBaseSize;
    // Right
    QPoint pointA(m_selection.right() + EXTENSION, m_selection.bottom());
    QPoint pointB(pointA.x(), m_selection.top());
    m_blockedRight =
      !(screenRegion.contains(pointA) && screenRegion.contains(pointB));
    // Left
    pointA.setX(m_selection.left() - EXTENSION);
    pointB.setX(pointA.x());
    m_blockedLeft =
      !(screenRegion.contains(pointA) && screenRegion.contains(pointB));
    // Bottom
    pointA = QPoint(m_selection.left(), m_selection.bottom() + EXTENSION);
    pointB = QPoint(m_selection.right(), pointA.y());
    m_blockedBotton =
      !(screenRegion.contains(pointA) && screenRegion.contains(pointB));
    // Top
    pointA.setY(m_selection.top() - EXTENSION);
    pointB.setY(pointA.y());
    m_blockedTop =
      !(screenRegion.contains(pointA) && screenRegion.contains(pointB));
    // Auxiliary
    m_oneHorizontalBlocked =
      (!m_blockedRight && m_blockedLeft) || (m_blockedRight && !m_blockedLeft);
    m_horizontalyBlocked = (m_blockedRight && m_blockedLeft);
    m_allSidesBlocked =
      (m_blockedBotton && m_horizontalyBlocked && m_blockedTop);
}

void ButtonHandler::expandSelection()
{
    int& s = m_buttonExtendedSize;
    m_selection = m_selection + QMargins(s, s, s, s);
    m_selection = intersectWithAreas(m_selection);
}

void ButtonHandler::positionButtonsInside(int index)
{
    // Position the buttons in the botton-center of the main but inside of the
    // selection.
    QRect mainArea = m_selection;
    mainArea = intersectWithAreas(mainArea);
    const int buttonsPerRow = (mainArea.width()) / (m_buttonExtendedSize);
    if (buttonsPerRow == 0) {
        return;
    }
    QPoint center =
      QPoint(mainArea.center().x(), mainArea.bottom() - m_buttonExtendedSize);

    while (m_vectorButtons.size() > index) {
        int addCounter = buttonsPerRow;
        addCounter = qBound(0, addCounter, m_vectorButtons.size() - index);
        QVector<QPoint> positions = horizontalPoints(center, addCounter, true);
        moveButtonsToPoints(positions, index);
        center.setY(center.y() - m_buttonExtendedSize);
    }

    m_buttonsAreInside = true;
}

void ButtonHandler::ensureSelectionMinimumSize()
{
    // Detect if a side is smaller than a button in order to prevent collision
    // and redimension the base area to the base size of a single button per
    // side
    if (m_selection.width() < m_buttonBaseSize) {
        if (!m_blockedLeft) {
            m_selection.setX(m_selection.x() -
                             (m_buttonBaseSize - m_selection.width()) / 2);
        }
        m_selection.setWidth(m_buttonBaseSize);
    }
    if (m_selection.height() < m_buttonBaseSize) {
        if (!m_blockedTop) {
            m_selection.setY(m_selection.y() -
                             (m_buttonBaseSize - m_selection.height()) / 2);
        }
        m_selection.setHeight(m_buttonBaseSize);
    }
}

void ButtonHandler::moveButtonsToPoints(const QVector<QPoint>& points,
                                        int& index)
{
    for (const QPoint& p : points) {
        auto* button = m_vectorButtons[index];
        button->move(p);
        ++index;
    }
}

void ButtonHandler::adjustHorizontalCenter(QPoint& center)
{
    if (m_blockedLeft) {
        center.setX(center.x() + m_buttonExtendedSize / 2);
    } else if (m_blockedRight) {
        center.setX(center.x() - m_buttonExtendedSize / 2);
    }
}

// setButtons redefines the buttons of the button handler
void ButtonHandler::setButtons(const QVector<CaptureToolButton*>& v)
{
    if (v.isEmpty()) {
        return;
    }

    for (CaptureToolButton* b : m_vectorButtons) {
        delete (b);
    }
    m_vectorButtons = v;
    m_buttonBaseSize = GlobalValues::buttonBaseSize();
    m_buttonExtendedSize = m_buttonBaseSize + m_separator;
}

bool ButtonHandler::contains(const QPoint& p) const
{
    if (m_vectorButtons.isEmpty()) {
        return false;
    }
    QPoint first(m_vectorButtons.first()->pos());
    QPoint last(m_vectorButtons.last()->pos());
    bool firstIsTopLeft = (first.x() <= last.x() && first.y() <= last.y());
    QPoint topLeft = firstIsTopLeft ? first : last;
    QPoint bottonRight = firstIsTopLeft ? last : first;
    topLeft += QPoint(-m_separator, -m_separator);
    bottonRight += QPoint(m_buttonExtendedSize, m_buttonExtendedSize);
    QRegion r(QRect(topLeft, bottonRight).normalized());
    return r.contains(p);
}

void ButtonHandler::updateScreenRegions(const QVector<QRect>& rects)
{
    m_screenRegions = rects;
}

void ButtonHandler::updateScreenRegions(const QRect& rect)
{
    m_screenRegions = { rect };
}
