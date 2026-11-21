// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "capturebutton.h"
#include "utils/colorutils.h"
#include "utils/globalvalues.h"
#include "utils/themeutils.h"

#include <QGraphicsDropShadowEffect>

CaptureButton::CaptureButton(QWidget* parent)
  : QPushButton(parent)
{
    init(true);
}

CaptureButton::CaptureButton(QWidget* parent, bool withDropShadow)
  : QPushButton(parent)
{
    init(withDropShadow);
}

CaptureButton::CaptureButton(const QString& text, QWidget* parent)
  : QPushButton(text, parent)
{
    init(true);
}

CaptureButton::CaptureButton(const QIcon& icon,
                             const QString& text,
                             QWidget* parent)
  : QPushButton(icon, text, parent)
{
    init(true);
}

void CaptureButton::init(bool withDropShadow)
{
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);

    if (withDropShadow) {
        auto* dsEffect = new QGraphicsDropShadowEffect(this);
        dsEffect->setBlurRadius(10);
        dsEffect->setOffset(0, 3);
        dsEffect->setColor(QColor(0, 0, 0, 150));

        setGraphicsEffect(dsEffect);
    }
}

QString CaptureButton::globalStyleSheet()
{
    return ThemeUtils::buttonStyleSheet();
}

QString CaptureButton::styleSheet() const
{
    return ThemeUtils::buttonStyleSheet(m_mainColor);
}

void CaptureButton::setColor(const QColor& c)
{
    m_mainColor = c;
    setStyleSheet(styleSheet());
}
