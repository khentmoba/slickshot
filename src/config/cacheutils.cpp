// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Jeremy Borgman

#include "cacheutils.h"
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QRect>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>

namespace {
constexpr QDataStream::Version kRegionStreamVersion = QDataStream::Qt_6_0;
}

QString getCachePath()
{
    auto cachePath =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QDir(cachePath).exists()) {
        QDir().mkpath(cachePath);
    }
    return cachePath;
}

void setLastRegion(QRect const& newRegion)
{
    auto cachePath = getCachePath() + "/region.txt";

    QSaveFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out.setVersion(kRegionStreamVersion);
        out << newRegion;
        if (out.status() == QDataStream::Ok) {
            file.commit();
        } else {
            file.cancelWriting();
        }
    }
}

QRect getLastRegion()
{
    auto cachePath = getCachePath() + "/region.txt";
    QFile file(cachePath);

    QRect lastRegion;
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream input(&file);
        input.setVersion(kRegionStreamVersion);
        input >> lastRegion;
        file.close();
        if (input.status() != QDataStream::Ok || !lastRegion.isValid()) {
            lastRegion = QRect();
        }
    } else {
        lastRegion = QRect(0, 0, 0, 0);
    }

    return lastRegion;
}
