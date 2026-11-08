// SPDX-License-Identifier: GPL-3.0-or-later

#include "themeutils.h"
#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#if defined(Q_OS_WIN)
#include <QSettings>
#endif

namespace ThemeUtils {

bool isSystemDarkMode()
{
#if defined(Q_OS_WIN)
    QSettings personalizeSettings(
      QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
      QSettings::NativeFormat);
    QVariant appsUseLight = personalizeSettings.value(QStringLiteral("AppsUseLightTheme"));
    if (appsUseLight.isValid()) {
        return appsUseLight.toInt() == 0;
    }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif

    const QPalette defaultPalette = QGuiApplication::palette();
    return defaultPalette.color(QPalette::WindowText).lightness() >
           defaultPalette.color(QPalette::Window).lightness();
}

QColor systemAccentColor()
{
#if defined(Q_OS_WIN)
    QSettings dwmSettings(
      QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM"),
      QSettings::NativeFormat);
    QVariant accentVal = dwmSettings.value(QStringLiteral("AccentColor"));
    if (accentVal.isValid()) {
        // AccentColor is stored as 0xAABBGGRR in the registry
        uint raw = accentVal.toUInt();
        int r = raw & 0xFF;
        int g = (raw >> 8) & 0xFF;
        int b = (raw >> 16) & 0xFF;
        return QColor(r, g, b);
    }
#endif

    // Fallback: Modern Fluent blue or SlickShot accent
    return isSystemDarkMode() ? QColor(96, 205, 255) : QColor(0, 103, 192);
}

QColor toolbarBackgroundColor()
{
    // Windows 11 Acrylic / Mica inspired floating bar background
    return isSystemDarkMode() ? QColor(32, 32, 32, 230)
                              : QColor(243, 243, 243, 230);
}

QColor toolbarBorderColor()
{
    return isSystemDarkMode() ? QColor(255, 255, 255, 30)
                              : QColor(0, 0, 0, 20);
}

QColor buttonHoverColor()
{
    return isSystemDarkMode() ? QColor(255, 255, 255, 22)
                              : QColor(0, 0, 0, 16);
}

QColor buttonPressedColor()
{
    return isSystemDarkMode() ? QColor(255, 255, 255, 38)
                              : QColor(0, 0, 0, 28);
}

QColor buttonTextColor()
{
    return isSystemDarkMode() ? QColor(240, 240, 240) : QColor(28, 28, 28);
}

int buttonCornerRadius()
{
    return 6;
}

QString buttonStyleSheet(const QColor& accentColor)
{
    const bool dark = isSystemDarkMode();
    const QString textColor = dark ? "#ffffff" : "#1a1a1a";
    const QString hoverBg = dark ? "rgba(255, 255, 255, 0.10)" : "rgba(0, 0, 0, 0.06)";
    const QString pressedBg = dark ? "rgba(255, 255, 255, 0.18)" : "rgba(0, 0, 0, 0.12)";
    const QString activeAccent = accentColor.isValid() ? accentColor.name() : (dark ? "#60cdff" : "#0067c0");

    return QString(
      "CaptureButton {"
      "  border: 1px solid transparent;"
      "  border-radius: 6px;"
      "  background-color: transparent;"
      "  color: %1;"
      "  padding: 4px;"
      "}"
      "CaptureToolButton {"
      "  border: 1px solid transparent;"
      "  border-radius: 6px;"
      "  background-color: transparent;"
      "  padding: 0px;"
      "}"
      "CaptureButton:hover, CaptureToolButton:hover {"
      "  background-color: %2;"
      "  border: 1px solid %3;"
      "}"
      "CaptureButton:pressed, CaptureToolButton:pressed {"
      "  background-color: %4;"
      "}"
      "CaptureToolButton:checked {"
      "  background-color: %2;"
      "  border-bottom: 2px solid %5;"
      "}"
    ).arg(textColor, hoverBg, dark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.06)", pressedBg, activeAccent);
}

QString toolbarContainerStyleSheet()
{
    const bool dark = isSystemDarkMode();
    const QString bgColor = dark ? "rgba(32, 32, 32, 0.92)" : "rgba(246, 246, 246, 0.92)";
    const QString borderColor = dark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.10)";

    return QString(
      "background-color: %1;"
      "border: 1px solid %2;"
      "border-radius: 8px;"
    ).arg(bgColor, borderColor);
}

void applyApplicationTheme()
{
    const bool dark = isSystemDarkMode();
    const QColor accent = systemAccentColor();

    QPalette palette;
    if (dark) {
        const QColor darkBg(32, 32, 32);
        const QColor darkSurface(43, 43, 43);
        const QColor text(240, 240, 240);
        const QColor disabledText(128, 128, 128);

        palette.setColor(QPalette::Window, darkBg);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, darkSurface);
        palette.setColor(QPalette::AlternateBase, darkBg);
        palette.setColor(QPalette::ToolTipBase, darkSurface);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        palette.setColor(
          QPalette::Disabled, QPalette::WindowText, disabledText);
        palette.setColor(QPalette::Button, darkSurface);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, accent);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, Qt::white);
    } else {
        const QColor lightBg(243, 243, 243);
        const QColor lightSurface(255, 255, 255);
        const QColor text(26, 26, 26);
        const QColor disabledText(160, 160, 160);

        palette.setColor(QPalette::Window, lightBg);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, lightSurface);
        palette.setColor(QPalette::AlternateBase, lightBg);
        palette.setColor(QPalette::ToolTipBase, lightSurface);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        palette.setColor(
          QPalette::Disabled, QPalette::WindowText, disabledText);
        palette.setColor(QPalette::Button, lightSurface);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, accent);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, Qt::white);
    }
    QApplication::setPalette(palette);
}

} // namespace ThemeUtils
