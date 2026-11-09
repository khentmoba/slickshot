// SPDX-License-Identifier: GPL-3.0-or-later
#include "snippingtoolmanager.h"
#include "confighandler.h"

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <qt_windows.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <string>

namespace {

struct ScopedHKey
{
    HKEY key = nullptr;
    ScopedHKey() = default;
    explicit ScopedHKey(HKEY k)
      : key(k)
    {}
    ~ScopedHKey()
    {
        if (key) {
            RegCloseKey(key);
        }
    }
    ScopedHKey(const ScopedHKey&) = delete;
    ScopedHKey& operator=(const ScopedHKey&) = delete;
    ScopedHKey(ScopedHKey&& other) noexcept
      : key(other.key)
    {
        other.key = nullptr;
    }
    ScopedHKey& operator=(ScopedHKey&& other) noexcept
    {
        if (this != &other) {
            if (key) {
                RegCloseKey(key);
            }
            key = other.key;
            other.key = nullptr;
        }
        return *this;
    }
    operator HKEY() const { return key; }
    HKEY* get() { return &key; }
};

bool setRegString(HKEY root,
                  const std::wstring& subKey,
                  const std::wstring& valueName,
                  const std::wstring& data)
{
    ScopedHKey hKey;
    LSTATUS status = RegCreateKeyExW(root,
                                     subKey.c_str(),
                                     0,
                                     nullptr,
                                     REG_OPTION_NON_VOLATILE,
                                     KEY_SET_VALUE,
                                     nullptr,
                                     hKey.get(),
                                     nullptr);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* valName = valueName.empty() ? nullptr : valueName.c_str();
    status =
      RegSetValueExW(hKey,
                     valName,
                     0,
                     REG_SZ,
                     reinterpret_cast<const BYTE*>(data.c_str()),
                     static_cast<DWORD>((data.length() + 1) * sizeof(wchar_t)));
    return status == ERROR_SUCCESS;
}

bool setRegDword(HKEY root,
                 const std::wstring& subKey,
                 const std::wstring& valueName,
                 DWORD data)
{
    ScopedHKey hKey;
    LSTATUS status = RegCreateKeyExW(root,
                                     subKey.c_str(),
                                     0,
                                     nullptr,
                                     REG_OPTION_NON_VOLATILE,
                                     KEY_SET_VALUE,
                                     nullptr,
                                     hKey.get(),
                                     nullptr);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* valName = valueName.empty() ? nullptr : valueName.c_str();
    status = RegSetValueExW(hKey,
                            valName,
                            0,
                            REG_DWORD,
                            reinterpret_cast<const BYTE*>(&data),
                            sizeof(DWORD));
    return status == ERROR_SUCCESS;
}

bool getRegString(HKEY root,
                  const std::wstring& subKey,
                  const std::wstring& valueName,
                  std::wstring& out)
{
    ScopedHKey hKey;
    LSTATUS status =
      RegOpenKeyExW(root, subKey.c_str(), 0, KEY_QUERY_VALUE, hKey.get());
    if (status != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* valName = valueName.empty() ? nullptr : valueName.c_str();
    wchar_t buffer[1024] = { 0 };
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    status = RegQueryValueExW(
      hKey, valName, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size);
    if (status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        out = buffer;
        return true;
    }
    return false;
}

bool getRegDword(HKEY root,
                 const std::wstring& subKey,
                 const std::wstring& valueName,
                 DWORD& out)
{
    ScopedHKey hKey;
    LSTATUS status =
      RegOpenKeyExW(root, subKey.c_str(), 0, KEY_QUERY_VALUE, hKey.get());
    if (status != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* valName = valueName.empty() ? nullptr : valueName.c_str();
    DWORD val = 0;
    DWORD size = sizeof(val);
    DWORD type = 0;
    status = RegQueryValueExW(
      hKey, valName, nullptr, &type, reinterpret_cast<BYTE*>(&val), &size);
    if (status == ERROR_SUCCESS && type == REG_DWORD) {
        out = val;
        return true;
    }
    return false;
}

void deleteRegTree(HKEY root, const std::wstring& subKey)
{
    RegDeleteTreeW(root, subKey.c_str());
}

void deleteRegValue(HKEY root,
                    const std::wstring& subKey,
                    const std::wstring& valueName)
{
    ScopedHKey hKey;
    if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_SET_VALUE, hKey.get()) ==
        ERROR_SUCCESS) {
        RegDeleteValueW(hKey, valueName.c_str());
    }
}

std::wstring currentAppPath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
      .toStdWString();
}

std::wstring currentAppDir()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationDirPath())
      .toStdWString();
}

} // namespace

bool SnippingToolManager::isPrintScreenHijackDisabled()
{
    DWORD val = 1;
    if (getRegDword(HKEY_CURRENT_USER,
                    L"Control Panel\\Keyboard",
                    L"PrintScreenKeyForSnippingEnabled",
                    val)) {
        return val == 0;
    }
    // If not set, Windows 11 defaults to enabling Snipping Tool on PrintScreen
    return false;
}

bool SnippingToolManager::disablePrintScreenHijack(bool disable)
{
    if (disable) {
        return setRegDword(HKEY_CURRENT_USER,
                           L"Control Panel\\Keyboard",
                           L"PrintScreenKeyForSnippingEnabled",
                           0);
    } else {
        deleteRegValue(HKEY_CURRENT_USER,
                       L"Control Panel\\Keyboard",
                       L"PrintScreenKeyForSnippingEnabled");
        return true;
    }
}

bool SnippingToolManager::isMsScreenclipRegistered()
{
    std::wstring cmd;
    if (!getRegString(HKEY_CURRENT_USER,
                      L"Software\\Classes\\ms-screenclip\\shell\\open\\command",
                      L"",
                      cmd)) {
        return false;
    }

    std::wstring appPath = currentAppPath();
    if (cmd.find(appPath) == std::wstring::npos &&
        cmd.find(L"flameshot") == std::wstring::npos &&
        cmd.find(L"slickshot") == std::wstring::npos) {
        return false;
    }
    return true;
}

bool SnippingToolManager::registerMsScreenclip(bool reg)
{
    if (reg) {
        std::wstring appPath = currentAppPath();
        std::wstring openCmd = L"\"" + appPath + L"\" gui \"%1\"";

        bool ok = true;
        // 1. Direct ms-screenclip protocol handler
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Classes\\ms-screenclip",
                           L"",
                           L"URL:ms-screenclip");
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Classes\\ms-screenclip",
                           L"URL Protocol",
                           L"");
        ok &= setRegString(
          HKEY_CURRENT_USER,
          L"Software\\Classes\\ms-screenclip\\shell\\open\\command",
          L"",
          openCmd);

        // 2. SlickShot.ms-screenclip ProgID
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Classes\\SlickShot.ms-screenclip",
                           L"",
                           L"SlickShot Screen Clip");
        ok &= setRegString(
          HKEY_CURRENT_USER,
          L"Software\\Classes\\SlickShot.ms-screenclip\\shell\\open\\command",
          L"",
          openCmd);

        // 3. Capabilities & URLAssociations
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\SlickShot\\Capabilities",
                           L"ApplicationName",
                           L"SlickShot");
        ok &=
          setRegString(HKEY_CURRENT_USER,
                       L"Software\\SlickShot\\Capabilities",
                       L"ApplicationDescription",
                       L"Fast modern replacement for Windows Snipping Tool");
        ok &=
          setRegString(HKEY_CURRENT_USER,
                       L"Software\\SlickShot\\Capabilities\\URLAssociations",
                       L"ms-screenclip",
                       L"SlickShot.ms-screenclip");

        // 4. RegisteredApplications
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\RegisteredApplications",
                           L"SlickShot",
                           L"Software\\SlickShot\\Capabilities");

        return ok;
    } else {
        deleteRegTree(HKEY_CURRENT_USER, L"Software\\Classes\\ms-screenclip");
        deleteRegTree(HKEY_CURRENT_USER,
                      L"Software\\Classes\\SlickShot.ms-screenclip");
        deleteRegTree(HKEY_CURRENT_USER, L"Software\\SlickShot");
        deleteRegValue(
          HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"SlickShot");
        return true;
    }
}

bool SnippingToolManager::isAppPathsRegistered()
{
    std::wstring cmd;
    if (!getRegString(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                      L"Paths\\SnippingTool.exe",
                      L"",
                      cmd)) {
        return false;
    }

    std::wstring appPath = currentAppPath();
    if (cmd.find(appPath) == std::wstring::npos &&
        cmd.find(L"flameshot") == std::wstring::npos &&
        cmd.find(L"slickshot") == std::wstring::npos) {
        return false;
    }
    return true;
}

bool SnippingToolManager::registerAppPaths(bool reg)
{
    if (reg) {
        std::wstring appPath = currentAppPath();
        std::wstring appDir = currentAppDir();
        std::wstring quotedAppPath = L"\"" + appPath + L"\"";

        bool ok = true;
        // SnippingTool.exe redirection
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                           L"Paths\\SnippingTool.exe",
                           L"",
                           quotedAppPath);
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                           L"Paths\\SnippingTool.exe",
                           L"Path",
                           appDir);

        // ScreenClippingHost.exe redirection
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                           L"Paths\\ScreenClippingHost.exe",
                           L"",
                           quotedAppPath);
        ok &= setRegString(HKEY_CURRENT_USER,
                           L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                           L"Paths\\ScreenClippingHost.exe",
                           L"Path",
                           appDir);

        return ok;
    } else {
        deleteRegTree(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                      L"Paths\\SnippingTool.exe");
        deleteRegTree(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\App "
                      L"Paths\\ScreenClippingHost.exe");
        return true;
    }
}

bool SnippingToolManager::isSnippingToolReplaced()
{
    return isPrintScreenHijackDisabled() && isMsScreenclipRegistered() &&
           isAppPathsRegistered();
}

bool SnippingToolManager::replaceSnippingTool()
{
    bool ok = true;
    ok &= disablePrintScreenHijack(true);
    ok &= registerMsScreenclip(true);
    ok &= registerAppPaths(true);
    return ok;
}

bool SnippingToolManager::restoreSnippingTool()
{
    disablePrintScreenHijack(false);
    registerMsScreenclip(false);
    registerAppPaths(false);
    return true;
}

void SnippingToolManager::autoConfigureOnStartup()
{
    ConfigHandler config;
    if (config.replaceSnippingTool()) {
        if (!isSnippingToolReplaced()) {
            replaceSnippingTool();
        }
        if (config.startupLaunch()) {
            config.ensureStartupLaunch(true);
        }
    }
}

#endif // Q_OS_WIN
