// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <qt_windows.h>
#endif

class GlobalShortcutFilter
  : public QObject
  , public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalShortcutFilter(QObject* parent = nullptr);
    ~GlobalShortcutFilter() override;

    bool nativeEventFilter(const QByteArray& eventType,
                           void* message,
                           qintptr* result) override;

private:
#if defined(Q_OS_WIN)
    static LRESULT CALLBACK lowLevelKeyboardProc(int nCode,
                                                 WPARAM wParam,
                                                 LPARAM lParam);
    static HHOOK s_keyboardHook;
    static GlobalShortcutFilter* s_filterInstance;
    static bool s_winShiftSConsumed;
    static bool s_prntScrConsumed;
    static bool s_replaceSnippingTool;
#endif
    quint32 getNativeModifier(Qt::KeyboardModifiers modifiers);
    quint32 nativeKeycode(Qt::Key key);
    bool registerShortcut(quint32 nativeKey, quint32 nativeMods);
    bool unregisterShortcut(quint32 nativeKey, quint32 nativeMods);
};
