// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Yurii Puchkov & Contributors

#pragma once

#include "tools/capturetool.h"

#include <QList>
#include <QPointer>

class CaptureToolObjects : public QObject
{
public:
    explicit CaptureToolObjects(QObject* parent = nullptr);
    QList<QPointer<CaptureTool>> captureToolObjects();
    void append(const QPointer<CaptureTool>& captureTool);
    void insert(int index, const QPointer<CaptureTool>& captureTool);
    void removeAt(int index);
    void clear();
    int size() const;
    int find(const QPoint& pos, QSize captureSize);
    // Draws objects with index in [first, end) onto pixmap, in layer order.
    void composeRange(QPixmap& pixmap, int first, int end) const;
    QPointer<CaptureTool> at(int index);
    CaptureToolObjects& operator=(const CaptureToolObjects& other);

private:
    int findWithRadius(QPixmap& pixmap, const QPoint& pos, int radius);

    // class members
    QList<QPointer<CaptureTool>> m_captureToolObjects;
};
