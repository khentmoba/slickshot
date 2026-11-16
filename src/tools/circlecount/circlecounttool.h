// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "tools/abstracttwopointtool.h"

class CircleCountTool : public AbstractTwoPointTool
{
    Q_OBJECT
public:
    explicit CircleCountTool(QObject* parent = nullptr);

    QIcon icon(const QColor& background, bool inEditor) const override;
    QString name() const override;
    QString description() const override;
    QString info() override;
    bool isValid() const override;

    QRect mousePreviewRect(const CaptureContext& context) const override;
    QRect boundingRect() const override;

    CaptureTool* copy(QObject* parent = nullptr) override;
    void process(QPainter& painter, const QPixmap& pixmap) override;
    void paintMousePreview(QPainter& painter,
                           const CaptureContext& context) override;
    bool handleMouseWheelEvent(int, bool, CaptureContext& ctx) override;

protected:
    CaptureTool::Type type() const override;
    void copyParams(const CircleCountTool* from, CircleCountTool* to);

public slots:
    void drawStart(const CaptureContext& context) override;
    void pressed(CaptureContext& context) override;

private:
    QString m_tempString;
    bool m_valid;
    bool m_drawOutline = false;

    // Fitted pixel size for the count bubble. Its only inputs are the count,
    // the bubble size and the font family, so it is computed once per change.
    int m_cachedFontSize = -1;
    int m_cachedCount = -1;
    int m_cachedBubbleSize = -1;
    QString m_cachedFontFamily;
};
