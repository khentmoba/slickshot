// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>

namespace ThemeUtils {

bool isSystemDarkMode();

QColor systemAccentColor();

QColor toolbarBackgroundColor();

QColor toolbarBorderColor();

QColor buttonHoverColor();

QColor buttonPressedColor();

QColor buttonTextColor();

int buttonCornerRadius();

QString buttonStyleSheet(const QColor& accentColor = QColor());

QString toolbarContainerStyleSheet();

void applyApplicationTheme();

} // namespace ThemeUtils
