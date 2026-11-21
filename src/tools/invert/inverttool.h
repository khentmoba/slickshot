// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "tools/abstracttwopointtool.h"

#include <QImage>

class InvertTool : public AbstractTwoPointTool
{
    Q_OBJECT
public:
    explicit InvertTool(QObject* parent = nullptr);

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

private:
    struct EffectKey
    {
        QRect selection;
        qint64 pixmapKey = 0;
        qreal devicePixelRatio = 1.0;

        bool operator==(const EffectKey& other) const
        {
            return selection == other.selection &&
                   pixmapKey == other.pixmapKey &&
                   qFuzzyCompare(devicePixelRatio, other.devicePixelRatio);
        }
    };

    void invalidateCache();

    EffectKey m_effectKey;
    QImage m_cachedEffect;
    bool m_effectValid = false;
};
