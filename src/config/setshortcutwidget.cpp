// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Yurii Puchkov at Namecheap & Contributors

#include "setshortcutwidget.h"
#include "utils/globalvalues.h"
#include "utils/shortcututils.h"

#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QTimer>

SetShortcutDialog::SetShortcutDialog(QDialog* parent,
                                     const QString& shortcutName)
  : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Set Shortcut"));
    m_ks = QKeySequence();

    m_layout = new QVBoxLayout(this);
    m_layout->setAlignment(Qt::AlignHCenter);

    auto* infoTop = new QLabel(tr("Enter new shortcut to change "));
    infoTop->setMargin(10);
    infoTop->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(infoTop);

    auto* infoIcon = new QLabel();
    infoIcon->setAlignment(Qt::AlignCenter);
    infoIcon->setPixmap(QPixmap(":/img/app/keyboard.svg"));
    m_layout->addWidget(infoIcon);

    m_layout->addWidget(infoIcon);

    QString msg = "";
#if defined(Q_OS_MACOS)
    msg = tr(
      "Press Esc to cancel or ⌘+Backspace to disable the keyboard shortcut.");
#else
    msg =
      tr("Press Esc to cancel or Backspace to disable the keyboard shortcut.");
#endif

    auto restartMessageAdded = false;
    if ((shortcutName == "TAKE_SCREENSHOT" ||
         shortcutName == "TAKE_SCREENSHOT_INSTANT" ||
         shortcutName == "TAKE_SCREENSHOT_NO_OVERLAY") &&
        restartMessageAdded == false) {
        msg +=
          "\n" + tr("SlickShot must be restarted for changes to take effect.");
        restartMessageAdded = true;
    }
    if (shortcutName == "SCREENSHOT_HISTORY" && restartMessageAdded == false) {
        msg +=
          "\n" + tr("SlickShot must be restarted for changes to take effect.");
        restartMessageAdded = true;
    }

    auto* infoBottom = new QLabel(msg);
    infoBottom->setMargin(10);
    infoBottom->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(infoBottom);

    // 0ms Delay: Event loop waits until after show(); widget fully initialized
    QTimer::singleShot(0, this, &SetShortcutDialog::startCapture);
}

void SetShortcutDialog::startCapture()
{
    grabKeyboard(); // Call AFTER show()!
    setFocus();
}

const QKeySequence& SetShortcutDialog::shortcut()
{
    return m_ks;
}

void SetShortcutDialog::keyPressEvent(QKeyEvent* ke)
{
    // Build the sequence from this event only. Accumulating modifiers across
    // events produced sequences such as "Ctrl+Shift+Ctrl+Shift+Print" and made
    // combinations like Ctrl+Shift+PrintScreen impossible to assign.
    m_ks = shortcututils::sequenceFromKeyEvent(ke);
}

void SetShortcutDialog::keyReleaseEvent(QKeyEvent* event)
{
    if (m_ks == QKeySequence(Qt::Key_Escape)) {
        reject();
    }
    accept();
}

void SetShortcutDialog::accept()
{
    releaseKeyboard();
    QDialog::accept();
}

void SetShortcutDialog::reject()
{
    releaseKeyboard();
    QDialog::reject();
}
