// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

} // namespace launcherutils
