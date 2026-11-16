// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "tools/abstracttwopointtool.h"

#include <QImage>

class PixelateTool : public AbstractTwoPointTool
{
    Q_OBJECT
public:
    explicit PixelateTool(QObject* parent = nullptr);

    QIcon icon(const QColor& background, bool inEditor) const override;
    QString name() const override;
    QString description() const override;
    QRect boundingRect() const override;

    CaptureTool* copy(QObject* parent = nullptr) override;
    void process(QPainter& painter, const QPixmap& pixmap) override;
    void drawSearchArea(QPainter& painter, const QPixmap& pixmap) override;
    void paintMousePreview(QPainter& painter,
                           const CaptureContext& context) override;

protected:
    CaptureTool::Type type() const override;

public slots:
    void pressed(CaptureContext& context) override;
    void drawMove(const QPoint& p) override;
    void drawMoveWithAdjustment(const QPoint& p) override;
    void move(const QPoint& pos) override;
    void onSizeChanged(int size) override;

private:
    // The effect is deterministic for a given selection, size, mode and source
    // pixmap, so it is computed once and reused across paints.
    struct EffectKey
    {
        QRect selection;
        int size = 0;
        bool insecure = false;
        qint64 pixmapKey = 0;
        bool operator==(const EffectKey& other) const
        {
            return selection == other.selection && size == other.size &&
                   insecure == other.insecure && pixmapKey == other.pixmapKey;
        }
    };

    void invalidateCache();
    QImage computeEffect(const QPixmap& pixmap,
                         const QRect& selection,
                         const QRect& selectionScaled,
                         qreal pixelRatio,
                         bool useInsecurePixelate) const;

    bool m_insecurePixelate = false;
    EffectKey m_effectKey;
    QImage m_cachedEffect;
    bool m_effectValid = false;
};
