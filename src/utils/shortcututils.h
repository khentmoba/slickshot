// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeyCombination>
#include <QKeyEvent>
#include <QKeySequence>

namespace shortcututils {

/**
 * Build a shortcut from one key event without going through a string.
 *
 * The old approach concatenated modifier prefixes into a member string that
 * survived across key presses, and relied on QKeySequence parsing the result.
 * Both are unreliable for combinations such as Ctrl+Shift+PrintScreen.
 */
inline QKeySequence sequenceFromKeyEvent(QKeyEvent* event)
{
    if (event == nullptr) {
        return QKeySequence();
    }

    Qt::KeyboardModifiers modifiers = event->modifiers();
    // Qt does not always report the Meta modifier when the Win key itself is
    // the key being pressed.
    if (event->key() == Qt::Key_Meta) {
        modifiers |= Qt::MetaModifier;
    }

    return QKeySequence(
      QKeyCombination(modifiers, static_cast<Qt::Key>(event->key())));
}

/**
 * True when the configured shortcut is Ctrl+\. On ISO layouts Qt reports the
 * physical IntlBackslash key (SC056 / VK_OEM_102) as Key_Backslash, which
 * QHotkey maps to VK_OEM_5. Such users need an additional native fallback.
 */
inline bool needsIntlBackslashFallback(const QKeySequence& sequence)
{
    return sequence == QKeySequence(Qt::ControlModifier | Qt::Key_Backslash);
}

} // namespace shortcututils
