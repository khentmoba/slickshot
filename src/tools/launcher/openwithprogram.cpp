// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "openwithprogram.h"

#if defined(Q_OS_WIN)
#include "utils/confighandler.h"
#include "utils/filenamehandler.h"
#include "utils/launcherutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <windows.h>
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x601
#endif
#include <Shlobj.h>

#pragma comment(lib, "Shell32.lib")

namespace {

constexpr int kOpenWithTempCleanupMs = 60 * 1000;

} // namespace
#else
#include "tools/launcher/applauncherwidget.h"
#endif

void showOpenWithMenu(const QPixmap& capture)
{
#if defined(Q_OS_WIN)
    QString tempFile =
      FileNameHandler().properScreenshotPath(QDir::tempPath(), "png");
    bool ok = capture.save(tempFile);
    if (!ok) {
        QMessageBox::about(nullptr,
                           QObject::tr("Error"),
                           QObject::tr("Unable to write in") +
                             QDir::tempPath());
        return;
    }

    const QString defaultApp = ConfigHandler().openWithDefaultApp();
    if (!defaultApp.trimmed().isEmpty()) {
        const QStringList arguments =
          launcherutils::commandArguments(defaultApp, tempFile);
        if (!arguments.isEmpty()) {
            const QString program = arguments.first();
            const bool launched = QProcess::startDetached(
              program, arguments.mid(1), QFileInfo(tempFile).absolutePath());
            if (launched) {
                // Give the external application time to open the file.
                QTimer::singleShot(kOpenWithTempCleanupMs,
                                   [tempFile]() { QFile::remove(tempFile); });
                return;
            }
        }
        QMessageBox::about(nullptr,
                           QObject::tr("Error"),
                           QObject::tr("Unable to launch application."));
    }

    OPENASINFO info;
    auto wStringFile = tempFile.replace("/", "\\").toStdWString();
    info.pcszFile = wStringFile.c_str();
    info.pcszClass = nullptr;
    info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_EXEC;
    SHOpenWithDialog(nullptr, &info);
#else
    auto* w = new AppLauncherWidget(capture);
    w->show();
#endif
}
