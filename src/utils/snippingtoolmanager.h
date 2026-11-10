// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class SnippingToolManager
{
public:
    static bool commandMatchesAppPath(const QString& command,
                                      const QString& expectedAppPath);

#if defined(Q_OS_WIN)
    // Check if SlickShot is currently registered to replace Windows Snipping
    // Tool
    static bool isSnippingToolReplaced();

    // Configure registry and system settings to replace Snipping Tool
    static bool replaceSnippingTool();

    // Restore Windows default settings
    static bool restoreSnippingTool();

    // Auto-configure on app startup if enabled in settings
    static void autoConfigureOnStartup();

    // Check / toggle PrintScreen hijacking by Windows 11 Snipping Tool
    static bool isPrintScreenHijackDisabled();
    static bool disablePrintScreenHijack(bool disable = true);

    // Check / register ms-screenclip protocol handler
    static bool isMsScreenclipRegistered();
    static bool registerMsScreenclip(bool reg = true);

    // Check / register SnippingTool.exe and ScreenClippingHost.exe App Paths
    static bool isAppPathsRegistered();
    static bool registerAppPaths(bool reg = true);
#else
    static bool isSnippingToolReplaced() { return false; }
    static bool replaceSnippingTool() { return false; }
    static bool restoreSnippingTool() { return false; }
    static void autoConfigureOnStartup() {}
    static bool isPrintScreenHijackDisabled() { return false; }
    static bool disablePrintScreenHijack(bool = true) { return false; }
    static bool isMsScreenclipRegistered() { return false; }
    static bool registerMsScreenclip(bool = true) { return false; }
    static bool isAppPathsRegistered() { return false; }
    static bool registerAppPaths(bool = true) { return false; }
#endif
};
