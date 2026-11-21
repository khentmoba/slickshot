// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

static void writeAll(HANDLE handle, const char* data, DWORD size)
{
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD offset = 0;
    while (offset < size) {
        DWORD written = 0;
        if (!WriteFile(handle, data + offset, size - offset, &written, NULL) ||
            written == 0) {
            break;
        }
        offset += written;
    }
}

static void relayPipe(HANDLE readHandle, HANDLE destHandle)
{
    char buffer[4096];
    DWORD n = 0;
    while (ReadFile(readHandle, buffer, sizeof(buffer), &n, NULL) && n > 0) {
        writeAll(destHandle, buffer, n);
    }
}

// Quote a single argument per the CommandLineToArgvW rules so that spaces,
// quotes, and backslashes are preserved as literal data.
static void appendQuotedArg(std::wstring& cmdline, const std::wstring& arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        cmdline.append(arg);
        return;
    }

    cmdline.push_back(L'"');
    for (auto it = arg.begin();; ++it) {
        unsigned backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }

        if (it == arg.end()) {
            cmdline.append(backslashes * 2, L'\\');
            break;
        } else if (*it == L'"') {
            cmdline.append(backslashes * 2 + 1, L'\\');
            cmdline.push_back(*it);
        } else {
            cmdline.append(backslashes, L'\\');
            cmdline.push_back(*it);
        }
    }
    cmdline.push_back(L'"');
}

// Launch flameshot.exe (located next to this wrapper) with argv[1..],
// forwarding each argument as literal data. flameshot.exe is passed via
// lpApplicationName so no command interpreter is involved and shell
// metacharacters have no effect. When wait is true, the child's stdout is
// captured and relayed. Returns child's process exit code. If wait==false,
// returns 0 (child running).
int CallFlameshot(int argc, wchar_t* argv[], bool wait)
{
    // Full path to flameshot.exe, in the same directory as this executable.
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return static_cast<int>(GetLastError());
    }

    std::wstring pathstring(path);
    size_t lastBackslash = pathstring.find_last_of(L'\\');
    std::wstring directory = (lastBackslash != std::wstring::npos)
                               ? pathstring.substr(0, lastBackslash + 1)
                               : L"";
    std::wstring exePath = directory + L"flameshot.exe";

    // Build the command line with each argument individually quoted.
    std::wstring cmdline;
    appendQuotedArg(cmdline, exePath);
    for (int i = 1; i < argc; ++i) {
        cmdline.push_back(L' ');
        appendQuotedArg(cmdline, argv[i]);
    }
    std::vector<wchar_t> mutableCmd(cmdline.begin(), cmdline.end());
    mutableCmd.push_back(L'\0');

    HANDLE outRead = NULL, outWrite = NULL;
    HANDLE errRead = NULL, errWrite = NULL;
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    if (wait) {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
            return static_cast<int>(GetLastError());
        }
        SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

        if (!CreatePipe(&errRead, &errWrite, &sa, 0)) {
            const DWORD err = GetLastError();
            CloseHandle(outRead);
            CloseHandle(outWrite);
            return static_cast<int>(err);
        }
        SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = outWrite;
        si.hStdError = errWrite;
    }

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
      exePath.c_str(),
      mutableCmd.data(),
      NULL,
      NULL,
      wait ? TRUE : FALSE, // only inherit handles when we created pipes
      0,
      NULL,
      NULL,
      &si,
      &pi);

    if (outWrite) {
        CloseHandle(outWrite);
        outWrite = NULL;
    }
    if (errWrite) {
        CloseHandle(errWrite);
        errWrite = NULL;
    }

    if (!ok) {
        const DWORD err = GetLastError();
        if (outRead) {
            CloseHandle(outRead);
        }
        if (errRead) {
            CloseHandle(errRead);
        }
        return static_cast<int>(err);
    }

    int exitCode = 0;

    if (wait) {
        // Relay child's stdout and stderr separately as raw bytes so binary
        // outputs (such as --raw PNGs) are not altered by text-mode CR/LF
        // translation or mixed with diagnostic messages.
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        const HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        const HANDLE stdErr = GetStdHandle(STD_ERROR_HANDLE);

        std::thread errThread(relayPipe, errRead, stdErr);
        relayPipe(outRead, stdOut);
        errThread.join();

        CloseHandle(outRead);
        CloseHandle(errRead);
        outRead = NULL;
        errRead = NULL;

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) {
            exitCode = static_cast<int>(code);
        } else {
            exitCode = static_cast<int>(GetLastError());
        }
    } else {
        // Not waiting: Exit code can't be known
        exitCode = 0;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
}

// Console 'wrapper' for flameshot on windows
int wmain(int argc, wchar_t* argv[])
{
    // if no args, do not wait for stdout => return 0.
    // If args exist, wait and return flameshot's exit code.
    if (argc == 1) {
        std::cout << "Starting flameshot in daemon mode" << std::endl;
        int code = CallFlameshot(argc, argv, false);
        std::cout.flush();
        return code;
    } else {
        int code = CallFlameshot(argc, argv, true);
        std::cout.flush();
        return code;
    }
}
