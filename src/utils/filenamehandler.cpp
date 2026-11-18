// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "filenamehandler.h"
#include "utils/abstractlogger.h"
#include "utils/confighandler.h"
#include "utils/strfparse.h"

#include <QDir>
#if defined(Q_OS_WIN)
#include <QSet>
#endif
#include <exception>
#include <locale>

#if defined(Q_OS_WIN)
namespace {

// Windows reserves these device names (with any extension) regardless of case.
bool isWindowsReservedName(const QString& name)
{
    const QString base = name.section(QLatin1Char('.'), 0, 0).toUpper();
    static const QSet<QString> reserved{
        QStringLiteral("CON"),    QStringLiteral("PRN"),
        QStringLiteral("AUX"),    QStringLiteral("NUL"),
        QStringLiteral("CONIN$"), QStringLiteral("CONOUT$")
    };
    if (reserved.contains(base)) {
        return true;
    }
    if (base.size() == 4 && (base.startsWith(QLatin1String("COM")) ||
                             base.startsWith(QLatin1String("LPT")))) {
        const QChar digit = base.at(3);
        return digit >= QChar('1') && digit <= QChar('9');
    }
    return false;
}

} // namespace
#endif

namespace {
// Replaces the directory separators entered in the filename pattern while it is
// formatted, to tell them apart from the slashes produced by strftime
// expansions such as %D (ASCII unit separator, not expected in a pattern)
const QChar PATTERN_DIR_SEPARATOR(0x1F);
} // namespace

FileNameHandler::FileNameHandler(QObject* parent)
  : QObject(parent)
{
    auto err = AbstractLogger::error(AbstractLogger::Stderr);
    try {
        std::locale::global(std::locale());
    } catch (std::exception&) {
        err << "Locales on your system are not properly configured. Falling "
               "back to defaults";

        std::locale::global(std::locale("en_US.UTF-8"));
    }
}

QString FileNameHandler::parsedPattern()
{
    return parseFilename(ConfigHandler().filenamePattern());
}

QString FileNameHandler::parseFilename(const QString& name)
{
    QString res = name;
    if (name.isEmpty()) {
        res = ConfigHandler().filenamePatternDefault();
    }

    // remove trailing characters '%' in the pattern
    while (res.endsWith('%')) {
        res.chop(1);
    }

    // directory separators may be '/', and also '\' on Windows
    QString pattern = QDir::fromNativeSeparators(res);
    // Directory patterns must stay relative to the save directory. This also
    // rejects root-relative ("\dir") and drive-relative ("C:dir\") Windows
    // paths. Only patterns with a separator are checked, so filename-only
    // patterns such as "C:%F" keep working as before.
    if (pattern.contains('/') && !QDir::isRelativePath(pattern)) {
        return {};
    }
    if (pattern.endsWith('/')) {
        // only directories are given, use the default file name inside them
        pattern += ConfigHandler().filenamePatternDefault();
    }

    // format the whole pattern at once so all its parts share the same time
    pattern.replace('/', PATTERN_DIR_SEPARATOR);
    res = QString::fromStdString(
      strfparse::format_time_string(pattern.toStdString()));

    // add the parsed pattern in a correct format for the filesystem, as a path
    // relative to the save directory. Separators produced by the specifiers are
    // part of a name and get sanitized.
    QStringList parts;
    for (QString part : res.split(PATTERN_DIR_SEPARATOR, Qt::SkipEmptyParts)) {
        part = QDir::fromNativeSeparators(part)
                 .replace(QLatin1String("/"), QStringLiteral("⁄"))
                 .replace(QLatin1String(":"), QLatin1String("-"));
        // Control characters are not valid in file names on any platform.
        for (int i = 0; i < part.size(); ++i) {
            const ushort code = part.at(i).unicode();
            if (code < 0x20 || (code >= 0x7f && code < 0xa0)) {
                part[i] = QChar('_');
            }
        }
        if (part == QLatin1String(".") || part == QLatin1String("..")) {
            // not a name, and ".." would leave the save directory
            return {};
        }
#if defined(Q_OS_WIN)
        // A bare reserved device name is not a usable path component on Windows.
        if (isWindowsReservedName(part)) {
            part.prepend(QLatin1Char('_'));
        }
#endif
        parts << part;
    }
    return parts.join('/');
}

/**
 * @brief Generate a valid destination path from the possibly incomplete `path`.
 * The input `path` can be one of:
 * - empty string
 * - an existing directory
 * - a file in an existing directory
 * In each case, the output path will be an absolute path to a file with a
 * suffix matching the specified `format`.
 * @note
 * - If `path` points to a directory, the file name will be generated from the
 *   formatted file name from the user configuration
 * - If `path` points to a file, its suffix will be changed to match `format`
 * - If `format` is not given, the suffix will remain untouched, unless `path`
 *   has no suffix, in which case it will be given the "png" suffix
 * - If the path generated by the previous steps points to an existing file,
 *   "_NUM" will be appended to its base name, where NUM is the first
 * available number that produces a non-existent path (starting from 1).
 * @param path Possibly incomplete file name to transform
 * @param format Desired output file suffix (excluding an initial '.' character)
 */
QString FileNameHandler::properScreenshotPath(QString path,
                                              const QString& format)
{
    QFileInfo info(path);
    QString suffix;

    if (info.isDir()) {
        // path is a directory => generate filename from configured pattern.
        // The directory's own name must not contribute a suffix: a directory
        // called "shots.v2" would otherwise yield "<pattern>.v2".
        QString name = parsedPattern();
        if (name.isEmpty()) {
            AbstractLogger::error()
              << tr("Invalid filename pattern '%1': it must be a relative "
                    "path without '.' or '..' parts")
                   .arg(ConfigHandler().filenamePattern());
            return {};
        }
        path = QDir(QDir(path).absolutePath() + "/" + name).path();
    } else {
        // path points to a file => strip it of its suffix for now
        suffix = info.suffix();
        path = QDir(info.dir().absolutePath() + "/" + info.completeBaseName())
                 .path();
    }

    if (!format.isEmpty()) {
        // Override suffix to match format
        path += "." + format;
    } else if (!suffix.isEmpty()) {
        // Leave the suffix as it was
        path += "." + suffix;
    } else {
        path += ".png";
    }

    if (!QFileInfo::exists(path)) {
        return path;
    } else {
        return autoNumerateDuplicate(path);
    }
}

QString FileNameHandler::autoNumerateDuplicate(const QString& path)
{
    // add numeration in case of repeated filename in the directory
    // find unused name adding _n where n is a number
    QFileInfo checkFile(path);
    QString directory = checkFile.dir().absolutePath(),
            filename = checkFile.completeBaseName(),
            suffix = checkFile.suffix();
    if (!suffix.isEmpty()) {
        suffix = QStringLiteral(".") + suffix;
    }
    if (checkFile.exists()) {
        filename += QLatin1String("_");
        int i = 1;
        while (true) {
            checkFile.setFile(directory + "/" + filename + QString::number(i) +
                              suffix);
            if (!checkFile.exists()) {
                filename += QString::number(i);
                break;
            }
            ++i;
        }
    }
    return checkFile.filePath();
}
