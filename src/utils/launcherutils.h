// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace launcherutils {

/**
 * Turn a user supplied "Open With" command into a program plus arguments.
 *
 * On Windows the program may be an unquoted path containing spaces, so the
 * command is split at the screenshot placeholder when one is present. `%` and
 * `%1` are both accepted as the file placeholder.
 */
inline QStringList commandArguments(const QString& command,
                                    const QString& filePath)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty() || filePath.isEmpty()) {
        return {};
    }

    const int placeholderPos = trimmed.indexOf(QLatin1Char('%'));
    QString program;
    QStringList arguments;
    bool substituted = false;

    if (placeholderPos >= 0) {
        const QString prefix = trimmed.left(placeholderPos).trimmed();

        if (prefix.startsWith(QLatin1Char('"'))) {
            const int closingQuote = prefix.indexOf(QLatin1Char('"'), 1);
            if (closingQuote > 0) {
                program = prefix.mid(1, closingQuote - 1);
                arguments = QProcess::splitCommand(
                  prefix.mid(closingQuote + 1).trimmed());
            } else {
                program = prefix.mid(1);
            }
        } else if (prefix.contains(QLatin1Char('/')) ||
                   prefix.contains(QLatin1Char('\\'))) {
            // A Windows path may contain spaces. Split off the first flag so
            // it is not swallowed into the program name.
            int splitAt = -1;
            for (int i = 1; i < prefix.size() - 1; ++i) {
                if (prefix.at(i) == QLatin1Char(' ') &&
                    (prefix.at(i + 1) == QLatin1Char('-') ||
                     prefix.at(i + 1) == QLatin1Char('/'))) {
                    splitAt = i;
                    break;
                }
            }
            if (splitAt > 0) {
                program = prefix.left(splitAt);
                arguments =
                  QProcess::splitCommand(prefix.mid(splitAt).trimmed());
            } else {
                program = prefix;
            }
        } else {
            const QStringList parts = QProcess::splitCommand(prefix);
            if (!parts.isEmpty()) {
                program = parts.first();
                arguments = parts.mid(1);
            }
        }

        arguments.append(QProcess::splitCommand(trimmed.mid(placeholderPos)));
        static const QRegularExpression placeholder(QStringLiteral("%1?"));
        for (QString& argument : arguments) {
            const QString before = argument;
            argument.replace(placeholder, filePath);
            substituted = substituted || argument != before;
        }
    } else {
#if defined(Q_OS_WIN)
        program = trimmed;
#else
        const QStringList parts = QProcess::splitCommand(trimmed);
        if (parts.isEmpty()) {
            return {};
        }
        program = parts.first();
        arguments = parts.mid(1);
#endif
    }

    if (program.isEmpty()) {
        return {};
    }
    if (!substituted) {
        arguments.append(filePath);
    }
    arguments.prepend(program);
    return arguments;
}

/**
 * Dedicated subfolder inside the user temp directory for temporary "Open With"
 * screenshot files, keeping them isolated from the shared %TEMP% root.
 */
inline QString openWithTempDir()
{
    return QDir::temp().filePath(QStringLiteral("SlickShot-openwith"));
}

/**
 * Choose a safe working directory for launching an external "Open With"
 * program, avoiding %TEMP% where untrusted DLLs may reside.
 */
inline QString safeWorkingDirectory(const QString& program)
{
    const QFileInfo fi(program);
    if (fi.isAbsolute() && fi.dir().exists()) {
        return fi.absolutePath();
    }
    return QDir::homePath();
}

/**
 * Remove stale temporary screenshot files left behind in the "Open With"
 * folder (for example when SlickShot exited before the cleanup timer fired).
 */
inline int cleanupStaleOpenWithFiles(const QString& dirPath,
                                     qint64 maxAgeSeconds = 60)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        return 0;
    }
    const QDateTime cutoff =
      QDateTime::currentDateTimeUtc().addSecs(-maxAgeSeconds);
    int removed = 0;
    const QFileInfoList entries =
      dir.entryInfoList(QStringList{ QStringLiteral("*.png") },
                        QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo& entry : entries) {
        if (entry.lastModified().toUTC() <= cutoff) {
            if (QFile::remove(entry.absoluteFilePath())) {
                ++removed;
            }
        }
    }
    return removed;
}

} // namespace launcherutils
