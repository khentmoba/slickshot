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
    init();
}

CaptureButton::CaptureButton(const QString& text, QWidget* parent)
  : QPushButton(text, parent)
{
    init();
}

CaptureButton::CaptureButton(const QIcon& icon,
                             const QString& text,
                             QWidget* parent)
  : QPushButton(icon, text, parent)
{
    init();
}

void CaptureButton::init()
{
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);

    auto* dsEffect = new QGraphicsDropShadowEffect(this);
    dsEffect->setBlurRadius(5);
    dsEffect->setOffset(0);
    dsEffect->setColor(QColor(Qt::black));

    setGraphicsEffect(dsEffect);
}

QString CaptureButton::globalStyleSheet()
{
    return CaptureButton(nullptr).styleSheet();
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

QColor CaptureButton::m_mainColor;
