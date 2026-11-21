// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "globalshortcutfilter.h"
#include "core/flameshot.h"
#include "utils/confighandler.h"
#include "utils/shortcututils.h"

#include <QApplication>
#include <QElapsedTimer>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <qt_windows.h>

HHOOK GlobalShortcutFilter::s_keyboardHook = nullptr;
GlobalShortcutFilter* GlobalShortcutFilter::s_filterInstance = nullptr;
bool GlobalShortcutFilter::s_winShiftSConsumed = false;
bool GlobalShortcutFilter::s_prntScrConsumed = false;
bool GlobalShortcutFilter::s_replaceSnippingTool = false;
bool GlobalShortcutFilter::s_intlBackslashCapture = false;

LRESULT CALLBACK GlobalShortcutFilter::lowLevelKeyboardProc(int nCode,
                                                            WPARAM wParam,
                                                            LPARAM lParam)
{
    if (nCode == HC_ACTION && s_filterInstance) {
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if ((pKey->flags & LLKHF_INJECTED) != 0) {
            return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
        }

        // A matching WM_KEYUP can be missed (e.g. when triggering a capture
        // changes focus). Expire stale "consumed" flags after a bounded time
        // so the next press is still able to trigger a capture.
        static QElapsedTimer winShiftConsumedAt;
        static QElapsedTimer prntScrConsumedAt;
        static QElapsedTimer intlBackslashConsumedAt;
        static bool intlBackslashConsumed = false;
        constexpr qint64 consumedTimeoutMs = 5000;
        if (intlBackslashConsumed &&
            intlBackslashConsumedAt.hasExpired(consumedTimeoutMs)) {
            intlBackslashConsumed = false;
        }
        if (s_winShiftSConsumed &&
            winShiftConsumedAt.hasExpired(consumedTimeoutMs)) {
            s_winShiftSConsumed = false;
        }
        if (s_prntScrConsumed &&
            prntScrConsumedAt.hasExpired(consumedTimeoutMs)) {
            s_prntScrConsumed = false;
        }

        // Releasing one of the combo modifiers ends the gesture too.
        if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) &&
            (pKey->vkCode == VK_SHIFT || pKey->vkCode == VK_LSHIFT ||
             pKey->vkCode == VK_RSHIFT || pKey->vkCode == VK_LWIN ||
             pKey->vkCode == VK_RWIN || pKey->vkCode == VK_CONTROL ||
             pKey->vkCode == VK_LCONTROL || pKey->vkCode == VK_RCONTROL ||
             pKey->vkCode == VK_MENU || pKey->vkCode == VK_LMENU ||
             pKey->vkCode == VK_RMENU)) {
            s_winShiftSConsumed = false;
            s_prntScrConsumed = false;
            intlBackslashConsumed = false;
        }

        // ISO layouts report the key between Left Shift and Z as VK_OEM_102
        // (scan code 0x56), while Qt maps it to Key_Backslash and QHotkey
        // registers VK_OEM_5. When Ctrl+\  is the capture shortcut, honour the
        // physical key as well so the configured hotkey actually works.
        if (s_intlBackslashCapture && pKey->vkCode == VK_OEM_102) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                const bool ctrlDown =
                  (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool shiftDown =
                  (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                const bool winDown =
                  ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) ||
                  ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
                if (ctrlDown && !shiftDown && !altDown && !winDown) {
                    const bool isRepeat =
                      intlBackslashConsumed &&
                      ((GetAsyncKeyState(VK_OEM_102) & 0x8000) != 0);
                    intlBackslashConsumed = true;
                    intlBackslashConsumedAt.start();
                    if (!isRepeat) {
                        QMetaObject::invokeMethod(
                          qApp,
                          []() {
                              Flameshot::instance()->requestCapture(
                                CaptureRequest(CaptureRequest::GRAPHICAL_MODE));
                          },
                          Qt::QueuedConnection);
                    }
                    return 1;
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (intlBackslashConsumed) {
                    intlBackslashConsumed = false;
                    return 1;
                }
            }
        }

        if (s_replaceSnippingTool &&
            (pKey->vkCode == 'S' || pKey->vkCode == 's' ||
             pKey->vkCode == VK_SNAPSHOT)) {
            bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                           (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
            bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            // Intercept Win + Shift + S (Windows Snipping Tool default
            // shortcut)
            if (pKey->vkCode == 'S' || pKey->vkCode == 's') {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    if (winDown && shiftDown && !ctrlDown && !altDown) {
                        const bool isRepeat =
                          s_winShiftSConsumed &&
                          ((GetAsyncKeyState('S') & 0x8000) != 0);
                        s_winShiftSConsumed = true;
                        winShiftConsumedAt.start();
                        if (!isRepeat) {
                            // Suppress Start Menu without triggering Ctrl+Shift
                            // keyboard layout switches
                            INPUT inputs[2] = {};
                            inputs[0].type = INPUT_KEYBOARD;
                            inputs[0].ki.wVk = 0xFF;
                            inputs[1].type = INPUT_KEYBOARD;
                            inputs[1].ki.wVk = 0xFF;
                            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                            SendInput(2, inputs, sizeof(INPUT));

                            // Asynchronously trigger capture so hook returns
                            // immediately (< 1ms)
                            QMetaObject::invokeMethod(
                              qApp,
                              []() {
                                  Flameshot::instance()->requestCapture(
                                    CaptureRequest(
                                      CaptureRequest::GRAPHICAL_MODE));
                              },
                              Qt::QueuedConnection);
                        }

                        return 1; // Consume key! Windows Snipping Tool never
                                  // receives it
                    }
                } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                    if (s_winShiftSConsumed) {
                        s_winShiftSConsumed = false;
                        return 1;
                    }
                }
            }

            // Intercept PrintScreen (VK_SNAPSHOT)
            if (pKey->vkCode == VK_SNAPSHOT) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    if (!winDown && !ctrlDown && !altDown) {
                        const bool isRepeat =
                          s_prntScrConsumed &&
                          ((GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) != 0);
                        s_prntScrConsumed = true;
                        prntScrConsumedAt.start();
                        if (!isRepeat) {
                            if (shiftDown) {
#ifdef ENABLE_IMGUR
                                QMetaObject::invokeMethod(
                                  qApp,
                                  []() { Flameshot::instance()->history(); },
                                  Qt::QueuedConnection);
#else
                                QMetaObject::invokeMethod(
                                  qApp,
                                  []() {
                                      Flameshot::instance()->requestCapture(
                                        CaptureRequest(
                                          CaptureRequest::GRAPHICAL_MODE));
                                  },
                                  Qt::QueuedConnection);
#endif
                            } else {
                                QMetaObject::invokeMethod(
                                  qApp,
                                  []() {
                                      Flameshot::instance()->requestCapture(
                                        CaptureRequest(
                                          CaptureRequest::GRAPHICAL_MODE));
                                  },
                                  Qt::QueuedConnection);
                            }
                        }
                        return 1; // Consume key!
                    }
                } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                    if (s_prntScrConsumed) {
                        s_prntScrConsumed = false;
                        return 1;
                    }
                }
            }
        }
    }
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}

void GlobalShortcutFilter::syncHotkeys()
{
    const bool needHook = s_replaceSnippingTool || s_intlBackslashCapture;
    if (needHook && s_keyboardHook == nullptr) {
        s_keyboardHook = SetWindowsHookEx(
          WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    } else if (!needHook && s_keyboardHook != nullptr) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }

    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);
    UnregisterHotKey(NULL, 3);
    if (s_replaceSnippingTool) {
        // Fallback hotkeys in case hook is temporarily bypassed
        RegisterHotKey(NULL, 1, 0, VK_SNAPSHOT);
        RegisterHotKey(NULL, 2, MOD_SHIFT, VK_SNAPSHOT);
        RegisterHotKey(NULL, 3, MOD_WIN | MOD_SHIFT, 'S');
    }
}
#endif

GlobalShortcutFilter::GlobalShortcutFilter(QObject* parent)
  : QObject(parent)
{
#if defined(Q_OS_WIN)
    s_filterInstance = this;
    s_replaceSnippingTool = ConfigHandler().replaceSnippingTool();
    s_intlBackslashCapture =
      shortcututils::needsIntlBackslashFallback(QKeySequence(
        ConfigHandler().shortcut(QStringLiteral("TAKE_SCREENSHOT"))));

    syncHotkeys();

    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            [this]() {
                const bool newVal = ConfigHandler().replaceSnippingTool();
                const bool newIntl = shortcututils::needsIntlBackslashFallback(
                  QKeySequence(ConfigHandler().shortcut(
                    QStringLiteral("TAKE_SCREENSHOT"))));
                if (newVal != s_replaceSnippingTool ||
                    newIntl != s_intlBackslashCapture) {
                    s_replaceSnippingTool = newVal;
                    s_intlBackslashCapture = newIntl;
                    s_winShiftSConsumed = false;
                    s_prntScrConsumed = false;
                    syncHotkeys();
                }
            });
#endif
}

GlobalShortcutFilter::~GlobalShortcutFilter()
{
#if defined(Q_OS_WIN)
    if (s_keyboardHook) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }
    s_filterInstance = nullptr;

    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);
    UnregisterHotKey(NULL, 3);
#endif
}

bool GlobalShortcutFilter::nativeEventFilter(const QByteArray& eventType,
                                             void* message,
                                             qintptr* result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)

#if defined(Q_OS_WIN)
    if (eventType != QByteArrayLiteral("windows_generic_MSG") &&
        eventType != QByteArrayLiteral("windows_dispatcher_MSG")) {
        return false;
    }
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_KILLFOCUS) {
        // Focus loss means a matching keyup may never arrive.
        s_winShiftSConsumed = false;
        s_prntScrConsumed = false;
    }
    if (msg->message == WM_HOTKEY) {
        const quint32 keycode = HIWORD(msg->lParam);
        const quint32 modifiers = LOWORD(msg->lParam);

        if (s_replaceSnippingTool) {
            if (('S' == keycode || 's' == keycode) &&
                ((modifiers & MOD_WIN) && (modifiers & MOD_SHIFT))) {
                Flameshot::instance()->requestCapture(
                  CaptureRequest(CaptureRequest::GRAPHICAL_MODE));
                return true;
            }

#ifdef ENABLE_IMGUR
            // Show screenshots history
            if (VK_SNAPSHOT == keycode && MOD_SHIFT == modifiers) {
                Flameshot::instance()->history();
                return true;
            }
#endif
            // Capture screen
            if (VK_SNAPSHOT == keycode && 0 == modifiers) {
                Flameshot::instance()->requestCapture(
                  CaptureRequest(CaptureRequest::GRAPHICAL_MODE));
                return true;
            }
        }
    }
#endif
    return false; // Forward event to Qt
}
