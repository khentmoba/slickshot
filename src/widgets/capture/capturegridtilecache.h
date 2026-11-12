// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 SlickShot contributors

#pragma once

#include <QImage>
#include <QPointF>
#include <QtGlobal>

class CaptureGridTileCache
{
public:
    struct Key
    {
        QPointF limit;
        qreal dpr = 1.0;
        qreal scale = 1.0;
        int gridSize = 0;
        QRgb color = 0;
        bool operator==(const Key&) const = default;
    };

    enum class Request
    {
        DrawDirect,
        BuildTile,
        UseCachedTile,
    };

    Request request(const Key& key)
    {
        if (!m_tile.isNull() && key == m_tileKey) {
            return Request::UseCachedTile;
        }
        m_tile = QImage();
        if (m_pending && key == m_pendingKey) {
            m_tileKey = key;
            m_pending = false;
            return Request::BuildTile;
        }
        m_pendingKey = key;
        m_pending = true;
        return Request::DrawDirect;
    }

    void setTile(const QImage& tile) { m_tile = tile; }
    const QImage& tile() const { return m_tile; }

    void clear()
    {
        m_tile = QImage();
        m_pending = false;
    }

    static bool supportsDevicePixelRatio(qreal dpr, qreal scale)
    {
        return qFuzzyCompare(dpr, scale) && qFuzzyCompare(dpr, qRound(dpr));
    }

private:
    Key m_tileKey;
    QImage m_tile;
    Key m_pendingKey;
    bool m_pending{ false };
};
