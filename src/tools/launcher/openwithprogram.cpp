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
#include <QTemporaryFile>
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
    const QString openWithDir = launcherutils::openWithTempDir();
    if (!QDir().mkpath(openWithDir)) {
        QMessageBox::about(nullptr,
                           QObject::tr("Error"),
                           QObject::tr("Unable to write in ") +
                             QDir::toNativeSeparators(openWithDir));
        return;
    }
    QString baseName = QFileInfo(FileNameHandler().parsedPattern()).fileName();
    if (baseName.isEmpty() || baseName == QLatin1String(".") ||
        baseName == QLatin1String("..")) {
        baseName = QStringLiteral("screenshot");
    }
    QTemporaryFile tmpFile(
      QDir(openWithDir).filePath(baseName + QStringLiteral("_XXXXXX.png")));
    tmpFile.setAutoRemove(false);
    const bool opened = tmpFile.open();
    const QString tempFile = opened ? tmpFile.fileName() : QString();
    const bool ok = opened && capture.save(&tmpFile, "PNG");
    tmpFile.close();
    if (!ok) {
        if (!tempFile.isEmpty()) {
            QFile::remove(tempFile);
        }
        QMessageBox::about(nullptr,
                           QObject::tr("Error"),
                           QObject::tr("Unable to write in ") +
                             QDir::toNativeSeparators(openWithDir));
        return;
    }

    const QString defaultApp = ConfigHandler().openWithDefaultApp();
    if (!defaultApp.trimmed().isEmpty()) {
        const QStringList arguments =
          launcherutils::commandArguments(defaultApp, tempFile);
        if (!arguments.isEmpty()) {
            const QString program = arguments.first();
            const bool launched = QProcess::startDetached(
              program,
              arguments.mid(1),
              launcherutils::safeWorkingDirectory(program));
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

    OPENASINFO info{};
    const auto wStringFile = QDir::toNativeSeparators(tempFile).toStdWString();
    info.pcszFile = wStringFile.c_str();
    info.pcszClass = nullptr;
    info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_EXEC;
    const HRESULT hr = SHOpenWithDialog(nullptr, &info);
    if (SUCCEEDED(hr)) {
        QTimer::singleShot(kOpenWithTempCleanupMs,
                           [tempFile]() { QFile::remove(tempFile); });
    } else {
        QFile::remove(tempFile);
    }
#else
    auto* w = new AppLauncherWidget(capture);
    w->show();
#endif
}
